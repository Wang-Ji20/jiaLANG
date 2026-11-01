# Design Docs

Each design doc explains *why* one part of the compiler works the way it
does. Read them in pipeline order:

1. [cpp-subset.md](cpp-subset.md) — the C++ we write, and why
2. [lexing.md](lexing.md) — source text to tokens
3. [parsing.md](parsing.md) — tokens to a tree
4. [ast-and-types.md](ast-and-types.md) — the tree and type data structures
5. [sema.md](sema.md) — checking and annotating the tree
6. [codegen.md](codegen.md) — the tree to LLVM IR
7. [testing.md](testing.md) — how we know it works
8. [benchmarking.md](benchmarking.md) — how we measure speed
