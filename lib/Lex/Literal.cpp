//===- Literal.cpp - Decoding literal tokens ------------------------------===//
#include "jia/Lex/Literal.h"

#include <charconv>
#include <format>

namespace jia {
namespace {

int hexValue(char c) {
  if (c >= '0' && c <= '9')
    return c - '0';
  if (c >= 'a' && c <= 'f')
    return c - 'a' + 10;
  if (c >= 'A' && c <= 'F')
    return c - 'A' + 10;
  return -1;
}

/// Decodes one character or escape sequence at the front of \p body and
/// removes it from \p body.
std::expected<char, std::string> decodeOne(std::string_view &body) {
  if (body.front() != '\\') {
    const char c = body.front();
    body.remove_prefix(1);
    return c;
  }
  body.remove_prefix(1);
  if (body.empty())
    return std::unexpected("incomplete escape sequence");
  const char e = body.front();
  body.remove_prefix(1);
  switch (e) {
  case 'n': return '\n';
  case 't': return '\t';
  case 'r': return '\r';
  case 'a': return '\a';
  case 'b': return '\b';
  case 'f': return '\f';
  case 'v': return '\v';
  case '\\': return '\\';
  case '\'': return '\'';
  case '"': return '"';
  case '?': return '?';
  case 'x': {
    unsigned value = 0;
    int digits = 0;
    while (!body.empty() && hexValue(body.front()) >= 0) {
      value = value * 16 + static_cast<unsigned>(hexValue(body.front()));
      body.remove_prefix(1);
      ++digits;
    }
    if (digits == 0)
      return std::unexpected("\\x used with no following hex digits");
    if (value > 0xFF)
      return std::unexpected("hex escape sequence out of range");
    return static_cast<char>(value);
  }
  default:
    if (e >= '0' && e <= '7') {
      unsigned value = static_cast<unsigned>(e - '0');
      for (int i = 0; i < 2 && !body.empty() && body.front() >= '0' &&
                      body.front() <= '7';
           ++i) {
        value = value * 8 + static_cast<unsigned>(body.front() - '0');
        body.remove_prefix(1);
      }
      if (value > 0xFF)
        return std::unexpected("octal escape sequence out of range");
      return static_cast<char>(value);
    }
    return std::unexpected(std::format("unknown escape sequence '\\{}'", e));
  }
}

} // namespace

std::expected<std::uint64_t, std::string>
parseIntLiteral(std::string_view text) {
  int base = 10;
  std::string_view digits = text;
  if (text.size() > 2 && text[0] == '0' && (text[1] == 'x' || text[1] == 'X')) {
    base = 16;
    digits.remove_prefix(2);
  } else if (text.size() > 1 && text[0] == '0') {
    base = 8;
    digits.remove_prefix(1);
  }
  std::uint64_t value = 0;
  const auto [end, ec] =
      std::from_chars(digits.data(), digits.data() + digits.size(), value, base);
  if (ec == std::errc::result_out_of_range)
    return std::unexpected("integer literal is too large");
  if (ec != std::errc() || end != digits.data() + digits.size())
    return std::unexpected(std::format("invalid integer literal '{}'", text));
  return value;
}

std::expected<double, std::string> parseFloatLiteral(std::string_view text) {
  std::string_view body = text;
  if (body.ends_with('f') || body.ends_with('F'))
    body.remove_suffix(1);
  double value = 0;
  const auto [end, ec] = std::from_chars(body.data(), body.data() + body.size(),
                                         value, std::chars_format::general);
  if (ec == std::errc::result_out_of_range)
    return std::unexpected("floating literal is out of range");
  if (ec != std::errc() || end != body.data() + body.size())
    return std::unexpected(std::format("invalid floating literal '{}'", text));
  return value;
}

std::expected<char, std::string> parseCharLiteral(std::string_view text) {
  std::string_view body = text.substr(1, text.size() - 2);
  if (body.empty())
    return std::unexpected("empty character constant");
  auto c = decodeOne(body);
  if (c && !body.empty())
    return std::unexpected("multi-character character constant");
  return c;
}

std::expected<std::string, std::string>
parseStringLiteral(std::string_view text) {
  std::string_view body = text.substr(1, text.size() - 2);
  std::string out;
  while (!body.empty()) {
    auto c = decodeOne(body);
    if (!c)
      return std::unexpected(c.error());
    out += *c;
  }
  return out;
}

} // namespace jia
