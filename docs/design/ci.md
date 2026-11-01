# Design: Continuous Integration

Every push to `main` and every pull request runs our checks on GitHub
Actions (`.github/workflows/ci.yml`). The repository is public, so
GitHub's standard Linux runners cost nothing and have no minute limit.

## Jobs

| Job | Compiler | LLVM | What it proves |
|---|---|---|---|
| GCC 14 / LLVM 21 / Debug | GCC 14 | 21 (oldest supported) | We still build with an older standard library and the oldest LLVM we claim |
| Clang / LLVM 23 / Debug | Clang 23 | 23 (newest) | We build with Clang and the newest LLVM |
| Clang / LLVM 23 / ASan + UBSan | Clang 23 | 23 | No memory errors, leaks, or undefined behaviour while running every test |
| clang-format | — | 23 | Every source file is formatted |
| Benchmark (Release) | Clang 23 | 23 | Benchmarks build and agree; reports timings |

All build jobs use `-DJIA_WERROR=ON`, so a new warning fails CI. Each
runs the full `ctest` suite: unit, check, end-to-end, and the benchmark
smoke test.

## Decisions

**Test the edges of what we support.** We claim LLVM 21 or newer and
any C++23 compiler. Testing only the newest versions would let us drift
onto APIs that older ones lack, so one job pins the oldest LLVM and
Ubuntu's GCC 14. This already shaped the code: `std::formattable` is
missing from GCC 14's library, and `BasicBlock::hasTerminator()` is
missing from LLVM 21, so we avoid both.

**LLVM from apt.llvm.org.** Ubuntu's own packages lag behind; the
official LLVM apt repository has every recent version.
`.github/scripts/install-llvm.sh` installs one, along with the extra
libraries LLVM's CMake package expects.

**Benchmarks report; they do not gate.** Shared runners are noisy, so a
timing threshold would fail at random. The benchmark job runs only on
`main` and on demand, writes the report to the run's summary page, and
uploads it as an artifact. Correctness is still enforced: a benchmark
whose builds disagree fails the job.

**Cancel stale runs.** A new push to the same branch cancels the run
for the previous one.

## Running it

- Push or open a pull request; results appear on the commit and in the
  repository's *Actions* tab.
- Run on demand: *Actions → CI → Run workflow*, or
  `gh workflow run ci.yml`.
- Reproduce a job locally by running the same `cmake` commands (see
  [Getting Started](../getting-started.md)); for example, the sanitizer
  job is `-DJIA_SANITIZE=ON -DJIA_WERROR=ON` with Clang.
