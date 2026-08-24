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
| D-019 | Transpiler source targets C23 | Decided | 07 |
| D-020 | Emitted C standard | Deferred | 07 |
| D-021 | String interning for identifiers | Deferred | 07 |
| D-022 | Macros permitted for what only macros can do | Decided | 08 |
| D-023 | Assistant interaction rules live in CLAUDE.md | Decided | 09 |

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

### D-019 — Transpiler source targets C23

**Status:** Decided · **Session:** 07 · **Spec:** n/a (implementation, not language)

**Decided.** The transpiler is compiled with `-std=c23`, pinned explicitly in the Makefile.

**Rejected.**
- *C99.* Would require the negative-array-size idiom for compile-time assertions, `<stdbool.h>`,
  `#define`/`enum` workarounds for scalar constants, and `__attribute__` extensions for
  `noreturn`.
- *C11.* Gets `_Static_assert` and anonymous unions but not `constexpr`, keyword `static_assert`,
  fixed-underlying-type enums, or standard attributes.
- *Leaving the standard unpinned.* A compiler upgrade would then silently change language semantics
  and break build reproducibility — the same argument as pinning the clang-format version.

**Why.** C23's additions remove sharp edges the coding standards were otherwise written to work
around, rather than adding conveniences. Concretely: `constexpr` repeals the "const is not a
constant expression" wart; keyword `static_assert` needs no header; fixed-underlying-type enums
shrink high-volume struct tags; standard attributes replace compiler extensions; `bool` needs no
include.

**Consequences.**
- Toolchain floor: GCC 14+ or Clang 18+.
- STYLE.md §5.3, §5.4, §6.2, §6.4, §7.3, §8.1 are written against C23 and would need reverting
  if this changed.
- clangd and clang-format support for the newest syntax may lag; verify editor integration before
  relying on a feature.
- `-Wstrict-prototypes` loses its correctness role, since `f()` and `f(void)` are equivalent in C23.
- **Open verification:** fixed-underlying-type enums may weaken `-Wswitch` exhaustiveness checking.
  Test before adopting `enum TokenKind : uint8_t`. If the warning stops firing, keep the plain enum.

**Independent of D-020.** This governs only what compiles the transpiler. It places no constraint
on what the transpiler emits.

---

### D-020 — Emitted C standard

**Status:** Deferred · **Session:** 07 · **Spec:** §13 item 3

**Deferred.** No decision until a genuine fork appears in generated code.

**The question.** What standard must a *user's* compiler accept to build funC output? This is a
portability promise made to users and is unrelated to D-019.

**Expected forks.** `_Bool` / `<stdbool.h>`, anonymous structs and unions, `_Static_assert`.
`_Generic` is explicitly *not* a fork — spec §9.4 rules it out on other grounds.

**Why deferred.** Milestone 1 emits only `<stdint.h>`, `<stdio.h>`, `int32_t`, `float`, `uint8_t`,
`printf`, and the trap helpers. All of that is valid C99. Nothing forces the choice yet.

**Revisit when.** The first emitted construct that requires C11 or later. Likely trigger: `bool`
arriving in funC (phase 2), which raises `_Bool` versus an `int`-based encoding.

---

### D-021 — String interning for identifiers

**Status:** Deferred · **Session:** 07 · **Spec:** n/a (implementation)

**Deferred.** Milestone 1 keeps identifiers as `{ptr, len}` slices into the source buffer (D-004).
No intern pool.

**The question.** Should each unique identifier spelling get one canonical allocation, so that
comparing two identifiers is pointer equality rather than length-plus-`memcmp`?

**Rejected as a motivation.** Interning *keywords* up front to replace scan-time keyword
recognition. Spec §3.3 post-filters `IDENT` against 8 keywords — a length check and a handful of
`memcmp`s. Interning performs equivalent work under a different name, and the real win in that path
is already captured: once keywords are distinct `TokenKind` values, the parser compares integers.

