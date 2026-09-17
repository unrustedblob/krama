# funC — Git branch workflow and Makefile targets

**Date:** 2026-09-17
**Status:** Discussion record. **Not normative.** Input to `D-030` and the matching `COMMITS.md`
and `Makefile` changes.

> Nothing here is binding until it lands in `DECISIONS.md` / `COMMITS.md`. Where this and those
> disagree, those win.

---

## 1. How to read this

- **§2** — the starting idea and what prompted it.
- **§3** — what chibicc actually does (checked, not assumed).
- **§4** — the long-lived-branch trap, and reachability.
- **§5** — the three merge styles and the choice.
- **§6** — merge commit subject format.
- **§7** — Rule 1 and `git rebase --exec`.
- **§8** — Makefile targets.
- **§9** — settled, deferred, owed.

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

## 8. Makefile targets

### 8.1 Current state (project copy)

- `test` builds `$(TEST_TARGET)` but does not run it.
- No `fmt` target, though `STYLE.md` §1 references `make fmt`.
- `.PHONY` was missing `test` and `clean_test` — **fixed**: `.PHONY: all clean test clean_test`.
  A stray file named `test` would otherwise silently stop the target running.

### 8.2 Planned

**`check`** — named for *what* it verifies, not *who* calls it (rejected: `test_exec`). The same
command serves `rebase --exec`, manual pre-merge runs, and the deferred `pre-push` hook.

```
check:
    depends on: test binary built
    run test binary         → stop if non-zero exit
    format check, no edits  → stop if any file would change
```

**`fmt` is two jobs:**

- rewrite files in place — for manual use;
- report only (clang-format `--dry-run --Werror`) — the one `check` uses.

`--exec` must never rewrite files mid-rebase. Editor format-on-save is not sufficient on its own: it
misses files changed during a rebase, a conflict resolution, or outside that editor.

**`make test` running tests:** planned, once the `.func` / `.exec` pairs exist. Until then `check`
runs the unit-test binary directly.

When `check` exists, add it to `.PHONY`.

---

## 9. Settled / deferred / owed

### Settled

- Short-lived branch per stage, from an up-to-date `main`, deleted after merge.
- Option 2: `rebase -i` on branch → `--no-ff` merge with dense summary message.
- Merge subject: `<branch-name>: <subject>`.
- `rebase --exec` enforces Rule 1 per logical commit.
- Existing history untouched.

### Deferred

- **Hooks** (`commit-msg`, `pre-commit`, `pre-push`) — limited hobby time. When revisited, the
  `commit-msg` regex needs a second pattern for merge subjects, or it rejects every merge.
- `make test` executing tests — waits for `.func` / `.exec` pairs.

### Owed

- **`D-030`** — option 2, merge subject format, `--exec` as the Rule 1 check.
- **`COMMITS.md`** — Rule 3 wording, merge-subject format, hook-regex note.
- **`Makefile`** — `check`, `fmt` (rewrite), report-only format target, `.PHONY` update.
- **Branch naming rule** — so merge labels are consistent.
- **Open (raised during walkthrough):** does Rule 4 ("never rewrite pushed history") cover pushed
  *feature branches*, or only `main`? Decides whether a branch may be pushed before `rebase -i`.
