# Krama — Workflow 2.0: pushing stage branches
 
**Date:** 2026-10-20 / 2026-10-21
**Status:** Discussion record. **Not normative.** Held as an aside until harness part 1
(`m1/cli-args`) lands; then upgraded to a `krama-notes` entry. Nothing here is decided yet: every
point in §5 is open. Decision IDs are assigned only when recorded.
 
> Where this and `COMMITS.md` / `DECISIONS.md` disagree, those win until the decisions here are
> recorded.
 
---
 
## 1. How to read this
 
- **§2** — what is being proposed, and why it collides with D-030.
- **§3** — point 1, Rule 4's scope: the question, the answer so far, two corrections.
- **§4** — the `/ground-up` it surfaced, accepted and then put on hold.
- **§5** — the open points, in order.
- **§6** — records this will touch.
---
 
## 2. The proposal
 
Push stage branches (`m<N>/<stage>`) to GitHub, mainly so work in progress is visible against the
issues already being tracked there.
 
**Collision with D-030.** D-030 rejected pushed branches for one reason: a pushed branch makes
COMMITS.md Rule 4 ("never rewrite pushed history") ambiguous, and the merge gate depends on
`git rebase -i` rewriting the branch freely.
 
- D-030's *Revisit if* (a second contributor, or CI) **has not fired**. The reason here — issue
  visibility — is a new one. Legitimate, but the superseding entry should say so rather than claim
  the trigger fired.
- **Side benefit:** unmerged work stops living on one machine only, which D-030 recorded as a cost.
---
 
## 3. Point 1 — what does Rule 4 protect?
 
Today only `main` is pushed, so "pushed" and "`main`" mean the same thing. Pushing stage branches
splits them, and Rule 4 has to pick one: `main` only, or every pushed ref.
 
### 3.1 The question asked
 
*When a commit already on GitHub is rewritten, who or what could be holding the old version, and
what goes wrong for them?*
 
### 3.2 The answer given
 
Two holders, both correct:
 
1. **GitHub issues.** A commit carrying `Refs: #N` posts a reference event on the issue. After a
   rebase and re-push the issue shows events pointing at commits no branch holds.
2. **A second machine.** If machine A rebases and pushes, machine B's copy of the branch — and any
   work done on it — is built on the old version.
### 3.3 Corrections and additions
 
- **Rebasing abandons nothing.** The old commits still exist; the branch now points at new commits
  with the same changes and different hashes, so the old ones become unreachable from any branch —
  *dangling*. GitHub keeps them reachable by hash for a while, which is why the issue links still
  open rather than 404.
- **The issue timeline is cosmetic.** Nothing breaks; it fills with references to ghosts.
- **The second machine is where the real harm is**, and the answer stopped short of it: what Git
  *does* when machine B next pulls or pushes. Depending on the command, Git refuses, or merges old
  and new versions into a branch holding every change twice, or lets B overwrite A's rewrite. Which
  of those happen, and which Git prevents, is what Rule 4 is guarding — and what the lab (§4) is
  built to show.
---
 
## 4. `/ground-up` — remote-tracking branches and non-fast-forward pushes
 
| Concept | Score | Evidence | Status |
|---|---|---|---|
| Remote-tracking branches and non-fast-forward pushes | 5 | Described the two-machine rewrite unprompted and correctly (§3.2). Did not say what Git does about it, or what `origin/<branch>` is on each machine | Accepted 2026-10-21, **on hold** 2026-10-23 |
 
**Scope:** full — a predict-then-run lab in a throwaway sandbox (a bare repository playing GitHub,
two clones playing two machines), no code to write. Covers points 1 and 2 together, since
`--force-with-lease` is defined in terms of remote-tracking branches.
 
**Packet:** `learning/remote-tracking/packet.md` — brief, guide set, reading (Pro Git §3.5; the
local `git help push`), six stages, three questions to bring back. Every stage was rehearsed in a
sandbox on Git 2.43 before the packet was written; the results are deliberately kept out of the
packet so the predictions stay honest. The programmer's log goes in
`learning/remote-tracking/lab-log.md`.
 
**On hold, by the programmer's call.** Nothing was changed for it: no ledger row written.
 
**Unresolved before it opens:** CLAUDE.md §8.7 allows one active detour, and the ledger in context
shows `portability-layers` as `open` (score 4, opened 2026-10-10). Options raised: pause
`portability-layers` (and name it in `PROJECT.md` **Current**), queue this lab, or confirm the ledger
in context is stale.
 
**Also owed:** the ledger row and the **Didn't** line for the declined incremental-compilation
detour (`aside-notes-02-m1-descope` §11.1).
 
---
 
## 5. Open points, in order
 
Taken one at a time. Point 1 waits on the lab.
 
1. **Rule 4's scope** — `main` only, or every pushed ref? The common resolution is `main` only, with
   stage branches free to be force-pushed; to be decided from the lab's three closing questions.
2. **Force-push policy.** `--force-with-lease` refuses to push if GitHub's branch has moved since
   this machine last looked at it (e.g. a push from another machine). When that check stops
   protecting you is guide question 4 of the lab.
3. **When `Refs:` trailers go in.** Every push of a commit carrying `Refs: #N` posts an event on the
   issue, so each rewrite-and-push leaves a set of ghost references (§3.3). Options raised: accept
   the noise, or add trailers only during the cleanup rebase at the merge gate.
4. **PRs, and how to merge.** If PRs: merge locally with `--no-ff` and push `main`. GitHub still
   marks the PR merged, and the `<branch>: <subject>` merge message survives. GitHub's merge button
   defaults the subject to "Merge pull request #N".
5. **Remote branch deletion** — GitHub's auto-delete setting or by hand — and `git fetch --prune` to
   clear stale `origin/…` references locally.
6. **The record.** A new entry superseding D-030, not an amendment of it.
---
 
## 6. Records this will touch
 
- **DECISIONS.md** — a new entry superseding D-030; D-030's status becomes `Superseded by`.
- **COMMITS.md** — *Branches → Lifecycle* ("Local only. Branches are never pushed."), Rule 4, the
  *Trailers* note on local-only branches, and appendix step 7 (push the branch; delete it remotely
  after merging).
- **PROJECT.md** — a log entry; the **Didn't** line from the 2026-09-18 entry ("Pushed branches and
  pull requests — rejected") stays as written and is answered by a later entry, never edited.
- **learning/LEDGER.md** — the remote-tracking row when the detour opens; the declined row owed from
  `aside-notes-02`.
