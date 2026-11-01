//===- Lexer.cpp - Source text to tokens ----------------------------------===//
#include "jia/Lex/Lexer.h"

#include <array>
#include <optional>
#include <format>
#include <utility>

namespace jia {

std::string_view spelling(TokenKind kind) {
  static constexpr std::array names = {
#define TOKEN(Name, Spelling) std::string_view(Spelling),
#include "jia/Lex/TokenKinds.def"
  };
  return names[static_cast<std::size_t>(kind)];
}

std::string_view kindName(TokenKind kind) {
  static constexpr std::array names = {
#define TOKEN(Name, Spelling) std::string_view(#Name),
#include "jia/Lex/TokenKinds.def"
  };
  return names[static_cast<std::size_t>(kind)];
}

namespace {

constexpr std::array keywords = {
#define KEYWORD(Name, Spelling) std::pair{std::string_view(Spelling), TokenKind::Name},
#include "jia/Lex/TokenKinds.def"
};

constexpr std::array punctuators = {
#define PUNCT(Name, Spelling) std::pair{std::string_view(Spelling), TokenKind::Name},
#include "jia/Lex/TokenKinds.def"
};

bool isIdentStart(char c) {
  return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}
bool isDigit(char c) { return c >= '0' && c <= '9'; }
bool isIdentBody(char c) { return isIdentStart(c) || isDigit(c); }

/// The lexer's private state: a cursor into the source. It never escapes
/// lex(), so lex() stays a pure function.
class Lexer {
public:
  explicit Lexer(std::string_view source) : src_(source) {}

  Result<std::vector<Token>> run() {
    std::vector<Token> tokens;
    while (true) {
      skipTrivia();
      if (atEnd()) {
        tokens.push_back({TokenKind::Eof, src_.substr(pos_, 0), here()});
        break;
      }
      if (auto token = next())
        tokens.push_back(*token);
    }
    if (!diags_.empty())
      return std::unexpected(std::move(diags_));
    return tokens;
  }

private:
  [[nodiscard]] bool atEnd() const { return pos_ >= src_.size(); }
  [[nodiscard]] char peek(std::size_t ahead = 0) const {
    return pos_ + ahead < src_.size() ? src_[pos_ + ahead] : '\0';
  }
  [[nodiscard]] SourceLocation here() const { return {line_, column_}; }

  void advance() {
    if (src_[pos_] == '\n') {
      ++line_;
      column_ = 1;
    } else {
      ++column_;
    }
    ++pos_;
  }

  void error(SourceLocation loc, std::string message) {
    diags_.push_back(makeError(loc, std::move(message)));
  }

  void skipTrivia() {
    while (!atEnd()) {
      const char c = peek();
      if (c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' ||
          c == '\f') {
        advance();
      } else if (c == '/' && peek(1) == '/') {
        while (!atEnd() && peek() != '\n')
          advance();
      } else if (c == '/' && peek(1) == '*') {
        const SourceLocation start = here();
        advance();
        advance();
        while (!atEnd() && !(peek() == '*' && peek(1) == '/'))
          advance();
        if (atEnd()) {
          error(start, "unterminated /* comment");
          return;
        }
        advance();
        advance();
      } else {
        return;
      }
    }
  }

  Token make(TokenKind kind, std::size_t begin, SourceLocation loc) const {
    return {kind, src_.substr(begin, pos_ - begin), loc};
  }

  std::optional<Token> next() {
    const std::size_t begin = pos_;
    const SourceLocation loc = here();
    const char c = peek();

    if (isIdentStart(c)) {
      while (isIdentBody(peek()))
        advance();
      const std::string_view word = src_.substr(begin, pos_ - begin);
      for (const auto &[text, kind] : keywords)
        if (text == word)
          return make(kind, begin, loc);
      return make(TokenKind::Identifier, begin, loc);
    }

    if (isDigit(c) || (c == '.' && isDigit(peek(1))))
      return number(begin, loc);

    if (c == '\'' || c == '"')
      return quoted(c, begin, loc);

    for (const auto &[text, kind] : punctuators) {
      if (src_.substr(pos_).starts_with(text)) {
        for (std::size_t i = 0; i < text.size(); ++i)
          advance();
        return make(kind, begin, loc);
      }
    }

    advance();
    error(loc, std::format("unexpected character '{}'", c));
    return std::nullopt;
  }

  /// Scans a "preprocessing number": digits, letters, dots, and a sign
  /// right after an exponent letter. Literal.h decides if it is valid.
  Token number(std::size_t begin, SourceLocation loc) {
    const bool hex = peek() == '0' && (peek(1) == 'x' || peek(1) == 'X');
    bool isFloat = false;
    while (isIdentBody(peek()) || peek() == '.') {
      const char c = peek();
      if (c == '.' || (!hex && (c == 'e' || c == 'E')))
        isFloat = true;
      advance();
      if (!hex && (c == 'e' || c == 'E') && (peek() == '+' || peek() == '-'))
        advance();
    }
    return make(isFloat ? TokenKind::FloatLiteral : TokenKind::IntLiteral,
                begin, loc);
  }

  std::optional<Token> quoted(char quote, std::size_t begin,
                              SourceLocation loc) {
    advance(); // opening quote
    while (!atEnd() && peek() != quote && peek() != '\n') {
      if (peek() == '\\' && pos_ + 1 < src_.size())
        advance();
      advance();
    }
    if (peek() != quote) {
      error(loc, quote == '"' ? "missing terminating '\"' character"
                              : "missing terminating ' character");
      return std::nullopt;
    }
    advance(); // closing quote
    return make(quote == '"' ? TokenKind::StringLiteral
                             : TokenKind::CharLiteral,
                begin, loc);
  }

  std::string_view src_;
  std::size_t pos_ = 0;
  std::uint32_t line_ = 1;
  std::uint32_t column_ = 1;
  Diagnostics diags_;
};

} // namespace

Result<std::vector<Token>> lex(std::string_view source) {
  return Lexer(source).run();
}

} // namespace jia
