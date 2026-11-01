//===- Token.h - Lexical tokens ---------------------------------*- C++ -*-===//
#pragma once

#include "jia/Basic/SourceLocation.h"

#include <cstdint>
#include <string_view>

namespace jia {

enum class TokenKind : std::uint8_t {
#define TOKEN(Name, Spelling) Name,
#include "jia/Lex/TokenKinds.def"
};

/// A token is a kind plus the exact source text it came from.
///
/// \c text borrows from the source buffer passed to lex(); the buffer must
/// outlive every token. Literal values are decoded later, by the parser,
/// with the functions in Literal.h.
struct Token {
  TokenKind kind = TokenKind::Eof;
  std::string_view text;
  SourceLocation loc;

  [[nodiscard]] bool is(TokenKind k) const { return kind == k; }
};

/// A human-readable name: the spelling for keywords and punctuation
/// ("while", "=="), a description otherwise ("identifier").
[[nodiscard]] std::string_view spelling(TokenKind kind);

/// The enumerator name, e.g. "KwWhile"; used by --dump-tokens and tests.
[[nodiscard]] std::string_view kindName(TokenKind kind);

} // namespace jia
