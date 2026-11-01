# Research: Our Own Back End, Assembler, Linker, and C Library

**Status:** initial survey for roadmap milestones M7–M10. Nothing here
is decided; each milestone will start by revisiting this note.

## Where LLVM sits today

`jiac` hands LLVM a module of IR and gets back an object file. LLVM does
optimization, instruction selection, register allocation, and machine
code emission; the system `cc` then links against the system C library.
Arc 2 of the roadmap replaces each of these.

## Prior art worth studying

| Project | What it shows | Size |
|---|---|---|
| [chibicc](https://github.com/rui314/chibicc) (Rui Ueyama) | A C11 compiler that emits x86-64 assembly directly; built step by step in small commits; compiles real programs such as SQLite | ~10k lines of C |
| [QBE](https://c9x.me/compile/) | A small optimizing back end: SSA IR in, assembly out for amd64, arm64, and riscv64 | ~15k lines of C |
| [cproc](https://sr.ht/~mcf/cproc/) | A C11 front end on top of QBE: proof that a small front end plus a small back end can build real software | ~10k lines |
| [mold](https://github.com/rui314/mold) (Rui Ueyama) | A fast modern ELF linker; its design notes explain linking clearly | large |
| [musl](https://musl.libc.org) | A small, readable, standards-focused C library for Linux | ~80k lines |
| LLVM itself | `SelectionDAG` / `GlobalISel`, the MC layer (assembler and object writer), and LLD are the "full-size" references | very large |

## M7: the back end

**Input.** The roadmap says "LLVM IR to assembly". Two stages:

1. *In-memory input.* Walk the `llvm::Module` after LLVM's optimizer
   has run. Simplest start: we keep LLVM's optimizations and its IR
   verifier, and we can compare our assembly against LLVM's on every
   test.
2. *Text input.* Parse the subset of textual LLVM IR (`.ll`) that our
   front end and LLVM's optimizer produce. Then the back end no longer
   links against LLVM. The optimizer would also need replacing for full
   independence; that is a separate, later decision (our own SSA
   optimizer, or a QBE-style IR with a few classic passes).

**Pipeline per function:**

1. *Instruction selection.* Map each IR instruction (or small tree of
   them) to target instructions. Start with a direct, one-instruction-
   at-a-time translation (as chibicc does); add tree pattern matching
   later where it pays.
2. *Register allocation.* Start with "every value in a stack slot"
   (correct, slow), then linear scan (Poletto and Sarkar, 1999), which
   is simple and fast and gives decent code.
3. *Frame lowering and calling conventions.* System V AMD64, AAPCS64,
   and RISC-V LP64D: argument classification, callee-saved registers,
   stack alignment, variadic calls.
4. *Assembly printing* in GNU `as` syntax, so the system assembler can
   check our output before M8 exists.

**Testing.** The existing differential tests run unchanged with a
`--backend=jia` switch. The three architectures run under QEMU user
mode in CI.

## M8: the assembler

Because our back end is the only producer, the assembler needs to accept
only the syntax we print: a few dozen directives (`.text`, `.globl`,
`.section`, `.byte`, ...) and the instructions we select. The work is
encoding (x86-64 has the hardest encoding; AArch64 and RISC-V are
regular), symbol tables, and relocations, written to ELF64 relocatable
objects. Test by assembling the same file with `as` and comparing
section bytes and relocations.

## M9: the linker

Static linking first: read ELF objects and `ar` archives, resolve
symbols, lay out sections, apply relocations, write an executable.
Dynamic linking against glibc (PLT, GOT, `PT_INTERP`, `.dynamic`) is
much larger; with our own C library (M10) linked statically, we might
not need it for the M11 goal. Test by comparing program behaviour with
`ld`-linked builds.

## M10: the C library

Order of work follows what tests and then Redis need: startup code and
`exit`, system call wrappers per architecture, `string.h`, `stdlib.h`
(`malloc` first), `stdio.h` (`printf` is the largest single piece),
`errno`, time, signals, sockets and `epoll`, then POSIX threads (thread
creation, mutexes, condition variables, thread-local storage), which is
the hardest part because it touches the linker and the ABI.

## Open questions

- Do we keep LLVM's optimizer after M7, or write our own? (Decide at the
  end of M7 with benchmark data.)
- Is "LLVM IR text" the right long-term interface, or should the front
  end emit our own IR directly once the back end is mature?
- Static-only linking for M11, or dynamic linking too?
