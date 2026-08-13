# funC — Commit Conventions

## Format

```
<type>(<scope>): <subject>

<body>

<trailers>
```

### Subject line

```
feat(lex): add span tracking to token stream
```

- **Imperative mood.** "add", not "added" or "adds". Reads as an instruction to the codebase.
- **Under 50 characters.** Hard ceiling 72; `git log --oneline` truncates.
- **Lowercase after the colon.** No trailing period.
- **`type` and `scope` are mandatory.** No bare subjects.

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
| `build` | Makefile, hooks, CI |
| `spec` | Specification and decision documents |

Two scopes are allowed when a change genuinely spans phases: `feat(lex,parse): ...`. Three means the
commit should be split.

---

## Trailers

```
Refs: D-014
Refs: D-003, D-011
Closes: #12
```

`Refs` points at `DECISIONS.md` entries. Use it wherever an implementation choice traces back to a
recorded decision — it makes `git log --grep="D-014"` return every commit touching that decision.

---

## Rules

**1. Every commit on `main` builds and passes tests.**

The only invariant that really matters. It is what makes `git bisect run ./run-tests.sh` work —
automatically locating a regression across a hundred commits. A history of half-built states makes
bisect useless, which forfeits the main practical payoff of version control as a debugging tool.

**2. One logical change per commit — not one file.**

File-granular rules ("each source file committed separately") break rule 1: adding a token type
touches both `lexer.c` and `lexer.h`, and splitting that yields a commit that does not compile.
Group by change, not by file.

**3. Squash noise, keep structure.**

Commit freely while working (`wip`, `fix build`, `typo`), then rewrite before pushing —
`git rebase -i`, or `git commit --fixup=<sha>` with `git rebase --autosquash`.

Squash into **N logical commits, not one.** A week on the lexer might legitimately be three: token
types, span tracking, error reporting. Each builds, each is independently revertable. Collapsing to
a single 2000-line commit means bisect can only report "somewhere in here."

**4. Never rewrite pushed history.**

Rebase freely before push. After push, corrections are new commits.

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

- **`commit-msg` hook** validating the format. A shell script reading `$1` and grepping the subject
  line against `^(feat|fix|refactor|test|docs|build|perf|chore)\(...\): .{1,50}$`. Roughly twenty
  lines, and a legitimate aim-2 exercise.

- **`pre-commit` hook: keep it fast.** Build and format check only. Once a hook takes fifteen
  seconds, `--no-verify` becomes reflex and the enforcement is theatre.

- **`pre-push` hook** for the full test suite. A slow check is tolerable where it runs rarely.

- **Tag milestone completions** — `git tag v0.1-milestone1`. Gives a stable reference for "the
  language as specified in v1."

- **Reference:** Tim Pope, "A Note About Git Commit Messages" — the canonical short piece on
  subject/body conventions.
