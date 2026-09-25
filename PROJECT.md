# Krama — Project State

> **Current** is overwritten each update. **Log** is append-only, newest last.
> Read Current for where things stand; read the Log tail backwards to rewind.

---

## Current

**Milestone:** 1 single `main`, three scalar types, arithmetic, `@print`

**Working on:** `m1/cli-args` — `main` takes one source path, plus the first test driver. Five
commits planned (`krama-notes-15` §4), plus one unplanned docs commit for D-038, which came out of
the work rather than ahead of it

**Blocked on:** -

**Last green:** 39/39 unit tests passing under `make check`, clean under the §12 flags and under
`-fsanitize=address,undefined -fno-sanitize-recover=undefined`

**Next up:** Finish `m1/cli-args`. Then `m1/fatal-cleanup`, a small branch settling what the fatal
family is: rename `FATAL_*` to `KRM_FATAL_*`, fix the macro-prefix convention once (header guards
included — `KRAMA_EXIT_CODES_H` is currently a third spelling), and make the arena's allocation
failure exit `OOM` rather than `INTERNAL`, since `fatal()` currently derives its status from *how*
to die rather than *why*. Reserve `KRM_ERR_PATH_EXIT` / `_GOTO` for the diagnostic family while
there. It lands before the lexer, which is the first heavy user of both families.

Then the lexer stage — which carries the diagnostic sink (D-033), stop-at-first-error (D-034) and
the `.expected` dump format, and must reconcile with spec §3 first (`krama-notes-11` §8). Arena
initialization and AST node definitions follow the lexer, not precede it: the AST's input is the
token, so it cannot be designed before one exists.

**Deliberately incomplete** —

| Location | State | Intent |
|---|---|---|
| `arena.c` — block size | Round starting number, not derived from node size | Revisit once `struct ASTNode` exists and its size is known |
| `arena.c` — mark/release | Absent | Nothing in milestone 1 reclaims early; the AST lives to teardown. Additive when multi-file arrives |
| `arena.c` — alignment | Fixed at `alignof(max_align_t)` | Per-type alignment is a later refinement, STYLE.md §9.2 |
| `arena.c` — ASan poisoning | Absent | `ASAN_POISON_MEMORY_REGION` on block creation, unpoison per allocation. Planned follow-up, STYLE.md §9.2 |
| `tests/` — abort paths | Untested | Every `FATAL_PATH_ABORT` ends the process, which the in-process suite cannot survive. Needs a process-per-case driver; the Python driver of D-036 / D-037 is its seed. First concrete case is D-029 |
| `tests/unit_tests/test_arena.c` — test 2 capacity check | Compares against the initial capacity, not the current one | Passes regardless of cursor movement. Left until the milestone 2 arena revision (D-027, D-028) rewrites the fixture anyway |
| `Makefile` — `make test` | Builds the test binary, does not run it | `check` runs the binary directly meanwhile. Revisit when the `.krm`/`.expected` pairs land and `test` gains a runner |
| `tests/` — output volume | Full per-assertion output on every run | A quiet mode reducing a pass to `N / Total Passed` would make a full gate run read as a short checklist. Cosmetic until the corpus grows |
| Hooks — `commit-msg`, `pre-commit`, `pre-push` | Absent | COMMITS.md carries the intended shape. Automating a workflow that has not settled tends to enforce the wrong thing; revisit once the first few merges are habitual |
| `src/main.c` — source path | Hardcoded absolute path to a fixture, `/home/unrust/projects/krama/tests/krama_src/test.krm` | Breaks if the working copy moves — it already did once, during D-032. Goes when `main` takes a path argument, due on `m1/cli-args` (`krama-notes-15` §4, commit 4) |
| `tests/unit_tests/test_arena.c` - `CHECK` macro | Potentially could be a function wrapped in a macro | Per STYLE.md's §8.4 a macro should only be used for the listed use cases, the `CHECK` macro can effectively be replaced by a function, deferring this till the next unit test as at that point it's possible `CHECK` would move to its own file |

---


## Log

<!--
One entry per working session. Append at the bottom. Never edit a past entry —
if something turned out wrong, say so in a later entry.

Keep it short. Three sentences per field is plenty. The failures are the most
valuable part: they are what redirects review and what workout exercises get
built from.
-->

### 2026-08-12 — Spec closed, pre-implementation

