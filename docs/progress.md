# Progress

What works today. Update this page whenever a feature lands or a known
bug turns up.

## Infrastructure

- [x] CMake build against LLVM 21+
- [x] Diagnostics with source excerpts (`jiaBasic`)
- [x] Unit test runner (`tests/unit`)

## Front end

- [x] Lexer: keywords, identifiers, decimal/octal/hex integers, floats,
      char and string literals with escapes, `//` and `/* */` comments
- [x] AST as `std::variant` sum types; uniqued types; AST dumper
- [x] Constant folding shared by parser, Sema, and code generation
