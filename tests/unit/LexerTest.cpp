#include "Suites.h"
#include "jia/Lex/Lexer.h"
#include "jia/Lex/Literal.h"

namespace jia::test {
namespace {

/// "Kind:text Kind:text ..." for every token, or the first error message.
std::string summarize(std::string_view source) {
  const auto tokens = lex(source);
  if (!tokens)
    return "error: " + tokens.error().front().message;
  std::string out;
  for (const Token &tok : *tokens) {
    if (!out.empty())
      out += ' ';
    out += kindName(tok.kind);
    if (!tok.text.empty())
      out += std::format(":{}", tok.text);
  }
  return out;
}

} // namespace

const TestSuite lexerSuite{
    "lexer",
    {
        {"keywords and identifiers",
         [](Tester &t) {
           t.expectEq(summarize("int intx _y while"),
                      std::string("KwInt:int Identifier:intx Identifier:_y "
                                  "KwWhile:while Eof"));
         }},
        {"longest punctuator wins",
         [](Tester &t) {
           t.expectEq(summarize("a<=b==c=!d..."),
                      std::string("Identifier:a LessEqual:<= Identifier:b "
                                  "EqualEqual:== Identifier:c Equal:= "
                                  "Exclaim:! Identifier:d Ellipsis:... Eof"));
         }},
        {"numbers",
         [](Tester &t) {
           t.expectEq(summarize("42 0x1F 3.5 1e-3 .5f"),
                      std::string("IntLiteral:42 IntLiteral:0x1F "
                                  "FloatLiteral:3.5 FloatLiteral:1e-3 "
                                  "FloatLiteral:.5f Eof"));
         }},
        {"comments are skipped",
         [](Tester &t) {
           t.expectEq(summarize("a /* x\n y */ b // c\n d"),
                      std::string("Identifier:a Identifier:b Identifier:d Eof"));
         }},
        {"locations track lines and columns",
         [](Tester &t) {
           const auto tokens = lex("a\n  bc");
           t.expect(tokens.has_value());
           t.expectEq((*tokens)[1].loc.line, 2u);
           t.expectEq((*tokens)[1].loc.column, 3u);
         }},
        {"reports bad characters and unterminated tokens",
         [](Tester &t) {
           t.expectEq(summarize("a @ b"),
                      std::string("error: unexpected character '@'"));
           t.expectEq(summarize("\"abc"),
                      std::string("error: missing terminating '\"' character"));
           t.expectEq(summarize("/* x"),
                      std::string("error: unterminated /* comment"));
           const auto all = lex("@ $");
           t.expectEq(all.error().size(), std::size_t{2});
         }},
        {"decodes literals",
         [](Tester &t) {
           t.expectEq(*parseIntLiteral("0x1F"), std::uint64_t{31});
           t.expectEq(*parseIntLiteral("017"), std::uint64_t{15});
           t.expectEq(parseIntLiteral("12abc").error(),
                      std::string("invalid integer literal '12abc'"));
           t.expectEq(*parseFloatLiteral("2.5f"), 2.5);
           t.expectEq(*parseCharLiteral("'\\n'"), '\n');
           t.expectEq(*parseCharLiteral("'\\x41'"), 'A');
           t.expectEq(*parseStringLiteral("\"a\\tb\\0\""),
                      std::string("a\tb\0", 4));
           t.expectEq(parseCharLiteral("'\\q'").error(),
                      std::string("unknown escape sequence '\\q'"));
         }},
    }};

} // namespace jia::test
