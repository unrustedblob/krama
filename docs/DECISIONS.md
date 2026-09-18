# Krama — Decisions Index

Rationale record. The **specification** (`krama-spec-v1.md`) says what Krama *is*; this document says
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
| D-024 | Arena initial capacity is fixed, not caller-supplied | Decided | ## |
| D-025 | `CHECK` stays a macro until the lexer tests land | Deferred | ## |
| D-026 | `arena_reset` deferred past milestone 2 | Deferred | ## |
| D-027 | Block header and buffer are separate allocations | Decided | ## |
| D-028 | Pointer cursor with a `size_t` companion, not an offset | Decided | ## |
| D-029 | Zero-size allocation requests abort | Decided | ## |
| D-030 | Short-lived local branches, merged with `--no-ff` | Decided | ## |
| D-031 | Commit scope is optional; issue numbers go in trailers only | Decided | ## |
| D-032 | The language is named Krama; `.krm`, `kramac` | Decided | ## |

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

### D-025 — `CHECK` stays a macro until the lexer tests land

**Status:** Deferred · **Session:** ## · **Spec:** n/a (test suite) — STYLE.md §8.4

**Deferred.** `CHECK` in `tests/test_arena.c` remains a function-like macro. It does not currently do
any of the five jobs STYLE.md §8.4 lists, so §8.4 as written says it should have been a function.
The exception is deliberate and time-boxed: the question is reopened when the lexer tests give
`CHECK` a second consumer.

**Rejected.**

- *Convert it to a function now.* Correct under §8.4 as the rule stands, and cheap: the counter is
  taken by name and mutated, which a `struct TestCounter *` parameter does identically, and the
  printf-style arguments would go through `vfprintf` exactly as `fatal` already does. Rejected as
  premature — with a single consumer there is no evidence about what the interface wants, and the
  conversion costs the same whenever it happens.
- *Keep it as a macro and add `#cond` to justify it.* Would settle the question permanently in the
  macro's favour and would genuinely improve failure output, since a red test would print the source
  text of the condition alongside its message. Rejected because it inverts §8.4's test: the rule is
  *name the job the macro does*, not *find a job for the macro already written*. If the failure
  output wants the condition text, that should be decided on its own merits and the macro then
  justified by it — not the reverse.
- *Leave the macro uncommented.* The §14 checklist now requires every macro to name its job, and
  silence would read as an oversight a year from now rather than as a deferral with a trigger.

**Why.** §8.4's discriminator is whether the macro does work that has no non-macro spelling. `CHECK`
currently does not, which makes converting it the default answer. But the test suite has exactly one
consumer, and one call site is not enough evidence about what an interface wants — the shape that
looks obvious with one caller is routinely wrong with two. The lexer tests are the second consumer
and arrive soon enough that waiting costs a comment rather than a milestone. Deferring with a named
trigger is cheaper than converting now and converting back.

**Consequences.**

- `CHECK` carries a §8.4 comment stating that it does not yet name a job, written as
  `// TODO(D-025): ...` per §10.4 so the deferral is traceable rather than looking like a lapse.
- A documented exception to §8.4 now exists. It is an exception, not an amendment — §8.4 is
  unchanged and no other macro inherits this latitude.
- The question at the trigger is narrow and already framed: **does the failure output want the
  condition's source text?** If yes, `CHECK` is a macro permanently and its comment names
  stringification. If no, it becomes an ordinary function taking `struct TestCounter *`.
- This lands in the same conversation as STYLE.md §13's deferred testing conventions — corpus
  layout, `.func` / `.expected` naming, driver contract — which have the same trigger.

**Revisit when.** The lexer tests land and `CHECK` has a second consumer.

**Reference.** D-022, which established the name-the-job test this entry is a temporary exception to.

---

### D-026 — `arena_reset` deferred past milestone 2

**Status:** Deferred · **Session:** ## · **Spec:** n/a (implementation) — refines D-003

