//===- SourceLocation.h - Positions in a source file ------------*- C++ -*-===//
//
// A SourceLocation names a character in the file being compiled. Every
// token, tree node, and diagnostic carries one, so errors can point at the
// exact spot that caused them.
//
//===----------------------------------------------------------------------===//
#pragma once

#include <compare>
#include <cstdint>

namespace jia {

struct SourceLocation {
  /// 1-based line number; 0 means "unknown".
  std::uint32_t line = 0;
  /// 1-based column, counted in bytes.
  std::uint32_t column = 0;

  [[nodiscard]] constexpr bool isValid() const { return line != 0; }
  friend constexpr auto operator<=>(SourceLocation, SourceLocation) = default;
};

} // namespace jia
