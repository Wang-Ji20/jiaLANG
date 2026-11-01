# Design: Lexing

The *lexer* is the first phase. It reads raw text and cuts it into
*tokens*, the "words" of the language: `while`, `x`, `<=`, `42`. Every
later phase works on tokens, never on characters.

```cpp
Result<std::vector<Token>> lex(std::string_view source);
```

## Decisions

**Hand-written, not generated.** The 2022 compiler used Flex. A
hand-written lexer is about the same length, needs no extra tool, gives
us full control over error messages, and is what every production C
compiler uses. It also prepares us for C's preprocessor, which works on
tokens and is hard to bolt onto a generated lexer.

**One table of token kinds.** `TokenKinds.def` lists every kind once, with
its spelling. The enum, the keyword lookup, the punctuator list, and the
names printed by `--dump-tokens` all expand from that table, so they
cannot drift apart. Adding a keyword is a one-line change.

**Tokens borrow their text.** A `Token` holds a `std::string_view` into
the source instead of a copy. Lexing allocates only the token vector.
The price is a lifetime rule: the source must outlive the tokens. The
driver keeps the file's text alive for the whole compilation, and the
parser copies names into the tree, so nothing after parsing depends on
the source buffer.

**Values are decoded later.** The lexer decides only *where* a literal
starts and ends. `Literal.h` decodes the value (`0x1F` → 31, `'\n'` →
10). Keeping decoding separate makes both halves simple, pure, and
testable on their own. Like C's "preprocessing numbers", the lexer
accepts any run of digits, letters, and dots as one number token, then
the decoder rejects malformed ones such as `12abc` with a clear message.

**Report all errors.** On a bad character, the lexer records a diagnostic,
skips the character, and keeps going. A user who has three typos sees
three errors in one run.

## Longest match

When several punctuators share a prefix (`=` and `==`), the lexer takes
the longest one. The table lists longer spellings first, so taking the
first match is enough.
