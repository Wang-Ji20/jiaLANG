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

## Back end

- [x] LLVM IR generation for every construct; short-circuit `&&`/`||`;
      C-compatible calls including variadic ones; constant global
      initializers
- [x] Driver: `-c`, `-S`, `-emit-llvm`, `-O0..3`, `--target`,
      `-fsyntax-only`, `--dump-tokens`, `--dump-ast`; links with `$CC`

## Testing

- [x] Unit tests (`tests/unit`, 33 cases)
- [x] `jia-lit`: self-contained lit/FileCheck replacement
- [x] Check tests for the driver, dumps, and diagnostics of each phase
- [x] Differential end-to-end tests against the system C compiler at
      `-O0` and `-O2`: arithmetic, control flow, short-circuit, pointers,
      arrays, strings, recursion
- [x] The 2022 test program passes unchanged (`tests/e2e/legacy.jia`)

## Documentation

- [x] README, getting started, user manual (CLI, language reference,
      grammar, C interop), architecture, design docs, glossary
- [x] `jia-bench`: six programs against `cc` at `-O0`/`-O2`, plus compile
      throughput; `bench-smoke` in ctest. Baseline in
      `docs/research/benchmark-baseline-2026-11-01.md`

## Milestones

See the [roadmap](roadmap.md) for what each milestone means.

- [x] M0 — Jia on LLVM
- [ ] Phase 0 — harden the base (CI, sanitizers, warnings, source
      manager, TargetInfo, fuzzing); see the [plan](plan.md)
- [ ] M1 — C scalar types and operators
- [ ] M2 — aggregates and declarations
- [ ] M3 — preprocessor
- [ ] M4 — system headers and GNU C
- [ ] M5 — real programs: Lua, hiredis, SQLite
- [ ] M6 — Redis on LLVM
- [ ] M7 — own back end (x86-64, AArch64, RISC-V)
- [ ] M8 — own assembler
- [ ] M9 — own linker
- [ ] M10 — own C library
- [ ] M11 — Redis with no external tools
