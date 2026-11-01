# Design: Code Generation

Code generation turns the typed tree into *LLVM IR*, a low-level,
machine-independent language. LLVM then optimizes the IR and translates
it into machine code for the target processor.

```cpp
Result<std::unique_ptr<llvm::Module>>
generateModule(const TranslationUnit &tu, llvm::LLVMContext &ctx,
               std::string_view name);
```

## Decisions

**Trust Sema completely.** Code generation contains no type checks and
no conversion rules. Each tree node maps to a fixed handful of
instructions: `Cast IntToFloat` becomes `sitofp`, `Binary +` on floats
becomes `fadd`. When something impossible shows up, an `assert` fires,
because it means a Sema bug, not a user mistake.

**Every variable lives in memory first.** Each local variable and
parameter gets a stack slot (`alloca`) in the function's entry block.
Reads are loads and writes are stores. This is simple and always
correct, even for variables whose address is taken. LLVM's `mem2reg`
pass, part of every optimization level above `-O0`, moves the slots into
registers. Clang does exactly this.

**Two ways to evaluate an expression.** `lvalue(e)` returns the
*address* an expression names; `rvalue(e)` returns its *value*. Reading
a variable is "load from its lvalue". Only four constructs have lvalues:
variable names, `*p`, string literals, and (after Sema's rewrite)
indexing, which is just `*p`.

**Control flow as basic blocks.** An `if` becomes three *basic blocks*
(then, else, end) joined by branches. A `while` becomes condition, body,
and end blocks; `break` and `continue` jump to the end and condition
blocks of the innermost loop, tracked on a stack. After `return`,
`break`, or `continue`, any further code is unreachable, so the
generator starts a fresh "dead" block for it. This keeps LLVM's rule
that each block ends with exactly one terminator.

**Short-circuit logic with a phi.** `a && b` evaluates `b` only when `a`
is true. The generator branches around `b` and merges the two paths with
a *phi* node that picks `false` (from the `a` path) or the value of `b`.

**C calling conventions come free.** `int` is `i32`, `char` is `i8`,
`float` is `float`, and pointers are `ptr`, so Jia functions can call C
functions and be called from C. One detail needs care: C passes `float`
arguments to variadic functions such as `printf` as `double`, so the
generator widens them with `fpext`.

**Global initializers are constants.** Globals are initialized with LLVM
constants built by `constant()`, which reuses the shared constant folder
for arithmetic and handles addresses (`&g`, string literals) directly.
Uninitialized globals are zero, as in C.

## Behaviour that differs from C

- Falling off the end of a non-void function returns zero instead of an
  unspecified value.
- Identical string literals share one global.