**Why it will be worth doing later.**
- **Symbol lookup becomes pointer comparison.** The type checker resolves every `VarRef` against a
  scope; interned, that is `a == b`. With nested scopes, functions, and struct fields this is the
  checker's hot path.
- **Uniqueness becomes a structural invariant**, not a discipline. Tables keyed on names get a
  free perfect hash — the pointer itself.
- **Names acquire a home for attached data.** `is_keyword`, later `is_builtin`, a precomputed hash,
  and a cached mangled name all live on the entry. Range predicates over the token enum do not
  scale past keywords: `@print` / `@cast` / `@sizeof` are not a contiguous range of anything.

**Revisit when.** The type checker gains a real symbol table with nested scopes. That is the point
at which pointer-equality lookup stops being theoretical.

**Consequences when adopted.**
- Partially supersedes D-004. Slices remain — spans need them — but *identity* moves to the pool.
- **Spans must stay on the token, not on the interned entry.** One entry for `x`, but the `x` on
  line 4 and the `x` on line 9 need different spans. Easy to conflate.
- The arena (D-003) is the right substrate: interned strings never die. Hash buckets are the
  growable part and stay separately allocated.
- Adds a hash table and one allocation per unique spelling — structure the current design does not
  have.

**Target design.** Clang's `IdentifierTable`: one entry per unique spelling, keywords pre-populated
with their token kind stored on the entry, so scanning becomes "intern, then read the kind off the
result" with no separate keyword check at all. The argument for interning is not that it is faster
than what milestone 1 does — it is that it collapses two lookups into one.

**Reference.** *Crafting Interpreters* ch. 20, closing section (clox interns every string).
Clang Internals Manual, Lexer and Preprocessor Library. Lisp symbols / obarray as the idea in its
original form.

---

### D-022 — Macros permitted for what only macros can do

**Status:** Decided · **Session:** 08 · **Spec:** n/a (implementation) — STYLE.md §8.4

**Decided.** STYLE.md gains §8.4, naming the five jobs that have no non-macro spelling: include
guards and conditional compilation, `#`/`##`, call-site capture via `__FILE__` / `__LINE__` /
`__func__`, taking a type as an argument, and a value the preprocessor itself must read. A macro
doing one of these names which, in a comment; a macro that cannot name one should have been a
`static inline` function. §7.1's clause about `#define` gains a scope qualifier.

**Rejected.**

- *Leaving it unwritten.* No rule is smaller than a rule, and nothing in the guide actually banned
  macros. But it restricted `#define` in two places (§5.4, §7.1) and permitted it nowhere, and
  silence in a rules document is read as prohibition. §7.1's "textual, unscoped, untyped, and
  invisible in a debugger" is an argument about naming values; lifted out of context it reads as a
  general indictment, and out of context is how a rule gets read during review a year later.
- *A general prohibition with narrow exceptions.* The honest written form of what was previously
  assumed. Rejected because the premise does not hold: what C23 removed the need for is the
  macro-as-scalar-constant, and `static inline` had already displaced the macro-as-function long
  before that. Neither touches the five remaining jobs, so a prohibition would be a rule whose
  exceptions are its entire content.
- *`constexpr` tables as a way to avoid macro-built tables.* Considered directly, as an alternative
  route to the enum-to-string problem §7.6 defers X-macros for. **Tested and rejected on the
  language, not on taste:** C23 requires a `constexpr` object of pointer type to be initialized with
  a null pointer constant, so `constexpr const char *names[]` does not compile; the 2D form
  `constexpr char names[][N]` compiles but silently drops the NUL when a string exactly fills its
  row, with no warning under `-Wall -Wextra -Wpedantic`; and elements of a `constexpr` aggregate are
  not constant expressions, so there is no compile-time indexing to be had. `static const char
  *const` remains correct and is what §5.4 already blesses. Verified on GCC 13; re-verify on the
  GCC 14+ floor.
- *A new top-level section.* Placed where it belongs topically — near §5 or §7 — it would renumber
  sections that D-019 cites by number. Appended after §14 it would sit orphaned past the review
  checklist. §8.4 puts it beside §3.7 and §8.1, next to the thing a macro is most often mistaken
  for.

