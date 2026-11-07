# Krama — Closing commit 4, and the handoff to the harness

**Date:** 2026-09-25
**Status:** Discussion record. **Not normative.** Doubles as the starting context for the commit 5
(test driver) session — §8 is the checklist.

> Nothing here is binding. Where this and `DECISIONS.md` / `STYLE.md` / `COMMITS.md` /
> `krama-spec-v1.md` disagree, those win.

Continues `krama-notes-16-exit-codes-posix`, which stops at the POSIX decision (D-038).

---

## 1. How to read this

- **§2** — branch state: what is committed on `m1/cli-args`.
- **§3** — `fstat` details: the mode bits, the race, the return value.
- **§4** — why no `FATAL_PATH_GOTO`: the two error families.
- **§5** — `fatal()` exited `1`; the `INTERNAL` class.
- **§6** — `exit_codes.h` as a public header.
- **§7** — `m1/fatal-cleanup`, scoped.
- **§8** — **commit 5 checklist.** Start the next session here.

---

## 2. Branch state

`m1/cli-args`, cut from `main` at `517b4f2`. Six commits, one more than planned:

| # | Commit | Notes |
|---|---|---|
| 1 | `docs: record lexer-first order and test decisions` | D-033–D-037, STYLE 1.5, `cli` scope, spec §11.1 amendment |
| 2 | `chore(test): restructure tree, repoint makefile` | `tests/unit_tests/`, `tests/harness/`, `tests/stage/<name>/` |
| 3 | `build: add -I flags, split obj trees, main in gate` | Three builds, three object trees |
| 4a | `docs: record the POSIX decision for the transpiler` | **Unplanned.** D-038, came out of the work |
| 4b | `feat(cli): take a source path, add exit codes` | `main`'s argument contract, `exit_codes.h`, `fstat` |
| 5 | *next* — driver, manifest, fixtures, Makefile part B | §8 |

**Why the extra commit.** D-038 was a real decision with rejected alternatives, arrived mid-branch,
and changed the build (`_POSIX_C_SOURCE`). Folding it into the code commit would have crowded the
body; a docs commit ahead of the code keeps both readable, and the code commit can `Refs:` an entry
that already exists.

**Where the two PROJECT.md rows went, and why they split.** The *Deliberately incomplete* table must
never describe a state that is not on disk:

- The row about `check` not compiling `main.c` was **already stale** — commit 3 closed it — so it went
  in the docs commit, the next one available. Commit 3's body was deliberately worded "can now be
  closed", which is true of the gate regardless of when the row is deleted.
- The hardcoded-path row stayed **true until `main` took an argument**, so it went with the code
  commit. Deleting it earlier would have made the table wrong for one commit.

Two stale paths (`tests/test_arena.c` → `tests/unit_tests/test_arena.c`) were also retargeted in the
docs commit — strictly commit 2's omission.

**Workflow note.** Splitting staged work into two commits: `git restore --staged <files>` for the
second commit's files, commit the first, then re-stage. No partial staging was needed once each
file's changes belonged wholly to one commit.

---

## 3. `fstat` details

### 3.1 `S_IF*` versus `S_IS*`

Both are POSIX; they work at different levels. The file type is **an enumerated field packed into
`st_mode`, not a set of independent flags**, and the values overlap: `S_IFDIR` is `0040000`,
`S_IFCHR` is `0020000`, `S_IFBLK` is `0060000` — both bits together. So a naive `mode & S_IFDIR` is
**true for a block device**. The field has to be isolated and compared:

```c
(st.st_mode & S_IFMT) == S_IFDIR
```

`S_ISDIR(m)` is a macro doing exactly that. The man page uses a `switch` over the masked value
because it prints *which* of seven types a file is; one yes/no question wants `S_ISREG`, which cannot
forget the mask.

### 3.2 `fstat` over `stat`

The descriptor is the *mechanism*, not the *reason*. `stat(path)` works fine and needs no `fileno`.
The reason is the race: `stat(path)` then `fopen(path)` resolves the name twice, and between the two
calls it can come to mean a different file (TOCTOU). `fstat` interrogates the descriptor already
held, so what was checked is what will be read.

`stat` follows symlinks (`lstat` does not); `fstat` operates past resolution. A symlink to a regular
source file is accepted by all of them, which is correct.

### 3.3 Checking `fstat`'s return

With a freshly opened stream and a stack `struct stat`, `EBADF` and `EFAULT` cannot occur — but the
return is checked anyway, because **if `fstat` fails, `stat_buf` is uninitialised** and
`S_ISREG(stat_buf.st_mode)` reads an indeterminate value. That is undefined behaviour of the kind
nothing catches: `-Wmaybe-uninitialized` gives up once the address is passed to a function, and ASan
does not track uninitialised reads. "These errors cannot happen here" is an argument about one call
site that a later refactor can invalidate silently; three lines make the initialisation guaranteed.

One error **can** occur: `EOVERFLOW`, when `st_size` does not fit `off_t` — a 32-bit build without
`_FILE_OFFSET_BITS=64` on a file over 2 GB.

