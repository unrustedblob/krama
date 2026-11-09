# Krama — Commit Conventions

Covers commit messages, branches, pushing and merging, and the git side of reviews. The reasoning behind the branch,
push and merge workflow is `D-041` (superseding `D-030`); the reasoning behind optional scopes and
issue trailers is `D-031`; the `learn` type and learning branches come from `D-042`.

---

## Format

```
<type>[(<scope>)]: <subject>

<body>

<trailers>
```

### Subject line

```
feat(lex): add span tracking to token stream
build: add sanitizer target to Makefile
```

- **Imperative mood.** "add", not "added" or "adds". Reads as an instruction to the codebase.
- **Under 50 characters.** Hard ceiling 72; `git log --oneline` truncates. The budget covers the
  whole line, prefix included.
- **Lowercase after the colon.** No trailing period.
- **`type` is mandatory.** No bare subjects.
- **`scope` is used when it adds information.** Include it when the change sits within a pipeline
  phase (see *Scopes*). Omit it when the type already says where — `build: ...`, not
  `build(build): ...`.
- **No issue numbers in the subject.** They go in trailers (see *Trailers*). A scope names a phase;
  `fix(#2)` gives it a second meaning and loses the phase.
- **Merge commits** use a different subject format — see *Merging*.

### Body

- **Blank line between subject and body.** Git treats the first paragraph as the subject; without
  the blank line the whole message becomes one line.
- **Wrap at 72 characters.** Git indents when displaying, so wider lines wrap badly in terminals.
- **Explain *why*, not *what*.** The diff already shows what changed. The body carries the reasoning
  that is not recoverable from the code.
- **Omit for obvious changes.** A typo fix needs no body. Write one whenever a reader could
  reasonably ask "why this way?"

Highest-value case: code that looks wrong but is not.

```
fix(rt): avoid forming |b| in Euclidean adjustment

Computing abs(b) overflows when b == INT32_MIN, which is UB and
would defeat the point of the trap. The adjustment now branches on
the sign of b and adds or subtracts b directly, never producing an
absolute value.

Refs: D-014
```

---

## Types

| Type | Use for |
|---|---|
| `feat` | New capability in the language or the transpiler |
| `fix` | Corrects wrong behavior |
| `refactor` | Restructures without changing behavior |
| `test` | Adds or changes tests only |
| `docs` | Spec, README, PROJECT.md, DECISIONS.md, CLAUDE.md, comments |
| `build` | Makefile, compiler flags, hooks, tooling |
| `perf` | Measured performance change — not speculative |
| `chore` | Housekeeping with no source effect (.gitignore, file moves) |
| `learn` | Work under `learn/` — packets, exercises, summaries, ledger rows |

`refactor` means behavior is unchanged and tests are untouched. If tests changed, it is `feat` or
`fix`.

---

## Scopes — pipeline phases

Optional (see *Subject line*), but when used, drawn **only** from this table. A closed vocabulary is
what makes `git log --grep="(lex"` find every lexer commit; a new word needs a new row, not an
ad-hoc spelling (`feat(token)` is `feat(lex)`).

| Scope | Covers |
|---|---|
| `lex` | Scanner, tokens, spans |
| `parse` | Parser, grammar |
| `ast` | Node definitions, tree construction, walking |
| `check` | Type checker, milestone restrictions, semantic diagnostics |
| `codegen` | C emission, `#line` directives, format synthesis |
| `interp` | Tree-walking interpreter (`--interpret`) |
| `rt` | Emitted runtime — traps, division helpers |
| `arena` | Allocator |
| `cli` | Entry point, argument parsing, exit codes, stage flags |
| `diag` | Diagnostic formatting and reporting machinery |
| `test` | Test corpus, driver, property tests |
| `spec` | Specification and decision documents |

Two scopes are allowed when a change genuinely spans phases: `feat(lex,parse): ...`. Three means the
commit should be split.

