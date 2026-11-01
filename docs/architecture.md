# Architecture

This page explains how the code is organized and how a program flows
through the compiler. For *why* it is organized this way, see the
[design docs](design/).

## The pipeline

A compiler is a chain of translations. Each stage takes the previous
stage's output and produces something closer to machine code:

```
source text ──lex──▶ tokens ──parse──▶ AST ──analyze──▶ typed AST
    ──generate──▶ LLVM IR ──optimize──▶ LLVM IR ──emit──▶ object file ──link──▶ executable
```

Each arrow is one library, and each library exposes one pure function.

## Libraries

Like LLVM, the compiler is a stack of small libraries. A library may use
only the libraries below it in this table.

| Library | Header | Responsibility | Status |
|---|---|---|---|
| `jiaDriver` | `jia/Driver/` | Command line, files, running the pipeline, linking | done |
| `jiaCodeGen` | `jia/CodeGen/` | Typed AST → LLVM IR | done |
| `jiaSema` | `jia/Sema/` | Name resolution, type checking, implicit conversions | done |
| `jiaParse` | `jia/Parse/` | Tokens → AST | done |
| `jiaAST` | `jia/AST/` | Tree and type data structures, AST dumper, constant folding | done |
| `jiaLex` | `jia/Lex/` | Source text → tokens; literal decoding | done |
| `jiaBasic` | `jia/Basic/` | Source locations, diagnostics, `Result<T>` | done |

## Directory layout

```
include/jia/<Lib>/   public headers of each library
lib/<Lib>/           implementation of each library
tools/jiac/          the compiler executable (a thin wrapper over jiaDriver)
tests/unit/          unit tests of library functions
tests/check/         FileCheck tests of compiler output and diagnostics
tests/e2e/           programs that are compiled, run, and checked
bench/               benchmarks
docs/                documentation (see AGENTS.md for the map)
```

## The driver: a functional core in an imperative shell

Everything below `jiaDriver` is pure: no files, no printing, no
processes. The driver is the only place with side effects. It

1. parses the command line (`parseArguments`, itself a pure function);
2. reads each source file and keeps its text alive while its tokens
   borrow from it;
3. runs the pure front end, `frontEnd(source) = lex → parse → analyze`,
   written as a chain of `Result::and_then` calls;
4. generates an LLVM module, then optimizes and emits it
   (`Backend.h`, a thin wrapper over LLVM's target machinery);
5. links the objects by running the system C compiler (`$CC`, default
   `cc`), which also supplies the C library.

`tools/jiac/jiac.cpp` is fifteen lines: it parses arguments and calls
`run`.

## Errors

No phase prints or exits. A phase that finds errors returns them as
`Diagnostics` inside a `Result<T>`; only the driver turns them into text
with `jia::render`. See `include/jia/Basic/Diagnostic.h`.
