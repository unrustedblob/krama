# Krama — Git branch workflow and Makefile targets

**Date:** 2026-09-17 / 2026-09-18
**Status:** Discussion record. **Not normative.** Decided as `D-030` and `D-031`; COMMITS.md
rewritten and the Makefile targets added on branch `m1/workflow`.

> Nothing here is binding. Where this and `DECISIONS.md` / `COMMITS.md` disagree, those win.

---

## 1. How to read this

- **§2** — the starting idea and what prompted it.
- **§3** — what chibicc actually does (checked, not assumed).
- **§4** — the long-lived-branch trap, and reachability.
- **§5** — the three merge styles and the choice.
- **§6** — merge commit subject format.
- **§7** — Rule 1 and `git rebase --exec`.
- **§8** — Makefile targets, as built.
- **§9** — exit status: the mechanism the whole gate rests on, and the UBSan gap.
- **§10** — commit hygiene: staging, amend, reachability.
- **§11** — settled, deferred, owed.

---

## 2. Starting idea

Use a working branch per milestone (`dev` / `milestone-1`) and fold each achievement into `main` as
one consolidated commit — source-file read, arena, AST nodes, diagnostic sink, and so on. `main`
reads as a *summarised-with-relevant-details* story: `chore`/`docs` noise gone, `feat`s and `fix`es
consolidated.

Solo project, so arguably more effort than needed. Adopted anyway as practice.

**Existing history is left as-is.** It is pushed, and issues may reference it (`COMMITS.md` Rule 4).
The workflow starts from the next branch.

---

## 3. chibicc, checked

The idea was attributed to chibicc. The repository shows otherwise:

