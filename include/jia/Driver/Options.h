//===- Options.h - Command-line options -------------------------*- C++ -*-===//
#pragma once

#include <expected>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace jia::driver {

/// What the user asked jiac to do.
enum class Action {
  Link,       // (default) build an executable
  Compile,    // -c: stop at an object (or bitcode with -emit-llvm)
  Assemble,   // -S: stop at assembly (or textual IR with -emit-llvm)
  SyntaxOnly, // -fsyntax-only: check the program, produce nothing
  DumpTokens, // --dump-tokens
  DumpAst,    // --dump-ast: print the typed tree
  Help,
  Version,
};

struct Options {
  Action action = Action::Link;
  bool emitLLVM = false;
  unsigned optLevel = 0;
  std::optional<std::string> output;
  std::string targetTriple;              // empty: the host
  std::vector<std::string> sources;      // .jia files we compile
  std::vector<std::string> linkerInputs; // everything else, passed to cc
};

/// Parses the arguments after the program name. Pure: it only reads
/// \p args. Returns an error message for unknown or malformed options.
[[nodiscard]] std::expected<Options, std::string>
parseArguments(std::span<const std::string_view> args);

[[nodiscard]] std::string_view helpText();

} // namespace jia::driver
