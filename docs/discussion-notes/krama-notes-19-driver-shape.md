# Krama — Commit 5 plan, error levels, and the driver's shape

**Date:** 2026-09-30
**Status:** Discussion record. **Not normative.** §8 is where the next session starts.

> Nothing here is binding. Where this and `DECISIONS.md` / `STYLE.md` / `COMMITS.md` /
> `krama-spec-v1.md` disagree, those win.

Continues `krama-notes-18-case-manifest`, after the interruption recorded in
`aside-notes-01-ground-up`. Back on `m1/cli-args`, commit 5.

---

## 1. How to read this

- **§2** — which records go on which branch, and numbering.
- **§3** — the commit 5 plan.
- **§4** — step 1: manifest errors, case errors, case failures.
- **§5** — step 2: the driver's shape.
- **§6** — step 3, in progress: fixtures.
- **§7** — deferred by the straight-and-narrow rule.
- **§8** — queued records, open, next session.
- **§9** — conclusions.

---

## 2. Branches and numbering

**The records split by what is on disk.** The project context holds the `m1/ground-up` versions of
CLAUDE.md, DECISIONS.md, STYLE.md and COMMITS.md; none of that is on `m1/cli-args`.

| Record | Branch | Why |
|---|---|---|
| Log entry, 2026-09-25 + 2026-09-29 manifest session | `m1/cli-args` | Commits 3–4, D-038, notes-16/17/18 — all on this branch. The Log currently stops at 2026-09-23 |
| Log entry, 2026-09-29/30 `/ground-up` | `m1/ground-up`, first commit | It cites decisions and a folder that do not exist on `cli-args`; notes-17 §2's rule — never describe a state not on disk |
| Current → Next up | `m1/cli-args` | A plan, not a state: `cli-args` → `ground-up` → `fatal-cleanup` → `learn-alignment` → lexer |

**`cli-args` goes first; `ground-up` renumbers from it.**

- Commit 5's decision entry is **D-039**. `ground-up`'s entries become **D-040** and **D-041**.
- Commit 5's STYLE.md bump is **1.6**. `ground-up` becomes **1.7**.
- On `ground-up`, update the pointers: CLAUDE.md's closing line, COMMITS.md's intro, the aside.
- Never copy the project-context documents into `cli-args`. Edit by hand; `git diff` before staging.

**The `/ground-up` log entry, when written.** *Didn't* carries the separate repository,
`learn(<concept>)` and Gleam; also worth including: the full fluency target at the gate, the tight
trigger, and `m1/learn/alignment`. The pattern-matching score in aside §3.2 was an example only —
**not logged, not queued.**

---

## 3. Commit 5 plan

| # | Step | Kind |
|---|---|---|
| 1 | The `.expected` existence check — grew into error levels (§4) | decision |
| 2 | Driver structure (§5) | decision |
| 3 | Fixtures — stage name, fixture names, `krama_src/test.krm` (§6) | decision |
| 4 | Driver pseudocode — the programmer writes, Claude reviews | code |
| 5 | Makefile part B — harness into `check`; the sanitized-build `TODO` | code |
| 6 | Docs — D-039, STYLE.md §13 Python row, `exit_codes.h` comment, PROJECT.md rows, log | records |
| 7 | Commit body — states the one-commit window is closed | records |

**Part 1 and 2 of the driver can be written before step 3 settles:** version check, root, header,
`--stage`, finding `cases.toml`, validating the contract. Execution waits for the fixtures.

---

## 4. Step 1 — what is wrong, and whose it is

### 4.1 A missing `.expected` is one case's problem

The first answer: fail the case, not the manifest — the fault is local, and every other case still
runs. The mentor's first attempt at a principle ("the driver cannot check files while validating
text, because `missing_path` must not exist") was **wrong**: that file is in `args`, and its
`expected` is `#none`. Corrected by the programmer.

### 4.2 The line: where does it become apparent?

