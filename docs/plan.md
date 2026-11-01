# Plan

The [roadmap](roadmap.md) sets the destination; this page lists the
next steps in order, each small enough for a few commits. Every task
says what changes, how to test it, and when it is done. Pick the first
unfinished task in the current phase, and tick it in
[progress.md](progress.md) when it lands.

Task sizes: **S** is a commit or two, **M** a few days of focused work,
**L** a week or more (split it before starting).

## Phase 0 — Harden the base

Groundwork that every later phase needs. Do these before M1.

| # | Task | Size | Done when |
|---|---|---|---|
| 0.1 | ~~**CI pipeline** on GitHub Actions: build with `-DJIA_WERROR=ON`, run `ctest`, on GCC and Clang.~~ Done, see [design/ci.md](design/ci.md). | S | Every push runs the tests; a warning fails the build. |
| 0.2 | ~~**Sanitizer job**: a CI job with `-DJIA_SANITIZE=ON`.~~ Done. | S | `ctest` passes under ASan and UBSan. |
| 0.3 | **UB-checked references**: e2e tests build their `cc` reference with `-fsanitize=undefined -fno-sanitize-recover`. | S | An e2e program with signed overflow fails its test. |
| 0.4 | **Warnings channel**: phases return warnings alongside a successful result (`struct Output<T> { T value; Diagnostics warnings; }`). First warning: control reaches the end of a non-void function. | M | `jiac` prints warnings and still produces output; a check test covers the first warning. |
| 0.5 | **Source manager**: `SourceLocation` gains a file id; a `SourceManager` owns every buffer. Diagnostics print the right file. | M | Unit tests cover two buffers. Needed by `#include` (M3). |
| 0.6 | **TargetInfo**: type sizes, alignments, and `char` signedness come from the target triple instead of hard-coded LP64. | M | `--target=i686-linux-gnu` gives 4-byte pointers in `sizeof`. |
| 0.7 | **Fuzzing**: a libFuzzer harness over `frontEnd` (`-DJIA_FUZZ=ON`, Clang only). | S | One hour of fuzzing finds no crash, hang, or sanitizer report. |
| 0.8 | **Source ranges** in diagnostics (underline `~~~~` the whole operand). | S | Check tests show ranges for type errors. |

## Phase 1 — M1: C scalar types and operators

The aim is C's full expression language over scalar types. Each task
adds a differential e2e test (checked against `cc`) as well as unit and
check tests.

### 1.1 Integer types — L

- **Change:** add `short`, `long`, `long long`, and the `unsigned`
  versions to `TypeKind`. Implement integer conversion rank, integer
  promotion, and the usual arithmetic conversions of C11 §6.3.1.8.
  Integer literals take suffixes (`u`, `l`, `ll`) and get the type rules
  of §6.4.4.1 (decimal vs. octal/hex). Unsigned arithmetic wraps; signed
  comparison and division become signedness-aware in code generation.
- **Test:** e2e programs mixing signed and unsigned (`-1 < 0u` is false);
  unit tests for every row of the conversion table.
- **Done when** `long`-heavy programs (hashing, 64-bit arithmetic) match
  `cc`, and `malloc`/`strlen` can be declared with correct types.

### 1.2 `double` and C floating semantics — M

- **Change:** add `double` (and `long double`, mapped to `double` until
  the ABI work in 2.8). **Breaking:** an unsuffixed literal such as
  `3.0` becomes a `double`, as in C; `3.0f` stays `float`. Variadic
  promotion moves from code generation into Sema.
- **Test:** update existing tests; a differential test of mixed
  `float`/`double` expressions.
- **Done when** floating programs written without `f` suffixes match
  `cc`.

### 1.3 `_Bool` — S

Conversion to `_Bool` yields 0 or 1. **Done when** `(_Bool)256 == 1`.

### 1.4 Operators — M

- **Change:** `~ & | ^ << >>`, prefix and postfix `++`/`--`, compound
  assignment (`+= -= *= /= %= &= |= ^= <<= >>=`), `?:`, and the comma
  operator. New tokens in `TokenKinds.def`; the precedence table grows
  to C's 15 levels. `?:` needs the usual conversions on its arms and
  null-pointer rules.
