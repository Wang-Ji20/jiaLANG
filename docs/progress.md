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
- [x] Parser: declarations with C declarators (pointers, arrays,
      prototypes, `...`), initializer lists, all statements, all
      operators, casts, `sizeof`; error recovery
- [x] Sema: scopes, symbol resolution, type checking, implicit
      conversions, pointer arithmetic, constant global initializers,
      array-size inference, non-cascading errors