**A manifest error is what the TOML contract can check on its own.** Anything that needs another
file becomes apparent only at run time, and belongs to the case. The contract can say a value is a
string; only `exit_codes.h` can say it is a real exit name.

| Check | Level |
|---|---|
| A table other than `[[case]]` | manifest |
| Missing or unknown key (`agrs`) | manifest |
| Wrong type (`args = "x.krm"`) | manifest |
| Duplicate `name` | manifest |
| Unknown `#` command | manifest |
| Explicit path not ending `.expected` | manifest |
| `#derive` without exactly one `.krm` in `args` | manifest |
| `return_code` not in the header map; `UNKNOWN` asserted | **case error** |
| `.expected` missing on disk | **case error** |
| Exit code, stderr or stdout wrong | **case failure** |

An `error_code` field was floated as a way to make the name checkable by the contract. TOML has no
enum type, so a field could only check "is a string" too.

### 4.3 Errors and failures report differently

A **failure** means kramac did the wrong thing. An **error** means the case could not be judged —
fix the fixture, not kramac. pytest makes the same split. Both count against `N / Total`; both make
the run exit non-zero. A misspelled exit name must read `ERROR — unknown exit name …`, not
`expected KMR_…, got 3`, which blames kramac.

**Commit type for a manifest fix:** `test: …`. `harness` is not in the closed scope vocabulary
(D-031), and COMMITS.md already writes driver changes as `test:`.

---

## 5. Step 2 — the driver's shape

### 5.1 One file

**`tests/harness/kramat.py`.** Straight and narrow until a fork: a package only when a job must
stand alone, or when cases stop being uniform. PEP 8 asks only for short, lowercase names; the one
pattern to avoid is `test_*.py` / `*_test.py`, which pytest collects.

The programmer's framing: the harness is kept small because verification lives in program output and
files on disk, and the TOML contract, with a fixed key set and explicit markers for empty and
unneeded, decides the flow.

### 5.2 Responsibilities, in order

1. **Version check** — before anything imports `tomllib` (D-037).
2. **Repository root** — see §5.4.
3. **Header** — parse `src/exit_codes.h` on every run (notes-18 §4); what is on disk is the truth.
4. **Discovery** — `--stage <name>`, or all stages by a sorted walk. A filter matching nothing is
   an error.
5. **Validation** — each manifest when its stage is reached, not all up front.
6. **Execution** — per case: run `kramac-san`, capture exit, stderr, stdout; compare; `#derive`,
   explicit path, or `#none` as notes-18 §7. *Waits for step 3.*
7. **Report** — pass / failure / error per case; `N / Total`; what did not match. No diagnostics
   beyond that — the harness does not know kramac's internals.

### 5.3 Three levels of error

| Level | Examples | Effect |
|---|---|---|
| Run | Python too old; header unparseable or self-check fails; `kramac-san` missing | Stop. Nothing can be judged |
| Manifest | A §4.2 contract violation | That stage is rejected; the run continues |
| Case | §4.2's case errors | That case is an error; the rest run |

**A rejected manifest fails the run** even when every counted case passed. Its cases never enter
`Total`, so `N / Total` stays true of what ran. The **verdict goes last** —
`FAILED — 1 manifest rejected (tests/stage/parse)` after the counts — so the final line is never a
clean-looking `10/10 passed`.

### 5.4 Finding the root

**Two levels above the driver file**, resolved from `__file__` made absolute — **not** the string
`../../`, which resolves against the current directory and silently goes wrong when `make` runs
from elsewhere. Recorded as a *Deliberately incomplete* row with a `TODO` pointing at it (STYLE.md
§10.4); expected to evolve into a deliberate entry.

### 5.5 Invocation

**`argparse`**, as the standard library intends — `--stage args`, later `--case <name>`. Hand-rolled
`key=value` would be engineering for its own sake. `argparse` exits `2` on a bad invocation: the
*driver's* status, unrelated to `KRM_EXIT_ERR_USAGE` — a "whose exit status is this?" moment
(Friction, 2026-09-18).