**Deferred.** The arena has no reset operation. Nothing in milestone 1 reclaims arena memory early:
the AST is allocated during parsing and read by the type checker, the code generator, and the
differential interpreter, so no phase boundary exists at which any of it becomes dead. The
declaration currently in `arena.h` is removed rather than kept as a no-op stub.

**The question.** Should the arena be able to return to an earlier point — releasing everything
allocated since — without being destroyed and recreated? The general technique is mark/release: save
the current block and cursor as a value, restore them later, and either free or retain the blocks
chained since. Resetting to the arena's base is the degenerate case where the mark is the beginning.

**Rejected.**

- *Ship it as a no-op returning `nullptr`.* Its current form. Genuinely tempting because the API then
  looks complete and the signature is settled early. Rejected because a public function that accepts
  a call and silently does nothing is worse than an absent one: a caller who believes memory was
  reclaimed gets no error, no diagnostic, and no crash — just an arena that keeps growing. An
  absent function is a compile error, which is the correct outcome for calling something that does
  not exist yet.
- *Implement reset now, since it is only a few lines.* True for the reset-to-base case: restore the
  cursor to the first block's buffer, and free or retain the rest of the chain. Rejected because
  neither branch of that choice can be made without a caller. Freeing the chain is right if resets
  are rare; retaining it for reuse is right if they are frequent and the same sizes recur — and
  nothing currently generates either pattern to decide from.
- *Implement full mark/release rather than reset.* The more general and more useful operation, and
  the one the eventual callers probably want. Rejected on the same ground and more strongly: it adds
  a lifetime rule the plain arena had abolished, where a pointer's validity depends on a scope
  invisible at the use site — and STYLE.md §9.2 already records that ASan cannot see arena bugs,
  since nothing is freed. Adopting that hazard before anything needs it is the wrong order.

**Why.** The arena's bargain is allocate-never-free-teardown-once, and every milestone 1 allocation
lives to teardown. A reclaim operation with no caller cannot be designed, only guessed at — the two
open sub-questions (free the chain or retain it; reset-to-base or a general mark) both have answers
that depend entirely on the access pattern of the code that will call it. Waiting costs nothing,
because adding the operation later changes no existing signature and invalidates no existing
pointer.

**Consequences.**

- `arena_reset` is removed from `arena.h` and `arena.c`. There is no stub.
- `PROJECT.md`'s *Deliberately incomplete* table carries the absence, so review does not report it
  as missing.
- The arena's public surface for milestone 1 is exactly `arena_create`, `arena_alloc`,
  `arena_destroy`, plus the `FUNC_TEST_SUITE` accessors.
- When this is reopened, mark/release should be evaluated alongside plain reset rather than after
  it. Reset-to-base is a special case of mark/release, so implementing reset first and generalising
  later means writing the block-disposal policy twice.

**Revisit when.** A phase acquires arena memory that provably dies before teardown. The two expected
triggers, neither in milestone 2: **multi-file compilation**, where each file's AST dies once that
file's output is written, making per-file reset the natural unit; and **code generation**, if it
accumulates working structures per function that are dead once that function is emitted. Neither is
speculative-but-vague — each has an identifiable point at which the memory is known dead, which is
exactly the evidence this entry is waiting for.

**Reference.** D-003 (the arena decision this refines); STYLE.md §9.2 (allocate, never free, one
teardown at exit). Ryan Fleury, *Untangling Lifetimes: The Arena Allocator*, for the mark/release
form and the scratch-arena pattern it enables.

---

### D-027 — Block header and buffer are separate allocations

**Status:** Decided · **Session:** ## · **Spec:** n/a (implementation) — refines D-003

**Decided.** Each block costs two allocations: one for `struct Block_` and one for the buffer it
describes, with the block holding a `buffer` pointer. The arena therefore performs `1 + 2N`
allocations for `N` blocks — one for the header, two per block.

**Rejected.**

