# Krama — The first branch: `main`'s argument contract, the gate, and exit codes

**Date:** 2026-09-23
**Status:** Discussion record. **Not normative.** §8 lists what is queued for `DECISIONS.md`.

> Nothing here is binding. Where this and `DECISIONS.md` / `STYLE.md` / `krama-spec-v1.md`
> disagree, those win.

Companion to `krama-notes-11-lexer-design.md` (why the lexer is next) and
`krama-notes-12-test-harness.md` (how Sandler's suite works). This session is the branch *before*
the lexer.

---

## 1. How to read this

- **§2** — the branch's goal, and what is actually testable in it.
- **§3** — the four cases, and what each one proves.
- **§4** — which binary the gate builds and which one the harness runs.
- **§5** — Makefile changes.
- **§6** — driver hygiene.
- **§7** — exit codes: own enum, and the collision with `1`.
- **§8** — queued records, owed, open.
- **§9** — conclusions.

---

## 2. What this branch is, and what it can prove

**Goal.** `main` takes exactly one argument, a file path. That is the branch.

**The trap avoided.** `main` currently dumps the file's contents, so the first instinct was a
`.krm` / `.expected` pair where `.expected` is a **byte-for-byte copy of the input**. That is not a
contract, it is a duplicate: two files kept in sync by hand, both invalidated the moment the lexer
emits tokens instead of bytes.

**The reframe.** What the branch delivers is not a dump, it is *"`main` reads the file named by the
argument."* The observable behaviour worth pinning down is therefore the **argument contract**, not
the output content. The argument contract outlives every mode the compiler will ever grow; the dump
does not survive this milestone.

**Where it landed.** Self-comparison (stdout versus the `.krm` itself) was considered as a way to
avoid the duplicate file, then dropped as unnecessary: the lexer's own cases cannot pass unless the
whole file was read, so a roundtrip check is redundant the moment they exist. For a valid file,
**exit `0` is the whole assertion**.

**The dump itself** stays in `main` as a debugging convenience, with nothing asserting on it. That
belongs in PROJECT.md's *Deliberately incomplete* table so it does not harden into a feature by
accident. "Dump the file" is not a compiler feature.

**Deferred, again and deliberately.** STYLE.md §13's corpus layout / naming / driver contract is
triggered by "once the lexer produces output worth testing". A verbatim dump is not that, so this
branch decides the minimum the driver forces and no more.

---

## 3. The cases

| # | Case | Assertion |
|---|---|---|
| 1 | No argument | usage on **stderr**, non-zero exit |
| 2 | Two or more arguments | error on stderr, non-zero exit |
| 3 | Path that does not exist | error on stderr, non-zero exit |
| 3b | A **directory** as the argument | non-zero exit |
| 4 | Valid file | exit `0` |
| 4b | Empty file | exit `0` |

Notes on each:

- **Assert that stderr is non-empty, not what it says.** Same argument as error codes: pin the
  wording in a fixture and every rewording breaks the suite.
- **Case 2 is not redundant.** The natural implementation (`if (argc < 2)`) passes case 1 and
  silently ignores the extra argument in case 2.
- **Case 3b earns its place.** On Linux, `fopen` on a directory **succeeds** for reading; the
  failure appears only at read time with `EISDIR`. Code that checks `fopen` alone passes this and
  then misbehaves. `tests/` is a ready-made fixture.
- **An unreadable file is deliberately skipped.** Git records only the executable bit, so a
  `chmod 000` fixture cannot be committed; the driver would have to create it at run time, and it
  behaves differently as root.
- **Exit `0` alone is weak** — a `main` that ignored its argument entirely would pass case 4. Two
  things carry it: **case 3**, since 3 and 4 differ only in whether the file exists, so passing both
  means something was really opened; and **the sanitized binary** (§4), under which a leaked buffer
  or unclosed `FILE *` turns exit `0` into a failure.
- **Case 4b is free** and pre-tests the `n + 1` bound from notes-11 §3.2.

### 3.1 Invocation belongs to the case

Later phases add flags — `kramac --lex file.krm` dumping tokens, and similar per phase. The driver
should read **how `kramac` is invoked** from the case rather than hardcoding it. Then adding `--lex`
adds cases; it rewrites neither the driver nor any existing fixture. This is the same open question
parked as notes-12 §7 item 1.

### 3.2 What this branch does *not* settle

- **D-025's `CHECK` question is not answered here.** This branch adds no unit tests: testing
  "no argument → usage" means running the binary and reading its exit status, which is the harness,
  not the in-process suite. `CHECK` still waits for the lexer's unit tests.
- It is, however, the **seed of the process-per-case driver** that D-029's `FATAL_PATH_ABORT` cases
  need — the same mechanism, at its smallest.

---

## 4. Which binary

The two builds are not substitutes:

| Build | What it buys |
|---|---|
| Production `kramac` (`all`) | The shipped flag set. Compiling it in the gate catches a break confined to `main.c`, and anything specific to that flag set (e.g. an `-O2`-only warning under `-Werror`) |
| Sanitized `kramac` | Same sources plus ASan/UBSan and `-fno-sanitize-recover=undefined`. Its exit status means something when the read path leaks or does something undefined |

**The clarification that settled it.** PROJECT.md's row says: *"A break confined to `main.c` passes
`check`. Fold `all` into `check` once `main.c` does more than exist."* What it asks for is
**compilation coverage** — today `main.c` is filtered out of the test build entirely, so nothing
compiles it. Building it closes that. **Execution is a separate question**, already answered by the
harness against the sanitized binary. There is no gap forcing the production binary to be *run*.

- `main.c` **cannot** simply be linked into the test binary — the test binary has its own `main`.
  A sanitized `kramac` is therefore a separate target.
- The two builds must write to **different output paths**. If both produce `./kramac`, the last one
  built wins and the harness may silently run the wrong one. `clean` must know about both.
- The driver takes **the binary path as a parameter**, so a second pass over the production binary
  can be added later without touching a fixture. Benefit is narrow (optimizer-dependent behaviour);
  real UB is already caught by the sanitized pass.

**`--version` as the thing to run was rejected**: it proves almost nothing, contradicts the
one-argument contract just established, and would need cases of its own. Do not add a flag merely to
give the gate something to invoke.

---

## 5. Makefile

- `-Isrc` so tests include `src` headers by name.
- `-Itests` **only in the test build**, never the production one — then `src` physically cannot
  include a test-only header. This is the guardrail behind "test-specific headers never called from
  `src`".
- `all` folded into `check` for compilation coverage (§4).
- **Gate order stays cheapest-first** (notes-09 §8.2): `fmt_check` → unit tests → harness.
- `FMT_FILES` already recurses via `find`, so new test sources are picked up without an edit.

---

## 6. Driver hygiene

Small now, but this is D-029's seed, so the habits are worth setting:

- Resolve paths relative to **the driver's own location**, not the caller's `cwd`, or it only works
  from the repository root.
- Capture stdout and stderr to **separate** temp files (`mktemp`); clean up on exit, including on
  failure.
- **Run every case**, report `N / Total passed`, exit non-zero if any failed. Stopping at the first
  failure hides the rest; the pass line is the quiet mode PROJECT.md already wants.

---

## 7. Exit codes

**Specific codes, not just "non-zero".** The reason is diagnostic: the fixture should record *what*
was being checked.

**`errno` values rejected as exit statuses.** Three reasons:

1. An exit status is **8 bits** (0–255); `errno` is unbounded in principle, and values ≥ 128 collide
   with the shell's "killed by signal N" convention (134 = `SIGABRT`, 139 = `SIGSEGV`).
2. The **symbols** are standard; the **integers** are not. A fixture asserting `2` for a missing
   path asserts a Linux detail.
3. Wrong granularity — one code per libc failure, where what is wanted is a small set of stable
   **classes**.

**`sysexits.h` rejected too** (`EX_USAGE` 64, `EX_NOINPUT` 66, `EX_DATAERR` 65). Real prior art, but
BSD rather than POSIX, ignored by most compilers (`gcc` returns 1 for everything), and 15 categories
for a program that needs three. The author's stated reason is the deciding one: a list you wrote
yourself, one entry at a time, is one you can hold in your head without re-reading a header.

**The collision with `1`.** From notes-09 §9.1, a sanitized binary exits `1` when ASan or UBSan
fires. If `1` also means "kramac rejected the input as designed", the harness cannot distinguish a
correct rejection from a sanitizer catch, and the sanitized pass silently stops being a check.
**Leave `1` unused.**

**Shape of the scheme.**

- Classes to start: **success**, **usage error** (cases 1–2), **input error** (case 3). A
  **compile error** class waits until there is something to reject (`LEX001` and friends).
- **Not the `EXIT_` prefix** — `<stdlib.h>` already owns `EXIT_SUCCESS` / `EXIT_FAILURE`, so
  `EXIT_USAGE` beside them reads as standard-library. `KRM_EXIT_USAGE` per STYLE.md §2.1.
- **Append-only once a fixture asserts a value.** Same property as `LEX001`: adding a class is free,
  renumbering an existing one silently breaks every case that checks it.
- The header is **the binary's public contract**, not an internal detail — worth saying so in the
  header itself.
- **The duplication trap:** a shell driver cannot include a C header, so the numbers are written
  twice. Mitigations: keep the class list short, and name the header as the single source of truth
  in a comment where the driver hardcodes them.

---

## 8. Queued records, owed, open

### Queued records (adds to notes-11 §9)

5. **DECISIONS entry — exit-code scheme.** Own enum, classes, `1` left unused. The sanitizer
   collision is exactly the reasoning that looks arbitrary six months on.

### Owed

- PROJECT.md — *Deliberately incomplete*: the untested dump in `main` (§2); the hardcoded-path row
  closes on this branch.

### Open

- **Directory layout for the cases** — sidecar expectations versus Sandler-style directories
  (notes-12 §8 has the same question for stage fixtures).
- **The branch's commit plan** — path argument, exit-code header, Makefile, driver, fixtures.
- Whether a production-binary pass is added to the driver later (§4).

---

## 9. Conclusions — what we are going ahead with

1. **The branch is `main` taking exactly one argument**, a file path.
2. **Test the argument contract, not the dump.** Cases: no argument, extra arguments, missing path,
   a directory, a valid file, an empty file. Valid ⇒ exit `0`, nothing more.
3. **No `.expected` duplicate of the input.** The dump stays as an untested convenience, recorded as
   deliberately incomplete.
4. **stderr is asserted non-empty, never by wording.**
5. **Invocation is a property of the case**, so `--lex` and later flags cost no rewrites.
6. **The gate compiles the production binary; the harness runs the sanitized one.** Distinct output
   paths; `clean` knows both; the driver takes the binary path as a parameter.
7. **Makefile:** `-Isrc` everywhere, `-Itests` only in the test build, `all` folded into `check`,
   order `fmt_check` → unit tests → harness.
8. **Driver:** self-relative paths, separate temp captures, run all cases, `N / Total passed`,
   non-zero on any failure.
9. **Exit codes:** own enum, `KRM_EXIT_` prefix, success / usage / input to start, `1` left unused,
   append-only, header is the public contract.
10. **Not settled here:** D-025's `CHECK` (no unit tests this branch), corpus layout, the commit
    plan.
