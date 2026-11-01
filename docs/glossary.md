# Glossary

Terms used in this project, in alphabetical order. If a word in the docs
puzzles you and is not here, add it.

**AST (abstract syntax tree).** A tree that represents the structure of a
program. `a + b * c` becomes a `+` node whose children are `a` and a `*`
node. "Abstract" means it drops details such as parentheses and
semicolons.

**Backend.** The part of a compiler that turns an intermediate form into
machine code. We use LLVM as our backend.

**Borrow.** Using an object without owning it, through a reference or a
plain pointer. The borrower must not use it after the owner frees it.
The term comes from Rust.

**Diagnostic.** A message from the compiler about the user's program: an
error, a warning, or a note, with a source location.

**Escape sequence.** A backslash code inside a character or string
literal that stands for one character, such as `\n` (newline) or `\x41`
(`A`).

**Jia.** The language this project compiles: a small subset of C. Named
after 贾 (Jiǎ), a pun on "false" (假) from *Dream of the Red Chamber*.

**Lexer** (also *scanner* or *tokenizer*). The phase that cuts source text
into tokens.

**Literal.** A value written directly in the source: `42`, `3.5`, `'a'`,
`"hi"`.

**LLVM.** A widely used compiler toolkit. We hand it a program in its
intermediate language (LLVM IR), and it optimizes the program and
produces machine code for many processors.

**Pure function.** A function whose result depends only on its arguments
and which changes nothing outside itself. Pure functions are easy to
test and reason about.

**`Result<T>`.** Our return type for operations that can fail: either a
value of type `T` or a list of diagnostics. It is `std::expected` under
the hood, modelled on Rust's `Result`.

**Sum type** (also *variant* or *tagged union*). A type whose values are
exactly one of several alternatives, such as "an integer literal *or* a
binary expression *or* a call". In C++ we write `std::variant`.

**Token.** The smallest meaningful unit of source text: a keyword, a name,
a number, an operator.

**X-macro.** A C/C++ technique where a list is written once in a file and
expanded several times with different macro definitions. Our
`TokenKinds.def` is one.

**Array decay.** C's rule that an array used as a value turns into a
pointer to its first element. In `int a[3]; int *p = a;` the `a` decays.
Sema shows decay as a `Cast ArrayToPointer` node.

**Constant folding.** Computing an expression at compile time, so
`2 * 8` becomes `16` before the program runs.

**Lvalue / rvalue.** An lvalue names a memory location and can be
assigned to (`x`, `*p`, `a[i]`). An rvalue is a temporary value
(`x + 1`). See [design/ast-and-types.md](design/ast-and-types.md).

**Symbol.** A declared entity: a variable, parameter, or function. Sema
gives each one a `SymbolId`.

**Translation unit.** One source file as the compiler sees it, with all
its declarations. It is the unit that becomes one object file.

**Uniquing.** Keeping only one copy of each distinct value, so equal
values share one address. Our types are uniqued.
