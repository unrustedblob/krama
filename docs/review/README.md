# Reviews

How a review of a stage branch is written, filed and followed up. The git side — pushing before a
review, the trailers on fix commits — is in `docs/COMMITS.md`. The reasoning is `D-041`.

A review exists to find what the code gets wrong **and** where the understanding behind it has a
gap. So findings are stated as inputs that fail, not as lines to change: the fix, and the reason it
is a fix, stay with the programmer.

---

## Layout

```
docs/review/<milestone>/<branch>/rNNN.md
```

- **One file per pass.** `rNNN` counts passes on a branch and restarts at `r001` on each branch. A
  pass may cover any part of the branch — build work in one, the test driver in the next.
- **The folder outlives the branch.** After the branch is merged and deleted, its reviews stay here,
  and the `--no-ff` merge commit records the branch's scope.

---

## The header

| Field | Holds |
|---|---|
| **Milestone** | `M1`, … |
| **Branch** | The branch name, verbatim |
| **Iteration** | `rNNN` |
| **Follows** | The pass whose fixes this one re-checks, or `—` for a new area |
| **Date** | When the pass was made |
| **Reviewed** | Files and parts of the branch covered |
| **Against** | The notes, decisions and spec sections the code is held to |

Only pushed commits are reviewed: a finding must point at something that exists on GitHub.

---

## Findings

Each finding is stated at its **escalation step** (CLAUDE.md §5.3):

| Step | The finding gives |
|---|---|
| 1 — inputs | The inputs, what was expected, what happened, and what to watch for. Never the line |
| 2 — category | The class of defect and where to look in general terms, with the reason no input can reach it |
| 3 — invariant | The invariant to re-derive |

- **Start at step 1.** Start at step 2 only where no input can reach the defect, and say why.
- **Choose inputs that can fail.** Asymmetric values, every digit, the boundaries. A palindrome or a
  single digit cannot catch reversed order.
- **Style findings are not defects.** They are stated plainly, with their `STYLE.md` section.
- **Direct naming of the location** is not written into a review file.

Findings are numbered within their group — `1.1`, `1.2`, `2.1` — and the numbers are kept in every
issue and commit that touches them.

---

## Issues

- **One issue per group of findings**, titled `<branch>-rNNN · <group>: <summary>`, for example
  `m1/cli-args-r001 · 2: parse_c_int misreads C integer literals`. The body is the group's section
  of the review file, verbatim, and names the file.
- **Every title is approved before anything is filed.**
- **The review file ends with a *Filed as* table** mapping each group to its issue URL, filled in
  after filing.
- **Not filed** — findings decided in review, resolved by records alone, or deliberately deferred —
  get their own section, so nothing raised disappears.

Fix commits carry `Refs: #N` while an issue is partly fixed and `Closes: #N`, one line per issue, on
the commit that finishes it (`docs/COMMITS.md` *Trailers*).

---

## Follow-up passes

A pass that follows another:

1. **Opens with a status table** of the earlier pass's issues — closed, carried, won't fix.
2. **Reruns every input the earlier pass showed.**
3. **Adds outliers** — fresh inputs from the same defect class, derived when this pass is made,
   never written down in advance. Their job is to fail the fix.
4. **Records the result per finding:**
   - shown inputs and outliers pass — the issue may close;
   - shown inputs pass, an outlier fails — the examples were patched, not the defect. The issue is
     **carried**, the finding climbs one step, and the gap is a candidate for a learning detour;
   - an outlier fails only because the reviewer misread the code — recorded as the reviewer's
     error, not the programmer's.

A finding climbs **one step per pass**, never more.

---

## Closing a review

When every issue a pass filed is closed, a short closing note is appended to its file: what each
group turned out to be, the commits that fixed it, and anything learned. `gh issue view <N>` and
`git log --grep "#N"` supply the material.

---

## History

`m1/cli-args/r001.md` predates these rules. It names locations directly and has no **Follows**
field. It stays as written.