**`EOVERFLOW` maps to `ENV`, not `INPUT`.** `INPUT` means the file is at fault. Here the same 3 GB
file is fine for a 64-bit `kramac` and impossible for a 32-bit one — *this build* cannot represent
it. It shares the general "Unable to stat file" message; `strerror` renders it as "Value too large
for defined data type", terse but honest.

### 3.4 The silent branch

The `S_ISREG` branch first set `INPUT` and jumped **without printing anything**. Every other failure
writes to stderr, and corpus case 3b asserts stderr is non-empty — so it would have exited `3` in
silence and failed its fixture. Fixed: `Not a regular file: <path>`. No `errno` applies, since
nothing failed.

---

## 4. Why no `FATAL_PATH_GOTO`

**The idea.** The `fprintf` + set status + `return`/`goto` pattern repeats in `main`; route it
through `FATAL_PATH_EXIT` and a new `FATAL_PATH_GOTO`, adding `__FILE__` / `__LINE__`.

**Why not, on STYLE.md §11.** Diagnostics and assertions are two mechanisms that must never
substitute for each other:

| | Diagnostic | Assertion |
|---|---|---|
| Means | The **input program** is wrong | The **transpiler** is wrong |
| Response | Message with a span, controlled exit | Crash loudly |
| Expected? | Yes — normal operation | No — never |

Every failure in `main` is the left column — a missing file is expected operation. `FATAL_*` belongs
to the right (D-029 uses `FATAL_PATH_ABORT` precisely because `arena_alloc(a, 0)` is a transpiler
bug). Routing a usage error through a `FATAL_` macro inverts the distinction at every future call
site. The intent was only to reuse the prefix, which confirms the point: **same shape, different
family** — a future `KRM_ERR_PATH_EXIT` / `_GOTO` pair, kept visibly apart from `KRM_FATAL_*`.

**`__FILE__` / `__LINE__` belong on the assertion side only.** They tell the developer where the
transpiler broke. On a diagnostic they mislead: a user seeing `main.c:57` beside "file not found"
reasonably concludes the compiler crashed. The diagnostic-side coordinates are the **user's** file
and line — which is what spans are for.

**Deferred anyway.** Here a macro would only save a `fprintf`, and would invent an error-reporting
convention in `main.c` three stages before the code that needs one. D-033 moved the sink trigger into
the lexer stage on exactly this reasoning: design against the first real diagnostic, a lexical error
with a span. `FATAL_PATH_ABORT` earns its keep because it does real work; this would not yet.

If it is ever written: **pass the label as an argument**, so the jump target is visible at the call
site.

---

## 5. `fatal()` exited `1`

`fatal()`'s exit path ended in `exit(1)` — the value D-035 reserves for the sanitizers. A fatal exit
would have been indistinguishable from ASan firing, which is the case the reservation exists to
prevent, and it becomes assertable once per-process tests land (D-029). `abort()` was never the
problem: it dies on `SIGABRT`, observed as `134`.

**New class: `KRM_EXIT_ERR_INTERNAL`** — kramac itself is wrong. Named for the *condition*, not the
mechanism (`ASSERT` was considered and rejected on that ground), matching STYLE.md §11's "the
transpiler is wrong" and the prior art's "internal compiler error" (GCC bug 22600, distinct code
added for 4.2).

**The problem it exposes, deferred.** `fatal()` derives its exit status from `FatalPath`, which
encodes only *how to die* (abort or exit). The arena's allocation failure and a violated invariant
both take the exit path, but they are different classes — `OOM` and `INTERNAL`. So **the arena's OOM
currently misreports as `INTERNAL`.** Three ways out, not yet chosen:

1. `fatal()` takes an exit-status parameter — honest; changes the signature and every call site.
2. Extend `FatalPath` so each path implies a status — no signature change, but it conflates *how* with
   *why*, which is the root cause.
3. Keep `fatal()` for bugs only; OOM reports through something else — cleanest, most work.

Commit 4 was kept scoped: `exit(1)` → `INTERNAL`, so nothing exits `1`. The rest goes to
`m1/fatal-cleanup` (§7).

---

## 6. `exit_codes.h`

The enum left `main.c` for `src/exit_codes.h` once it was clear the values are used everywhere —
including by the fatal path, and by future per-process unit tests. D-035 already called the header
the binary's public contract; it only became true once it was a header.

- **Include direction is one-way:** `fatal.c` and `main.c` include `exit_codes.h`; it includes nothing
  of the project's.
- **Header comment** states that the test driver duplicates these numbers (a Python driver cannot
  include a C header), so the header is the single source of truth and the driver's mapping follows
  it; and that values are **append-only once a fixture asserts one**.
- **`UNKNOWN`'s comment** says no fixture may assert it — its value moves as classes are inserted
  before it. It is a moving upper bound, usable as a computed bound but never a remembered one.
- **Filename.** `exit_codes_pub.h` was floated; the `_pub` suffix only means something if a `_priv`
  counterpart ever exists, so plain `exit_codes.h` plus the `[PUBLIC]` comment.
