//===- Driver.h - The compiler pipeline -------------------------*- C++ -*-===//
//
// The driver is the "imperative shell" around the pure phases: it reads
// files, prints diagnostics, writes outputs, and runs the linker. Nothing
// below it touches the outside world.
//
//===----------------------------------------------------------------------===//
#pragma once

#include "jia/AST/AST.h"
#include "jia/Basic/Diagnostic.h"
#include "jia/Driver/Options.h"

#include <string_view>

namespace jia::driver {

/// The pure front end: lex, parse, and analyze \p source.
[[nodiscard]] Result<TranslationUnit> frontEnd(std::string_view source);

/// Carries out \p opts. Returns the process exit code.
[[nodiscard]] int run(const Options &opts);

} // namespace jia::driver