- *Flexible array member — one allocation per block, with the buffer trailing the header.* The
  better design on every count that can be counted: `1 + N` allocations, no stored `buffer` pointer,
  header and data contiguous so reading `available` and writing at the cursor touch one cache line
  rather than two, and no partial-failure state where the header succeeded and the buffer did not.
  Rejected for iteration 1 on legibility, not on merit. Keeping the metadata in a separate heap
  object means an overrun of the arena's allocatable memory corrupts a buffer rather than the block
  structure describing it, which is worth something while the allocator is being written for the
  first time. The FAM form also needs `alignas(max_align_t)` on the trailing array — otherwise the
  data begins at `sizeof(struct Block_)` from a `malloc`'d address, which is 8-aligned but not
  16-aligned — and that subtlety is one more thing to hold at once.
- *A single allocation sized `sizeof(header) + cap` with the buffer pointer computed by hand.* The
  FAM without the language feature. Rejected outright: it has the FAM's alignment subtlety plus
  manual pointer arithmetic the compiler would otherwise do, and C23 has the feature.

**Why.** The extra allocation is per *block*, not per node — with a block sized for a few thousand
nodes, a realistic milestone 1 program performs three `malloc`s in total. The cost is invisible
against the work of transpiling, and the choice is reversible for free: block layout is private to
`arena.c`, so `arena_create` / `arena_alloc` / `arena_destroy` are untouched by a later change and
no call site knows. That asymmetry is the whole argument — a wrong answer here costs one function,
where a wrong answer on the handle-versus-singleton question would have cost every caller.

**Consequences.**

- `1 + 2N` allocations and frees. `arena_destroy` frees the buffer and then the block, per link.
- 8 bytes per block for the stored `buffer` pointer. Per block, so negligible.
- The partial-failure state the two-allocation form creates costs nothing, because the
  allocate-or-die policy exits rather than unwinding — there is no cleanup path to write. The error
  decision paid for the layout decision.
- Block base alignment comes from `malloc`, which guarantees `alignof(max_align_t)`. The FAM form
  would have had to establish that itself.

**Revisit when.** The milestone 2 arena revision, where this is expected to change together with
D-028 — the FAM and the offset cursor are natural partners, since a buffer at a known offset from
the header makes `size_t` bookkeeping the obvious representation.

**Reference.** D-003 (the arena decision this refines); C23's flexible array member rules.

---

### D-028 — Pointer cursor with a `size_t` companion, not an offset

**Status:** Decided · **Session:** ## · **Spec:** n/a (implementation) — refines D-003

**Decided.** The arena tracks its position as an `unsigned char *cursor` into the current block,
paired with a `size_t available`. Every decision — fit tests, padding — is arithmetic on
`available`; the pointer only ever moves by an amount already validated. `move_cursor` is the sole
mutator of both.

**Rejected.**

- *A pure `size_t` offset from the block's base, with no cursor pointer.* Safer by construction:
  every value is bounded by `cap`, so overflow is unreachable rather than merely avoided; no
  `uintptr_t` cast is needed anywhere; and no pointer exists until the final line, which removes the
  possibility of forming a pointer past one-past-the-end — itself undefined behaviour in C, whether
  or not it is dereferenced. Rejected because the pointer form is the one that could be reasoned
  about confidently at this point in the project. A representation whose maintainer can picture it
  is worth more in iteration 1 than one that is safer on paper and opaque in the head.
- *Pointer plus an `end` pointer, computing `end - cursor` at each test.* One fewer field and no
  duplicated state. Rejected because the subtraction is the fit test anyway, and naming the
  remaining span makes the division of labour explicit: the pointer answers *where*, the `size_t`
  answers *how much*.
- *All three — `cursor`, `end` and `available`.* The original sketch. Rejected because `end` and
  `available` are derived from each other, and duplicated state that must agree is what drifts.

**Why.** The hybrid keeps the offset form's arithmetic safety while keeping the pointer's mental
model. Every comparison is unsigned and subtractive — `size > available - padding`, never
`used + size > cap` — so nothing can wrap. The single cast to `uintptr_t` in `compute_padding` is a
*read* of the address to obtain a number, not arithmetic producing a pointer, so the
pointer-to-integer-to-pointer round trip that STYLE.md §9.2 warns about never happens.

**Consequences.**

