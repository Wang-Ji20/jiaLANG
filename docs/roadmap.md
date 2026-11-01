# Roadmap

This page describes where jiaLang is going over the long run. The
[plan](plan.md) breaks the next stages into concrete tasks, and
[progress](progress.md) tracks what is done.

## The goal

> **jiaLang builds Redis from source and passes Redis's test suite, on
> x86-64, AArch64, and RISC-V, using only our own tools: our compiler,
> back end, assembler, linker, and C library.**

Redis is a good target because it is real, widely used C: about 200,000
lines with dependencies, using structs, unions, bit-fields, function
pointers, variadic functions, the preprocessor, POSIX threads, sockets,
and atomics. It also ships a thorough test suite, so "it works" has a
precise meaning. See
[research/redis-requirements.md](research/redis-requirements.md).

We get there in two arcs. First we become a **C compiler** that uses
LLVM as its back end and the system's assembler, linker, and C library.
Then we replace those borrowed parts one at a time, keeping Redis
building at every step.

```
Arc 1: a C compiler                 Arc 2: our own toolchain
───────────────────────             ──────────────────────────────
M0  Jia on LLVM (done)              M7  Own back end (LLVM IR → asm)
M1  C scalar types and operators    M8  Own assembler (asm → ELF .o)
M2  Aggregates and declarations     M9  Own linker (.o → executable)
M3  Preprocessor                    M10 Own C library
M4  System headers and GNU C        M11 Redis with no external tools
M5  Real programs: Lua, SQLite
M6  Redis on LLVM
```

## Arc 1: a C compiler

**M0 — Jia on LLVM (done).** The front end is a stack of pure phases
over LLVM, with diagnostics, tests, and benchmarks. The 2022 test
program passes.

**M1 — C scalar types and operators.** `long`, `short`, `unsigned`,
`double`, `_Bool`; every operator (bitwise, shifts, `++`/`--`, compound
assignment, `?:`, comma); `for`, `do`, `switch`, `goto`. A real
`TargetInfo` replaces the hard-coded LP64 sizes.
*Done when* integer-only programs from
[c-testsuite](https://github.com/c-testsuite/c-testsuite) pass.

**M2 — Aggregates and declarations.** `struct`, `union`, `enum`,
`typedef` (which makes parsing context-sensitive), function pointers,
bit-fields, `static`/`extern`, designated initializers, compound
literals, multi-file programs.
*Done when* all of c-testsuite that needs no preprocessor passes.

**M3 — Preprocessor.** Our own: `#include` with search paths, object-
and function-like macros, `#`/`##`, `__VA_ARGS__`, conditional
compilation, `#line`, predefined macros. It works on tokens, reusing the
lexer, and keeps macro-expansion locations for diagnostics.
*Done when* all of c-testsuite passes and our preprocessor's output
matches `cc -E` on a sample of real files.

**M4 — System headers and GNU C.** Real programs include glibc or musl
headers, which use extensions: `__attribute__`, `__extension__`,
`__asm__` labels, `__builtin_va_list` and `stdarg.h`, `inline`,
`restrict`, `typeof`, `_Static_assert`, `_Alignof`, `__builtin_expect`,
`__builtin_offsetof`, and the `__atomic`/`__sync` builtins. We support
what headers and Redis actually use, measured by the feature census
described in the Redis research note.
*Done when* `#include <stdio.h>`, `<stdlib.h>`, `<string.h>`,
`<pthread.h>`, and `<sys/socket.h>` compile on glibc and musl.

**M5 — Real programs.** A ladder of open-source C, each a little harder:
Redis's own dependencies `linenoise`, `hdr_histogram`, and Lua 5.1, then
`hiredis`, then SQLite, a famously demanding single-file test.
*Done when* each builds with `jiac` and passes its own tests.

**M6 — Redis on LLVM.** `make CC=jiac MALLOC=libc` builds `redis-server`
and `redis-cli`, and `make test` passes. (`MALLOC=libc` skips jemalloc,
a stretch goal of its own.)

## Arc 2: our own toolchain

Each step replaces one borrowed component. LLVM, `as`, `ld`, and glibc
remain available as *oracles*: we test each new component by comparing
it against the component it replaces, the same differential approach
our end-to-end tests already use.

**M7 — Own back end.** Translate LLVM IR into assembly for x86-64,
AArch64, and RV64GC ourselves. We keep LLVM's optimizer at first and
take over only instruction selection, register allocation, stack frame
layout, calling conventions (System V AMD64, AAPCS64, RISC-V LP64D), and
assembly printing. The first version aims for correctness with simple
code (every value in a stack slot); linear-scan register allocation
follows. Later the back end reads LLVM IR *text* through our own parser,
so it no longer links against LLVM at all.
*Done when* the full test suite and Redis pass with our back end on all
three architectures. Design starting points are in
[research/own-toolchain.md](research/own-toolchain.md).

**M8 — Own assembler.** Turn the assembly our back end prints into ELF
relocatable object files: instruction encoders for the three ISAs, the
directives we emit, symbol tables, and relocations. Because we control
the assembly syntax, the assembler only has to accept what our back end
produces. *Done when* our objects are interchangeable with `as` output
across the test suite.

**M9 — Own linker.** Link ELF objects and `.a` archives into
executables: symbol resolution, section layout, relocation processing.
Static linking comes first; dynamic linking against a system C library
(PLT/GOT) second. *Done when* the test suite and Redis link with it.

**M10 — Own C library.** Program startup (`crt0`), system calls for
each architecture, then `string.h`, `stdlib.h`, `stdio.h`, `malloc`,
`printf`, time, signals, sockets, `epoll`, and POSIX threads, in the
order Redis needs them. musl is the reference for behaviour and scope.
*Done when* Redis runs on it.

**M11 — Redis with no external tools.** `jiac`, our back end, assembler,
linker, and C library build Redis on all three architectures, and the
Redis test suite passes.

## Principles along the way

- **Always shippable.** Every commit builds and passes the tests. Large
  features land behind small, tested steps.
- **Measure before building.** Before supporting a feature "because C
  has it", check whether our target programs use it.
- **Oracles everywhere.** Each component we write has a mature
  counterpart to compare against: `cc` for the compiler, LLVM for the
  back end, `as`, `ld`, and glibc or musl.
- **Keep the design.** Pure phases, sum types, single ownership, and
  errors as values scale from Jia to C; we keep them as the compiler
  grows.
