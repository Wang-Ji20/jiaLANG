//===- Diagnostic.h - Errors as values --------------------------*- C++ -*-===//
//
// Compiler phases never throw and never print. A phase that finds a mistake
// in the user's program returns Diagnostics instead of a result; the driver
// decides how to show them. Result<T> is the Rust-style return type of
// every fallible phase.
//
//===----------------------------------------------------------------------===//
#pragma once

#include "jia/Basic/SourceLocation.h"

#include <cstdint>
#include <expected>
#include <string>
#include <string_view>
#include <vector>

namespace jia {

enum class Severity : std::uint8_t { Error, Warning, Note };

struct Diagnostic {
  Severity severity = Severity::Error;
  SourceLocation loc;
  std::string message;
};

using Diagnostics = std::vector<Diagnostic>;

/// The result of a fallible phase: a value, or the diagnostics explaining
/// why there is none.
template <class T> using Result = std::expected<T, Diagnostics>;

[[nodiscard]] inline Diagnostic makeError(SourceLocation loc,
                                          std::string message) {
  return {.severity = Severity::Error, .loc = loc, .message = std::move(message)};
}

/// A failed Result holding a single error.
[[nodiscard]] inline std::unexpected<Diagnostics> fail(SourceLocation loc,
                                                       std::string message) {
  return std::unexpected(Diagnostics{makeError(loc, std::move(message))});
}

/// Formats a diagnostic the way GCC and Clang do:
///
///   file.jia:3:7: error: use of undeclared identifier 'x'
///       3 |   y = x + 1;
///         |       ^
///
/// \p source is the whole file text; pass an empty view to omit the excerpt.
[[nodiscard]] std::string render(const Diagnostic &diag,
                                 std::string_view fileName,
                                 std::string_view source);

} // namespace jia
