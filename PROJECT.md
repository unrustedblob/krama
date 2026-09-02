# funC — Project State

> **Current** is overwritten each update. **Log** is append-only, newest last.
> Read Current for where things stand; read the Log tail backwards to rewind.

---

## Current

**Milestone:** 1 single `main`, three scalar types, arithmetic, `@print`

**Working on:** Arena (iteration 1) testing complete and fatal error module in place

**Blocked on:** -

**Last green:** Tests clean under all flags; arena iteration 1 compiles and links

**Next up:** Get it reviewed. Initialize in main. Then decision records for the arena's implementation
choices, then AST node definitions.

**Deliberately incomplete** —

| Location | State | Intent |
|---|---|---|
| `arena.c` — block size | Round starting number, not derived from node size | Revisit once `struct ASTNode` exists and its size is known |
| `arena.c` — mark/release | Absent | Nothing in milestone 1 reclaims early; the AST lives to teardown. Additive when multi-file arrives |
| `arena.c` — alignment | Fixed at `alignof(max_align_t)` | Per-type alignment is a later refinement, STYLE.md §9.2 |
| `arena.c` — ASan poisoning | Absent | `ASAN_POISON_MEMORY_REGION` on block creation, unpoison per allocation. Planned follow-up, STYLE.md §9.2 |

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

