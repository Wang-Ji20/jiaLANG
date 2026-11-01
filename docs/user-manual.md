# User Manual

This manual describes what `jiac`, the jiaLang compiler, can do today
and how to use it. To build the compiler first, see
[Getting Started](getting-started.md).

- [The `jiac` command](#the-jiac-command)
- [Language reference](#language-reference)
- [Working with C](#working-with-c)
- [Differences from C](#differences-from-c)
- [Running the tests and benchmarks](#running-the-tests-and-benchmarks)

## The `jiac` command

```
jiac [options] <inputs>
```

Inputs ending in `.jia` are compiled. Any other input (`.c`, `.o`, `.a`)
is handed to the system C compiler when linking, so you can mix Jia and
C in one program.

### Common tasks

| Task | Command |
|---|---|
| Build an executable | `jiac prog.jia -o prog` |
| Build with optimization | `jiac -O2 prog.jia -o prog` |
| Mix Jia and C | `jiac main.jia helpers.c -o prog` |
| Make an object file | `jiac -c prog.jia` (writes `prog.o`) |
| See the assembly | `jiac -S prog.jia -o -` |
| See the LLVM IR | `jiac -S -emit-llvm prog.jia -o -` |
| Check for errors only | `jiac -fsyntax-only prog.jia` |
| Cross-compile | `jiac -c --target=aarch64-linux-gnu prog.jia` |

### Options

| Option | Meaning |
|---|---|
| `-o <file>` | Write the output to `<file>`. With `-S`, `-` means standard output. Default: `a.out`, or the input name with `.o`/`.s`/`.ll`/`.bc`. |
| `-c` | Stop after making an object file. |
| `-S` | Stop after making assembly. |
| `-emit-llvm` | With `-c`, write LLVM bitcode (`.bc`); with `-S`, LLVM IR text (`.ll`). |
| `-O0` … `-O3` | Optimization level. Default `-O0`. The levels run the same LLVM pipelines as Clang's. |
| `-fsyntax-only` | Parse and type-check, then stop. |
| `--dump-tokens` | Print each token with its kind and position. |
| `--dump-ast` | Print the type-checked syntax tree. |
| `--target=<triple>` | Generate code for another processor, such as `riscv64-linux-gnu`. Linking needs a matching C toolchain. |
| `-l<lib>`, `-L<dir>` | Passed to the linker. |
| `-h`, `--help` | Show help. |
| `--version` | Show the version. |

The environment variable `CC` selects the C compiler used for linking
(default `cc`).

### Error messages

`jiac` reports errors in the same format as GCC and Clang, with the
offending line and a caret:

```
bad.jia:3:11: error: use of undeclared identifier 'missing'
    3 |   p = p + missing;
      |           ^
```

It reports as many errors as it can in one run. Each phase must succeed
before the next starts, so type errors appear only once the syntax
errors are fixed.

## Language reference

Jia is a small subset of C. Every valid Jia program is a valid C
program with the same meaning, with the few exceptions listed under
[Differences from C](#differences-from-c).

### Lexical elements

- **Comments:** `/* ... */` and `// ...` to end of line.
- **Identifiers:** a letter or `_`, then letters, digits, or `_`.
  Case-sensitive.
- **Keywords:** `break char const continue else float if int return
  sizeof void while`.
- **Integer literals:** decimal (`42`), octal (`017`), hexadecimal
  (`0x1F`). Suffixes such as `u` or `L` are not supported. A literal
  must fit in `int`.
- **Floating literals:** `3.5`, `.5`, `1e-3`, optionally ending in `f`.
- **Character literals:** `'a'`, with escapes `\n \t \r \a \b \f \v \\
  \' \" \?`, octal `\101`, and hex `\x41`. Their type is `int`, as in C.
- **String literals:** `"text"` with the same escapes. Adjacent literals
  join: `"ab" "cd"` is `"abcd"`.
- **Operators and punctuation:** `+ - * / % ! = == != < > <= >= && || &
  , ; ( ) [ ] { } ...`

### Types

| Type | Meaning | Size |
|---|---|---|
| `void` | no value; only as a return type or in `void *` | — |
| `char` | signed 8-bit integer | 1 |
| `int` | signed 32-bit integer | 4 |
| `float` | 32-bit IEEE floating point | 4 |
| `T *` | pointer to `T` | 8 |
| `T [N]` | array of `N` elements of `T` | `N × sizeof(T)` |
| `R (P...)` | function taking `P...`, returning `R` | — |

`const` may appear in a declaration (`const int k = 3;`); assigning to
such a variable is an error.

### Declarations

```c
int x;                     // a global, initialized to 0
int y = 2 * 21, *p = &y;   // several declarators share a base type
char name[] = "jia";       // size 4, taken from the initializer
int grid[2][3] = {{1, 2, 3}, {4}};  // missing elements are 0
int *ptrs[4];              // array of four pointers
int (*row)[3];             // pointer to an array of three ints
int add(int a, int b);     // a prototype
int printf(char *fmt, ...);// a variadic prototype
```

Global initializers must be constants: arithmetic on literals, `0` for
pointers, string literals, or addresses of globals. Local initializers
may be any expression.

Declarations may appear anywhere in a block, as in C99. A name is
visible from its declaration to the end of its block.

### Functions

```c
int gcd(int a, int b) {
  while (b != 0) {
    int t = a % b;
    a = b;
    b = t;
  }
  return a;
}
```

- `int f()` and `int f(void)` both mean "no parameters".
- An array parameter `int a[]` or `int a[10]` is a pointer `int *a`.
- A function must be declared before it is called. A prototype lets
  two functions call each other.
- Calls check the number and types of arguments. Arguments that match a
  `...` undergo C's default promotions (`char` to `int`, `float` to
  `double`).
- Falling off the end of a non-void function returns 0.

### Statements

| Statement | Form |
|---|---|
| Expression | `expr;` |
| Empty | `;` |
| Block | `{ declarations and statements }` |
| If | `if (expr) stmt` and `if (expr) stmt else stmt` |
| While | `while (expr) stmt` |
| Break, continue | `break;` `continue;` (inside a loop) |
| Return | `return;` `return expr;` |

Conditions must have a scalar type (arithmetic or pointer); nonzero
means true.

### Expressions

From tightest to loosest binding:

| Operators | Associativity | Notes |
|---|---|---|
| `f(args)` `a[i]` | left | `a[i]` means `*(a + i)` |
| `+x` `-x` `!x` `*p` `&x` `(type)x` `sizeof x` `sizeof(type)` | right | |
| `*` `/` `%` | left | `%` needs integers; `/` truncates toward 0 |
| `+` `-` | left | also pointer ± integer, pointer − pointer |
| `<` `>` `<=` `>=` | left | result is `int` 0 or 1 |
| `==` `!=` | left | result is `int` 0 or 1 |
| `&&` | left | short-circuit; result 0 or 1 |
| `\|\|` | left | short-circuit; result 0 or 1 |
| `=` | right | the left side must be a modifiable lvalue |

Conversions follow C: `char` becomes `int` in arithmetic; if either
operand is `float`, the other becomes `float`; arrays decay to pointers
to their first element; `0` converts to any pointer type; `void *`
converts to and from any object pointer.

### Grammar

The grammar in the style of the C standard. `opt` marks an optional
part.

```
translation-unit:   external-decl*
external-decl:      decl-spec declarator compound-stmt
                    decl-spec init-declarator-list opt ;
decl-spec:          (const | void | char | int | float)+
init-declarator-list: init-declarator (, init-declarator)*
init-declarator:    declarator (= initializer)opt
declarator:         (* const opt)* direct-declarator
direct-declarator:  identifier | ( declarator )
                    direct-declarator [ constant-expr opt ]
                    direct-declarator ( param-list opt )
param-list:         void | param (, param)* (, ...)opt
param:              decl-spec declarator | decl-spec abstract-declarator opt
initializer:        assignment-expr | { initializer (, initializer)* ,opt }

compound-stmt:      { (declaration | statement)* }
statement:          compound-stmt | expr opt ;
                    if ( expr ) statement (else statement)opt
                    while ( expr ) statement
                    break ; | continue ; | return expr opt ;

expr:               assignment-expr
assignment-expr:    binary-expr (= assignment-expr)opt
binary-expr:        unary-expr (binary-op unary-expr)*   -- by precedence table
unary-expr:         postfix-expr | unary-op unary-expr | ( type-name ) unary-expr
                    sizeof unary-expr | sizeof ( type-name )
postfix-expr:       primary-expr ( ( args opt ) | [ expr ] )*
primary-expr:       identifier | constant | string-literal+ | ( expr )
type-name:          decl-spec abstract-declarator opt
```

## Working with C

Jia's types match C's on 64-bit Linux and macOS (`int` is 32 bits,
pointers are 64), and `jiac` uses the platform's C calling convention.
So:

- **Jia can call C.** Declare the C function and call it:
  `int puts(char *s);`. The C library is linked automatically.
- **C can call Jia.** Compile the Jia file with `-c`, declare its
  functions in C, and link: `jiac lib.jia main.c -o prog`.

C functions that take or return types Jia lacks (`long`, `size_t`,
`double`, structs) cannot be declared correctly yet. In particular,
`malloc(size_t)` and `strlen` return values wider than `int`; avoid
them until `long` arrives (see the [plan](plan.md)).

## Differences from C

Jia leaves out most of C. The parts it keeps behave as in C, except:

| Area | Jia | C |
|---|---|---|
| Missing `return` in a non-void function | returns 0 | undefined value |
| `int f()` | no parameters (as in C23) | unspecified parameters (before C23) |
| Implicit declarations | error | error in C99 and later |
| Function pointers | not supported | supported |
| `const` through pointers | parsed, ignored | enforced |

Not supported yet: `long`, `short`, `unsigned`, `double`, `struct`,
`union`, `enum`, `typedef`, `for`, `do`, `switch`, `goto`, `?:`, `,`,
bitwise operators, `++`/`--`, compound assignment, `static`/`extern`,
and the preprocessor. The [plan](plan.md) lists when each arrives.

## Running the tests and benchmarks

```sh
ctest --test-dir build --output-on-failure   # all tests
./build/tests/unit/jia-unittests sema        # unit tests matching "sema"
./build/tests/jia-lit --jiac build/tools/jiac -v tests/e2e/pointers.jia
```

How to write tests is explained in [design/testing.md](design/testing.md).

Benchmarks compare `jiac` with the system C compiler (use a Release
build):

```sh
cmake --build build --target bench   # report in build/bench/report.md
```

See [design/benchmarking.md](design/benchmarking.md) for what is
measured and how to add a benchmark.