No `build` scope: the `build` type already says where. `test(test)` is redundant for the same
reason — write `test: ...` for a change to the test driver itself, and `test(rt): ...` for tests of
another phase.

`learn` never takes a scope, for the same reason as `build`: the type already says where. Topics are
folders, not scopes — the branch name already carries the topic and packet. A change to CLAUDE.md
§8 or D-042 is `docs`, not `learn`; only material inside `learn/` is.

---

## Trailers

```
Refs: D-014
Refs: D-003, D-011
Refs: #7
Closes: #12
Closes: #13
```

| Trailer | Meaning | Effect on GitHub |
|---|---|---|
| `Refs: D-###` | Implementation choice traces back to a `DECISIONS.md` entry | none |
| `Refs: #N` | Commit is *part of* the work on an issue | links the commit to the issue |
| `Closes: #N` | Commit *finishes* the issue | links, and closes the issue when the commit reaches the default branch (`main`) on GitHub |

- **`Refs: D-###`** makes `git log --grep="D-014"` return every commit touching that decision.
- **`Closes:` goes on the logical commit that does the fixing**, not only on the merge commit, so
  the closure points at the actual change. Repeating the issue in the merge body is harmless.
- GitHub accepts the colon and any capitalisation (`Closes: #10`). Only its closing keywords close
  an issue — `close(s|d)`, `fix(es|ed)`, `resolve(s|d)`. `Refs:` never closes anything.
- **One closing keyword per issue.** GitHub binds a keyword to the single reference after it;
  `Closes: #16, #17` closes #16 and only mentions #17. Write one `Closes:` line per issue. Several in
  one commit is fine. `Refs:` may stay a list — it never closes anything.
- **Issues close when the fixing commit reaches `main`**, not when it is pushed on a stage branch.
  The closing event still points at the fixing commit. After pushing `main`, check each one shows
  `closed`, not `referenced`.

---

## Branches

### Naming

```
m<N>/<stage>
```

- `<N>` — milestone number. `<stage>` — the component or goal being worked on.
- **Lowercase, hyphen-separated words:** `m1/ast-nodes`, `m2/structs`.
- **At most two components**, comma-separated, mirroring scopes: `m2/lex,parse`. Three means the
  branch should be split — a branch covering too much is the same problem as a commit covering too
  much, one level up.
- The branch name is used **verbatim** as the merge commit label (see *Merging*), so the spelling
  chosen at creation is the spelling in `main`'s history.

### Lifecycle

- **Short-lived: one branch per stage.** Created from an up-to-date `main`, merged, deleted. The next
  stage branches from the updated `main`. No long-lived `dev` or `milestone-N` branch. Learning
  branches are the exception — see *Learning branches*.
- **Pushed. Pushed history is final.** Branches are pushed so that review issues link to commits that
  exist. Once pushed, a commit is never rewritten (Rule 4). Clean up before pushing — see *Pushing*.
- **Branch from a fresh `main`.** Pull first. A stale local `main` reintroduces the long-lived-branch
  problem `D-030` was written to avoid. To confirm, `git merge-base main <branch>` on a new branch
  should print the same hash as `git rev-parse main`.
- **Delete after merging**, locally with `git branch -d` and remotely with
  `git push origin --delete <branch>`, then `git fetch --prune`. After a `--no-ff` merge the branch
  tip is reachable from `main`, so `-d` succeeds and issue links still resolve. If `-d` refuses, the
  branch is *not* merged — investigate rather than reaching for `-D`.

### Learning branches

```
learn/<topic>/<packet>
```

- **One branch per packet**, named after its folder: `learn/git/remote-tracking` holds
  `learn/git/remote-tracking/`. The `/ground-up` track belongs to no milestone, so the name carries
  none (D-042).
- **The one exception to short-lived branches.** A packet may stay open across any number of stage
  branches, and a paused packet's branch stays pushed. It touches only `learn/`, so it never needs
  `main` merged into it.
- **Cut from an up-to-date `main`**, like any branch. Pushing, the push gate and Rule 4 apply as
  written.