- Fit tests must be written subtractively, and in an order where the first test licenses the second:
  `padding > available || size > available - padding`. The short-circuit is load-bearing, not
  stylistic — reversing the operands lets the subtraction underflow before the guard runs.
- `cursor` and `available` are two views of one fact and nothing enforces their agreement. That is
  the cost of the hybrid, and the reason `move_cursor` is the only function permitted to change
  either.
- Padding is computed, never rounded up (STYLE.md §9.2). `compute_padding` is a pure query taking
  `const struct Arena *`; the separation of the computation from the move is what makes the
  alignment invariant provable rather than argued, after an earlier version that both computed and
  moved produced two defects in a row.
- `compute_padding` depends on `align` being a power of two, which is checked on entry to
  `allocate`, and on `align <= BLOCK_MAX_ALIGNMENT`, which bounds `align - 1` for every downstream
  subtraction.

**Revisit when.** The milestone 2 arena revision, alongside D-027. With a flexible array member the
buffer sits at a fixed offset from the block header, at which point the offset representation is
both safer and no less legible — which is the condition that decided this entry.

**Reference.** STYLE.md §9.2 (compute the padding, do not round the address up); CERT C INT30-C on
unsigned wraparound.

---

### D-029 — Zero-size allocation requests abort

**Status:** Decided · **Session:** ## · **Spec:** n/a (implementation)

**Decided.** `arena_alloc(arena, 0)` is a transpiler bug. It reports through `FATAL_PATH_ABORT`
rather than returning. The precondition — `size` must be greater than zero — is documented in
`arena.h`, so it is part of the API contract rather than an implementation quirk.

**Rejected.**

- *Permissive: return the current cursor.* What the code did before this entry, and genuinely
  defensible — the pointer returned is valid, aligned and non-null, which avoids `malloc(0)`'s real
  problem of possibly returning `NULL` and being misread as failure. Rejected because two zero-size
  requests return the *same* address. Two callers each believe they own a distinct object and
  silently share storage, with no crash and nothing for AddressSanitizer to see, since arena memory
  is never freed (STYLE.md §9.2). The failure mode is not the call itself but the next one.
- *Return `nullptr` for a zero-size request*, mirroring what `malloc` is permitted to do. Rejected
  because `arena_alloc`'s contract is that it never returns null and callers therefore do not check
  (STYLE.md §9.6). Adding a single null-returning case reintroduces a check at every call site, to
  serve a condition that is a bug in the caller anyway.
- *Treat it as a diagnostic rather than an assertion.* Rejected on STYLE.md §11's split: a
  diagnostic means the *input program* is wrong. No funC source can cause a zero-size arena request;
  only a defect in the transpiler can.

**Why.** Nothing in milestone 1 has a legitimate reason to request zero bytes. Every way the
request can arise is a symptom of something else — a node count that came out zero, a length that
underflowed, a `count * sizeof(...)` where `count` was empty. Aborting turns all of those into a
stack trace at the point of the mistake instead of a shared pointer discovered three phases later.
Strictness is also cheap here in a way it would not be in a library: every caller is internal and
known.

**Consequences.**

- `arena.h` carries the precondition; a caller does not have to read `arena.c` to learn it.
- The abort path is currently unverified. The in-process unit suite cannot test a path that ends the
  process, and a process-per-case driver does not exist yet — recorded in `PROJECT.md`'s
  *Deliberately incomplete* table rather than left implicit.
- This is the first `FATAL_PATH_ABORT` reachable from a caller's arguments rather than from an
  internal invariant, which makes the missing death-test harness concrete rather than theoretical.

**Revisit if.** A caller appears with a legitimate zero-size request — a node type with a genuinely
empty child array, where allocating zero elements is meaningful rather than mistaken. At that point
permissive-and-unique becomes the better contract, and the aliasing problem has to be solved rather
than avoided.

**Reference.** STYLE.md §11 (assertions versus diagnostics); §9.6 (allocation failure is not
propagated).

---

### D-030 — Short-lived local branches, merged with `--no-ff`

**Status:** Decided · **Session:** ## · **Spec:** n/a (process) — detailed in COMMITS.md

