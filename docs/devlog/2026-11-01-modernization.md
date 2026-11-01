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