- **Test:** a precedence test that parenthesizes an expression both ways
  and compares results; differential tests of each operator.
- **Done when** every C operator is supported.

### 1.5 Statements — M

`for` (with a C99 declaration in the header), `do`/`while`,
`switch`/`case`/`default` (lowered to LLVM `switch`, with fallthrough
and `break`), `goto` and labels (function-scope label table, checked in
Sema). **Done when** a differential test of Duff's device passes.

### 1.6 `size_t` and `sizeof` — S

`sizeof` yields `unsigned long` on LP64 (from TargetInfo); pointer
difference yields `long`. **Done when** `sizeof(int) * -1` behaves as in C.

**Phase 1 acceptance:** the
[c-testsuite](https://github.com/c-testsuite/c-testsuite) tests that use
only scalar types and no preprocessor pass. Import them under
`tests/external/` with a script that records the pass list, so
regressions show up.

## Phase 2 — M2: Aggregates and declarations

### 2.1 `typedef` and context-sensitive parsing — M

The parser must know which identifiers name types: `T * x;` is a
declaration if `T` is a typedef and a multiplication otherwise. The
parser keeps its own scope stack of typedef names (Sema stays the
authority on everything else). **Done when** typedef shadowing by a
variable in an inner scope parses correctly.

### 2.2 `struct` and `union` — L

Tags with C's separate tag namespace and scope rules; incomplete and
self-referential types; member access `.` and `->`; anonymous members
(C11); layout and alignment from TargetInfo, verified against
`offsetof` results from `cc`. **Done when** a linked-list and a binary
tree program match `cc`.

### 2.3 `enum` — S

Enumeration constants as `int` constants usable in constant expressions
and `case` labels.

### 2.4 Function pointers — M

Function designators decay to pointers; calls through pointers;
declarator support for `int (*f)(int)` (removing today's "not supported"
error). **Done when** a callback table (like Redis's `dictType`) works.

### 2.5 Storage classes and linkage — M

`static` (internal linkage, and function-local statics), `extern`,
tentative definitions, multiple translation units. **Done when** a
three-file program with shared globals links and runs.

### 2.6 Initializers — M

Brace initialization of structs and unions, designated initializers
(`.x = 1`, `[3] = 2`), compound literals, string literals inside
aggregates.

### 2.7 Bit-fields — M

Layout matching the System V ABI; loads and stores with masking.
**Done when** Redis's `robj` header layout (`unsigned type:4,
encoding:4, lru:24`) matches `cc`.

### 2.8 Struct arguments and return values (ABI) — L

LLVM does not implement C calling conventions for aggregates; the front
end must classify and lower them (System V AMD64 first, then AAPCS64
and RISC-V). Follow Clang's `ABIInfo` design. **Done when** Jia and C
code exchange structs of every size class correctly, tested both ways.

### 2.9 Variadic function definitions — M

`stdarg.h` support: `va_list`, `va_start`, `va_arg`, `va_end`, lowered
per ABI (LLVM's `va_arg` instruction is incomplete on x86-64).

### 2.10 Qualifiers — M

A `QualType` (type plus `const`/`volatile`/`restrict`) replaces today's
`isConst` flag, enforcing `const` through pointers.

**Phase 2 acceptance:** all c-testsuite tests that need no preprocessor
pass.

## Phase 3 and beyond

Plans for M3 (preprocessor) onwards will be written in detail when
Phase 2 is underway, informed by the feature census in
[research/redis-requirements.md](research/redis-requirements.md). The
first tasks are already clear:

- **3.1** Token-based preprocessor driver over `SourceManager` (0.5),
  with `#include` search paths and include guards.
- **3.2** Object-like and function-like macros, `#`, `##`, `__VA_ARGS__`,
  and the "blue paint" rule that stops recursive expansion.
- **3.3** `#if` expressions using the shared constant folder.
- **4.1** Run the Redis feature census (below) and turn its results into
  a task list for M4.

## How to take a task

1. Read the task and the design docs it touches.
2. Write any research you need in `docs/research/` first.
3. Land it in commits of about 500 lines, each building and passing
   `ctest`, each updating the docs it affects (see
   [AGENTS.md](../AGENTS.md)).
4. Tick it in [progress.md](progress.md) and note what you learned in
   the dev log.
