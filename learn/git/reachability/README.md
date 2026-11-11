# Branches as pointers, and reachability — packet

**Ledger:** `git` / `reachability` · score at queue **3** · queued 2026-11-11
**Branch:** `learn/git/reachability` (cut when the packet opens, not before)
**Surfaced by:** `tool/commit-hook`, step 6 — finding the commits a `pre-push` hook must check
**Scope:** reading + lab — hands-on in a throwaway sandbox, no code to write
**Evidence at queue:** followed `A..B` as a subtraction of two reachable sets, and confirmed by running
it that forty zeros cannot be a range endpoint. Then modelled a branch as a *container* of the
commits made on it: with `tool` cut from a local `main` two commits ahead of the remote, held that
`H1` "is not part of the tool branch", and so counted two outgoing commits instead of three. Once
shown the parent walk, counted three correctly. Did not know where the machine records what the
remote's branches point at.

**Relation to other packets.** This sits underneath `git/remote-tracking` (paused): a remote-tracking
branch is a pointer like any other, and that packet's stage 4 already leans on reachability. Doing
this one first makes that one cheaper. Both are subject to the one-active-packet limit
(CLAUDE.md §8.7).

---

## Brief

The `pre-push` hook has to answer one question for every push: *which commits are leaving this
machine?* The first answer tried — `main..local` for a new branch — silently missed commits
whenever local `main` was ahead of the remote's. The rule that works is "everything reachable from
what is being pushed, minus everything reachable from what the remote already has". That rule only
makes sense once a branch is understood as a label on one commit, and a branch's history as
whatever the parent links lead back to. This packet builds that model, and checks it against Git
itself rather than against a diagram.

## Guide set

The aim, phrased as questions. Answer them in your own words in `lab-log.md` as the lab answers them.

1. What is a branch, physically, on disk — and what does a commit record about branches?
2. What does "reachable from X" mean, and why does it follow *every* parent link back, not one step?
3. What does `A..B` select, and what job is `A` doing in it?
4. When you push one branch, which commits travel — and how does your machine know which ones the
   remote already has?

## Reading

Read in this order. Each is free and was checked reachable on 2026-11-11.

- **Pro Git, §3.1 "Branches in a Nutshell"** —
  <https://git-scm.com/book/en/v2/Git-Branching-Branches-in-a-Nutshell>.
  The canonical statement of the pointer model: commits point at parents, a branch is a movable
  label on one commit. Read it before stage 1 — it is the vocabulary the lab uses.
- **Think Like (a) Git** — three short pages, in order:
  [Reachability](https://think-like-a-git.net/sections/graph-theory/reachability.html),
  [Graphs and Git](https://think-like-a-git.net/sections/graphs-and-git.html),
  [References Make Commits Reachable](https://think-like-a-git.net/sections/experimenting-with-git/references-make-commits-reachable.html).
  Why it is good: it treats the repository as a graph first and as Git second, which is exactly the
  shift this packet is about. Read it before stage 2.
- **Pro Git, §10.3 "Git References"** —
  <https://git-scm.com/book/en/v2/Git-Internals-Git-References>.
  Shows a branch as a file holding a SHA, made and moved by hand. Its last part, on
  `refs/remotes/`, is the bridge to the `remote-tracking` packet — read it, but do not chase it
  further here. Read before stage 1.
- **`git help revisions`**, on your own machine — the *Specifying Ranges* section only. Your local
  copy matches your installed Git. It states the `..` notation as an include/exclude of reachable
  sets, and its `A..B C..D` example is a good test of whether the model has landed. Read before
  stage 3.

---

## Lab

### Rules for the lab

- **Predict, then run.** Before each step marked **▶ predict**, write your prediction in
  `lab-log.md`, then run it and write what actually happened. A wrong prediction is the most useful
  line in the log.
- **Run the sandbox outside the Krama repository.** Only `lab-log.md` lives in
  `learn/git/reachability/`.
- One command to look at every stage: `git log --oneline --graph --all`.
- Record `git --version` at the top of the log.
- Tag each commit's message with its name from the chat's diagram (`R`, `H1`, `H2`, `T`, `L`) so
  the graph reads the same as the picture.

### Stage 0 — the picture from the chat

In a new directory `~/reach-lab`, create a bare repository `remote.git` to play GitHub, and a
working repository `work` that clones it.

In `work`, on `main`: make one commit `R` and push `main`. Then make `H1` and `H2` on `main` and
**do not push**. Cut `tool` from `main` and commit `T` on it.

`git log --oneline --graph --all`. Draw it in the log next to the chat's diagram. They should match.

### Stage 1 — a branch is a file

**▶ predict** what is inside the file `.git/refs/heads/tool`: how many lines, and what on them.
Then look. (If the file is missing, Git has packed it into `.git/packed-refs` — look there.)

Compare it with `git rev-parse tool`.

> Log question: if a branch is that file, what does *creating* a branch cost? What does *deleting*
> one delete? (Guide question 1.)

### Stage 2 — commits do not know their branch

`git cat-file -p` on `T`. **▶ predict** first: will the word `tool` appear anywhere in it? Will
`main`? What will?

Then `git cat-file -p` on `H2`, and on `R`.

> Log question: which field links the commits together, and in which direction does it point —
> forwards to children, or backwards to parents?

`git log --oneline tool`. **▶ predict** whether `H1` appears.

`git branch --contains` with `H1`'s SHA. **▶ predict** which branches it names.

> Log question: the chat's step 5 said `H1` "won't be reached from the tool branch". Using the
> field you found above, explain in two sentences why it is. (Guide question 2.)

### Stage 3 — ranges are subtraction

For each, **▶ predict** the exact list of commits before running `git log --oneline <range>`:

1. `main..tool`
2. `tool..main`
3. `R..tool` (use `R`'s SHA)
4. `origin/main..tool`

Now cut `learn` from `H2` and commit `L` on it. **▶ predict**, then run:

5. `main..learn`
6. `tool..learn`

> Log question: in range 1, which commits did `main` remove, and why does that make it the wrong
> left side for a push of `tool`? (Guide question 3.)

### Stage 4 — move a label, lose a commit

Move `tool` back to `H2` without making a commit — find the command in §10.3 or `git help branch`.

`git log --oneline --graph --all`. **▶ predict** first: is `T` in the picture?

Then find `T` again with `git reflog`, and put `tool` back on it.

> Log question: `T` was never deleted. In what sense was it gone? What would eventually happen to
> it if nothing pointed at it — and what does that tell you about what deleting a branch does?

### Stage 5 — what the remote already has

`git branch -r`. **▶ predict** what it lists and where each entry points, before running it.

Push `tool`. **▶ predict** first: which commits travel? Check afterwards with `git branch -r` and
the graph.

Before pushing `learn`, **▶ predict** which commits it will send. Then read the `--not` and
`--remotes` options in `git help rev-list` and find a `rev-list` invocation that lists exactly those
commits without pushing. Run it, then push `learn` and compare.

> Log question: what did your machine consult to know what `remote.git` already had? Was that a
> live answer from the remote, or a record? (Guide question 4.)

---

## Bring back to the chat

Answer these in the log, then post them.

1. In your own words, why did `main..local` miss commits in the chat's scenario, and in which
   situations would it have been right by luck?
2. A new branch on the remote, and an existing one: why does one rule — subtract the remote's
   tracking refs — cover both?
3. Stage 4: a commit nothing points at. Name one Git command that can make that happen to work you
   care about, and how you would get the work back.

The check set is held back until the log is reviewed (CLAUDE.md §8.5).