- `main` is **316 commits, strictly linear, zero merge commits.**
- Each commit is one small feature ("Add + and - operators", "Support multi-letter local
  variables"). The README states each commit was written for readability — one commit teaches one
  feature.
- The original history was moved to a `historical/old` branch; the clean `main` was uploaded fresh
  in September 2020.

So chibicc **curated by rewriting history wholesale** — acceptable for a book companion, forbidden
here by Rule 4. Branches achieve the same effect legitimately: curation happens *before* anything
reaches `main`. chibicc's granularity is also finer than "one commit per stage".

---

## 4. The trap, and reachability

### 4.1 Long-lived branch + squash

```
milestone-1:  a1 - a2 - a3 ---- b1 - b2
                          \
main:         M ---------- S1          (S1 = a1+a2+a3 flattened)
```

S1 has the same content as a1–a3 but no parent link to them. The merge base of the two branches
stays at **M**, so the next squash re-proposes a1–a3 → conflicts or duplicated changes, growing
every stage.

**Ways out:**

- **Short-lived branch per stage** (recommended) — branch from `main`, merge, delete. Next stage
  branches from the updated `main`. `main` is the spine; everything comes out of it and goes back.
  (Analogy offered: a borrowed pointer — the borrow ends before the next one starts.)
- Hard-reset the long-lived branch to `main` after every merge — works until the day it is
  forgotten.

### 4.2 Stale `main`

Merging on GitHub moves only the *remote* `main`. Branching from a stale local `main` recreates the
trap. **"Branch from `main`" means pull first, then branch.**

### 4.3 Reachability — why `git branch -d` behaves differently

`-d` asks: *can the branch tip be reached from `main`'s tip by following parent links?*

| Merge style | Parents of the new `main` commit | Branch tip reachable? | `-d` |
|---|---|---|---|
| Squash | one (old `main` tip) | no | refuses; needs `-D` |
| `--no-ff` | two (old `main` tip, branch tip) | yes | succeeds |

Precision point: squashed commits are not *lost* — they still exist on the branch until it is
deleted. Git is not failing a hash lookup; it walks parents.

`git log --first-parent` follows only the first parent of each merge, which is what makes the
per-stage summary view work (§5).

### 4.4 Deleting branches

Yes — delete after merging. GitHub offers a "Delete branch" button and an auto-delete setting; the
PR page remains the record. Locally, `git fetch --prune` clears references to deleted remote
branches.

---

## 5. Merge styles — the choice

**Conflict found:** "one dense commit per stage" contradicts `COMMITS.md` Rule 3 (*N logical commits,
not one* — otherwise bisect can only say "somewhere in here"). Rule 5 already covers the "failures
are lost" concern: git is codebase evolution, `PROJECT.md` is the work log.

| Option | Shape on `main` | Summary per stage | Bisect | Rule 3 |
|---|---|---|---|---|
| 1. Squash-merge | one commit per stage | yes (the commit) | coarse | must be amended |
| **2. `rebase -i` on branch, then `--no-ff` merge** | logical commits + merge commit | yes (merge message) | precise | satisfied |
| 3. Rebase-and-merge | linear logical commits | no (tags only) | precise | satisfied |

**Chosen: option 2.**

- On the branch: `git rebase -i` down to a few logical commits (what Rule 3 already asks).
- Merge with `--no-ff`; the dense stage summary is the **merge commit's** message.
- `git log --first-parent main` → one line per stage. Full log → logical commits.

---

## 6. Merge commit subject

The `<type>(<scope>)` format has no type for a merge. **Use the merged branch name as the label:**

```
m1/ast-nodes: define expression node inventory
```

- Keeps the `<label>: <subject>` shape; `git log --first-parent --oneline` lists stages.
- **Override the default message every time** — Git's `Merge branch '...'` and GitHub's
  `Merge pull request #N from ...`.
- **Label must be the exact branch string.** Needs one naming rule (e.g. match the scope name).
  Not yet chosen.
- `Refs:` / `Closes:` trailers go in the merge body.
- **Tags** follow the pattern already in `COMMITS.md` deferred notes: `v0.1-milestone1`.

---

## 7. Rule 1 and `git rebase --exec`

Position taken: every logical commit is already covered by build and tests on the branch; merge only
happens once everything is green.

**Gap:** green on the branch means green **at the tip**. `rebase -i` creates new intermediate states
that never existed while working. A reorder can leave commit 2 calling something whose header lands
in commit 3 — tip passes, commit 2 does not compile, bisect later stops on a false failure.

**Fix:**

```
git rebase -i <base> --exec "<build and test command>"
```

Runs the command after every rewritten commit and stops at the first failure, at that commit. Turns
"every logical commit builds" from an assumption into a check. Needs a single command → §8.

---

## 8. Makefile targets, as built

### 8.1 Starting state

- `test` built `$(TEST_TARGET)` but never ran it. `make test; echo $?` printed `0` after a
  compile-only run — a success that says nothing about whether tests pass.
- No `fmt` target, though `STYLE.md` §1 references `make fmt`.
- `.PHONY` was missing `test` and `clean_test`. A stray file named `test` would otherwise silently
  stop the target running.

### 8.2 Three targets

| Target | Purpose | Output |
|---|---|---|
| `fmt` | `clang-format -i` over `$(FMT_FILES)` — rewrites | command echoed, so the file list is visible and coverage can be eyeballed |
| `fmt_check` | `clang-format --dry-run --Werror` over the same list — reports only, edits nothing | `@`-silenced; says nothing when there is nothing to say |
| `check` | The merge gate: `check: test fmt_check`, recipe runs `$(TEST_TARGET)` | full test output |

Both format targets share one `FMT_FILES` variable, so the two lists cannot drift apart.

`FMT_FILES` is built with `$(shell find ...)` rather than `wildcard`. `find` recurses, which is
deliberate: source subdirectories are plausible later. The costs accepted are a subprocess per
`make` invocation, breakage on filenames containing spaces, and picking up any stray `.c`/`.h`
under a build directory.

**Ordering inside `check`:** cheapest check first. The format check is near-instant; the sanitized
test binary is not. Under `rebase --exec` the gate runs once per commit, so a formatting slip should
fail in a second rather than after the full suite. (The tempting reason — that bad formatting
signals broken syntax — does not hold: clang-format has a forgiving parser and formats code that
does not compile, and anything that broken would already have failed the `test` prerequisite.)

### 8.3 `MAKECMDGOALS` — the trap that would have disarmed the gate

The test-mode block was guarded by:

```
ifneq ($(filter test,$(MAKECMDGOALS)),)
```

`MAKECMDGOALS` holds **the words typed on the command line**, not the targets Make ends up building.
With `check: test`, typing `make check` leaves `MAKECMDGOALS` as `check` alone, so the block stays
off: no sanitizers, no `-DFUNC_TEST_SUITE`, no test sources, and an empty `TEST_TARGET`. Needing a
target does not add it to what was typed.

Fixed by filtering a word list — `$(filter test check,$(MAKECMDGOALS))`. `TEST_SRCS` was hoisted out
of the conditional so `FMT_FILES` can see it unconditionally.

### 8.4 Deferred here

- `make test` running the tests — waits for the `.func` / `.exec` pairs.
- A quiet mode for the test binary, reducing output to the `N / Total Passed` line, so a full check
  run reads as a short checklist.

---

## 9. Exit status — the mechanism under everything

Every program hands back a number when it ends: `0` for success, anything else for failure. The
shell exposes the last one as `$?`. The entire gate is that one number passed hand to hand:

```
test binary  →  make  →  git rebase --exec
```

Make runs a recipe line by line and stops at the first non-zero, then exits non-zero itself.
`rebase --exec` stops the rebase at the first commit whose command exits non-zero. Nothing in the
chain reads output — only the number.

**Verified in the codebase:** `tests/test_arena.c` ends `return failed != 0;`, so the binary reports
correctly.

**Whose exit status is this?** The recurring trap. `gcc ... sanity.c; echo $?` reports the
*compiler*, not the program; `make test; echo $?` reported a *build*, not a test run. Ask whose
number it is before reading it.

### 9.1 The UBSan gap

`SANFLAGS` was `-fsanitize=address,undefined`. The two behave differently on a hit:

| Sanitizer | On detection | Exit status |
|---|---|---|
| ASan | reports and stops the program | non-zero — gate sees it |
| UBSan (default) | reports and **keeps running** | `0` if the program then succeeds |

So undefined behaviour would print a report and still tell Make everything was fine.

**Demonstrated**, with `INT_MAX + argc` (`argc`, not `1`, so the overflow is invisible at compile
time and the sanitizer is what catches it):

| Build | Report | Reached the final print | `$?` |
|---|---|---|---|
| `-fsanitize=undefined` | yes | yes | `0` |
| `+ -fno-sanitize-recover=undefined` | yes | no | `1` |

**Fixed** by adding `-fno-sanitize-recover=undefined` to `SANFLAGS`, not to `CFLAGS` and not to a
new variable. Flags that only mean something together live together: it is inert without
`-fsanitize=undefined`, and `SANFLAGS` is already used at both compile and link, so one edit covers
every sanitized build. The alternative, `UBSAN_OPTIONS=halt_on_error=1` at run time, was rejected as
forgettable — it lives outside the repository.

---

## 10. Commit hygiene

- **Three places a change lives:** working tree → `git add` → staging area → `git commit` → history.
  Staging exists so a commit can hold part of the work. Stage by name, not `git add .`.
- **Keep experiments out of the repository.** Scratch files created while testing a flag show up as
  untracked and a careless `git add .` commits them.
- **Read your own diff before staging** — `git diff` is the smallest possible review.
- **`git commit --amend` does not edit a commit.** Commits are immutable; amend writes a *new*
  commit with the same changes and moves the branch note onto it, so the hash changes. The old
  commit survives as an **unreachable** object, recoverable via `git reflog` until garbage
  collection. Safe here only because branches are never pushed (Rule 4).
- **Reachability is one idea, used twice:** it is why an amended commit disappears from the log, and
  why `git branch -d` succeeds after a `--no-ff` merge but refuses after a squash.

### 10.1 Rejected: check results in the commit message

Recording "format clean, 39/39 passed" in the message body was considered as evidence that the gate
ran. Rejected: a message is hand-written, so it is a claim rather than evidence, and it cannot be
verified after the fact. Enforcement belongs in the `pre-push` hook and, eventually, CI. If ever
wanted, a trailer (`Checks: fmt, tests 39/39`) is the right shape, since trailers are the
machine-readable part.

---

## 11. Settled / deferred / owed

### Settled

- Short-lived branch per stage, from an up-to-date `main`, deleted after merge. Local only.
- Option 2: `rebase -i` on branch → `--no-ff` merge with dense summary message.
- Merge subject: `<branch-name>: <subject>`; branch naming `m<N>/<stage>`.
- `rebase --exec "make check"` enforces Rule 1 per logical commit.
- Scope optional from a closed vocabulary; issue numbers in trailers only (`Refs:` / `Closes:`).
- Existing history untouched.

### Recorded

- **`D-030`** — branch and merge workflow.
- **`D-031`** — optional scope, issue trailers.
- **`COMMITS.md`** — rewritten: subject rules, scopes, trailers, Branches, Merging, Rules 1/3/4,
  examples, hook notes.
- **`Makefile`** — `fmt`, `fmt_check`, `check`, `SANFLAGS`, `MAKECMDGOALS` fix, `.PHONY`.

### Deferred

- **Hooks** (`commit-msg`, `pre-commit`, `pre-push`) — limited hobby time. The `commit-msg` regex
  needs an optional scope group and a second pattern for merge subjects, or it rejects every merge.
- `make test` executing tests; quiet mode for the test binary (§8.4).
- Milestone tags (`v0.1-milestone1`).
- CI, if the project ever gains a second contributor — see D-030's *Revisit if*.
