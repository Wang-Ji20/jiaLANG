# Research: What Redis Needs from a C Compiler

**Status:** initial notes, written from general knowledge of the Redis
code base. Claims marked *(verify)* have not yet been checked against
the source; the feature census below will replace guesses with counts.

## Why Redis

Redis is the roadmap's target because it is real, portable C with a
thorough test suite (`make test`, written in Tcl). If we can build it
and pass its tests, we can build most C programs of its kind.

## What we expect it to need

| Area | Expected use | Milestone |
|---|---|---|
| Integer types | `long long`, `unsigned`, `size_t`, `uint64_t`, `int64_t` everywhere | M1 |
| Floating point | `double` (scores in sorted sets, `INCRBYFLOAT`) | M1 |
| Operators | bitwise operations and shifts (hashing, bitmaps, `SETBIT`) | M1 |
| Statements | `for`, `switch`, `goto` for error cleanup | M1 |
| Structs, unions | core data structures (`redisObject`, `dict`, `quicklist`, `rax`) | M2 |
| Bit-fields | the `redisObject` header: `type:4`, `encoding:4`, `lru:24` | M2 |
| Function pointers | `dictType` callbacks, command table, event loop handlers | M2 |
| Variadic functions | logging (`serverLog`), reply formatting (`addReplyErrorFormat`) | M2 |
| `static`, `extern` | file-local helpers, globals shared across ~100 files | M2 |
| Preprocessor | heavy: configuration macros, `#ifdef` per platform | M3 |
| GNU extensions | `__attribute__((format, noreturn, unused))`, `__builtin_expect` *(verify)* | M4 |
| Atomics | `atomicvar.h` picks `__atomic` builtins, `__sync` builtins, or a mutex *(verify)* | M4 |
| Threads | POSIX threads (background I/O threads, `bio.c`) | M4, M10 |
| System APIs | sockets, `epoll` / `kqueue`, signals, `fork` (persistence) | M10 |

### Dependencies

Redis vendors its dependencies in `deps/` *(verify the current list)*:
`hiredis` (client library), `linenoise` (line editing for `redis-cli`),
`lua` (Lua 5.1 for scripting), `hdr_histogram`, `fpconv`, and
`jemalloc`. Building with `MALLOC=libc` avoids jemalloc, which is large
and uses more platform-specific code. Our M5 ladder builds these
dependencies one by one before Redis itself.

### Language standard

Older Redis releases built with `-std=c99`; newer ones use C11 features
in places *(verify which release switched and what it uses: likely
`_Atomic` or `<stdatomic.h>` in some paths, and anonymous unions)*.

## The feature census (to do, task 4.1)

Rather than guess, measure. The plan:

1. Build a pinned Redis release with Clang, saving preprocessed sources:
   `make CC='clang -save-temps=obj' MALLOC=libc`.
2. Run a small Clang-based tool (or `clang -Xclang -ast-dump=json`) over
   each preprocessed file and count: every keyword, operator, builtin
   (`__builtin_*`, `__atomic_*`, `__sync_*`), attribute, and GNU
   extension; struct passing by value; variadic definitions; bit-field
   declarations; inline assembly.
3. Do the same for the system headers alone (an empty file that
   includes the headers Redis uses), since headers need extensions the
   code itself may not.
4. Record the counts here, and turn them into M4 tasks ordered by how
   many files each feature blocks.

## Validation ladder

Programs to build before Redis, from easiest to hardest:

1. [c-testsuite](https://github.com/c-testsuite/c-testsuite): small
   single-file conformance tests (M1–M3).
2. `linenoise`, `hdr_histogram`, `fpconv`: small Redis dependencies.
3. Lua 5.1: about 17,000 lines of portable C89 with its own test suite.
4. `hiredis`: sockets and variadic formatting.
5. SQLite (the amalgamation): one 250,000-line file that exercises
   nearly all of C; a strong check before Redis.

## Open questions

- Which Redis release do we pin? (Prefer the newest stable release at
  the start of M6, then upgrade only between milestones.)
- Does `make test` need features beyond the server binary, such as
  modules (`dlopen`) or TLS? `BUILD_TLS=no` should avoid OpenSSL
  *(verify)*.
- How much inline assembly is reachable with default build options?
  Any is a problem for M7–M8, where we must parse it ourselves.
