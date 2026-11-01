//===- Literal.h - Decoding literal tokens ----------------------*- C++ -*-===//
//
// The lexer only finds where a literal begins and ends. These pure
// functions turn its text into a value, or explain why they cannot.
//
//===----------------------------------------------------------------------===//
#pragma once

#include <cstdint>
#include <expected>
#include <string>
#include <string_view>

namespace jia {

/// Decimal, octal (leading 0), or hex (0x) integer, without suffixes.
[[nodiscard]] std::expected<std::uint64_t, std::string>
parseIntLiteral(std::string_view text);

/// Decimal floating literal, optionally ending in 'f' or 'F'.
[[nodiscard]] std::expected<double, std::string>
parseFloatLiteral(std::string_view text);

/// A quoted character such as 'a' or '\n' (quotes included).
[[nodiscard]] std::expected<char, std::string>
parseCharLiteral(std::string_view text);

/// A quoted string such as "hi\n" (quotes included). The result holds the
/// decoded bytes, without the terminating NUL.
[[nodiscard]] std::expected<std::string, std::string>
parseStringLiteral(std::string_view text);

} // namespace jia