**Did.** Consolidated design sessions 01–06 into `funC-spec-v1.md`. Established `DECISIONS.md` and
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
| 1 | [#1](https://github.com/unrustedblob/funC/issues/1) | Heap overread as `buf` was not `NUL` terminated | `buf` is allocated `file_sz + 1` bytes and last byte is set to  `NUL` |
| 1 | [#2](https://github.com/unrustedblob/funC/issues/2) | Missed `goto cleanup_file` on `fseek` failure | Included `goto` |
| 2 | [#3](https://github.com/unrustedblob/funC/issues/3) | Setting `buf[file_sz]` to `NUL` before checking `malloc` status | Moved it after the `malloc` check. Consequence of "rushed update" related to bug:[#1](https://github.com/unrustedblob/funC/issues/1) |
| 2 | [#4](https://github.com/unrustedblob/funC/issues/4) | Incorrect format string `%ld` used for `size_t` | Missing `-Wformat-signedness` flag. Updated to `%zu`, the correct format string. **Note** Review claimed -Wformat alone would catch it, that was wrong; identified by leaving the incorrect string in to test it. |
| 3 | N/A - Style | No `void` in `main()` as parameter | C23 standard allows this, but as stated in STYLE.md - 8.1, `void` is kept for consistency. Updated parameter `void` |
| 3 | [#5](https://github.com/unrustedblob/funC/issues/5) | Empty file reported as error `malloc(0)` can return `NULL` | Fixed incidentally by adding 1 to the `file_sz` when calling `malloc` |

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
over a `static` buffer, outside funC — per the friction convention below.

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
| 1 | [#6](https://github.com/unrustedblob/funC/issues/6) | Program aborts when passing allocation request with exact fit | The contraint was updated to handle the equal scenario as well |


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

##1 2026-08-30 — Arena iteration 1, Unit Testing Complete

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
### 2026-09-02 — Arena implementation and unit testing complete

**Did.**

1. Arena API tested, fatal-error module ready

**Worked.**

1. Command-query separation in `allocate`. `compute_padding` returns a number and `move_cursor` is the only mutator, so alignment became a property of the call sequence rather than something each branch had to remember. This came from re-reading the code rather than from the review — the review only named it afterwards.
2. Design questions were answered before code was written, in dependency order: ownership, then what the AST asks of the allocator, then the API. Each answer constrained the next instead of being revisited.
3. Sanitizers found what the tests could not. Every block was leaking and all 34 tests passed; ASan reported it immediately. §9.2's warning that ASan cannot see arena bugs is true and separate — it sees block bookkeeping fine.**Didn't.**
4. Allocate-or-die paid for the block layout. Two allocations per block create a partial-failure state, but since OOM exits there was no unwind path to write.
5. Reading before implementing on alignment. The study list and hand-worked wrap case came first; the formula was understood rather than copied.

**Didn't.**

| Review Pass | Issue # | Bug | Resolution |
|---|---|--- | ---|
| 2 | [#7](https://github.com/unrustedblob/funC/issues/7) | Memory leak when adding block | Identified by reviewer, included `fsanitize` flags in Makefile. The issue was in prepending the new block, instead of pointing the new block to the current one, it pointed to `->next`, which was null - fixed |
| 2 | [#9](https://github.com/unrustedblob/funC/issues/9) | The OOM path prints identifiers | The `FATAL` macro printed the condtion on the `FATAL_EXIT_PATH` which is meant for system/env issues. Fix, restricted the condition printing to `FATAL_PATH_ASSERT` |
| 2 | [#10](https://github.com/unrustedblob/funC/issues/10) | `assert` instead of `static_assert` | Using a runtime check for somthing that could be validated at comile time. Fix, used `static_assert` |
| 2 | [#11](https://github.com/unrustedblob/funC/issues/11) | `assert` for internal invariants in `allocate` and `move_cursor` |  |
| 2 | [#12](https://github.com/unrustedblob/funC/issues/12) | `GROWTH_FACTOR` defined but not used | Replaced hardcoded numbers with `GROWTH_FACTOR` |
| 2 | [#13](https://github.com/unrustedblob/funC/issues/13) | Assumptions fail when `align` becomes a param | Arena interface currently aligns to `max_align_t` but the future plan is to pass in the required alignment. Enahancements: <br> 1.  `mak_align` is now replaced with `compute_padding` - this allows us to localize the cursor moving to just `move_cursor` <br> 2. `allocate` now checks and validates alignment, computes padding, checks if a grow is required and simply moves the cursor once by padding and once by requested size <br> 3. `add_block` checks for enough space including maximum possible padding bytes for a request|
| 2 | [#15](https://github.com/unrustedblob/funC/issues/15) | Handle `allocate(arena, 0)` | Allocation requests of 0 bytes are treated as `FATAL` and abort |

**Friction.**

1. The growth condition took three attempts. First version used `&&`, which made one conjunct permanently true and, in the case it appeared to guard, skipped growth and aborted instead. Second version split it into two `if`s, which fixed the gap but could grow twice for one allocation and left the dead conjunct in place. Third version — one subtractive test with `||` — was correct and shorter than both. The lesson is that the first two both looked like they handled the padding case; only tracing the false branch showed otherwise.
2. Padding accounting circled for several exchanges. Whether to fold `align - 1` into the request or subtract it from the ceiling — both work, doing both double-counts. Partly self-inflicted, partly because the review introduced `MAX_ALIGNMENT` mid-discussion without restating where the other checks then belonged. Worth pinning the frame before iterating next time.
3. The sanitizer build failed to link. `-fsanitize=address` was on the compile line but not the link line; ASan's runtime is pulled in by the driver at link time. Cost an unexplained wall of undefined `__asan_*` references.

**Stubbed.** See the *Deliberately incomplete* table. Mark/release, per-type alignment, and ASan
poisoning of arena memory are all absent by decision, not by oversight.

**Next.**

1. Get it reviewed.
2. Decision records for the arena's implementation choices — no capacity parameter, separate block
   and data allocations over a flexible array member, pointer cursor with a `size_t` companion.
3. AST node definitions.

---

## Conventions

- **Dates, not session numbers**, in the log. Session numbers live in `DECISIONS.md`.
- **Reference decision IDs** where relevant: "chose flat array over linked list here, follows D-003."
- **Do not delete failures.** A dead end that took three hours is worth more in this file than the
  fix that eventually worked — the fix is in the code, the dead end is not recoverable from
  anywhere else.
- **Friction drives exercises.** Anything listed there twice is a candidate for a standalone workout
  in a non-funC domain — arena allocation over an integer list, tagged unions over a toy JSON value.
  Keeping the domain separate keeps practice code out of the transpiler.
- **Update Current before pushing a repomix export.** It is the first thing read during review, and
  it is what prevents deliberate stubs from being flagged as bugs.