**Decided.** Each stage of a milestone is developed on its own short-lived, local-only branch named
`m<N>/<stage>`, created from an up-to-date `main`. Before merging, the branch is rewritten with
`git rebase -i --exec "make check" main` into a few logical commits, each of which builds and
passes. It is then reviewed and merged with `git merge --no-ff`. The merge commit carries a dense
summary of the whole stage under the subject `<branch-name>: <subject>`. The branch is deleted after
merging. Existing history on `main` from before this entry is left as it is.

**Rejected.**

- *A long-lived `dev` or `milestone-N` branch, squash-merged into `main` at each stage.* The original
  proposal, and it delivers the goal of `main` reading as one summarised entry per stage. Rejected
  because a squash commit has no parent link to the commits it flattens. The merge base between the
  two branches never advances, so every later squash re-proposes all the earlier work, and conflicts
  grow with each stage. The workaround — hard-resetting the long-lived branch to `main` after every
  merge — works until the day it is forgotten.
- *Short-lived branches, squash-merged.* Avoids the merge-base problem and gives one commit per
  stage. Rejected because it contradicts COMMITS.md Rule 3: a stage collapses into one large commit,
  so `git bisect` can only report "somewhere in this stage." It also forces `git branch -d` to refuse
  and `-D` to be used, which removes the check that a branch really was merged.
- *Rebase-and-merge — a linear run of logical commits, no merge commit.* chibicc's shape (316 linear
  commits, no merges) and the best for bisect. Rejected because the per-stage summary disappears;
  tags would have to stand in for it. Also, chibicc reached that shape by rewriting its published
  history wholesale, which Rule 4 forbids here.
- *Pushing feature branches, or working through pull requests.* Gives an off-machine backup and a
  review page. Rejected for a solo project: a pushed branch raises the question of whether it may
  still be rewritten, and the `rebase -i` step depends on rewriting freely. Keeping branches local
  makes Rule 4 unambiguous.
- *Checking only the branch tip before merging.* Rejected because `rebase -i` creates intermediate
  commits that never existed while working. A reorder can leave one commit calling something that
  only arrives in the next: the tip passes, that commit does not build, and a later bisect stops on
  a false failure.

**Why.** Of the three merge styles, this is the only one that meets both goals at once. The merge
commit holds the summary, so `git log --first-parent` reads as one line per stage. The logical commits
underneath keep bisect precise and keep Rule 3 intact. Short-lived branches from a fresh `main`
guarantee the merge base is the previous stage's merge, so earlier work is never re-proposed. After a
`--no-ff` merge the branch tip is reachable from `main`, which is what lets `git branch -d` act as a
genuine "was this merged?" check.

**Consequences.**

- The merge gate is fixed and ordered: `rebase -i --exec "make check"`, then review of
  `git diff main...<branch>` against STYLE.md §14, then goal complete, then records updated. Review
  follows the rebase because the rebase can change code.
- A `make check` target is required: build, run the tests, and verify formatting without rewriting
  any file. Until `make test` runs the `.func`/`.exec` pairs, `check` runs the unit-test binary
  directly. A separate rewriting format target stays for manual use.
- Merge commit subjects do not fit `<type>(<scope>)`. COMMITS.md defines their format, and the
  deferred `commit-msg` hook needs a second pattern or it rejects every merge.
- Git's default merge message must be replaced on every merge.
- `main` is updated with `git pull --ff-only`. A rebasing pull would flatten an unpushed merge commit.
- Unmerged work exists on one machine only. Stages are kept small partly to bound that risk.
- Branch names are fixed at creation, because each name appears verbatim in `main`'s history.

**Revisit if.** The project gains a second contributor, or needs CI. Either one makes pushed branches
and pull requests worth their cost, and reopens whether feature branches may be rewritten after
pushing.

**Reference.** chibicc's history (rui314/chibicc: linear `main`, original history on
`historical/old`); `git log --first-parent`; `git rebase --exec`; `krama-notes-09-git-make-workflow`.

---

### D-031 — Commit scope is optional; issue numbers go in trailers only