**Did.** Consolidated design sessions 01–06 into `krama-spec-v1.md`. Established `DECISIONS.md` and
this file.

**Worked.** Milestone 1 specification is closed. Lexical grammar, EBNF, precedence table, type
judgments, division semantics, and trap conditions are all settled. No open item blocks AST
construction.

**Didn't.** No code written yet by design.

**Friction.** n/a

**Stubbed.** n/a

**Next.** Baseline coding standards, then AST node definitions.

---

### 2026-08-14 - First commit submitted, implementation started

**Did.**

1. Coding standards setup, document STYLE.md - may be progressively updated if needed.
2. Simple `main()` function written. It opens the file (currently stored in variable in source), reads it into a buffer and dumps output to `stdout`

**Worked.**

1. `goto` ladder correct on first attempt.
2. `exit_status` initialized as 1 and set to 0 once at the end.
3. `ferror` and `feof` handling on `fread`.

**Didn't.**

| Review Pass | Issue # | Bug | Resolution |
|---|---|--- | ---|
| 1 | [#1](https://github.com/unrustedblob/krama/issues/1) | Heap overread as `buf` was not `NUL` terminated | `buf` is allocated `file_sz + 1` bytes and last byte is set to  `NUL` |
| 1 | [#2](https://github.com/unrustedblob/krama/issues/2) | Missed `goto cleanup_file` on `fseek` failure | Included `goto` |
| 2 | [#3](https://github.com/unrustedblob/krama/issues/3) | Setting `buf[file_sz]` to `NUL` before checking `malloc` status | Moved it after the `malloc` check. Consequence of "rushed update" related to bug:[#1](https://github.com/unrustedblob/krama/issues/1) |
| 2 | [#4](https://github.com/unrustedblob/krama/issues/4) | Incorrect format string `%ld` used for `size_t` | Missing `-Wformat-signedness` flag. Updated to `%zu`, the correct format string. **Note** Review claimed -Wformat alone would catch it, that was wrong; identified by leaving the incorrect string in to test it. |
| 3 | N/A - Style | No `void` in `main()` as parameter | C23 standard allows this, but as stated in STYLE.md - 8.1, `void` is kept for consistency. Updated parameter `void` |
| 3 | [#5](https://github.com/unrustedblob/krama/issues/5) | Empty file reported as error `malloc(0)` can return `NULL` | Fixed incidentally by adding 1 to the `file_sz` when calling `malloc` |

**Friction.**

Makefile - `CC ?= gcc` is inert. The build ran cc. Attempt to get it working by using below:

```make
ifeq ($(origin CC),default)
  CC = gcc
endif
```

This did not work. For now, using `override CC = gcc` but this will ignore any command line variables for `CC` - so this needs to be resolved before we need to try multiple compilers.

**Stubbed.** - n/a

**Next.**

1. Start work on the Arena and its API.
 
---

### 2026-08-21 — Arena iteration 1, macro rules settled

**Did.**

1. Arena allocator, first iteration. Opaque handle (`struct Arena *`), `arena_create` takes no
   parameters, chained fixed-size blocks prepended at the head, bump allocation with computed
   padding. `arena_reset` deferred past milestone 1.
2. STYLE.md §8.4 added, naming the five jobs only a macro can do; §7.1 scoped so its `#define`
   clause reads as being about naming values rather than about macros generally. Version 1.1 → 1.2.
   Recorded as D-022.
3. Alignment study — reading list and exercises collected before attempting the cursor design,
   rather than copying the arithmetic without understanding it.
4. Extended the technique-index skill with the named techniques this work surfaced.

**Worked.**

1. Design questions were answered in dependency order — ownership first, then what the AST asks of
   the allocator, then the API — so each answer constrained the next rather than being revisited.
2. Dropping the capacity parameter from `arena_create` removed a failure mode instead of handling
   it. `main` has no basis for choosing a block size, so the parameter had no legitimate caller.
3. The allocate-or-die decision paid for the block layout: with two allocations per block there is
   a partial-failure state, but since OOM exits there is no unwind path to write.

**Didn't.**

- n/a

**Friction.**

Alignment arithmetic did not go in on first reading. Specifically: why STYLE.md §9.2 says to compute
the padding rather than round the address up, and why forming a pointer past one-past-the-end is a
defect even when it never fires. Resolved by working the wrap case on a small address space by hand
rather than accepting the formula. This is a candidate for a standalone workout — a bump allocator
over a `static` buffer, outside Krama — per the friction convention below.

**Stubbed.** See the *Deliberately incomplete* table. Mark/release, per-type alignment, and ASan
poisoning of arena memory are all absent by decision, not by oversight.

**Next.**

1. Complete the arena unit tests, failure paths included.
2. Decision records for the arena's implementation choices — no capacity parameter, separate block
   and data allocations over a flexible array member, pointer cursor with a `size_t` companion.
3. The fatal-error module and the assertion mechanism, which STYLE.md §13 defers to "before the
   lexer" and which the arena has already forced into the open.
4. AST node definitions.

---

### 2026-08-25 — Arena iteration 1, Unit Testing (1)

**Did.**

1. Tested `arena_create` and validated initial state of the arena.
2. Tested three scenarios of allocation:
  a. **Regular Case**: The request is within the available space in the current block.
  b. **Tight Fit**: The request is exactly the sapce available in the current block.

**Worked.**

1. Arena created correctly, initial capacity matches the fixed block initial capacity.
2. Regular case allocation working as expected, cursor and available space checked and verfied cursor/retrned pointer is aligned


**Didn't.**

1. The tight fit case failed.

| Review Pass | Issue # | Bug | Resolution |
|---|---|--- | ---|
| 1 | [#6](https://github.com/unrustedblob/krama/issues/6) | Program aborts when passing allocationr equest with exact fit | The contraint was updated to handle teh equal scenario as well |


**Friction.**

- n/a

**Stubbed.** See the *Deliberately incomplete* table. Mark/release, per-type alignment, and ASan
poisoning of arena memory are all absent by decision, not by oversight.

**Next.**

1. Complete the arena unit tests, failure paths included.
2. Decision records for the arena's implementation choices — no capacity parameter, separate block
   and data allocations over a flexible array member, pointer cursor with a `size_t` companion.
3. The fatal-error module and the assertion mechanism, which STYLE.md §13 defers to "before the
   lexer" and which the arena has already forced into the open.
4. AST node definitions.

---

### 2026-08-30 — Arena iteration 1, Unit Testing Complete

**Did.**

1. Arena API tested, fatal-error module ready

**Worked.**

1. Arena created correctly
2. Allocations working, tested regular case, max fit and tight fit cases
3. Tests passing.
4. The fatal error module is working and records the file, function and line at which the failure occured before caloing the handling function.

**Didn't.**

- n/a

**Friction.**

- n/a

**Stubbed.** See the *Deliberately incomplete* table. Mark/release, per-type alignment, and ASan
poisoning of arena memory are all absent by decision, not by oversight.

**Next.**

1. Get it reviewed.
2. Decision records for the arena's implementation choices — no capacity parameter, separate block
   and data allocations over a flexible array member, pointer cursor with a `size_t` companion.
3. AST node definitions.

---

### 2026-09-17 — Arena iteration 1 reviewed and closed

**Did.**

1. Full review pass over `arena.c`, `arena.h`, `fatal.c`, `fatal.h` and `tests/test_arena.c` against
   STYLE.md and the spec. Everything raised is either fixed or recorded below.
2. `allocate` restructured around command-query separation — `compute_padding` returns a number,
   `move_cursor` is the only mutator, one growth decision per allocation.
3. Zero-size requests now abort, with the precondition documented in `arena.h` (D-029).
4. Header doc comments carry STYLE.md §9.5's lifetime vocabulary.
5. Decision records for the arena's implementation shape: D-027 (separate block and buffer
   allocations), D-028 (pointer cursor with a `size_t` companion), D-029 (zero-size aborts).
6. Test suite extended to 39 checks — padding-forced growth, and an allocation immediately after
   growth checked for alignment and placement rather than only for capacity change.

**Worked.**

1. Command-query separation in `allocate` came from re-reading the code, not from the review — the
   review only supplied the name afterwards. Splitting the computation from the move is what made
   the alignment invariant provable instead of argued.
2. Design questions were answered before code, in dependency order: ownership, then what the AST
   asks of the allocator, then the API. Each answer constrained the next rather than being revisited.
3. Sanitizers found what the tests could not. Every block was leaking while all 34 tests passed;
   ASan reported it on the first run. STYLE.md §9.2's warning is about *arena* bugs specifically —
   block bookkeeping is still visible to it.
4. The allocate-or-die decision paid for the block layout. Two allocations per block create a
   partial-failure state, but since OOM exits there is no unwind path to write (D-027).
5. Reading before implementing on alignment. The study list and the hand-worked wrap case came
   first, so the formula was understood rather than copied.

**Didn't.**

| Review Pass | Issue # | Bug | Resolution |
|---|---|--- | ---|
| 2 | [#7](https://github.com/unrustedblob/krama/issues/7) | Memory leak when adding block | Identified by reviewer, included `fsanitize` flags in Makefile. The issue was in prepending the new block, instead of pointing the new block to the current one, it pointed to `->next`, which was null - fixed |
| 2 | [#9](https://github.com/unrustedblob/krama/issues/9) | The OOM path prints identifiers | The `FATAL` macro printed the condtion on the `FATAL_EXIT_PATH` which is meant for system/env issues. Fix, restricted the condition printing to `FATAL_PATH_ASSERT` |
| 2 | [#10](https://github.com/unrustedblob/krama/issues/10) | `assert` instead of `static_assert` | Using a runtime check for somthing that could be validated at comile time. Fix, used `static_assert` |
| 2 | [#11](https://github.com/unrustedblob/krama/issues/11) | `assert` for internal invariants in `allocate` and `move_cursor` |  |
| 2 | [#12](https://github.com/unrustedblob/krama/issues/12) | `GROWTH_FACTOR` defined but not used | Replaced hardcoded numbers with `GROWTH_FACTOR` |
| 2 | [#13](https://github.com/unrustedblob/krama/issues/13) | Assumptions fail when `align` becomes a param | Arena interface currently aligns to `max_align_t` but the future plan is to pass in the required alignment. Enahancements: <br> 1.  `mak_align` is now replaced with `compute_padding` - this allows us to localize the cursor moving to just `move_cursor` <br> 2. `allocate` now checks and validates alignment, computes padding, checks if a grow is required and simply moves the cursor once by padding and once by requested size <br> 3. `add_block` checks for enough space including maximum possible padding bytes for a request|
| 2 | [#15](https://github.com/unrustedblob/krama/issues/15) | Handle `allocate(arena, 0)` | Allocation requests of 0 bytes are treated as `FATAL` and abort |

**Friction.**

1. **The growth condition took three attempts.** The first used `&&`, which made one conjunct
   permanently true and, in the case it appeared to guard, skipped growth and aborted instead. The
   second split it into two `if`s — which closed the gap but could grow twice for one allocation and
   left the dead conjunct in place. The third, one subtractive test with `||`, was correct and
   shorter than both. Both wrong versions *looked* like they handled the padding case; only tracing
   the false branch showed otherwise.
2. **Padding accounting circled for several exchanges.** Whether to fold `align - 1` into the
   request or subtract it from the ceiling — both work, doing both double-counts. Resolved by fixing
   which function owns which check before touching the arithmetic again.
3. **The sanitizer build failed to link.** `-fsanitize=address` was on the compile line but not the
   link line; ASan's runtime is pulled in by the driver at link time, so the result was a wall of
   undefined `__asan_*` references. STYLE.md §12 wants `address,undefined` as a separate target, not
   in `CFLAGS`.

**Stubbed.** See the *Deliberately incomplete* table. Two new rows this session: abort paths are
untested pending a process-per-case driver, and test 2's capacity assertion is weaker than it looks
and is deliberately left until the milestone 2 arena revision.

**Next.**

1. Initialize the arena in `main`.
2. AST node definitions — Appendix A is the checklist. This also settles the block size, which is
   currently a round number rather than a derived one.
3. The diagnostic sink. STYLE.md §13 defers it to "before the lexer" and spec §2.1 has the scanner
   emitting diagnostics, so it blocks the lexer rather than the AST.
4. Lexer.

**Note for next session.** Alignment arithmetic now appears in Friction twice (2026-08-21 and this
entry). Per the convention below, that graduates it from candidate to scheduled: a bump allocator
over a `static` buffer, outside Krama.

---

### 2026-09-18 — Git workflow settled, merge gate in the Makefile

**Did.**

1. Branch and merge workflow decided and recorded: short-lived, local-only `m<N>/<stage>` branches
   from an up-to-date `main`, rewritten with `git rebase -i --exec "make check"`, reviewed, then
   merged with `--no-ff` carrying a dense stage summary (D-030).
2. Commit subject rules amended: type mandatory, scope optional and drawn from a closed vocabulary,
   issue numbers moved out of the subject into `Refs:` / `Closes:` trailers (D-031).
3. COMMITS.md rewritten around both — new *Branches* and *Merging* sections, the merge gate as an
   ordered list, updates to Rules 1, 3 and 4, and hook-regex notes.
4. Makefile: `fmt`, `fmt_check` and `check` added, `.PHONY` completed,
   `-fno-sanitize-recover=undefined` added to `SANFLAGS`.
5. First run of the workflow on itself — `m1/workflow` is the branch that introduces the workflow.

**Worked.**

1. Checking the reference instead of trusting it. The per-stage-squash idea was attributed to
   chibicc; the repository turned out to be 316 linear commits with no merges at all, and a
   `historical/old` branch holding the original history. That killed one option and reframed two
   others.
2. Deciding the merge style *before* writing the Makefile targets. `check` exists because D-030
   needs a per-commit gate; the target's shape fell out of the decision rather than the reverse.
3. The exit-status experiment. Two builds of an `INT_MAX + argc` program showed UBSan reporting the
   overflow and still exiting `0`. A flag added on reasoning alone would have been believed; this
   one was watched failing first.
4. Reading the Makefile with the new target in mind surfaced the `MAKECMDGOALS` trap before it
   could bite (see Friction).

**Didn't.**

1. **Pushed branches and pull requests** — rejected for a solo project. A pushed branch makes Rule 4
   ambiguous, and the whole workflow depends on rewriting the branch freely before merging.
2. **Check results in the commit message.** A hand-written "39/39 passed" is a claim, not evidence.
   Left to `pre-push` and CI; a `Checks:` trailer is the shape if it is ever wanted.
3. **Retroactive history rewriting** to make past work fit the new workflow. Rule 4 forbids it and
   the history is pushed; the workflow starts from this branch.

**Friction.**

1. **`MAKECMDGOALS` holds what was typed, not what gets built.** The test-mode block was guarded by
   `$(filter test,$(MAKECMDGOALS))`, so `make check` — which *builds* `test` as a prerequisite —
   would have left sanitizers, `-DFUNC_TEST_SUITE` and the test sources switched off, with an empty
   `TEST_TARGET`. Fixed with `$(filter test check,...)`. The gate would have been silently
   configured wrong rather than failing loudly.
2. **UBSan does not fail by default.** `-fsanitize=undefined` reports and continues, so a run with
   undefined behaviour still exits `0` and every layer above it — Make, then
   `git rebase --exec` — sees success. Only `-fno-sanitize-recover=undefined` closes it.
3. **"Whose exit status is this?"** twice in one session: `make test` exiting `0` after a
   compile-only run, and `echo $?` after `gcc` reporting the compiler rather than the program that
   was never run. Both looked like passes.
4. **The first commit message needed an amend** — subject was a noun list rather than imperative,
   and the `Refs:` trailer was missing. The conventions are written down; applying them is not yet
   automatic.

**Stubbed.** Four new rows in *Deliberately incomplete*: `make test` still builds without running,
`check` never compiles `src/main.c`, test output has no quiet mode, and all three hooks are absent.

**Next.**

1. Merge `m1/workflow` into `main` with `--no-ff`, then delete the branch.
2. Initialize the arena in `main` — first stage under the new workflow, so first real branch.
3. AST node definitions — Appendix A is the checklist; also settles the block size.
4. The diagnostic sink, then the lexer.

**Note for next session.** The alignment-arithmetic exercise from the previous entry is still
scheduled and untouched: a bump allocator over a `static` buffer, outside Krama.

---

### 2026-09-19 — Renamed to Krama, workflow's second run

**Did.**

1. Renamed the project from funC to **Krama** (D-032): `.krm` sources, `.krm` / `.expected` test
   pairs, `kramac` binary, `KRAMA_` macros and guards, `krama_` emitted runtime, `Krama` in prose and
   `krama` in code.
2. Ran the workflow end to end for the second time on `m1/rename`, this time with three logical
   commits: the COMMITS.md command-sequence appendix, the documentation rename, then identifiers and
   fixtures.
3. Added the command-sequence appendix to COMMITS.md — the seven steps as runnable commands with the
   reasoning for each check inline.
4. Consolidated the fixture corpus: `tests/fc-src/` and `tests/funC_src/` with `.fc` and `.func`
   became `tests/krama_src/` with `.krm` throughout. Renamed the working copy to match.
5. Settled the scope of what gets renamed: live documents and every *pointer to a file* change;
   decided entries and note prose keep saying funC, because they record a project that carried that
   name at the time.

**Worked.**

1. Checking every candidate name against reality rather than taste. `mouse`, `koa`, `tav`, `naja`
   and `bug` each looked fine and each was taken — `tav` by an existing C-inspired language, which
   is the same category as this project. Three searches per candidate (the name as a language, the
   extension, the repository) cost minutes and removed five names.
2. Surveying with `git grep -i` before changing anything. 341 matching lines, of which 159 were the
   English word "function" — the real work was about 180 lines, and 11 identifiers. Knowing the
   split before starting is what made a blind `sed` avoidable.
3. Splitting the branch by logical change rather than by file. COMMITS.md carried two unrelated
   changes — a title rename and a new appendix — and `git add -p` staged them into separate commits
   from one file.
4. The gate caught nothing this time, which is the correct outcome for a mechanical change, and
   `make check` still proved the `KRAMA_TEST_SUITE` rename reached every header: a define renamed in
   the Makefile but missed in `arena.h` would have failed the test build outright.

**Didn't.**

1. **`sutra`** — the closest miss, naming the genre of terse rule-texts expanded by a teacher, which
   is what the spec and the planned book already are. Dropped for the Kama Sutra association.
2. **`kerf`** — the runner-up, and the better *story*: a kerf has width, and cutting as though it
   does not is the beginner's error that this language's position on overflow refuses to make.
   Dropped because `krama` describes both the artifact and the method of building it.
3. **`anu`** and the rest of that family — all girls' names, which ruled out the shortest and
   cleanest Sanskrit options.
4. **Rewriting history** so the project appears always to have been Krama, as chibicc did. Rule 4
   forbids it, and the rename is itself part of the record.
5. **A book chapter.** Agreed the K&R-style walkthrough is worth writing, and that chapters are
   written when a milestone *closes*, not as the language moves. Its examples should be actual
   corpus files, so `make check` proves the book still compiles.

**Friction.**

1. **`git grep` searches contents, never filenames.** Every sweep this session was blind to
   `funC2.func`, which survived the extension rename with its basename intact. The check for names
   is `git ls-files | grep -i func`, and it reads the *index*, so it only tells the truth after
   staging.
2. **Case-sensitivity is a tool, not a detail.** `funC` cannot match `FUNC_TEST_SUITE`; `FUNC_`
   cannot match `__func__`. Surveying wants `-i`; changing things wants precise patterns. A blind
   `sed 's/func/krama/g'` would have produced `kramation` and broken `fatal.h` outright.
3. **`sed 's/funC/krama/g'` would also have turned `cfunC` into `ckrama`.** Ordered substitutions,
   longest first, or exclude the file.
4. **Lowercase spellings hid from the sweeps.** `funcrt.c` in two spec sections and `.func` in
   several places matched neither `funC` nor `FUNC_`. Found only by searching lowercase `func`
   separately.
5. **An edit was undone by a file handed back for pasting.** The Status line of D-032 had a clause
   trimmed, then the trimmed version was overwritten by a regenerated copy built from a stale read.
   Caught in review. Worth preferring targeted edits over whole-file replacement when a file is
   already being edited by hand.

**Stubbed.** One new row in *Deliberately incomplete*: `src/main.c`'s hardcoded absolute fixture
path, which broke once already when the working copy was renamed.

**Next.**

1. Initialize the arena in `main`, on `m1/arena-init` — or folded into `m1/ast-nodes`, since block
   size depends on node size. Decide before naming the branch; the name is permanent once merged.
2. Fold `all` into `check` so the gate compiles `src/main.c`. The gap is harmless only while
   `main.c` does nothing, which stops being true with the arena in it.
3. AST node definitions — Appendix A is the checklist.
4. Rename the GitHub repository, then `git remote set-url`.

**Note for next session.** The alignment-arithmetic exercise is still scheduled and untouched: a
bump allocator over a `static` buffer, outside Krama. Session numbering is also owed — the
`DECISIONS.md` template's `Session: ##` has never been filled in, and tying it to the note number is
the cheapest fix, since both increment together.

---

### 2026-09-21 — Lexer before AST, first-iteration lexer design

**Did.**

1. Reordered the milestone: the lexer comes before AST node definitions, and the arena is no longer
   tied to the next step. The test applied was "which stage has every input either built or
   decided" — only the lexer passes, since the AST's input is the token, which does not exist yet.
2. Settled the token buffer: one allocation of `n + 1` tokens, justified by a documented invariant
   (every token covers at least one byte, no two share a byte, EOF is the only zero-width token) and
   asserted on every append, per STYLE.md §9.3.
3. Separated two tables that had been one: character classes (what the scanner reads) and token
   kinds (what it emits). Whitespace and comments have classes and no kinds.
4. Settled the scanner's shape: the first character picks the rule, and that rule alone decides
   where the token ends. The "context flags" idea (`in_char_literal`) is not state — it is which
   branch of the code is running.
5. Queued two decisions, later recorded as **D-033** (the diagnostic sink is settled inside the
   lexer stage, not before it) and **D-034** (stop at the first lexical error).

**Worked.**

1. Refusing to design the AST without its input. Reading chibicc's `struct Node` and harec's
   `ast.h` side by side produced no decision, and the reason was structural: both are shaped by what
   the parser needs to hold, and the parser does not exist. That was evidence the order was wrong,
   not that the reading had failed.
2. Tracing a concrete bad input through the whole pipeline. `let x$y: i32 = 1;` showed that the
   parser only asks whether an identifier makes sense *in position*, never whether it is one, so
   `x$y` would reach `cc`. That settled identifier validity as the lexer's job and produced the rule:
   each phase rejects what it is the first able to see.
3. Deriving the bound properly rather than from an example. `x+1` shows `n` tokens are *reachable*;
   the *ceiling* needs the two invariants. The distinction is what makes the comment at the buffer
   worth writing.

**Didn't.**

1. **`n / 2 + 1` as the token bound** — the first guess, assuming every token is followed by a
   separator. `x+1` is three tokens in three bytes and the bound gives two, so the third append
   overflows.
2. **"A separator is a character."** A token ends wherever the next character cannot continue it;
   `x` ends at `+` because `+` cannot be part of an identifier, not because `+` separates.
3. **Recording errors and continuing**, for now — D-034. It is the better behaviour and it is
   cheap in the lexer specifically, but nothing yet needs two errors from one run.
4. **A milestone-plus-counter branch scheme** (`m1/lex:1/…`). `:` is illegal in a Git ref name, and
   the merge commits on `main` already record order.

**Friction.**

1. **Claude stated twice that `krama-spec-v1.md` was not in the project files.** It was. The whole
   first-draft design was worked out without consulting §3, which meant `#` (comments) and `@`
   (builtins) were missing from the character table, `<`, `>`, `&`, `|`, `^`, `!`, `[`, `]` and `"`
   were included despite not being in milestone 1, and maximal munch and keyword post-filtering were
   treated as deferrable when §3.3 already requires them. Reconciled in `krama-notes-11` §8 — read
   that section before writing the lexer.
2. **A guessed bound looks like a proof.** `n / 2 + 1` reads as reasoning and is not; it survived
   until a three-byte example was tried against it. Any fixed-capacity buffer now gets the
   adversarial input written down next to the invariant.
3. **`STYLE.md` §13's trigger contradicted its own premise.** The section says conventions are
   settled when the work reaches them, yet the sink row said "before the lexer" — a guess about when
   the need would arrive, made before any error existed to report. D-033 moves it.

---

### 2026-09-23 — Test harness, argument contract, corpus layout, commit plan

**Did.**

1. Studied Nora Sandler's test suite and settled what Krama keeps (corpus layout, per-stage runs,
   accept/reject by exit status) and what it changes (content comparison at stage boundaries, rather
   than treating the compiler as a black box).
2. Scoped the branch now open as `m1/cli-args`: `main` takes exactly one argument, a file path.
   Reframed what is testable — the **argument contract**, not the dump, since the dump does not
   survive the lexer while the argument contract outlives every mode.
3. Settled exit codes (**D-035**): a small owned enum, `KRM_` prefix, success `0`, remaining classes
   from `2`, `1` left unused.
4. Settled the corpus (**D-036**) and the driver (**D-037**): one TOML manifest per stage,
   invocation explicit per case, expected dumps as sibling files, symbolic exit names, a Python
   driver using the standard library only. The last of these amends spec §11.1 — the first change to
   the spec since it closed.
5. Fixed the branch's commit plan: docs, unit-test moves, Makefile part A, exit-code header and
   `main`, then driver and fixtures. Added a `cli` scope to COMMITS.md, and tied `DECISIONS.md`'s
   `Session` field to the note number, which the 2026-09-19 entry had flagged as owed.

**Worked.**

1. Separating inputs from results. The first manifest sketch had `Actual` and `Pass/Fail` columns —
   a committed file the driver would rewrite on every run, dirtying the tree. Inputs only; results go
   to stdout.
2. Noticing that three of the six cases have no input file at all. "No argument", "two arguments"
   and "a directory as the argument" are defined entirely by their invocation, which is why a
   pairs-on-disk layout could not hold them and a manifest could.
3. Reading what `PROJECT.md`'s row actually asked. "Fold `all` into `check`" asks for **compilation**
   coverage — `main.c` is filtered out of the test build, so nothing compiles it — not for the
   production binary to be *run*. That dissolved an apparent need to invent something for the gate to
   invoke.
4. Checking the commit order against COMMITS.md Rule 1 before writing anything. The first plan put
   the driver before `main`, which would have failed `rebase --exec "make check"` at the merge gate.

**Didn't.**

1. **A `.expected` that duplicates the input.** With `main` dumping the file verbatim, the expected
   output *is* the input — two files kept in sync by hand, both dead when the lexer lands.
2. **Keeping a permanent dump mode** so the roundtrip stays testable. A flag whose only user is one
   test, proving something every lexer case proves implicitly.
3. **`--version` as the thing the gate runs.** Proves almost nothing and contradicts the
   one-argument contract just decided.
4. **`errno` values and `sysexits.h` as exit statuses** — D-035. Also **CSV** (quoting rules no
   shell can parse), **directory-inferred invocation** (renaming a folder would change what the tests
   do), and **a shell driver** (spec §11.1's original wording, amended by D-037).
5. **Collapsing commits 4 and 5** to remove the one-commit window where `main`'s new behaviour has
   no harness. The diff would be large and there would be nowhere to state the gap.

**Friction.**

1. **Exit code `1` is already taken.** A sanitized build exits `1` when ASan or UBSan fires
   (notes-09 §9.1). Had `1` also meant "rejected the input as designed", the harness could not have
   distinguished a correct rejection from a sanitizer catch, and the sanitized pass would have
   stopped being a check without anything failing. Found late, while numbering the enum.
2. **The catch-22 was only half seen.** "I cannot test `main` without the driver" is true; so is
   "the driver cannot be tested without `main`". Something has to be first, and the question is which
   untested window to accept — not how to avoid one.
3. **Two builds, one filename.** A sanitized `kramac` and a production `kramac` writing to the same
   path means the last build wins and the harness may silently run the wrong binary. Distinct output
   paths, and `clean` has to know about both.
4. **`krama-notes-12` closes with the pre-reorder milestone order** (arena init, AST nodes, sink,
   lexer). It was written in the same session as the reorder and did not pick it up. Notes are
   append-only records, so it stands — `krama-notes-11` supersedes it.

---

## Conventions

- **Dates, not session numbers**, in the log. Session numbers live in `DECISIONS.md`.
- **Reference decision IDs** where relevant: "chose flat array over linked list here, follows D-003."
- **Do not delete failures.** A dead end that took three hours is worth more in this file than the
  fix that eventually worked — the fix is in the code, the dead end is not recoverable from
  anywhere else.
- **Friction drives exercises.** Anything listed there twice is a candidate for a standalone workout
  in a non-Krama domain — arena allocation over an integer list, tagged unions over a toy JSON value.
  Keeping the domain separate keeps practice code out of the transpiler.
- **Update Current before pushing a repomix export.** It is the first thing read during review, and
  it is what prevents deliberate stubs from being flagged as bugs.