- The type sheds the file-local trailing underscore (STYLE.md §2.1) now that it is exported.

Final classes: `SUCCESS` 0, `USAGE` 2, `INPUT`, `OOM`, `ENV`, `INTERNAL`, `UNKNOWN`.

---

## 7. `m1/fatal-cleanup`

A small branch of its own, landing **before** the lexer, which is the first heavy user of both error
families. Its own branch rather than the lexer branch's first commit, so it gets its own gate run and
merge instead of being scrolled past in lexer review. Recorded in PROJECT.md *Next up*, not as a code
`TODO` — *Next up* is read at the start of every session; a `TODO` in `fatal.h` only when someone
opens it.

Scope — one question (what the fatal family is and how it is named), several mechanical consequences:

- **Rename `FATAL_*` → `KRM_FATAL_*`.** Do the rename *before* `KRM_ERR_*` exists: a rename is only
  mechanical while there is one family to rename.
- **Settle the macro-prefix convention once.** There are currently three spellings: `FATAL_PATH_ABORT`
  (no prefix), `KRM_EXIT_*` (prefixed), and the header guard `KRAMA_EXIT_CODES_H`. STYLE.md §2.1's
  prefix rule is written about *functions*, not macros, so none is wrong yet. Decide whether every
  project macro carries `KRM_`, or only public contracts.
- **The arena's OOM exits `OOM`**, via one of the three routes in §5.
- **Reserve `KRM_ERR_PATH_EXIT` / `_GOTO`** naming for the diagnostic family, even if nothing uses it.

---

## 8. Commit 5 checklist — the test driver

**Start with the manifest.** The driver's whole job is reading it; its shape decides the parser, the
validation and the reporting.

### Decisions still to make

- **Manifest keys** — exact names, and whether a stage gets a defaults or group table
  (`krama-notes-14` §7 left both open). D-036 records the shape, deliberately not the keys.
- **Driver structure** — one file (`tests/harness/kramat.py`) or a small package.
- **Argv-stage fixture names**, and **`krama_src/test.krm`**: reuse it as the valid-file case, or keep
  it as scratch and gitignore it. **Not both** — a fixture's contents are depended on by the gate, so
  editing it to try out syntax would break the suite. If it goes scratch: `git rm --cached`, since
  ignoring a tracked file does not untrack it.

### Already decided — implement

- **Python 3.11+, standard library only**, invoked as `python3`; version checked at start with a clear
  message rather than a traceback on `import tomllib` (D-037).
- **One `cases.toml` per stage** under `tests/stage/<name>/`, discovered by a **sorted** walk; paths
  resolve **relative to the manifest** (D-036).
- **Invocation explicit per case**, as an array of strings; never inferred from a path.
- **Required keys enforced, unknown keys rejected** — a typo like `stdderr` must not make a case
  assert nothing.
- **Symbolic exit names** (`OK`, `USAGE`, `INPUT`, …) mapped to numbers **in one place**, with a
  comment naming `src/exit_codes.h` as the source of truth. `UNKNOWN` is never assertable.
- **stderr asserted non-empty, never by wording.** No stdout assertion on this branch.
- **Run every selected case**; report `N / Total passed`; exit non-zero if any failed. Failures report
  as `stage/case`, with what was expected and what happened — the driver is a development tool, not
  only a gate.
- **Filters** (a stage, a case name) select but **never change the verdict**; the report says what was
  selected; **a filter matching nothing is an error**, not `0 / 0 passed`.
- **Run against `build/kramac-san`**, so a leak or UB in the read path becomes a non-zero exit. Exit
  `1` means the sanitizer fired, never a verdict.
- Temp files via `mktemp`-equivalents, cleaned up on failure too.

### The cases (`krama-notes-13` §3)

| Case | Invocation | Expect |
|---|---|---|
| No argument | none | `USAGE`, stderr non-empty |
| Extra arguments | two paths | `USAGE`, stderr non-empty |
| Missing path | a path that does not exist | `INPUT`, stderr non-empty |
| Directory as argument | `tests/` or similar | `INPUT`, stderr non-empty |
| Valid file | a small stable `.krm` | `SUCCESS` |
| Empty file | an empty `.krm` | `SUCCESS` — also pre-tests the lexer's `n + 1` bound |

The directory case is the one that proved `fstat`: before it, a directory exited `OOM`.

### Housekeeping owed in the same commit

- **Makefile part B:** wire the harness into `check` after the unit tests, and resolve the `TODO`
  about running the sanitized build.
- **STYLE.md §13:** add the deferred row for Python conventions — layout, formatting, whether the
  format gate covers it — now that Python exists.
- **`__pycache__`** is already covered by the author's `.gitignore`.
- **Commit body** states that commit 4's behaviour is now under test, closing the one-commit window
  (`krama-notes-15` §3).

### Then

Merge `m1/cli-args` (`--no-ff`, D-030), with a PROJECT.md Log entry for the session. Then
`m1/fatal-cleanup`. Then the lexer.
