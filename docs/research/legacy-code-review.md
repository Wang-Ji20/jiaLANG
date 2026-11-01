# Research: Review of the 2022 Course Compiler

Before rewriting anything, we read the original compiler (commit
`45325d4`) end to end. This note records what it did, what was wrong,
and why we chose to rebuild rather than patch.

## What it was

The 2022 compiler had about 1,700 lines of C++ in `src/`:

| File | Role |
|---|---|
| `scanner.ll` | Flex lexer |
| `parser.yy` | Bison LALR(1) grammar, builds the tree |
| `AST.hh/.cc` | Tree node classes and builder functions |
| `CodegenVisitor.hh/.cc` | LLVM IR generation and object-file output |
| `driver.hh/.cc`, `main.cc` | Glue: runs parser, prints tree, runs codegen |

It compiled one `.jia` file into `output.out`, an object file. A C++
harness (`examples/chelper.cc`) then called the compiled functions. There
was no linker step, no `main` in Jia programs, and no way to print.

## Problems found

### Structure

1. **Stringly-typed tree.** Every node was an `AST` with a
   `std::string name` and `std::string Type`. Operators were strings
   (`"+"`, `"=="`), types were strings (`"int"`), and code generation
   compared strings at every step. A typo compiled cleanly and failed
   at run time.
2. **Unchecked downcasts.** Code generation reached into children with
   `dynamic_pointer_cast` and used the result without checking it.
3. **No semantic analysis.** Type checking, name lookup, and code
   generation were one pass. Error messages were `panic("bad left
   value")` followed by `exit(-1)`, with no source location.
4. **Global singleton.** `CodegenVisitor::GetInstance()` held all state,
   so the compiler could compile only one module per process.
5. **Work in a destructor.** The object file was written from
   `~CodegenVisitor()`, at program exit. Errors there could not be
   reported.
6. **Debug output mixed with results.** Dozens of `printf` calls
   ("recognize decl type int", symbol table dumps) went to stdout on
   every compilation.

### Correctness

| Bug | Effect |
|---|---|
| `&&` and `\|\|` evaluated both sides | No short-circuit: `p != 0 && *p` crashes |
| Comparisons returned `i1` | `a + (b < c)` produced ill-typed IR |
| `!x` used bitwise `not` | `!2` was `-3`, not `0` |
| Char literals returned as float tokens | `'a'` did not type-check |
| Local initializers ignored | `int v = 3;` left `v` uninitialized |
| `while` and `if` bodies had to be `{ ... }` | Valid C rejected |
| `int` was 64-bit, `float` was `double` | Contradicted the spec; broke C interop |
| `&x` returned an integer | No pointer types at all |
| Unary `*` always loaded an `int` | Wrong for every other type |
| `if` without `else` produced no code | `IfStatAST` had no `codegen` |
| Empty statement `;` produced a null node | Crash in code generation |

### Toolchain

The code relied on LLVM APIs that no longer exist. Typed pointers
(`getPointerElementType`, `getInt64PtrTy`) were removed in LLVM 17,
`llvm::Optional` in LLVM 16, and `llvm/Support/Host.h` moved. It does
not build against current LLVM (23) without rewriting most of code
generation anyway.

## Decision

We rebuild the compiler with the same goal and test programs, but a new
structure:

- **Hand-written lexer and parser** instead of Flex and Bison. Our long
  goal is full C, whose grammar depends on `typedef` names. Clang, GCC,
  and chibicc all parse C by hand for this reason, and hand-written
  parsers give better error messages.
- **A separate semantic analysis phase** that resolves names, computes
  types, and inserts implicit conversions, so code generation never has
  to guess.
- **Typed tree built from sum types**, following our
  [C++ subset](../design/cpp-subset.md).

The original test program `examples/test.jia` stays as a regression test
(`tests/e2e/legacy.jia`), so the rebuild must still compile everything
the original could.
