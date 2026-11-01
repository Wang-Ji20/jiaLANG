# Design: Benchmarking

Benchmarks answer two questions: *how fast is the code we generate*, and
*how fast is the compiler itself*. `jia-bench` answers both, against the
system C compiler as a yardstick.

## What it measures

Every program in `bench/programs/` is valid in both Jia and C. For each
one, `jia-bench` builds four executables (`jiac -O0`, `cc -O0`,
`jiac -O2`, `cc -O2`) and reports, as the median of N runs:

- **compile time**: building the executable, linking included;
- **run time**: executing it;
- **output**: whether all four printed exactly the same thing.

A benchmark whose output differs fails, so a fast but wrong compiler
cannot win. A second table measures compile *throughput* on a generated
file of 2,000 functions, for the front end alone (`-fsyntax-only`) and
for a full `-O0` build.

| Program | Stresses |
|---|---|
| `fib` | function calls |
| `sieve` | memory-bound loops over a large global array |
| `matmul` | nested loops, 2-D indexing |
| `quicksort` | recursion, pointers, unpredictable branches |
| `collatz` | a tight integer loop with division |
| `mandelbrot` | `float` arithmetic |

## Running it

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build
cmake --build build --target bench     # full run; report in build/bench/report.md
./build/bench/jia-bench --jiac build/tools/jiac --runs 10 bench/programs/fib.jia
```

`ctest` includes `bench-smoke`, a single run of every benchmark with no
timing requirement: it only checks that each one builds everywhere and
prints the same output. This keeps benchmarks from silently rotting.

Measure a Release build of `jiac`; a Debug build's compile times say
little.

## Rules for benchmark programs

- **Valid in both languages.** Declare library functions yourself
  (`int printf(char *fmt, ...);`) and write floating literals with an
  `f` suffix, because in C a bare `3.0` is a `double`.
- **No undefined behaviour.** Check every program with
  `cc -fsanitize=undefined,address` before committing. Signed overflow
  in a benchmark makes its results meaningless.
- **Deterministic output** that depends on the whole computation, so
  the optimizer cannot delete the work.
- **About 50–200 ms at `-O2`**: long enough to measure, short enough to
  run often.

## Recording results

Record notable runs in `docs/research/` as dated notes with the machine,
versions, and an interpretation, such as
[benchmark-baseline-2026-11-01.md](../research/benchmark-baseline-2026-11-01.md).
Performance changes should quote before-and-after numbers.
