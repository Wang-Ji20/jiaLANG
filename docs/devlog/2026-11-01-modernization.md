# 2026-11-01 — Modernization begins

## Goal

Migrate the 2022 course compiler to modern C++ and current LLVM, and lay
the groundwork for a C compiler that can build Redis.

## Log

- Reviewed the legacy code; findings are in
  [research/legacy-code-review.md](../research/legacy-code-review.md).
  Decision: rebuild with a hand-written front end and a real semantic
  analysis phase.
- Wrote the working agreement (`AGENTS.md`) and the C++ subset
  ([design/cpp-subset.md](../design/cpp-subset.md)).
- Toolchain on the dev machine: g++ 16, clang 23, LLVM 23.1.4
  (`llvm23-devel`), CMake 4.4, Ninja, FileCheck, Python 3.13.
- Added the CMake build and the `jiaBasic` library (source locations,
  diagnostics, `Result<T>`), plus a macro-free unit test runner. Lesson:
  a namespace-scope `const` object has internal linkage, so test suites
  must be declared `extern` in a shared header (`tests/unit/Suites.h`).
- Wrote the hand-written lexer (`jiaLex`) and its design note.
- Added `jiaAST`: variant-based tree, uniqued types, dumper, constant folder. Turned off `-Wmissing-field-initializers`, which fires on designated initializers that rely on default member values.
- Wrote the recursive-descent parser (`jiaParse`). Bug caught while writing it: if a block contained an error, returning failure from `parseCompound` made file-scope recovery skip the *next* declaration too; the block is now kept and only its broken statements dropped.
- Wrote semantic analysis (`jiaSema`) with 8 test groups covering conversions, desugaring, scopes, and error messages.
- Wrote code generation (`jiaCodeGen`). LLVM 23 note: `BasicBlock::getTerminator()` now asserts on blocks without one; use `hasTerminator()` to ask.
