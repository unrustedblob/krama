# funC — Decisions Index

Rationale record. The **specification** (`funC-spec-v1.md`) says what funC *is*; this document says
why, what was rejected, and when to reopen.

**Precedence:** where this document and the spec disagree, **the spec wins**. If a decision changes,
edit the spec *and* add a new entry here that supersedes the old one. Never edit a decided entry in
place.

**IDs** are permanent and never reused. `D-###` in commit messages and code comments points here.

---

## Index

| ID | Decision | Status | Session |
|---|---|---|---|
| D-001 | Transpile to C, not to machine code or IR | Decided | 01 |
| D-002 | Build an AST rather than syntax-directed translation | Decided | 01 |
| D-003 | Arena allocator for the AST | Decided | 01 |
| D-004 | String slices into one source buffer | Decided | 01 |
| D-005 | Spans on every token | Decided | 01 |
| D-006 | Verification scope: semantics + interpreter + properties | Decided | 01 |
| D-007 | No implicit conversion anywhere | Decided | 02 |
| D-008 | Type-directed `@print`, not fixed format specifiers | Decided | 02 |
| D-009 | Cascading recursive descent over Pratt | Decided | 02 |
| D-010 | Iterative EBNF plus a separate associativity table | Decided | 02 |
| D-011 | Fixed-width types, defined by spec not host | Decided | 03 |
| D-012 | `#` line comments | Decided | 04 |
| D-013 | `/` has no integer signature | Decided | 04 |
| D-014 | Euclidean division for `//` and `%` | Decided | 05 |
| D-015 | Trap on integer division faults | Decided | 05 |
| D-016 | `@` prefix denotes the intrinsic namespace | Decided | 05 |
| D-017 | Single unary minus, not recursive | Decided | 06 |
| D-018 | Milestone restrictions live in the checker, not the grammar | Decided | 06 |

**Status values:** `Decided` · `Deferred` · `Superseded by D-###` · `Reopened`

---

## Entries

### D-014 — Euclidean division for `//` and `%`

**Status:** Decided · **Session:** 05 · **Spec:** §7

**Decided.** `//` and `%` yield the unique `(q, r)` with `a = qb + r` and `0 ≤ r < |b|`. The
remainder is always non-negative.

**Rejected.**
- *Truncated* (C's native behavior). Free to emit — no helper at all — but `-7 % 2 = -1`, so the
  result cannot be used as an index without a guard.
- *Floored* (Python). Remainder carries the divisor's sign. Agrees with Euclidean whenever the
  divisor is positive; diverges only on negative divisors.

**Why.** The division algorithm from number theory is the Euclidean one, and a non-negative
remainder makes `i % n` safe as an array index unguarded — the case where truncation actually bites.
Cost is identical to floored: both need a helper, both are a few lines.

**Consequences.**
- Requires a runtime helper; `//` and `%` are not emitted as native C operators.
- The helper must not form `|b|` directly — that overflows at `INT32_MIN`.
- The differential interpreter must implement this independently (D-006), not call the helper.
- Uniqueness of `(q, r)` means property tests fully characterize correctness for these operators.

**Revisit if.** Signed modulo appears in a measured hot path and the helper's branch costs something
real. Note that reversing this is a **breaking language change**, not an optimization.

**Reference.** Boute, *The Euclidean definition of the functions div and mod*.

---

### D-009 — Cascading recursive descent over Pratt

**Status:** Decided · **Session:** 02 · **Spec:** §10.2

**Decided.** One parser function per precedence level, mapping one-to-one onto the precedence table.

**Rejected.** Precedence climbing / Pratt parsing — one function driven by a binding-power table.

**Why.** The EBNF-to-code correspondence is literal and checkable by eye, which serves the
verification aim directly. funC's operator set is deliberately small, so the extra functions are
affordable.

**Consequences.**
- Each new precedence level costs a function and a rewiring of the chain.
- Adding an operator mid-chain means editing two functions.

**Revisit if.** Comparison, equality, and logical operators push the count past roughly eight
functions. **Planned migration, with a twist:** when refactoring to Pratt, *keep the old parser* and
differential-test the two on randomly generated expressions, asserting identical ASTs. A
verification harness falls out of a refactor that was happening anyway.

---

### D-0XX — [Template]

**Status:** Decided · **Session:** ## · **Spec:** §#

**Decided.** One or two sentences. What the rule now is.

**Rejected.** Each alternative that was seriously considered, with its actual merit stated — not a
strawman. If nothing was rejected, this was not a decision worth an entry.

**Why.** The argument that settled it.

**Consequences.** What this forces elsewhere. Downstream work created, constraints imposed on other
components, things now unrepresentable.

**Revisit if.** The condition that would justify reopening. If there is genuinely none, write
"Permanent — reversing this is a breaking language change."

**Reference.** Prior art, papers, or implementations. Optional.

---

## Notes on use

- **Entries are append-only once `Decided`.** To change one, add a new entry and set the old one's
  status to `Superseded by D-###`. The trail matters more than tidiness.
- **Not everything needs an entry.** If there was no real alternative, it is a spec fact, not a
  decision. Aim for entries where a reasonable person could have chosen otherwise.
- **"Revisit if" is the field that earns its keep.** It converts a decision from permanent to
  conditional, which most of them are. Spec §4.3 (`return` as block terminator) is a decision with
  an explicit trigger — that trigger belongs here.
- **Reference IDs from code** where a non-obvious implementation choice traces back to one:
  `/* Euclidean adjustment, see D-014 */`.
