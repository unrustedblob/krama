# Krama — Workflow 2.0: pushed branches, reviews as issues

**Date:** 2026-10-20 → 2026-11-08
**Status:** Discussion record. **Not normative.** Source note for **DR-OWED-22-01** (supersedes D-030)
and the COMMITS.md changes on `m1/workflow-2`.

> Nothing here is binding. Where this and `DECISIONS.md` / `COMMITS.md` disagree, those win.

Promotes `aside-notes-03-workflow-2` and `aside-notes-06-closes-trailer`, and records the session
that settled their open points. Background: `krama-notes-09` (D-030's origin), `krama-notes-21` §6
(reviews under workflow 2.0).

---

## 1. How to read this

- **§2** — what changed and why D-030 no longer fits.
- **§3** — pushed history is final.
- **§4** — the push gate.
- **§5** — trailers and closing.
- **§6** — reviews.
- **§7** — branches after merging; no PRs.
- **§8** — hooks.
- **§9** — owed decisions, and the convention for them.
- **§10** — what this supersedes, and what it leaves alone.
- **§11** — conclusions.

---

## 2. What changed

GitHub issues are now the tracker for everything a review raises. An issue links to commits, so
**the commits it links to must exist on GitHub** — which a local-only branch cannot promise.

D-030 rejected pushed branches for one reason: a pushed branch makes Rule 4 ("never rewrite pushed
history") ambiguous, and its merge gate depended on `git rebase -i` rewriting the branch freely.
D-030's *Revisit if* (a second contributor, or CI) **has not fired**. The reason here is new: review
issues need something real to point at. The superseding entry says so rather than claiming the
trigger fired.

**Evidence the old model was costing something:** r001's reviewed code was never committed, so the
"Today" column of its findings cannot be reproduced from git (`krama-notes-21` §6.2, PROJECT.md
2026-11-08 Friction 5).

**Side benefit:** unmerged work no longer lives on one machine only, a cost D-030 recorded.

---

## 3. Pushed history is final

Rule 4 now has **one meaning again**: it protects **every pushed ref**, stage branches included.
There is no force-pushing.

- **Why not force-push with `--force-with-lease`?** It keeps the old freedom to rewrite, but every
  rewrite orphans the commits an issue already links to. The links still open (GitHub keeps
  unreachable commits by hash for a while), but they point at commits no branch holds, and the
  issue timeline fills with references to ghosts.
- **Consequence:** `aside-notes-03`'s points 1 (Rule 4's scope) and 2 (force-push policy) are answered
  by the requirement itself, not by experiment. The remote-tracking `/ground-up` lab stays on hold as
  learning; nothing waits on it.
- **The `Refs:` timing question disappears.** Nothing pushed is ever rewritten, so a trailer in a
  pushed commit never produces a ghost reference.

---

## 4. The push gate

Cleanup moves from the merge gate to **before each push**.

- **`wip` commits stay local.** Squash or `reset --soft` them into logical commits freely — *before*
  pushing. After a push, a commit is final.
- **The check:**

  ```
  git rebase -i --exec "make check" @{upstream}
  ```

  `@{upstream}` is the local record of the branch on GitHub (`origin/<branch>`), so the rebase can
  only touch commits not yet pushed. Pushed history cannot be rewritten by accident. On a branch's
  **first** push there is no upstream yet; use `main`.
- **Rule 1** ("every commit on `main` builds and passes tests") is now enforceable only before a push:
  a commit that reaches GitHub broken stays broken.
- **The rebase -i squash at the merge gate is gone.** The merge itself stays `--no-ff` with the dense
  stage summary, so `git log --first-parent` still reads one line per branch.
- **Per-commit check at merge, without rewriting.** `git rebase --exec "make check" main` on a branch
  already sitting on `main`'s tip leaves every hash unchanged. If `main` has moved on, do not rebase;
  the `--no-ff` merge absorbs it.

---

## 5. Trailers and closing

### 5.1 The fixing commit carries the closure

`Closes: #N` goes on the commit that fixes the issue, never only on a merge commit later. Several
issues may close in one commit.

### 5.2 One keyword per issue (from `aside-notes-06`)

The r001 fix commit (`d350b75`) carried `Closes: #16, #17, #18, #19, #20`. GitHub closed **only
#16**; #17–#20 showed the commit as `referenced` and stayed open.

GitHub binds a closing keyword to the single reference after it; the rest of a comma list are plain
mentions. Its documentation ("Linking a pull request to an issue") says, for multiple issues, *"use
full syntax for each issue"* — `Resolves #10, resolves #123`.

**Rule:** one trailer line per issue.

```
Closes: #16
Closes: #17
```

Repeated trailer keys are ordinary Git trailer convention, and each line stays greppable. `Refs:` may
stay a list — it never closes anything, and every listed number is still a mention, which links.

**Verify on first use:** the one-line form is what the docs imply; the next multi-issue fix is the
first observation of it in this repository.

**Where the wrong claim came from is unclear.** Claude Code reported that `docs/COMMITS.md`
describes comma lists; neither copy of COMMITS.md in the project context contains that claim. Check
the working copy before assuming which text is being corrected.

### 5.3 When an issue closes

Closing keywords in a commit act **when that commit lands on the repository's default branch** —
`main` here. On a stage branch, `Closes: #N` only links. The issue closes at the merge, but the
closing event points at the fixing commit, not the merge commit.

**Check after pushing `main`:** each `Closes:` issue shows `closed`, not `referenced`.

---

## 6. Reviews

### 6.1 The record

- **Location:** `docs/review/<milestone>/<branch>/rNNN.md`. If `docs/` becomes the published story
  arc (`aside-notes-04` §7), reviews become public pages; accepted for now, dealt with if they move.
- **`rNNN` counts passes on a branch** and restarts at `r001` on each branch. A pass may cover any
  subject — build work in r001, the test driver in r002.
- **Commit and push before the review**, so the reviewed state exists in git.

### 6.2 Iteration — the `Follows` field

Iteration is a chain of passes, not a second number. Each review's header table gains:

| Field | Meaning |
|---|---|
| **Follows** | `rNNN` when this pass re-reviews that pass's fixes; `—` when it reviews a new area |

A pass that follows another **opens with a status table** of the earlier pass's issues — closed,
carried, won't fix. A fresh pass does not.

### 6.3 Issues

- **Title:** `<branch>-rNNN · <group>: <summary>` — for example `m1/cli-args-r001 · 1: Python version
  check — minimum and comparison`. Later readers can tie an issue back to its branch and pass even
  after the branch is deleted.
- **One issue per group** of findings; finding IDs (`1.1`, `1.2`) stay in the body, as in r001.
- **Labels** — later.

### 6.4 Scope of a deleted branch

The `--no-ff` merge commit is the record: its subject is the branch name, its body the stage summary.
The review folder also outlives the branch. An `index.md` stating scope at the *start* is optional.

---

## 7. After merging; no PRs

- **Delete the remote branch by hand** — `git push origin --delete <branch>` — then
  `git fetch --prune` to clear the stale `origin/…` reference. GitHub's auto-delete setting only
  fires on PR merges.
- **Safe:** after a `--no-ff` merge the branch's commits are reachable from `main`, so every issue
  link still resolves.
- **No pull requests for now.** Solo, small scale; a PR would add a second place to look. Revisit
  with a second contributor.

---

## 8. Hooks

- **`pre-push` running `make check`** — not wanted. A slow hook trains `--no-verify`
  (COMMITS.md *Notes*).
- **`pre-push` rejecting unsquashed commits** — the hook worth having. It is given the refs being
  pushed, so it can read the subjects of exactly the commits leaving the machine:

  ```
  for each ref being pushed:
      if deleting the branch          -> allow
      if the branch is new on GitHub  -> commits = main..local
      else                            -> commits = remote..local
      for each commit in commits:
          if its subject starts with "wip", "fixup!" or "squash!" -> refuse, naming the commit
  allow
  ```

  `fixup!` and `squash!` are what `git commit --fixup` / `--squash` write, so it also catches a
  forgotten fixup. Milliseconds per push. **The programmer writes it** (CLAUDE.md §2); when, is open.

---

## 9. Owed decisions, and the convention for them

**Convention (from this session):** a decision reached in discussion is **not numbered** until it is
written into `DECISIONS.md`. Until then it is **`DR-OWED-<source>-NN`**: `<source>` is the note it
came from — the number for a `krama-notes` entry (`22`), `A` plus the number for an aside (`A01`) —
and `NN` counts within that note. The source makes every label unique without a central registry.
IDs are assigned in the order entries land, not the order they were reached.

Why: PROJECT.md had already promised D-041/D-042 to `m1/ground-up`, and the earlier asides avoided
numbers for the same reason — whichever branch merges first takes the next ID, so pre-assigned
numbers go stale.

| Owed | Decision | Lands on |
|---|---|---|
| **DR-OWED-22-01** → **D-041** | Stage branches are pushed; pushed history is final; reviews are tracked as issues. Supersedes D-030 | `m1/workflow-2` |

**Owed elsewhere**, relabelled under this convention on 2026-11-08:

| Owed | Decision | Source | Lands on |
|---|---|---|---|
| DR-OWED-A01-01 | `/ground-up`: assessed learning detours (once drafted as D-039) | `aside-notes-01` | `m1/ground-up` |
| DR-OWED-A01-02 | `CLAUDE.md` is tracked in the repository (once drafted as D-040) | `aside-notes-01` | `m1/ground-up` |
| DR-OWED-A02-01 … -06 | The M1 descope set | `aside-notes-02` §8 | Spec v2 |

D-039 and D-040 were taken by the harness decisions on `m1/cli-args`, which is why the `/ground-up`
pair went stale and why this convention exists.

Not owed: the one-`Closes:`-per-issue rule (§5.2). It corrects a statement about GitHub's behaviour;
there was no alternative to choose between.

The convention itself is recorded as one line in `DECISIONS.md` *Notes on use*.

---

## 10. What this supersedes, and what it leaves

**Superseded:** D-030 — local-only branches, rewrite before merging, the squash at the merge gate.

**Kept from D-030:** short-lived `m<N>/<stage>` branches cut from a fresh `main`; `--no-ff` merges with
`<branch>: <subject>` and a dense summary; `git branch -d` as the merged check; `git pull --ff-only`.

**Unaffected:** D-031 (type mandatory, optional closed-vocabulary scope, issue numbers in trailers
only) — §5.2 corrects COMMITS.md's wording, not the decision.

**PROJECT.md:** the 2026-09-18 **Didn't** line ("pushed branches and pull requests — rejected")
stays as written and is answered by the new log entry.

**Branch:** `m1/workflow-2`, the first run under the new rules — cut from `main` after `m1/cli-args`
merges (the last merge under D-030), and pushed.

---

## 11. Conclusions

1. **Branches are pushed. Pushed history is final** — Rule 4 covers every pushed ref; no
   force-pushing.
2. **Push only logical commits.** The push gate is `rebase -i --exec "make check" @{upstream}`
   (`main` on a first push). `wip` stays local.
3. **The merge-gate squash goes;** the `--no-ff` merge with a stage summary stays.
4. **The fixing commit carries `Closes:`, one line per issue.** Issues close when it reaches the
   default branch.
5. **Commit and push before review.** Reviews live in `docs/review/<milestone>/<branch>/rNNN.md`;
   `rNNN` restarts per branch; `Follows` chains iterations.
6. **Issue titles:** `<branch>-rNNN · <group>: <summary>`.
7. **No PRs.** Delete the remote branch after merging; `git fetch --prune`.
8. **A `pre-push` hook rejecting `wip` / `fixup!` / `squash!`** is the hook worth writing; not
   `make check`.
9. **Decisions are `DR-OWED-<source>-NN` until written;** IDs assigned on landing.