**Why.** The restriction was written from an assumption that macros are a footgun as a class. They
are not; two specific uses were, and both have replacements. The rule that actually discriminates is
not "avoid macros" but "name the job" — the five listed jobs have no alternative spelling, and
anything else does, which makes the list itself the test.

**Consequences.**

- §7.1 amended to carry its own scope. §5.4 and §7.1's substance are untouched: both concern the
  macro-as-constant, which stays replaced.
- The two items §13 defers to "before the lexer" — the assertion mechanism and the diagnostic sink —
  now have their permission in place. Both need the caller's `__FILE__` and `__LINE__`, which forces
  a macro; that no longer has to be relitigated while designing them.
- New review item: every macro's comment names its job. §14 checklist gains a line.
- STYLE.md 1.1 → 1.2.
- Does not reopen D-019. C23 is what makes `constexpr` available for the constants; the macro jobs
  listed here predate it and are unaffected by the standard target.

**Revisit if.** A sixth job appears that the list does not cover. The list is the rule, so a macro
nobody can file under one of the five is either a mistake or evidence the list is incomplete — and
which of those it is has to be argued, not assumed.

**Reference.** Gustedt, *Modern C*, which treats macros as ordinary tools with a naming discipline
rather than as a hazard. Linux's `__user` / `__iomem` and Microsoft's SAL annotations as the prior
art for annotation macros, and the reason they are worth little without a tool that reads them.

---
### D-023 — Assistant interaction rules live in `CLAUDE.md`

**Status:** Decided · **Session:** 09 · **Spec:** n/a (process, not language or implementation)

**Decided.** A root-level `CLAUDE.md` states how the assistant participates: a default of not
writing this project's code, an escalation ladder from prose to generic C, four named roles
(`/mentor`, `/teacher`, `/reviewer`, `/toolman`), and a single-response override (`/BURNOUT`). It
carries no language or style rules of its own; it points at the four existing documents and states
their precedence. Design deliberation stays outside the repository — `CLAUDE.md` governs the coding
sessions, not the decisions that feed them.

**Rejected.**

- *No file; paste the documents into each session.* Zero drift, since there is only ever one copy of
  each rule. Rejected on cost and on fidelity: the paste is lossy in practice — `PROJECT.md`
  **Current** is the thing most often left out, and it is the thing that prevents a deliberate stub
  from being reported as a bug.
- *One instruction — "act as a mentor" — with no roles.* Less machinery, and machinery that is never
  used is worse than none. Rejected because the modes want opposite defaults: mentoring withholds
  the answer to make it be derived, review withholds the answer to make it be found, and teaching
  gives the answer and then tests it. A single mode cannot be all three, and in practice collapses
  to whichever the last message sounded like.
- *Making `CLAUDE.md` self-contained by restating the spec and STYLE rules inside it.* Genuinely
  attractive: the file would work standing alone, with no assumption about what else got read.
  Rejected because two documents asserting the same rule diverge on the first amendment to either,
  and the divergence is found by whoever follows the stale copy. This is the failure the precedence
  clause at the top of this file already exists to prevent; adding a fifth authority would make that
  clause harder to state, not easier.
- *A blanket prohibition on generated code with no override.* Cannot be eroded, which is its whole
  merit. Rejected because a rule with no relief valve is abandoned wholesale under pressure rather
  than suspended for one response — and the abandonment is silent, where a `/BURNOUT` is a legible
  event. It also discards the signal: the request marks fatigue, which is information worth having.
- *Permitting test code to be generated,* on the grounds that tests are scaffolding around the
  artifact rather than the artifact. Rejected: spec §11 makes the test strategy a first-class part
  of the project, and the differential harness in §11.2 is a harder design problem than most of the
  transpiler. Handing it over would remove the most instructive work in milestone 1.

