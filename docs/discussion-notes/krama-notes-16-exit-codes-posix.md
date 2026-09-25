# Krama — Three builds, exit-code classes, and the POSIX line

**Date:** 2026-09-25
**Status:** Discussion record. **Not normative.** §7 lists what is queued.

> Nothing here is binding. Where this and `DECISIONS.md` / `STYLE.md` / `COMMITS.md` /
> `krama-spec-v1.md` disagree, those win.

Covers commits 3 and 4 of `m1/cli-args`, planned in `krama-notes-15-commit-plan`.

---

## 1. How to read this

- **§2** — the three builds and why they cannot share objects.
- **§3** — four Makefile traps hit on the way.
- **§4** — `main`'s argument contract.
- **§5** — the exit-code enum: classes, `UNKNOWN`, and the prior art.
- **§6** — detecting a directory, and the POSIX decision that came out of it.
- **§7** — queued records, open.
- **§8** — conclusions.

---

## 2. Three builds, three object trees

| | unit tests | `kramac-san` | `kramac` |
|---|---|---|---|
| Sources | `src/*.c` minus `main.c`, plus `tests/unit_tests/*.c` | all of `src/*.c` | all of `src/*.c` |
| Objects | `tests/build/*.o` | `build/san/*.o` | `build/*.o` |
| Flags | `CFLAGS` + `SANFLAGS` + `-DKRAMA_TEST_SUITE` | `CFLAGS` + `SANFLAGS` | `CFLAGS` |
| Includes | `TEST_INCLUDES` | `INCLUDES` | `INCLUDES` |
| Built by | `test`, `check` | `san_build`, `check` | `all`, `check` |

**Why the object trees must differ.** Make compares **timestamps, not flags**. Two builds writing
`%.o` to the same directory will happily reuse each other's objects, producing a binary that is
sanitized — or not — by accident, with nothing failing. The unit-test and sanitized builds share
flags but disagree on `main.c`, which cannot be both absent and present in one tree, so the split is
forced rather than stylistic.

The cost is compiling `src/*.c` twice under sanitizers. A second of build time against a shared
tree that is correct only by accident.

**`all` folded into `check`.** The PROJECT.md row asked for *compilation* coverage — `main.c` was
filtered out of the test build, so nothing compiled it — not for the production binary to be run.
Execution is the harness's job, against `kramac-san`. Once the old `ifneq` block was deleted,
nothing rewrites `BUILD_DIR` mid-invocation, so `all` works as a plain prerequisite and a sub-make
was unnecessary.

**Prerequisite order** is `fmt_check all test san_build`: cheapest gate first, then compilation,
then the sanitized trees. Serial Make builds prerequisites left to right; under `-j` the order is
not guaranteed, and nothing runs the gate in parallel today.

---

## 3. Four Makefile traps

1. **A pattern rule with two prerequisites requires both to exist.**
   `$(TEST_BUILD_DIR)/%.o: $(SRC_DIR)/%.c $(TEST_DIR)/%.c` looks like "either source", but Make
   discards the rule unless *both* `src/arena.c` and `tests/unit_tests/arena.c` exist — hence
   *"No rule to make target 'tests/build/arena.o'"*. Two separate rules with the same target
   pattern, one per source directory, is the fix: Make picks whichever has an existing prerequisite.
2. **`=` replaces; three assignments to one variable keep only the last.** `TEST_OBJS` was assigned
   three times, so only the final line survived and the link found no `arena_*` symbols. Use `+=`
   for the second group.
3. **`filter-out` matches whole words.** `filter-out main.o` does nothing against
   `tests/build/main.o`. Filter at the source level before the `patsubst`, or match `%/main.o`.
4. **Only the production `.d` files were included.** Dependency tracking silently covered one of
   three trees; header changes would not have rebuilt the other two.

Also: `.PHONY` needs every phony target (`san_build` was missed); `clean_test` clears
`build/san/` as well, since sanitized objects are test artifacts that happen to live under `build/`;
`clean` needs no change, as it removes `build/` wholesale.

---

## 4. `main`'s argument contract

```
if argc != 2            -> usage error, message to stderr, exit KRM_EXIT_ERR_USAGE
path = argv[1]
open / stat / read      -> on failure, the matching class (§5)
dump contents           -> exit KRM_EXIT_SUCCESS
```

- **Both `argc` cases collapse into one check.** "No file provided" and "Too many arguments" differ
  only in the message; the corpus still distinguishes them because they are different *inputs*.