- **Merged with `--no-ff`** when the packet reaches a final status — `cleared` or
  `closed-by-evidence` — with the subject `learn/<topic>/<packet>: <subject>`. A packet the
  programmer abandons is merged too, with its ledger row saying so, because the record is never
  erased (CLAUDE.md §8.3).
- **Never name a branch `learn` or `learn/<topic>`.** Git stores refs as paths, so a branch
  `learn/git` cannot coexist with `learn/git/remote-tracking`.
- **Stage work never goes on a learning branch**, and a packet never goes on a stage branch.

---

## Pushing

Rewrite freely until a commit is pushed; never after.

- **`wip` commits are never pushed.** Squash or `reset --soft` them into logical commits first.
- **The push gate.** Before each push:

  ```
  git rebase -i --exec "make check" @{upstream}
  ```

  `@{upstream}` is the branch as last seen on GitHub, so only unpushed commits are touched. On a
  branch's first push there is no upstream: use `main`, then `git push -u origin <branch>`.
- **Push before review.** A review describes commits that exist on GitHub (see *Reviews*).

---

## Merging

### The merge gate

A branch merges into `main` only when all of these hold, in this order:

1. **Every commit checked.** Each was checked at the push gate. If `main` has not moved since the
   branch was cut, `git rebase --exec "make check" main` re-checks them all without changing a hash.
   If `main` has moved, do not rebase — the `--no-ff` merge absorbs it.
2. **Reviewed.** Every review's issues are closed by commits on the branch, or explicitly carried.
   `git diff main...<branch>` (three dots — everything the branch changed since it left `main`) is
   the whole of what is merging.
3. **The stage's goal is complete.** A half-built stage can pass every check.
4. **Records updated.** `PROJECT.md` log entry; `DECISIONS.md` if anything was decided.

### Merge commit

- **`git merge --no-ff <branch>`** on an up-to-date `main`. Never squash-merge; never fast-forward.
- **Update `main` without rebasing.** Use `git pull --ff-only`. A rebasing pull on a `main` that
  holds an unpushed merge commit flattens the merge and destroys the stage structure.
- **Subject: `<branch-name>: <subject>`**, replacing Git's default `Merge branch '...'` every time.
  The label is the exact branch string; the subject follows the ordinary subject-line rules.
- **Body: the dense stage summary.** What the stage delivered and why, across all its commits. This
  is where `main`'s story is told.
- **Trailers:** `Refs:` for every decision the stage touched. Issues are closed by the fixing
  commits, not here; mentioning them in the body is harmless.

```
m1/arena: add bump allocator over chained blocks

Arena backs AST allocation for the lifetime of a compilation. Each
block keeps its header and buffer as separate allocations. Padding
is computed from the cursor, never rounded up, and fit tests are
subtractive so no size arithmetic can wrap. Zero-size requests abort:
they can only come from a transpiler bug.

Refs: D-003, D-027, D-028, D-029
```

### Reading the history

| Command | Shows |
|---|---|
| `git log --first-parent --oneline main` | One line per merged stage — the summary view |
| `git log --oneline main` | Every logical commit |
| `git log --graph --oneline main` | Each stage as a loop off `main` |
| `git bisect start --first-parent` | Bisect over stages only, then over commits inside the guilty one |

---

## Reviews

How a review is written, filed and followed up is `docs/review/README.md`. Its git side:

- **Push before review.** A review covers commits that exist on GitHub.
- **Fix commits carry the trailers** — `Refs: #N` on a partial fix, `Closes: #N` (one line per issue)
  on the commit that finishes it. See *Trailers*.
- **A fix that fails a follow-up pass stays open.** Its issue is carried, not closed, so no
  `Closes:` goes on that commit.

---

## Rules

**1. Every commit on `main` builds and passes tests.**

The only invariant that really matters. It is what makes `git bisect run ./run-tests.sh` work —
automatically locating a regression across a hundred commits. A history of half-built states makes
bisect useless, which forfeits the main practical payoff of version control as a debugging tool.

