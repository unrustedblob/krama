# Remote-tracking branches and non-fast-forward pushes — packet

**Ledger:** `git` / `remote-tracking` · score at open **5** · opened 2026-10-21
**Branch:** `learn/git/remote-tracking`
**Surfaced by:** workflow 2.0, Rule 4 scope (`aside-notes-02-m1-descope` §10)
**Scope:** reading + lab — hands-on in a throwaway sandbox, no code to write
**Evidence at open:** described the two-machine rewrite unprompted and correctly (issue references
pointing at orphaned commits; the second machine holding the old version). Did not say what Git does
about it, or what `origin/<branch>` is on each machine.

---

## Brief

Today only `main` is pushed, so Rule 4 ("never rewrite pushed history") has one meaning. Pushing
stage branches gives it two. Before choosing which refs Rule 4 protects, see for yourself what a
rewrite does to the other copies of a branch. The lab simulates GitHub and two machines on one
computer, so nothing real is at risk.

> **Since this packet was written,** D-041 settled Rule 4's scope: every pushed ref is final. The lab
> no longer decides anything. It still answers what the decision rests on.

## Guide set

The aim, phrased as questions. Answer them in your own words in `lab-log.md` as the lab answers them.

1. When `git status` says your branch is "up to date", up to date with **what**?
2. What must be true about a push for GitHub to accept it without `--force`?
3. After a rewrite, a second machine runs `git pull`. What could Git do, and which outcomes damage
   history?
4. What does `--force-with-lease` compare, and when does that comparison stop protecting you?

## Reading

- **Pro Git, §3.5 "Remote Branches"** — <https://git-scm.com/book/en/v2> (chapter 3, section 5).
  The canonical explanation of `origin/<branch>` as a bookmark of where the remote was the last time
  you talked to it. Short, free, and the vocabulary the lab uses. Read it before stage 1.
- **`git help push`**, on your own machine — the `--force-with-lease` section. Your local copy
  matches your installed Git, which matters because `push` and `pull` defaults have changed across
  versions. Read it between stages 4 and 5.

---

## Lab

### Rules for the lab

- **Predict, then run.** Before each command marked **▶ predict**, write your prediction in
  `lab-log.md`, then run it and write what actually happened. A wrong prediction is the most useful
  line in the log.
- **Run the sandbox outside the Krama repository** (a Git repository inside another one confuses
  both). Only `lab-log.md` lives in `learn/git/remote-tracking/`.
- Two commands you will use to look, at every stage:
  - `git status -sb` — the first line compares your branch with your record of the remote.
  - `git log --oneline --graph --all` — every branch, including `origin/…`, as a picture.
- Record `git --version` at the top of the log. `git pull`'s default behaviour depends on it.

### Stage 0 — set up GitHub and two machines

```
mkdir ~/remote-lab && cd ~/remote-lab
git init --bare --initial-branch=main github.git     # plays GitHub
git clone github.git machine-a                        # plays your first machine
```

In `machine-a`: create a file, commit it on `main`, `git push origin main`. Then
`git switch -c m1/demo`, make **two** commits, and `git push -u origin m1/demo`.

Back in `~/remote-lab`: `git clone github.git machine-b`, then in `machine-b`:
`git switch m1/demo`.

`git branch -vv` in each machine. Note what the brackets say.

### Stage 1 — three copies of one branch

In `machine-a`, make a third commit. **Do not push.** Then print three hashes:

```
git rev-parse --short m1/demo            # your branch
git rev-parse --short origin/m1/demo     # your record of GitHub's branch
git ls-remote origin m1/demo             # GitHub's branch, asked right now
```

**▶ predict** which of the three moved when you committed. Run, record.

Now `git push`, and print the three again. **▶ predict** first.

Go to `machine-b`. Print the same three. **▶ predict** what `git status -sb` will say. Run it.

Then `git fetch` and run `git status -sb` again.

> Log question: `machine-b` was behind GitHub before the fetch. What did `git status` say, and why
> was it not lying? (Guide question 1.)

### Stage 2 — fast-forward

Still in `machine-b`: `git merge --ff-only`. Draw (in the log, as text) where the `m1/demo` label
was before and after.

> Log question: what did the merge actually change? Was any commit created?

### Stage 3 — the rewrite

In `machine-a`: switch to `main`, make one commit, push it. Switch back to `m1/demo` and
`git rebase main`.

Run `git status -sb`. **▶ predict** what "ahead" and "behind" will say, and why both are non-zero.

Now plain `git push`. **▶ predict** first. Record the message.

> Log question: in the graph, is `origin/m1/demo` an ancestor of `m1/demo`? What does that have to
> do with the refusal? (Guide question 2.)

Then `git push --force-with-lease`. Record the line Git prints, including the `+` and "forced
update".

### Stage 4 — the other machine

In `machine-b` (which has not fetched since stage 2): make one commit on `m1/demo`. Then
`git fetch` and `git status -sb`.

**▶ predict** the ahead/behind numbers before reading them. Explain them in the log — commit by
commit.

Now `git pull` with no flags. **▶ predict**. Record what happens. (It depends on your version.)

Then `git pull --no-rebase` and look at `git log --oneline --graph`.

> Log question: count how many times each commit message appears. What happened, and what would
> it look like merged into `main`? (Guide question 3.)

Undo it: `git reset --hard ORIG_HEAD`. Confirm with the graph that the merge commit is gone from
the branch.

> Log question: where is the merge commit now? (Recall reachability, notes-09 §10.)

### Stage 5 — the lease

Read the `--force-with-lease` section of `git help push` now.

In `machine-a`: `git commit --amend` with a new message, then `git push --force-with-lease`.

In `machine-b` — **do not fetch**. Still holding your stage-4 commit, run
`git push --force-with-lease`. **▶ predict**. Record the exact rejection reason.

Now `git fetch`, then `git push --force-with-lease` again. **▶ predict**.

Go to `machine-a`, `git fetch`, and look at `git log --oneline origin/m1/demo`.

> Log question: what did the lease compare against, in each of the two attempts? Why did the
> fetch change the answer? (Guide question 4.)

### Stage 6 — recover

Put `machine-a`'s reworded history back on GitHub.

> Log questions: which machine can do it, and why that one? Which flag, and what does the lease
> check this time?

---

## Bring back to the chat

Answer these in the log, then post them.

1. In stages 4 and 5, which damage did Git **refuse**, and which did it **allow**?
2. Of the things that went wrong, which needed **two machines** to happen?
3. Given 1 and 2: what discipline would make rewriting a pushed stage branch safe for one person
   — and what about `main` makes the same discipline insufficient there? Compare your answer with
   what D-041 chose.

The check set is held back until the log is reviewed (CLAUDE.md §8.5).
