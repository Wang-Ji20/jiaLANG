# AGENTS.md — How We Work on jiaLang

This file is the working agreement for everyone who changes this
repository, human or AI agent. Read it before your first commit. When the
agreement changes, change this file in the same commit.

## 1. The project in one paragraph

jiaLang began as a 2022 compiler-course project: a compiler for *Jia*, a
small subset of C, built on LLVM. We are now growing it, step by step,
into a C compiler good enough to build Redis. The [roadmap](docs/roadmap.md)
explains the long-term goal; the [plan](docs/plan.md) lists the next
concrete steps.

## 2. Commits

- **Size.** Keep each commit at or under 1000 changed lines, and aim for
  about 500. Generated files, moved files, and deletions of legacy code
  are the only exceptions; say so in the commit message.
- **Every commit builds and passes the tests.** Run `ctest` (see
  [Getting Started](docs/getting-started.md)) before committing.
- **Every commit updates the documentation** that its change affects.
  A change without matching docs is not finished.
- **Message format.** `area: summary in the imperative mood`, for
  example `sema: reject assignment to const variables`. Areas are the
  module names (`lex`, `parse`, `sema`, `codegen`, `driver`, `ast`,
  `basic`), plus `docs`, `test`, `bench`, `build`.
- **Signing.** Agents commit unsigned, because GPG signing needs an
  interactive terminal.

## 3. Documentation agreement

Our docs live in `docs/` and must stand on their own: a reader with
general computing knowledge, but no compiler background, should be able
to follow them. Unfamiliar terms go in the [glossary](docs/glossary.md).

| Document | Answers | Path |
|---|---|---|
| Getting started | How do I build, run, and test it in ten minutes? | `docs/getting-started.md` |
| User manual | What can the compiler do, and how do I use it? | `docs/user-manual.md` |
| Architecture | How is the code laid out, and how does data flow? | `docs/architecture.md` |
| Design docs | *Why* is the code written this way? | `docs/design/` |
| Glossary | What does this word mean here? | `docs/glossary.md` |
| Roadmap | Where are we going in the long run? | `docs/roadmap.md` |
| Plan | What exactly happens next? | `docs/plan.md` |
| Progress | What works today? | `docs/progress.md` |
| Dev log | What happened, and what did we learn? | `docs/devlog/` |
| Research | What did we find out before deciding? | `docs/research/` |

### Style

Write clear, short English prose, in the spirit of Williams' *Style:
Lessons in Clarity and Grace*:

- Make the main characters the subjects of your sentences and their
  actions the verbs. Write "the parser builds a tree", not "tree
  construction is performed".
- Put familiar information first and new information last.
- Prefer short sentences, but do not chop every idea into fragments.
  Clear is not the same as dense.
- Cut words that add nothing: *basically*, *very*, *in order to*.
- Show an example whenever a rule is abstract.

### Logs and research

- **Dev log** (`docs/devlog/YYYY-MM-DD-topic.md`): one entry per working
  session. Record what changed, what broke, and what we learned.
- **Research** (`docs/research/`): write down investigations *before*
  acting on them, with sources and open questions. A design decision
  should be able to point to the research behind it.
- **Progress** (`docs/progress.md`): keep the feature checklist honest.
  Update it whenever a feature lands or a known bug is found.

## 4. Code agreement

The full rules and their reasons are in
[design/cpp-subset.md](docs/design/cpp-subset.md). In short:

- **C++23**, LLVM coding style (`clang-format` with the repo's
  `.clang-format`).
- **Modular like LLVM.** Each module is a library under `lib/<Module>`
  with its public headers in `include/jia/<Module>`. A module depends
  only on the modules below it (see [architecture](docs/architecture.md)).
- **Ownership like Rust.** Every object has exactly one owner.
  `std::unique_ptr` is our `Box`; plain pointers and references only
  borrow. No raw `new`/`delete` (except when an LLVM API takes
  ownership), no `shared_ptr`, no mutable globals, no singletons.
- **Errors as values.** No exceptions. Fallible functions return
  `jia::Result<T>` (a `std::expected`) carrying diagnostics.
- **Functional core.** Each compiler phase is a pure function from its
  input to a result: `lex(source)`, `parse(tokens)`, `analyze(tree)`,
  `generateModule(tree)`. Side effects (files, processes, printing)
  live only in the driver.
- **Sum types over class hierarchies.** AST nodes are `std::variant`s
  handled with `std::visit`, like OCaml variants and `match`.

## 5. Tests

- New behaviour comes with a test: a unit test in `tests/unit/` for
  library logic, a FileCheck test in `tests/check/` for compiler output
  and diagnostics, or a run test in `tests/e2e/` for program behaviour.
- A bug fix comes with a test that fails without the fix.
- Performance work comes with numbers from `bench/` in the commit
  message or dev log.
