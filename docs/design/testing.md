# Design: Testing

A compiler is easy to break quietly: a change in Sema can produce IR that
still verifies but computes the wrong answer. We test at three levels,
each catching a different kind of mistake.

| Level | Where | What it checks | Runs with |
|---|---|---|---|
| Unit | `tests/unit/` | One library function at a time | `jia-unittests` |
| Check | `tests/check/` | Compiler output: diagnostics, dumps, IR | `jia-lit` |
| End to end | `tests/e2e/` | Compiled programs run and print the right thing | `jia-lit` |

`ctest --test-dir build` runs all three (`unit`, `check`, `e2e`).

## Why our own tools

LLVM's test tools, `lit` and `FileCheck`, are excellent, but they are
packaged separately from LLVM's libraries on many systems and `lit`
needs Python. We wanted the project to need nothing beyond CMake, a C++
compiler, and the LLVM libraries. So we wrote small versions of both:

- `tests/unit/Test.h` (60 lines): a unit test framework with no macros,
  using `std::source_location` to report the file and line of a failed
  check.
- `tests/lit/jia-lit.cpp` (about 300 lines): runs file-based tests and
  matches their output.

## Writing a file-based test

A test is a `.jia` file. Comments tell `jia-lit` what to run and what to
expect:

```c
// RUN: %jiac %s -o %t && %t
// CHECK: sum = 15
// CHECK-NEXT: done
// CHECK-NOT: error
int printf(char *fmt, ...);
int main() { printf("sum = %d\ndone\n", 15); return 0; }
```

| Directive | Meaning |
|---|---|
| `RUN: cmd` | Run `cmd` with `/bin/sh`; it must exit with 0 |
| `RUN-FAIL: cmd` | Run `cmd`; it must exit with non-zero |
| `CHECK: text` | `text` appears in the output, after the previous match |
| `CHECK-NEXT: text` | `text` appears on the line right after the previous match |
| `CHECK-NOT: text` | `text` does not appear between the previous and next match |

The output of all `RUN` lines (stdout and stderr together) forms one
text that the `CHECK` lines are matched against, in order.

| Substitution | Becomes |
|---|---|
| `%jiac` | the compiler under test |
| `%cc` | the C compiler CMake found |
| `%s` | this test file |
| `%S` | this test's directory |
| `%t` | a scratch path unique to this test |

Files in a directory named `Inputs` are helpers (for example, C
harnesses), not tests.

Run one test, showing its output even on success:

```sh
./build/tests/jia-lit --jiac build/tools/jiac -v tests/check/dump-ast.jia
```

## Differential testing

Jia is a subset of C, so every Jia program is also a C program. Most
end-to-end tests exploit this: they compile the same file with the
system C compiler and with `jiac` (at `-O0` and `-O2`) and require
identical output.

```c
// RUN: %cc -w -x c %s -o %t.ref && %t.ref > %t.expected
// RUN: %jiac %s -o %t && %t > %t.actual && diff %t.expected %t.actual
```

The C compiler acts as an *oracle*: we do not have to work out every
expected value by hand, and a wrong answer anywhere in the output fails
the test. A few `CHECK` lines still pin key values, so that a bug shared
by both compilers (or a test that prints nothing) cannot pass silently.

`tests/e2e/legacy.jia` is the 2022 compiler's test program, unchanged,
with its C++ harness ported to C in `tests/e2e/Inputs/`.

## Guidelines

- Test behaviour, not implementation. A check test should pin what a
  user sees: an error message and its location, or a program's output.
- When fixing a bug, add the smallest test that fails before the fix.
- When an expectation and the compiler disagree, decide which one is
  right before "fixing" either. While writing the first tests, the
  compiler was right three times out of five.