"Every commit" includes each logical commit brought in by a merge, not just the merge commit —
bisect walks into them. Enforced by `rebase -i --exec "make check"` at the push gate: a commit that
reaches GitHub broken stays in history.

**2. One logical change per commit — not one file.**

File-granular rules ("each source file committed separately") break rule 1: adding a token type
touches both `lexer.c` and `lexer.h`, and splitting that yields a commit that does not compile.
Group by change, not by file.

**3. Squash noise, keep structure.**

Commit freely on the branch (`wip`, `fix build`, `typo`), then rewrite **before pushing** —
`git rebase -i`, or `git commit --fixup=<sha>` with `git rebase --autosquash`.

Squash into **N logical commits, not one.** A week on the lexer might legitimately be three: token
types, span tracking, error reporting. Each builds, each is independently revertable. Collapsing to
a single 2000-line commit means bisect can only report "somewhere in here."

The one-line-per-stage summary does not come from squashing — it comes from the `--no-ff` merge
commit (see *Merging*).

**4. Never rewrite pushed history.**

Rebase freely on commits that have not been pushed. Every pushed ref — `main` and stage branches
alike — is final: corrections are new commits. A review issue links to pushed commits, and a rewrite
would leave it pointing at commits no branch holds.

**5. Git history is not the day log.**

Squashing deletes day-to-day texture by design, and abandoned approaches leave no commits at all.
Git records how the *codebase* evolved, for someone bisecting. `PROJECT.md` records how *work*
progressed, including failures, for review and for picking up after a gap. Different audiences,
different granularity, no overlap.

---

## Examples

```
feat(ast): define expression node inventory

Tagged union over a node-kind enum, arena-allocated. Evaluation
logic is deliberately excluded so codegen and the interpreter stay
independent walks over the tag.

Refs: D-002, D-003
```

```
test(rt): add Euclidean division property tests

Covers a == (a // b) * b + (a % b) and 0 <= a % b < |b| over random
pairs, with INT32_MIN, b == -1, and negative divisors forced in.
Uniqueness of (q, r) means these two properties fully characterize
correctness.

Refs: D-014
```

```
fix(lex): report span of unterminated token, not end of file

Closes: #2
```

```
build: add sanitizer target to Makefile
```

```
docs(spec): correct unary minus wording in section 4.4

"Single unary minus" was being read as banning a - -b, which is
legal. Made the four cases explicit.
```

```
refactor(parse): extract operand loop from additive and multiplicative
```

---

## Notes — deferred, not yet decided

<!--
  Enforcement mechanics. Revisit once the commit workflow is habitual;
  automating a process that has not settled tends to enforce the wrong thing.
-->

- **`commit-msg` hook** validating the format. A shell script reading `$1` and matching the subject
  line against three patterns:
  - ordinary commits — type mandatory, scope optional:
    `^(feat|fix|refactor|test|docs|build|perf|chore|learn)(\([a-z]+(,[a-z]+)?\))?: .+$`
  - merge commits — branch-name label:
    `^m[0-9]+/[a-z0-9-]+(,[a-z0-9-]+)?: .+$`
  - learning merge commits:
    `^learn/[a-z0-9-]+/[a-z0-9-]+: .+$`

  Length checked separately against the 50/72 limits. Roughly twenty lines, and a legitimate aim-2
  exercise. Without the merge patterns the hook rejects every merge.

- **`pre-commit` hook: keep it fast.** Build and format check only. Once a hook takes fifteen
  seconds, `--no-verify` becomes reflex and the enforcement is theatre.

- **`pre-push` hook rejecting unsquashed commits.** For each ref pushed, read the subjects of the
  commits leaving the machine (`main..local` for a new branch, `remote..local` otherwise) and refuse
  any starting `wip`, `fixup!` or `squash!`. Milliseconds per push. Not `make check` — a slow hook
  trains `--no-verify`.

