# jiaLang

[![CI](https://github.com/Wang-Ji20/jiaLANG/actions/workflows/ci.yml/badge.svg)](https://github.com/Wang-Ji20/jiaLANG/actions/workflows/ci.yml)

> 身后有余忘缩手，眼前无路想回头。——《红楼梦》
>
> *With plenty behind him he forgot to hold back; with no road ahead he
> thinks of turning round.* — *Dream of the Red Chamber*

jiaLang is a compiler for **Jia**, a small subset of C, built on LLVM and
written in modern C++23. It began as a 2022 compiler-course project at
Tsinghua University. We are now growing it into a C compiler good enough
to build Redis, and in the long run into a complete toolchain with our
own back end, assembler, linker, and C library.

```c
int printf(char *fmt, ...);

int fib(int n) { if (n < 2) return n; return fib(n - 1) + fib(n - 2); }

int main() {
  printf("fib(20) = %d\n", fib(20));
  return 0;
}
```

```sh
cmake -S . -B build -G Ninja && cmake --build build
./build/tools/jiac -O2 hello.jia -o hello && ./hello
```

## Documentation

| Read this | To learn |
|---|---|
| [Getting started](docs/getting-started.md) | how to build, run, and test in ten minutes |
| [User manual](docs/user-manual.md) | the `jiac` command and the Jia language |
| [Architecture](docs/architecture.md) | how the code is laid out |
| [Design docs](docs/design/) | why the code is written the way it is |
| [Glossary](docs/glossary.md) | compiler terms and project jargon |
| [Roadmap](docs/roadmap.md) and [plan](docs/plan.md) | where we are going, and what comes next |
| [Progress](docs/progress.md) | what works today |
| [AGENTS.md](AGENTS.md) | how we work: commits, docs, code style |

## Layout

```
include/jia/  lib/   the compiler, as LLVM-style libraries
tools/jiac/          the compiler executable
tests/               unit, check, and end-to-end tests
bench/               benchmarks against the system C compiler
examples/            small Jia programs
docs/                documentation, dev logs, and research notes
```