**Status:** Decided · **Session:** ## · **Spec:** n/a (process) — detailed in COMMITS.md

**Decided.** A commit subject must have a type. A scope is included when the change sits within a
pipeline phase, and is drawn only from COMMITS.md's scope table. It is omitted when the type already
says where. Issue numbers never appear in the subject: `Refs: #N` marks a commit as part of the work
on an issue, and `Closes: #N` goes on the logical commit that finishes it.

**Rejected.**

- *Scope mandatory, as COMMITS.md previously stated.* Uniform and trivially checkable by a hook.
  Rejected because it produces `build(build): ...` and `test(test): ...`, which repeat the type and
  add nothing. COMMITS.md's own `build:` example already broke the rule, which is evidence the rule
  was wrong rather than the example.
- *Issue number as the scope — `fix(#2)`.* The style used in commits before this entry, and GitHub
  links it to the issue. Rejected because it gives scope two meanings and drops the phase, so the
  subject no longer says what was fixed. `git log --grep="(lex"` misses these commits.
- *Open scope vocabulary — any meaningful word, such as `feat(token)`.* More expressive. Rejected
  because search depends on consistent spelling; `token` and `lex` would split one phase's history
  in two. A new word needs a new table row.

**Why.** GitHub recognises issue references anywhere in the message, including trailers with a colon
(`Closes: #10`), so moving them to the end loses nothing. Keeping the subject to type and phase keeps
it short and searchable, and trailers already hold the decision references.

**Consequences.**

- COMMITS.md: the scope table loses its `build` row; the subject rules say type is mandatory and
  scope conditional; the trailer section distinguishes `Refs: #N` from `Closes: #N`.
- Earlier `fix(#N)` commits stay as they are (Rule 4). They remain linked on GitHub but are not
  found by phase searches.
- The deferred `commit-msg` hook's scope group becomes optional.

**Revisit if.** Phase searches turn out not to be used in practice, or a scope-less commit type
proves ambiguous often enough that a mandatory scope would have prevented real confusion.

**Reference.** GitHub Docs, "Linking a pull request to an issue" (closing keywords in commit
messages, optional colon).

---

### D-032 — The language is named Krama; `.krm`, `kramac`

**Status:** Decided · **Session:** ## · **Spec:** title and throughout

**Decided.** The project is renamed from funC to **Krama** (Sanskrit क्रम — sequence, order, one
step following another). Source files take **`.krm`**; the binary is **`kramac`**. In prose the name
is capitalized, **Krama**; in code, paths, prefixes and the extension it is lowercase, **krama**.
Macros and include guards take `KRAMA_` (`KRAMA_ARENA_H`, `KRAMA_TEST_SUITE`); the emitted runtime
takes `krama_` (`krama_trap`, `krama_div_i32`), and the deferred linked-runtime option is
`krama_rt.c`. Test fixtures consolidate under `tests/krama_src/` as **`.krm` / `.expected`** pairs.
The GitHub repository is renamed to match.

**Supersedes.** The `.func` / `.exec` pairing named in **D-025** and **D-030**; both now read
`.krm` / `.expected`. The `FUNC_TEST_SUITE` define named in **D-024** and **D-026**, and the
`FUNC_<MODULE>_H` guard form, which now read `KRAMA_TEST_SUITE` and `KRAMA_<MODULE>_H`. STYLE.md
§2.2 is rewritten: its premise — that a language name keeps its own capitalization inside
`snake_case` — existed to explain the capital C in `funC` and has no work left to do.

**Rejected.**

- *Keeping funC.* It was a weak play on "fun C", it depends on the capital C to read correctly, and
  `func_` reads as an abbreviation of *function* in a C codebase, which is exactly the wrong signal
  in a transpiler whose source is full of functions.
- *`mouse` / `.mse`.* Personal and affectionate. Rejected: Peter Grogono's Mouse (1979, BYTE, with a
  1983 book and a retro following) owns the search results permanently.
