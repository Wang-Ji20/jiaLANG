# Design: Semantic Analysis

The parser checks that a program is *shaped* right. Semantic analysis
("Sema") checks that it *means* something: every name is declared, every
operator gets operands of the right type, every `break` is inside a
loop. It also records what it learned, so code generation never has to
work it out again.

```cpp
Result<TranslationUnit> analyze(TranslationUnit tu);
```

## Decisions

**Sema owns the tree while it works.** `analyze` takes the tree by value
and gives it back. Inside, it edits the tree in place, which is cheap,
but nobody else can see the half-finished state. To the caller, it is a
pure function from an untyped tree to a typed tree.

**Make everything explicit.** C has many silent conversions. In
`float f = c + 1;` with `char c`, C promotes `c` to `int`, adds, then
converts the sum to `float`. Sema writes each step into the tree as a
`Cast` node marked `implicit`:

```
VarDecl f 'float'
  Cast IntToFloat implicit 'float'
    Binary + 'int'
      Cast IntegralCast implicit 'int'
        DeclRef c 'char' lvalue
      IntLiteral 1 'int'
```

Code generation then turns each node into instructions without knowing
C's conversion rules at all. Clang does the same.

**Desugar early.** Sema rewrites constructs that are shorthand for
others, so later phases see fewer node kinds:

| Written | Rewritten to | Why |
|---|---|---|
| `a[i]` | `*(a + i)` | C defines indexing this way; `i[a]` works too |
| `sizeof x`, `sizeof(T)` | an integer literal | the size is known at compile time |
| `1 + p` | `p + 1` | code generation handles one pointer position |

**Errors do not cascade.** When a subexpression is wrong, Sema reports
it and leaves its `type` empty (`nullptr`). Any parent that sees an
empty type gives up quietly. `(x + 1) * 2 - y` with `x` and `y`
undeclared reports exactly two errors, not five.

## The rules

Sema implements C's rules for the subset we support:

- **Scopes.** File scope holds globals and functions. Each `{ }` opens a
  new scope. Parameters share a scope with the function body's
  top-level declarations, so `int f(int a) { int a; }` is an error.
- **Functions** may be declared many times but defined once, and all
  declarations must agree on the type.
- **Integer promotion.** `char` operands become `int` in arithmetic.
- **Usual arithmetic conversions.** If either operand is `float`, both
  become `float`; otherwise both become `int`.
- **Pointer arithmetic.** `p + n`, `n + p`, and `p - n` give a pointer;
  `p - q` gives an `int` count of elements. `void *` arithmetic is
  rejected.
- **Assignment conversions** (used for `=`, initializers, arguments, and
  `return`): between arithmetic types, between a `void *` and any
  object pointer, and from the constant `0` to any pointer.
- **Lvalues.** Only lvalues can be assigned or have their address taken.
  Arrays and `const` variables cannot be assigned.
- **Global initializers** must be constants: arithmetic constants, `0`
  for pointers, string literals, or addresses of globals.
- **Variadic calls.** Arguments past the `...` undergo default
  promotions: `char` becomes `int`. (`float` becomes `double` at code
  generation, since Jia has no `double` type.)
- **Incomplete arrays.** `int a[] = {1, 2, 3};` gets type `int[3]` from
  its initializer.

## What Sema does not check yet

These are planned (see the [plan](../plan.md)):

- warnings, such as unused variables or a missing `return` at the end of
  a non-void function (today the function returns zero);
- `const` through pointers (`const int *p` is parsed, but the `const` is
  ignored);
- integer overflow in constant expressions.
