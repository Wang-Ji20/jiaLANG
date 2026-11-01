# Getting Started

This page takes you from a fresh clone to a passing test run. It assumes
you know how to use a terminal and a C++ compiler, but nothing about
compilers' insides. Words in *italics* are explained in the
[glossary](glossary.md).

## 1. Install the tools

You need:

| Tool | Version | Why |
|---|---|---|
| A C++ compiler | GCC 14+ or Clang 18+ (C++23) | builds jiaLang |
| CMake | 3.20+ | configures the build |
| Ninja | any | runs the build (Make also works) |
| LLVM with headers | 21+ | the *backend* library we generate code with |
| FileCheck | same as LLVM | checks compiler output in tests |
| Python | 3.9+ | runs the test and benchmark scripts |
| A C compiler (`cc`) | any | links executables, builds benchmark baselines |

On openSUSE Tumbleweed:

```sh
sudo zypper install gcc-c++ cmake ninja llvm23-devel python3
```

On Debian or Ubuntu (with the [LLVM apt repository](https://apt.llvm.org)):

```sh
sudo apt install g++ cmake ninja-build llvm-21-dev llvm-21-tools python3
```

If CMake cannot find LLVM, point it at LLVM's CMake files with
`-DLLVM_DIR=$(llvm-config --cmakedir)`.

## 2. Build

```sh
cmake -S . -B build -G Ninja
cmake --build build
```

Useful options, passed to the first command:

- `-DCMAKE_BUILD_TYPE=Release` for an optimized compiler (default: Debug).
- `-DJIA_SANITIZE=ON` to catch memory errors with AddressSanitizer.
- `-DJIA_WERROR=ON` to turn warnings into errors, as CI should.

## 3. Test

```sh
ctest --test-dir build --output-on-failure
```

## 4. Where to go next

- Read [architecture.md](architecture.md) to see how the code is laid out.
- Read [AGENTS.md](../AGENTS.md) before your first commit.
