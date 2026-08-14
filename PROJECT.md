# funC — Project State

> **Current** is overwritten each update. **Log** is append-only, newest last.
> Read Current for where things stand; read the Log tail backwards to rewind.

---

## Current

**Milestone:** 1 single `main`, three scalar types, arithmetic, `@print`

**Working on:** Arena

**Blocked on:** -

**Last green:** Builds clean under all flags; reads and dumps test.fc

**Next up:** Arena and associated interface.

**Deliberately incomplete** — 

| Location | State | Intent |
|---|---|---|
| — | — | — |

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