- **`argv[1]`, not `argv[argc - 1]`.** Once `argc != 2` is a usage error the two are identical, and
  the latter implies a variable argument count that the contract does not allow.
- **Usage messages go to stderr.** Nothing asserts their wording (`krama-notes-13` §3), so they are
  written for a human.
- The dump is a debugging convenience, not a tested behaviour; it goes or hides behind a macro once
  the lexer lands.

---

## 5. The exit-code enum

Final shape, per D-035's rule (`0` for success, remaining classes from `2`, `1` unused):

| Name | Covers |
|---|---|
| `KRM_EXIT_SUCCESS` | — |
| `KRM_EXIT_ERR_USAGE` = 2 | wrong argument count, or an option not accepted. **Nothing was read** |
| `KRM_EXIT_ERR_INPUT` | the path is wrong — missing file, not a regular file. The user's to fix |
| `KRM_EXIT_ERR_OOM` | an allocation failed — arena, token buffer, diagnostic string |
| `KRM_EXIT_ERR_ENV` | an operation that should have worked did not, through no fault of the source |
| `KRM_EXIT_ERR_UNKNOWN` | unclassified; **never asserted by a fixture** |

### 5.1 `UNKNOWN` as an upper bound

New classes are inserted *before* `UNKNOWN`, so its value moves. That is safe only because **no
fixture may assert it** — a rule that now lives in the header comment, since it is not visible from
the enum alone. Its value is also usable as a *computed* bound ("is this a known class") rather than
a remembered one.

It reads like a default because it is the initialiser for `exit_status`, but after §5.2 the only
path that can actually reach it is a short `fread` with neither `ferror` nor `feof` set — a state
the code can express but cannot explain. That is the honest meaning: **a class is missing.**

### 5.2 How finely to cut

**Prior art first.** GCC returns `1` for ordinary errors; OOM prints a message and exits the same
way. The distinction GCC *did* eventually add (bug 22600, fixed for 4.2) was between an internal
compiler error and correct rejection of incorrect code — i.e. **whose fault it is**, not which
resource failed. `sysexits.h` does split `EX_IOERR` and `EX_OSERR`, but that convention grew up
around mail daemons, where a caller decides whether to retry. The small compilers — chibicc, tcc,
cproc, harec, and Nim's compiler — exit `1` for everything.

So the axis that matters is fault, giving three groups: **input** (the user fixes it), **environment**
(nobody fixes it in the source), and **program** (a bug — which goes through `FATAL_PATH_ABORT` and
never reaches an exit class at all, D-029).

**Why `OOM` is split out anyway**, against that prior art: allocation happens in several distinct
places in this program — arena, token buffer, diagnostic strings — so "an allocation failed" is a
class worth picking out of the environment group by hand. Krama can afford to be richer than the
field here for a specific reason worth remembering: **the exit code is consumed by a test driver**
that must distinguish "rejected as designed" from "the sanitizer fired". None of those compilers
has that constraint. It is also why the classes should stay few.

**`ENV` exists now, not later**, because `fread` and friends are already called. Untestable without
fault injection, but the code paths are real.

**Why Hare rather than Nim as an inspiration**, since Nim also transpiles to C: Nim's compiler is
written in Nim, so reading it teaches Nim; its C output carries a runtime with GC and exceptions,
not the readable C spec §1.1 wants; and its surface (macros, generics, effects) is the opposite of a
small explicit language. Hare is C, explicit, manually managed, small.

---

## 6. Detecting a directory, and the POSIX line

**The problem.** On Linux `fopen` on a directory **succeeds**; the failure appears later. With
`fseek`/`ftell` sizing, a directory yields an astronomical size and the failure surfaces as a failed
`malloc` — reported as `OOM`. That is a **misclassification**, not merely a vague message, which is
what forced a decision.

Three routes were weighed:

| Route | Verdict |
|---|---|
| ISO-only **size cap** (reject files over N MB) | Rejected. A policy standing in for a test: it rejects legitimate large files, and a small directory would still slip through to `fread` |
| `errno == EISDIR` after the read fails | Rejected. The macro is POSIX and universally defined, and it needs no feature-test macro — but **POSIX does not specify** that reading a directory produces it. Older BSDs returned raw directory bytes. It rests on observed behaviour |
| **`fstat` + `S_ISREG`** | **Chosen.** POSIX specifies both the call and the meaning of the mode bits, so the question is answered by contract |