- **Tag milestone completions** — `git tag v0.1-milestone1`. Gives a stable reference for "the
  language as specified in v1."

- **Reference:** Tim Pope, "A Note About Git Commit Messages" — the canonical short piece on
  subject/body conventions.

---

## Appendix — Command sequence

The mechanics of the workflow above, in order. This is a reference, not a second set of rules: where
it and *Branches* / *Merging* disagree, those sections win. `<branch>` is the stage branch
throughout.

### 1. Pre-checks, on `main`

```
git status                    # On branch main, working tree clean
git fetch                     # downloads only; never touches files or main
git status                    # up to date with 'origin/main'
git rev-parse main            # note the hash
```

`git status` compares against the *local copy* of `origin/main`, so the fetch has to come first or
"up to date" can be stale. Anything other than clean and up to date stops here.

### 2. Branch

```
git switch -c <branch>
git rev-parse main <branch>   # two identical hashes
git merge-base main <branch>  # the same hash again
```

Both checks confirm the branch was cut from the current tip of `main`. A `merge-base` older than
`main` means the branch started from a stale `main` — delete it and start again.

### 3. Work — repeat per commit

```
git status
git diff                      # read before staging
git add <paths>               # by name, never `git add .`
git commit                    # editor; see Format above
```

Keep experiments (scratch files, `a.out`) outside the repository.

### 4. Clean up and push — before every push

```
git status                    # must be clean
git rebase -i --exec "make check" @{upstream}   # first push: main instead of @{upstream}
git push                      # first push: git push -u origin <branch>
```

Only unpushed commits are rewritten; anything already on GitHub is final (Rule 4).

In the todo list: reorder, `squash` or `fixup` the noise into logical commits, and **delete any
`exec` line that would land inside a squash group** — it would check a half-finished state.

`squash` opens an editor to write the combined message; `fixup` discards the squashed commit's
message and keeps the earlier one unchanged.

If the rebase stops at a commit:

```
<fix the files>
git add <paths>
git commit --amend
git rebase --continue
```

or `git rebase --abort` to put everything back as it was.

### 5. Review

Write `docs/review/<milestone>/<branch>/rNNN.md` and file its issues (`docs/review/README.md`). Fixes go
through steps 3 and 4 again.

```
git log --oneline main..<branch>    # two dots: commits not on main
git diff --stat main...<branch>     # three dots: net change since branching
git diff main...<branch>
```

Three dots compares against the shared ancestor, so `main`'s own new commits are not shown as
removals.

### 6. Merge

```
git switch main
git status
git pull --ff-only                  # never a rebasing pull
git merge --no-ff <branch>          # editor: <branch>: <subject>, body, Refs:
```

`--ff-only` refuses rather than rebasing, which would flatten a merge commit not yet pushed. `--no-ff`
forces the merge commit that a fast-forward would skip.

### 7. Verify, push, delete

```
git log --first-parent --oneline -3 # one new line: the merge
git log --graph --oneline -6        # the stage as a side line rejoining main
git push
git status                          # up to date with 'origin/main'
gh issue view <N>                   # each Closes: issue shows closed, not referenced
git push origin --delete <branch>
git fetch --prune
git branch -d <branch>              # lowercase -d, always
```

If `-d` refuses, the branch is not merged — investigate. Reaching for `-D` discards the check.

### Notes

- **Recovery.** Amend and rebase write *new* commits and leave the old ones unreachable, not
  deleted. `git reflog` lists where `HEAD` has been; the old hash is usable until garbage collection.
  `git branch -d` prints the deleted tip's hash for the same reason.
- **On scripting this.** Steps 1, 2 and 7 automate cleanly — they are checks with predictable output.
  Steps 3 to 6 do not: choosing what goes in each commit, editing the todo list, reading the diff and
  writing the messages are the parts that carry the value. A `wip-start` / `wip-finish` pair around
  the manual middle is the natural split, and the same place the deferred hooks fit.
