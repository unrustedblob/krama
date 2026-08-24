# funC — Project State

> **Current** is overwritten each update. **Log** is append-only, newest last.
> Read Current for where things stand; read the Log tail backwards to rewind.

---

## Current

**Milestone:** 1 single `main`, three scalar types, arithmetic, `@print`

**Working on:** Arena — iteration 1 complete, unit test suite in progress

**Blocked on:** -

**Last green:** Builds clean under all flags; arena iteration 1 compiles and links

**Next up:** Finish the arena test suite. Then decision records for the arena's implementation
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