**`fstat`, not `stat`.** `stat(path)` followed by `fopen(path)` looks the name up twice and the file
can change in between — a TOCTOU race. `fstat(fileno(fp), &st)` asks about the descriptor already
held, so what is inspected is what will be read.

**What it costs.** One line, `#define _POSIX_C_SOURCE 200809L`, before **every** include (feature
macros are read when the first libc header is processed), plus `<sys/stat.h>`. `fileno` lives in
`<stdio.h>` but is only declared when a feature-test macro is active — the other reason the
`#define` must come first. Kept file-local while exactly one file needs it; it moves to `CFLAGS`
when a second one does.

**What it buys.** `st_size` replaces `fseek` + `ftell` + `rewind`, so **two failure paths disappear**
and the function gets shorter. The change removes more than it adds.

**What is given up.** Native Windows toolchains (MSVC has `_fstat` / `_fileno`; MinGW and Cygwin
have the POSIX spellings), plus bare-metal and other exotica. Every BSD, macOS, Solaris, illumos,
Linux with any libc, WSL and WASI are all in — **BSD is POSIX**, which was the misapprehension that
nearly sent this the other way. Also given up on glibc: defining `_POSIX_C_SOURCE` disables
`_DEFAULT_SOURCE`, so extensions outside POSIX.1-2008 (`asprintf`, `strsep`) need their own
justification. Arguably a feature.

**`-std=gnu23` was not taken** as a way to avoid the macro: D-019 pinned `-std=c23` deliberately, and
`gnu23` would also admit GNU language extensions — a larger change than one `#define`.

**The boundary this draws:** the *transpiler* may use POSIX; the *emitted runtime* stays portable C
per spec §8.1. Two programs, two promises. Much of the tooling already assumed a Unix host (spec
§11.1 compiles generated C with `cc`; the driver is Python from a Makefile), so this makes an
existing assumption explicit in the source.

### 6.1 Reference data — how big do source files get?

Gathered while weighing the size cap. SQLite's amalgamation, from SQLite's own documentation: over
3.8 MB / ~110K lines at 3.6.21 (2009); over 5.4 MB / ~153K lines at 3.8.8; over 6.4 MB / ~184K lines
at 3.9.0; over 7.5 MB / 220K+ lines at 2018-11-24; over 8.4 MB / ~238K lines at 2021-12-29. Roughly
half a megabyte a year. SQLite also ships a **split** amalgamation (`sqlite3-1.c`, `sqlite3-2.c`, …)
for compilers that cannot handle a file that large — real compilers do have practical ceilings, they
just are not documented as policy. Everything larger in the wild is machine-generated: preprocessed
translation units, generated parser tables, LLVM `.inc` files. Hand-written single files rarely pass
a megabyte.

---

## 7. Queued records, open

### Queued

9. **D-038 — the transpiler targets POSIX.1-2008; the emitted runtime stays ISO C.** Session 16.
   Rejected: the ISO-only size cap, and `errno == EISDIR`.

### Owed on commit 4

- Delete PROJECT.md's *Deliberately incomplete* row for the hardcoded source path, and the row about
  `check` not compiling `main.c` (the latter closed in commit 3, deliberately worded there as
  "can now be closed").
- Move `krama_src/test.krm` to the argv stage — or gitignore it as scratch. A fixture and a scratch
  file cannot be the same file: the gate would depend on contents that get edited for experiments.
- The commit body states the harness gap (`krama-notes-15` §5).

### Open

- Exact `.expected` dump format — still deferred to the lexer (STYLE.md §13).
- Whether `main`'s dump is deleted or macro-guarded when the lexer lands.

---

## 8. Conclusions — what we are going ahead with

1. **Three builds, three object trees**, because Make compares timestamps and not flags.
2. **`all` is a plain prerequisite of `check`** — compilation coverage, not execution.
3. **`main` takes exactly one path**; both `argc` failures share a class and differ only in message.
4. **Six exit classes**, cut along fault rather than resource, with `OOM` split out because
   allocation happens in many places and the driver consumes these codes.
5. **`UNKNOWN` is a moving upper bound** that no fixture may assert, and after classification the
   only path reaching it is a short read with neither `ferror` nor `feof`.
6. **`fstat` + `S_ISREG` for the directory case**, taking `_POSIX_C_SOURCE 200809L` in `main.c`
   alone, which also removes `fseek`, `ftell`, `rewind` and two failure paths.
7. **The transpiler may use POSIX; the emitted runtime stays portable C.** Recorded as D-038.
