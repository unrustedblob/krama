# funC — Project State

> **Current** is overwritten each update. **Log** is append-only, newest last.
> Read Current for where things stand; read the Log tail backwards to rewind.

---

## Current

**Milestone:** 1 — single `main`, three scalar types, arithmetic, `@print`
**Working on:** —
**Blocked on:** —
**Last green:** — *(what builds and passes right now)*
**Next up:** —

**Deliberately incomplete** — *stubs and placeholders, so they are not reported as bugs:*

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

### YYYY-MM-DD — [short title]

**Did.** What was built or changed.

**Worked.** What is now verifiably functioning, and how it was verified — tests passing, manual
run, sanitizer clean.

**Didn't.** What was attempted and abandoned, and why. Dead ends, approaches that turned out wrong,
things that compiled but behaved unexpectedly. *Do not skip this field.* It is the most useful
signal in the document.

**Friction.** Concepts that felt shaky rather than merely unfamiliar. C-specific stumbling —
pointer arithmetic, alignment, ownership, string handling, macro behavior, build errors that took
too long to decode.

**Stubbed.** Anything left deliberately incomplete this session. Mirror it into the Current table
above; remove it from there when filled in.

**Next.** The single next thing.

---

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
