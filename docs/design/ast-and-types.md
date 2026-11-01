# Design: The Tree and the Types

Every phase after the lexer talks in terms of two data structures: the
*AST*, which records what the program says, and `Type`, which records
what kind of value each part produces. This note explains how both are
built and why.

## The AST is data, not objects

The 2022 compiler had one `AST` class with string fields and virtual
`codegen` methods. Adding a phase meant adding a virtual method to every
class, and every node could be anything, so code was full of unchecked
downcasts.

Now the tree is a set of plain structs joined by `std::variant`:

```cpp
struct Binary { BinaryOp op; Box<Expr> lhs, rhs; };
struct Expr {
  std::variant<IntLiteral, ..., Binary, Call, Cast, ...> node;
  SourceLocation loc;
  const Type *type;        // filled in by Sema
  ValueCategory category;  // filled in by Sema
};
```

A phase is a function that `std::visit`s the variant. This is how OCaml
or Haskell programs handle trees, and it brings the same benefits:

- **Exhaustive.** If a new node kind is added, every `std::visit` that
  forgets it fails to compile.
- **Local.** All of Sema's logic is in `lib/Sema`, all of code
  generation's in `lib/CodeGen`. Nodes carry no behaviour.
- **Owned.** Children are `Box<T>` (`std::unique_ptr`). The tree is
  move-only, so it cannot be copied by accident, and freeing the root
  frees everything.

## One tree, two stages

The parser produces the tree with syntax only. Sema *consumes* that tree
and returns it with these additions:

- Every `Expr` has `type` and `category` set.
- Every `DeclRef` and declaration has a `SymbolId`, so later phases
  never look names up again.
- Implicit conversions are explicit `Cast` nodes with `isImplicit` set.
  For example, in `float f = 1;` the `1` is wrapped in
  `Cast IntToFloat implicit`.
- Some syntax is rewritten into simpler forms: `a[i]` becomes
  `*(a + i)`, and `sizeof` becomes an integer literal.

We chose one tree type with optional fields over two separate tree
types. Two types would be stricter, but would double the node
definitions, and every phase after Sema would still need the same
shapes. The rule "code generation only runs on Sema's output" is
enforced by the driver instead.

## Value categories

An *lvalue* names a place in memory (`x`, `*p`, `a[i]`); an *rvalue* is
just a value (`1 + 2`). Only lvalues may stand on the left of `=` or
under `&`. Sema records the category on each expression, and code
generation uses it to decide whether to load from an address.

## Types are uniqued

Types are immutable and owned by a `TypeContext`. Asking twice for
"pointer to int" returns the same `const Type*`. This is how LLVM and
Clang handle types, and it has two payoffs:

- **Cheap equality.** Two types are the same exactly when their pointers
  are equal.
- **Cheap sharing.** Hundreds of nodes can borrow one type without
  copies and without `shared_ptr`.

The `TranslationUnit` owns the `TypeContext` together with the
declarations, so every `const Type*` in the tree lives exactly as long as
the tree.

The supported types are `void`, `char` (8-bit signed), `int` (32-bit
signed), `float` (32-bit IEEE), pointers, arrays, and functions.
`Type::str()` spells types the way C does: `int(*)[3]` is a pointer to an
array of three ints.

## Constant folding

`evaluateConstant` computes an expression at compile time when it uses
only literals, arithmetic, comparisons, casts, and `sizeof`. Three
phases need it:

- the parser, for array sizes such as `int a[2 * 8]`;
- Sema, to check that global initializers are constant and to spot the
  null pointer constant `0`;
- code generation, to build global initial values.

It is a pure function, so all three share one implementation. Integer
arithmetic wraps instead of overflowing, and a result is narrowed to its
expression's type when Sema has set one, so `(char)300` folds to 44.