**Why.** The scarce resource in this project is not correct code — that is a search away — but the
experience of producing it. A rule that only says "teach, don't tell" is unenforceable because every
individual snippet looks justifiable in the moment. The ladder makes the concession ordinal instead:
each rung must be earned, so drifting to finished code requires visibly skipping steps rather than
just being helpful. The roles then exist because withholding is correct in three different ways and
naming which one is in force is cheaper than inferring it every turn.

**Consequences.**

- `STYLE.md` §3.1's layout gains `CLAUDE.md`; STYLE.md 1.3 → 1.4.
- Any language or style rule that appears in `CLAUDE.md` is a defect in `CLAUDE.md`, not a second
  opinion. It is a behaviour file and holds no authority over the code.
- Two venues, deliberately: design argument outside the repository, coding sessions inside it. A
  decision reached in a coding session is not decided until it lands here as an entry.
- Adding a role or a rung is an edit to `CLAUDE.md` **and** a superseding entry here. The set is
  small on purpose.
- `/toolman` is scoped to `tools/`, which makes `STYLE.md` §3.1's stdlib-only rule load-bearing for
  a second reason — it now also bounds what the assistant may write.
- The `technique-index` skill's guardrails are incorporated by reference rather than restated, for
  the same reason the spec is.

**Revisit if.** The project stops being primarily a learning exercise — at that point the ladder is
pure cost and should be dropped rather than tuned. Or if a fifth role is wanted twice, which is
evidence the four are cutting the space wrongly rather than that one is missing.

**Reference.** The `technique-index` skill's own guardrails ("name it; do not build it", pseudocode
first) as prior art for the same constraint arrived at independently.

---

### D-024 — Arena initial capacity is fixed, not caller-supplied

**Status:** Decided · **Session:** ## · **Spec:** n/a (implementation) — refines D-003

**Decided.** `arena_create` takes no capacity parameter — `struct Arena *arena_create(void)`. The
arena always starts its first block at a fixed internal baseline (`BLOCK_INIT_CAP`) and grows by
the doubling-then-chaining policy STYLE.md §9.2 already states: double until `BLOCK_MAX_CAP`, then
chain fixed-size blocks.

**Rejected.**
- *Caller-supplied initial capacity, sized off the source file's byte length.* Attractive because
  `main` is the one place in the pipeline that actually holds a concrete number. Rejected because
  milestone 1's arena holds AST nodes specifically, and byte count doesn't predict node count — token
  density, expression complexity, comments, and whitespace all move the ratio, so the hint would be
  noise wearing a signal's clothes.
- *Today's unenforced caller-supplied capacity.* `arena_create` currently takes any `size_t` and
  `malloc`s it directly for the first block, with no ceiling check — `BLOCK_MAX_CAP` is only enforced
  inside `add_block_`, so a caller can bypass it entirely on the very first allocation. Rejected as
  inconsistent: a limit meant to bound every block in the chain didn't bound the first one.

**Why.** No phase of the pipeline currently has a principled number to offer, so a parameter that
exists "for later" is an unused knob and a place for the `BLOCK_MAX_CAP` bypass to keep hiding.
Removing it at the type level is simpler than validating an input nothing can supply correctly yet.

**Consequences.**
- `arena_create(size_t capacity)` → `arena_create(void)`, changing every call site, including
  `tests/test_arena.c`'s `arena_create(request_cap)`.
- `BLOCK_MAX_CAP` moves out of the unconditional part of `arena.h` — nothing in the public contract
  takes it as input or returns it, so per §3.6's reasoning it belongs behind `FUNC_TEST_SUITE`
  alongside the existing test-only accessors, not in the ordinary header.
- A new constant for the fixed starting size, named consistently with `BLOCK_MAX_CAP` per §2.4's
  `_cap` suffix — `BLOCK_INITIAL_CAP`.

**Revisit if.** A later phase gains a genuine sizing signal for its own arena usage — e.g. a symbol
table sized off scope count. That would argue for a hint parameter on *that* phase's arena use, not a
return to caller-supplied capacity for the AST arena.

**Reference.** STYLE.md §9.2 (growth policy this formalizes); D-003 (arena allocator decision this
refines).

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
