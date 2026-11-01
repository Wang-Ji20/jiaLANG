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