- *`bug`.* A pun on debugging. Rejected: `.bug` is OpenBUGS's extension for saved execution images.
- *`koa`, `tav`, `naja`.* Rejected on collisions — Koa.js is a major Node framework;
  `jkingstonc/tav` is an existing C/Go/Jai-inspired language, the same category as this one; `naja`
  is an AJAX library for Nette. `naja` also carried no connection to the project beyond sounding
  well.
- *`anu`.* Sanskrit अणु, "atom" — the smallest unit that still works, which fits a deliberately
  minimal language. Rejected for a domestic reason: it is also a girl's name, as are `rita`,
  `nitya`, `lipi`, `vidhi` and `sadhana`, which removed most of the same family.
- *`sutra`.* The closest miss. It names the genre of maximally terse rule-texts meant to be expanded
  by a teacher, which is what the spec and the planned K&R-style book already are. Rejected for the
  Kama Sutra association, which would have to be fielded indefinitely.
- *`kerf`, the runner-up.* The slot a saw blade leaves. It carries the strongest story — a kerf has
  width, and cutting as though it does not is the classic beginner's error, which is precisely this
  language's position on signed overflow and `INT32_MIN / -1`. Rejected only because `krama`
  describes both the artifact and the method of building it, and is the more personal choice.
- *`.str` as the extension, punning on Sutra.* Rejected on the same grounds funC was: in a C
  codebase `str` means string to every reader, and a fixture sitting beside string-handling code
  would misread every time. Also taken by game string tables and PlayStation video files.
- *`.su`.* Taken: GCC writes `.su` files under `-fstack-usage` — the exact tooling this project
  lives in.
- *`KRM_` as the code prefix.* Shorter, and it matches the extension. Rejected: two spellings of one
  name means every reader has to learn which is which. The three-letter form earns its keep only
  where length is the point, which is the extension alone.
- *Rewriting history so the project appears to have always been Krama.* This is what chibicc did,
  keeping its original history on `historical/old`. Rejected under COMMITS.md Rule 4 — the history
  is pushed — and because the rename is itself part of the record.

**Why.** *Krama* is what a compiler is — lex, parse, check, emit, in order — and what building one
by hand has been. It is short, types easily, has no religious or comic baggage, and its search
results are effectively unclaimed. `kramac` follows `gcc`, `rustc` and `javac`, where the trailing
`c` denotes *compiler*, not the C language; transpiling to C does not change what the program is.

**Consequences.**

- Live documents carry the new name: the spec, STYLE.md, COMMITS.md, PROJECT.md's Current section,
  README.md, CLAUDE.md, and the note *titles*.
- **Decided entries and note prose keep saying funC.** They record a project that carried that name
  at the time, and editing them would claim the name existed earlier than it did — the same
  reasoning as "never edit a decided entry in place". **Pointers are the exception**: a reference to
  a *file* (`funC-spec-v1.md`, `funC-notes-09-…`) was updated wherever it appeared, frozen entries
  included, because a stale pointer is a dead link rather than a historical fact.
- Sixteen GitHub issue URLs in PROJECT.md were updated. GitHub redirects a renamed repository, but
  the redirect is dropped if a repository with the old name is ever created under the same account.
- `src/main.c` holds an absolute path containing the old directory name. It breaks if the working
  copy is moved, and is due to be replaced when the arena is initialized there.
- **CLAUDE.md is gitignored.** It carries the name and was updated by hand; nothing in the
  repository verifies it, and a fresh clone does not receive it.
- The `/note-it` convention and the note-numbering instruction live outside the repository and were
  updated separately.
- Two fixture directories (`tests/fc-src/`, `tests/funC_src/`) and two extensions (`.fc`, `.func`)
  existed before this entry; both collapse into one directory and one extension.

**Revisit if.** A collision emerges that search did not surface — another language named Krama, or
`.krm` claimed by tooling in this space. Renaming again costs roughly what this cost, and that cost
grows with every document.

**Reference.** Grogono's Mouse (BYTE, July 1979); Koa.js; `jkingstonc/tav`; OpenBUGS `.bug`; GCC
`-fstack-usage`; `krama-notes-09-git-make-workflow`.

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
