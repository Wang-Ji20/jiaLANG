//===- Options.cpp - Command-line options ---------------------------------===//
#include "jia/Driver/Options.h"

#include <format>

namespace jia::driver {

std::string_view helpText() {
  return R"(OVERVIEW: jiac, the jiaLang compiler

USAGE: jiac [options] <inputs>

Inputs ending in .jia are compiled; any other input (.c, .o, .a) is passed
to the system C compiler at link time.

OPTIONS:
  -o <file>         Write output to <file> ('-' for stdout with -S)
  -c                Compile to an object file; do not link
  -S                Compile to assembly; do not link
  -emit-llvm        With -c, write LLVM bitcode; with -S, LLVM IR text
  -O0 -O1 -O2 -O3   Optimization level (default -O0)
  -fsyntax-only     Check the program and stop
  --dump-tokens     Print the tokens of each input
  --dump-ast        Print the typed syntax tree of each input
  --target=<triple> Generate code for <triple> (default: the host)
  -l<lib> -L<dir>   Passed to the linker
  -h, --help        Show this help
  --version         Show the version

ENVIRONMENT:
  CC                The C compiler used to link (default: cc)
)";
}

std::expected<Options, std::string>
parseArguments(std::span<const std::string_view> args) {
  Options opts;
  for (std::size_t i = 0; i < args.size(); ++i) {
    const std::string_view arg = args[i];
    if (arg == "-h" || arg == "--help") {
      opts.action = Action::Help;
    } else if (arg == "--version") {
      opts.action = Action::Version;
    } else if (arg == "-c") {
      opts.action = Action::Compile;
    } else if (arg == "-S") {
      opts.action = Action::Assemble;
    } else if (arg == "-fsyntax-only") {
      opts.action = Action::SyntaxOnly;
    } else if (arg == "--dump-tokens") {
      opts.action = Action::DumpTokens;
    } else if (arg == "--dump-ast") {
      opts.action = Action::DumpAst;
    } else if (arg == "-emit-llvm") {
      opts.emitLLVM = true;
    } else if (arg.size() == 3 && arg.starts_with("-O") && arg[2] >= '0' &&
               arg[2] <= '3') {
      opts.optLevel = static_cast<unsigned>(arg[2] - '0');
    } else if (arg == "-o") {
      if (i + 1 == args.size())
        return std::unexpected("argument to '-o' is missing");
      opts.output = std::string(args[++i]);
    } else if (arg.starts_with("--target=")) {
      opts.targetTriple = std::string(arg.substr(9));
    } else if (arg.starts_with("-l") || arg.starts_with("-L")) {
      opts.linkerInputs.emplace_back(arg);
    } else if (arg.starts_with("-") && arg != "-") {
      return std::unexpected(std::format("unknown argument: '{}'", arg));
    } else if (arg.ends_with(".jia") || arg == "-") {
      opts.sources.emplace_back(arg);
    } else {
      opts.linkerInputs.emplace_back(arg);
    }
  }

  const bool stopsEarly =
      opts.action == Action::Compile || opts.action == Action::Assemble;
  if (stopsEarly && opts.output && opts.sources.size() > 1)
    return std::unexpected(
        "cannot specify '-o' with '-c' or '-S' and multiple files");
  return opts;
}

} // namespace jia::driver
