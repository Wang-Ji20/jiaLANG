# Design: The C++ Subset We Use

C++ is a large language. Most of its sharp edges come from a few
features: manual memory management, exceptions that skip cleanup code,
deep inheritance, and global state. We write in a deliberately small
subset of C++23 that keeps the good parts and avoids those edges. This
document lists the subset and explains each choice.

The guiding models are three other languages:

- **LLVM** for structure: small libraries with clear layers.
- **Rust** for ownership and errors: one owner per object, errors as
  values.
- **OCaml** for logic: immutable data, sum types, pure functions.

## Ownership

Every heap object has exactly one owner, and the type system shows who
it is.

| We want (Rust) | We write (C++) |
|---|---|
| `Box<T>` | `std::unique_ptr<T>` |
| `&T` | `const T&` or `const T*` |
| `&mut T` | `T&` |
| `Vec<T>` | `std::vector<T>` |
| `Option<T>` | `std::optional<T>` |
| `Result<T, E>` | `jia::Result<T>` = `std::expected<T, Diagnostics>` |
| `&str` | `std::string_view` |
| `&[T]` | `std::span<const T>` |

Rules:

- No raw `new` or `delete`. The one exception is LLVM APIs that take
  ownership of a freshly allocated object, such as
  `new llvm::GlobalVariable(module, ...)`; the module then owns it.
- No `std::shared_ptr`. Shared ownership hides who frees what. When many
  places need the same object, one place owns it and the others borrow.
  For example, `TypeContext` owns every `Type`, and AST nodes hold
  `const Type*` borrows.
- A borrow must not outlive its owner. When this is not obvious, a
  comment says what the borrow points into. For example, a `Token`'s
  text points into the source buffer, so the buffer must outlive the
  tokens.
- Functions that consume a value take it by value and return a new one:
  `Result<TranslationUnit> analyze(TranslationUnit tu)`. The caller
  writes `analyze(std::move(tu))`, which makes the hand-off visible.

## Errors

- No exceptions. We compile with `-fno-exceptions`, as LLVM does.
- A function that can fail returns `jia::Result<T>`. The error side is a
  list of diagnostics, each with a source location and a message.
- Chain fallible steps with `and_then`:

  ```cpp
  auto tu = lex(source).and_then(parse).and_then(analyze);
  ```

- Never call `std::get` on a variant (it throws). Use `std::visit` or
  `std::get_if`. Never call `std::stoi` (it throws). Use
  `std::from_chars`.
- Internal invariants that "cannot fail" use `assert`. User mistakes
  always produce a diagnostic, never a crash.

## Data and logic

- **Sum types.** A node that can be one of several kinds is a
  `std::variant`, handled with `std::visit` and the `Overloaded` helper.
  This is OCaml's `match`: if we forget a case, the compiler tells us.
- **Plain structs.** AST nodes are aggregates with public fields and no
  behaviour. Logic lives in free functions in the phase that needs it.
- **Pure functions.** Each phase is a function from input to result.
  Inside, it may use a private helper class with a cursor or a scope
  stack, but nothing escapes: same input, same output, no globals.
- **`const` by default.** Mark locals, parameters, and member functions
  `const` unless they must change.
- **`[[nodiscard]]`** on every function whose result matters, so
  ignoring an error is a compile-time warning.

## Allowed features

`auto` (when the type is obvious or unspeakable), range-based `for`,
structured bindings, `enum class`, designated initializers, `constexpr`,
`std::string_view`, `std::span`, `std::optional`, `std::variant`,
`std::expected`, `std::format`, lambdas, `std::source_location`.

## Banned features

| Feature | Why not | Instead |
|---|---|---|
| Exceptions | Hidden control flow; LLVM is built without them | `Result<T>` |
| `new` / `delete` | Leaks and double frees | `std::make_unique` |
| `shared_ptr` | Unclear ownership, reference cycles | single owner + borrows |
| Singletons, mutable globals | Hidden coupling, untestable | pass state explicitly |
| Deep inheritance, visitors of base classes | Scattered logic, unchecked casts | `std::variant` + `std::visit` |
| C-style casts | Unchecked | `static_cast` and friends |
| Macros | Not part of the language | `constexpr`, templates |

The one allowed macro pattern is the LLVM-style `.def` table (an
"X-macro"), used to keep a list such as token kinds in one place. See
`include/jia/Lex/TokenKinds.def`.

## Formatting

`clang-format` with the repository's `.clang-format` (LLVM style).
Names follow LLVM: `CamelCase` for types, `camelBack` for functions and
variables, trailing underscore for private data members.
