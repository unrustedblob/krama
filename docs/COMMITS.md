# Krama — Commit Conventions

Covers commit messages, branches and merging. The reasoning behind the branch and merge workflow is
`D-030`; the reasoning behind optional scopes and issue trailers is `D-031`.

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
| `docs` | Spec, README, PROJECT.md, DECISIONS.md, comments |
| `build` | Makefile, compiler flags, hooks, tooling |
| `perf` | Measured performance change — not speculative |
| `chore` | Housekeeping with no source effect (.gitignore, file moves) |

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
| `diag` | Diagnostic formatting and reporting machinery |
| `test` | Test corpus, driver, property tests |
| `spec` | Specification and decision documents |

Two scopes are allowed when a change genuinely spans phases: `feat(lex,parse): ...`. Three means the
commit should be split.

No `build` scope: the `build` type already says where. `test(test)` is redundant for the same
reason — write `test: ...` for a change to the test driver itself, and `test(rt): ...` for tests of
another phase.

---

## Trailers

```
Refs: D-014
Refs: D-003, D-011
Refs: #7
Closes: #12
```

| Trailer | Meaning | Effect on GitHub |
|---|---|---|
| `Refs: D-###` | Implementation choice traces back to a `DECISIONS.md` entry | none |
| `Refs: #N` | Commit is *part of* the work on an issue | links the commit to the issue |
| `Closes: #N` | Commit *finishes* the issue | links, and closes the issue when the commit reaches `main` on GitHub |

- **`Refs: D-###`** makes `git log --grep="D-014"` return every commit touching that decision.
- **`Closes:` goes on the logical commit that does the fixing**, not only on the merge commit, so
  the closure points at the actual change. Repeating the issue in the merge body is harmless.
- GitHub accepts the colon and any capitalisation (`Closes: #10`). Only its closing keywords close
  an issue — `close(s|d)`, `fix(es|ed)`, `resolve(s|d)`. `Refs:` never closes anything.
- With local-only branches, nothing reaches GitHub until `main` is pushed; issues close at that push.

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
  stage branches from the updated `main`. No long-lived `dev` or `milestone-N` branch.
- **Local only. Branches are never pushed.** This keeps them freely rewritable (Rule 4) and removes
  the question of whether rewriting a pushed branch is allowed. The cost: unmerged work exists on one
  machine only — another reason to keep stages small.
- **Branch from a fresh `main`.** Pull first. A stale local `main` reintroduces the long-lived-branch
  problem `D-030` exists to avoid. To confirm, `git merge-base main <branch>` on a new branch should
  print the same hash as `git rev-parse main`.
- **Delete after merging** with `git branch -d`. After a `--no-ff` merge the branch tip is reachable
  from `main`, so `-d` succeeds. If `-d` refuses, the branch is *not* merged — investigate rather
  than reaching for `-D`.

---

## Merging

### The merge gate

A branch merges into `main` only when all of these hold, in this order:

1. **Cleaned up and checked per commit.** `git rebase -i --exec "make check" main` — squash noise into
   logical commits (Rule 3); `make check` runs after *every* rewritten commit and stops at the first
   failure. Passing at the branch tip alone is not sufficient: a rebase creates intermediate states
   that never existed while working.
2. **Reviewed after the rebase.** `git diff main...<branch>` (three dots — everything the branch
   changed since it left `main`) against STYLE.md §14. A review done before the rebase covered code
   that may no longer exist. If the review produces fix commits, return to step 1.
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
- **Trailers:** `Refs:` for every decision the stage touched; issues optionally repeated.

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

## Rules

**1. Every commit on `main` builds and passes tests.**

The only invariant that really matters. It is what makes `git bisect run ./run-tests.sh` work —
automatically locating a regression across a hundred commits. A history of half-built states makes
bisect useless, which forfeits the main practical payoff of version control as a debugging tool.

"Every commit" includes each logical commit brought in by a merge, not just the merge commit —
bisect walks into them. Enforced by `rebase --exec "make check"` at the merge gate.

**2. One logical change per commit — not one file.**

File-granular rules ("each source file committed separately") break rule 1: adding a token type
touches both `lexer.c` and `lexer.h`, and splitting that yields a commit that does not compile.
Group by change, not by file.

**3. Squash noise, keep structure.**

Commit freely on the branch (`wip`, `fix build`, `typo`), then rewrite before merging —
`git rebase -i`, or `git commit --fixup=<sha>` with `git rebase --autosquash`.

Squash into **N logical commits, not one.** A week on the lexer might legitimately be three: token
types, span tracking, error reporting. Each builds, each is independently revertable. Collapsing to
a single 2000-line commit means bisect can only report "somewhere in here."

The one-line-per-stage summary does not come from squashing — it comes from the `--no-ff` merge
commit (see *Merging*).

**4. Never rewrite pushed history.**

Rebase freely on a local branch. Branches are never pushed, so they are always rewritable until
merged. After `main` is pushed, corrections are new commits.

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
  line against two patterns:
  - ordinary commits — type mandatory, scope optional:
    `^(feat|fix|refactor|test|docs|build|perf|chore)(\([a-z]+(,[a-z]+)?\))?: .+$`
  - merge commits — branch-name label:
    `^m[0-9]+/[a-z0-9-]+(,[a-z0-9-]+)?: .+$`

  Length checked separately against the 50/72 limits. Roughly twenty lines, and a legitimate aim-2
  exercise. Without the second pattern the hook rejects every merge.

- **`pre-commit` hook: keep it fast.** Build and format check only. Once a hook takes fifteen
  seconds, `--no-verify` becomes reflex and the enforcement is theatre.

- **`pre-push` hook** running `make check`. A slow check is tolerable where it runs rarely.

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

### 4. Clean up

```
git status                    # must be clean
git rebase -i --exec "make check" main
```

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

```
git log --oneline main..<branch>    # two dots: commits not on main
git diff --stat main...<branch>     # three dots: net change since branching
git diff main...<branch>
```

Three dots compares against the shared ancestor, so `main`'s own new commits are not shown as
removals. Anything found sends you back to step 4 — review follows the rebase, because the rebase
can change code.

### 6. Merge

```
git switch main
git status
git pull --ff-only                  # never a rebasing pull
git merge --no-ff <branch>          # editor: <branch>: <subject>, body, Refs:
```

`--ff-only` refuses rather than rebasing, which would flatten an unpushed merge commit. `--no-ff`
forces the merge commit that a fast-forward would skip.

### 7. Verify, push, delete

```
git log --first-parent --oneline -3 # one new line: the merge
git log --graph --oneline -6        # the stage as a side line rejoining main
git push
git status                          # up to date with 'origin/main'
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
