//===- Driver.cpp - The compiler pipeline ---------------------------------===//
#include "jia/Driver/Driver.h"

#include "jia/AST/ASTDumper.h"
#include "jia/CodeGen/CodeGen.h"
#include "jia/Driver/Backend.h"
#include "jia/Lex/Lexer.h"
#include "jia/Parse/Parser.h"
#include "jia/Sema/Sema.h"

#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Support/Path.h"
#include "llvm/Support/Program.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Target/TargetMachine.h"

#include <cstdlib>
#include <format>

namespace jia::driver {

Result<TranslationUnit> frontEnd(std::string_view source) {
  return lex(source)
      .and_then([](std::vector<Token> tokens) { return parse(tokens); })
      .and_then(analyze);
}

namespace {

constexpr std::string_view version = "jiac 0.2.0 (jiaLang)";

void reportError(std::string_view message) {
  llvm::errs() << "jiac: error: " << message << '\n';
}

void report(const Diagnostics &diags, std::string_view file,
            std::string_view source) {
  for (const Diagnostic &diag : diags)
    llvm::errs() << render(diag, file, source);
}

/// "dir/prog.jia" with extension ".o" becomes "prog.o", like cc does.
std::string replaceExtension(const std::string &path, std::string_view ext) {
  llvm::SmallString<128> name(llvm::sys::path::filename(path));
  llvm::sys::path::replace_extension(name, ext);
  return std::string(name);
}

/// Links \p objects and the user's extra inputs with the system C compiler.
int link(const std::vector<std::string> &objects, const Options &opts) {
  const char *ccEnv = std::getenv("CC");
  const std::string ccName = ccEnv && *ccEnv ? ccEnv : "cc";
  const auto cc = llvm::sys::findProgramByName(ccName);
  if (!cc) {
    reportError(std::format("cannot find the linker driver '{}'", ccName));
    return 1;
  }
  std::vector<std::string> argv = {ccName};
  argv.insert(argv.end(), objects.begin(), objects.end());
  argv.insert(argv.end(), opts.linkerInputs.begin(), opts.linkerInputs.end());
  argv.push_back("-o");
  argv.push_back(opts.output.value_or("a.out"));
  const std::vector<llvm::StringRef> refs(argv.begin(), argv.end());
  std::string error;
  const int status =
      llvm::sys::ExecuteAndWait(*cc, refs, std::nullopt, {}, 0, 0, &error);
  if (status != 0) {
    reportError(error.empty() ? "linker command failed" : error);
    return 1;
  }
  return 0;
}

class Compilation {
public:
  explicit Compilation(const Options &opts) : opts_(opts) {}

  int run() {
    if (opts_.sources.empty() && opts_.linkerInputs.empty()) {
      reportError("no input files");
      return 1;
    }
    if (needsBackend()) {
      initializeTargets();
      auto machine = createTargetMachine(opts_.targetTriple, opts_.optLevel);
      if (!machine) {
        reportError(machine.error());
        return 1;
      }
      machine_ = std::move(*machine);
    }

    bool ok = true;
    for (const std::string &source : opts_.sources)
      ok &= compile(source);

    int status = ok ? 0 : 1;
    if (ok && opts_.action == Action::Link)
      status = link(objects_, opts_);
    for (const std::string &temp : temporaries_)
      llvm::sys::fs::remove(temp);
    return status;
  }

private:
  bool needsBackend() const {
    return opts_.action == Action::Link || opts_.action == Action::Compile ||
           opts_.action == Action::Assemble;
  }

  bool compile(const std::string &path) {
    auto buffer = llvm::MemoryBuffer::getFileOrSTDIN(path);
    if (!buffer) {
      reportError(std::format("cannot open '{}': {}", path,
                              buffer.getError().message()));
      return false;
    }
    // The source text must outlive the tokens, which borrow from it.
    const std::string_view source = (*buffer)->getBuffer();

    if (opts_.action == Action::DumpTokens)
      return dumpTokens(path, source);

    auto tu = frontEnd(source);
    if (!tu) {
      report(tu.error(), path, source);
      return false;
    }
    if (opts_.action == Action::SyntaxOnly)
      return true;
    if (opts_.action == Action::DumpAst) {
      llvm::outs() << dumpAST(*tu);
      return true;
    }

    llvm::LLVMContext context;
    auto module = generateModule(*tu, context, path);
    if (!module) {
      report(module.error(), path, source);
      return false;
    }
    configureModule(**module, *machine_);
    optimize(**module, *machine_, opts_.optLevel);

    const auto [kind, output] = outputFor(path);
    if (!output)
      return false;
    if (auto written = emit(**module, *machine_, kind, *output); !written) {
      reportError(written.error());
      return false;
    }
    return true;
  }

  /// Where and in what form to write the result for \p path.
  std::pair<OutputKind, std::optional<std::string>>
  outputFor(const std::string &path) {
    switch (opts_.action) {
    case Action::Compile: {
      const auto kind =
          opts_.emitLLVM ? OutputKind::Bitcode : OutputKind::Object;
      return {kind, opts_.output.value_or(
                        replaceExtension(path, opts_.emitLLVM ? "bc" : "o"))};
    }
    case Action::Assemble: {
      const auto kind =
          opts_.emitLLVM ? OutputKind::LLVMText : OutputKind::Assembly;
      return {kind, opts_.output.value_or(
                        replaceExtension(path, opts_.emitLLVM ? "ll" : "s"))};
    }
    default: {
      llvm::SmallString<128> temp;
      if (const auto ec =
              llvm::sys::fs::createTemporaryFile("jiac", "o", temp)) {
        reportError(
            std::format("cannot create a temporary file: {}", ec.message()));
        return {OutputKind::Object, std::nullopt};
      }
      temporaries_.emplace_back(temp);
      objects_.emplace_back(temp);
      return {OutputKind::Object, std::string(temp)};
    }
    }
  }

  bool dumpTokens(const std::string &path, std::string_view source) {
    const auto tokens = lex(source);
    if (!tokens) {
      report(tokens.error(), path, source);
      return false;
    }
    for (const Token &tok : *tokens)
      llvm::outs() << std::format("{} '{}' {}:{}\n", kindName(tok.kind),
                                  tok.text, tok.loc.line, tok.loc.column);
    return true;
  }

  const Options &opts_;
  std::unique_ptr<llvm::TargetMachine> machine_;
  std::vector<std::string> objects_;
  std::vector<std::string> temporaries_;
};

} // namespace

int run(const Options &opts) {
  switch (opts.action) {
  case Action::Help:
    llvm::outs() << helpText();
    return 0;
  case Action::Version:
    llvm::outs() << version << '\n';
    return 0;
  default:
    return Compilation(opts).run();
  }
}

} // namespace jia::driver