### 5.6 kramac's working directory

D-036 resolves paths relative to the manifest, but kramac resolves its arguments against its own
working directory. Two ways to reconcile them:

- **(a) Run kramac with the stage directory as its working directory.** `args` reach kramac exactly
  as written; `build/kramac-san` needs an absolute path. **Chosen.**
- **(b) Rewrite `args`**, joining the stage directory onto each. Rejected: kramac then receives
  something the manifest does not say, which quietly assumes every argument is a path — against
  D-036's *invocation explicit per case*.
- **(b′) Full paths written in the manifest.** Honest about the command line, but moving a stage
  directory then needs edits inside it — the other half of D-036.

The programmer's reason for (a): the manifest reads naturally, and the file named is the file beside
it.

---

## 6. Step 3 — fixtures, in progress

- **Stage: `tests/stage/args/`.** More argument cases will join it. Harmless collision noted: `args`
  is also a manifest key, so `grep args` matches both.
- **`krama_src/test.krm` — open.** Fixture or scratch, never both (notes-17 §8). If scratch:
  `git rm --cached` and gitignore it, then decide what the valid-file fixture holds — nothing checks
  its content under `#none` today, but the lexer stage will.
- **Fixture names** — after that.

---

## 7. Deferred — straight and narrow

**No per-case timeout.** Nothing yet proves the need. A hang is not a *silent* failure — `make
check` simply does not return — so the fork announces itself. Recorded as a *Deliberately incomplete*
row (not STYLE.md, which governs how code is written). SQLite's test instrumentation was named as
possible prior art for loop counters visible to a harness; **unverified** — read it before citing it.

---

## 8. Queued records, open, next session

### Queued for commit 5

- **D-039** — the driver reads `exit_codes.h`; supersedes D-035's "duplicated in the driver"
  consequence. Session 18/19. Rejected: a hand-kept map, X-macros, program-side metadata
  (notes-18 §5).
- **STYLE.md 1.6** — §13 Python row: layout, naming (§5.1), formatting, whether `fmt_check` covers
  it.
- **`exit_codes.h` header comment** — no longer "the driver duplicates these numbers".
- **PROJECT.md *Deliberately incomplete*:**
  - root found two levels above the driver (§5.4);
  - no per-case timeout (§7);
  - stdout unchecked under `#none`, and `main` still dumps the file (notes-18 §7.4);
  - the `make test` row may close with Makefile part B — check whether `test` or only `check` runs
    the harness.
- **Manifest keys** — D-036 calls them implementation detail, so not a DECISIONS entry. Where they
  are written down (a comment atop each `cases.toml`, or a `tests/` README) is open.
- **Log entry** and **Current**, per §2.

### Open

- `krama_src/test.krm`: fixture or scratch (§6).
- Fixture names for the six cases.
- Rendering exit values outside the map — `1`, `134` (notes-18 §3).

### Next session

1. **Step 4, parts 1–2:** the programmer writes pseudocode for the version check, root, header
   parse, `--stage` and discovery, and contract validation. Review against notes-18 and §4–§5 here.
2. Then step 3's open items, then execution.

---

## 9. Conclusions — what we are going ahead with

1. **`cli-args` numbers first:** D-039, STYLE.md 1.6. `ground-up` renumbers to D-040/D-041 and 1.7.
2. **Records follow the disk:** the `/ground-up` log entry lands on `m1/ground-up`.
3. **Manifest error** = what the contract can check alone. **Case error** = needs another file.
   **Case failure** = kramac was wrong. Errors and failures are reported differently.
4. **Three levels:** run stops; manifest rejects its stage and fails the run; case is counted.
   Verdict last.
5. **One file, `tests/harness/kramat.py`, `argparse`, root from `__file__`.**
6. **kramac runs in the stage directory**; `args` pass through verbatim.
7. **Stage `tests/stage/args/`.**
8. **No timeout** until a hang proves the need.
