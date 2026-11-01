# Research: Benchmark Baseline, 2026-11-01

The first benchmark run of the rebuilt compiler. Later changes should be
compared against these numbers.

## Setup

- Machine: Intel Core Ultra 5 125H (18 threads), WSL2 on Windows,
  openSUSE Tumbleweed.
- jiac 0.2.0, Release build, LLVM 23.1.4. Reference: GCC 16.2.1 (`cc`).
- `jia-bench --runs 3`. WSL2 timings are noisy; treat differences under
  about 15% as noise.

## Results

Median of 3 runs, in milliseconds. Compile = build the executable; run = execute it.

| Program | jiac -O0 compile | jiac -O0 run | cc -O0 compile | cc -O0 run | jiac -O2 compile | jiac -O2 run | cc -O2 compile | cc -O2 run | Output |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---|
| collatz | 35.7 | 63.8 | 34.1 | 30.5 | 36.0 | 14.4 | 41.1 | 14.5 | same |
| fib | 30.6 | 21.3 | 35.0 | 18.3 | 39.8 | 8.6 | 67.1 | 5.4 | same |
| mandelbrot | 32.8 | 108.0 | 35.9 | 106.4 | 44.6 | 56.3 | 53.9 | 58.2 | same |
| matmul | 39.0 | 65.9 | 43.1 | 74.5 | 67.1 | 16.4 | 63.3 | 13.4 | same |
| quicksort | 42.4 | 159.1 | 43.5 | 140.2 | 54.4 | 96.6 | 65.3 | 91.9 | same |
| sieve | 37.7 | 140.5 | 42.2 | 179.8 | 48.6 | 75.8 | 63.4 | 74.5 | same |

### Compile throughput

A generated file of 2000 functions (20005 lines).

| Compiler | Front end only | Full build |
|---|---:|---:|
| jiac -O0 | 38.4 | 298.1 |
| cc -O0 | 106.9 | 2117.1 |


## Observations

1. **Every configuration printed the same output** for every program,
   so the numbers compare correct programs.
2. **Optimized code is competitive.** At `-O2`, jiac's run times are
   within noise of GCC's on collatz, mandelbrot, quicksort, and sieve,
   and close on matmul. This is expected: LLVM does the optimizing, and
   our IR is plain enough for it to see through.
3. **fib is the outlier** (8.6 ms vs 5.4 ms at `-O2`). GCC transforms
   the double recursion more aggressively than LLVM's default pipeline.
   Worth a look once we have our own back end, but not a front-end
   problem.
4. **Unoptimized code is mixed.** At `-O0`, jiac is slower on collatz
   (2×) and quicksort, and faster on matmul and sieve. Both compilers
   keep every variable in memory at `-O0`; differences come from how
   each lowers individual operations. `-O0` speed matters little, but
   collatz is a cheap test case for our future back end.
5. **The front end is fast.** On 20,000 generated lines, jiac's front
   end takes 38 ms against GCC's 107 ms, and a full `-O0` build takes
   0.3 s against 2.1 s. Part of the gap is that Jia is a far smaller
   language than C; expect it to narrow as we add C features and the
   preprocessor. We should keep this number in view as we grow.

## Lessons

- The first draft of the collatz benchmark overflowed `int`, which is
  undefined behaviour in C; the jiac `-O0` build looped forever while
  waiting for the value to reach 1. Every benchmark and e2e program must
  be checked with `cc -fsanitize=undefined,address` before it is
  committed.
- In C, a bare `3.0` is a `double`; in Jia every floating literal is a
  `float`. Programs meant for both compilers write `3.0f`.
