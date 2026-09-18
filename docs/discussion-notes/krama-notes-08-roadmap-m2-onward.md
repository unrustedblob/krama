# Krama — Roadmap, milestone 2 onward

**Date:** 2026-08-24
**Status:** Consolidated discussion record. **Not normative.** Input to `krama-spec-v2.md`.
**Supersedes as a reading order:** the milestone-2 design note (2026-08-20), the generics and
constraints note, and the references/`const` thread. Those stay as the record of how each
conclusion was reached; this is where they are reconciled.

> Nothing here is binding until it lands in the spec. Where this and a future spec disagree, the
> spec wins (`CLAUDE.md` §1).
>
> All three sources were *notes*, not decisions. Where they disagree, that is two sketches
> disagreeing — recorded in §6, not treated as a rule being broken.

---

## 1. How to read this

- **§2** — framings that recur. Worth reading once; everything below assumes them.
- **§3** — what is settled, deduplicated across all three sources.
- **§4** — the milestone plan.
- **§5** — what each milestone forces that was not asked for.
- **§6** — conflicts between the three sources, and how they resolve.
- **§7** — open items with triggers.
- **§8** — `DECISIONS.md` entries owed, and the numbering problem.
- **§9** — where this is most likely to go wrong.

---

## 2. Standing framings

**Types have semantics, and those semantics define the legal operations.** Spec principle 4 taken
further than milestone 1 needed. It produced the `b` family, keeps arithmetic off `c8`, and decides
every type question below.

**Three kinds of atomicity, kept separate.** *Semantic* atoms are what the operational semantics
needs a rule for, and should be minimal. *Surface* atoms are what the programmer types, and may be
generous. *Emission* atoms are whatever C does well. Connect them with rewrites; collapsing them is
where language design goes wrong.

**The self-hosting test.** Can this be written in funC itself once the primitives exist? Yes →
library. No → language.

**Checking happens at boundaries, and the expected type propagates inward.** A boundary is any place
a value of a known type meets a declared expected type: a `let` initialiser, a call argument, a
`set` right-hand side, a `return` expression. `let` and a call are **the same rule applied twice**.
Because every root carries a declaration (funC has no inference), every expression has an expected
type available from its context — so `if` arms are each checked against the expectation rather than
against each other.

**One exception, and it should be written down:** `@print` (spec §9.5) takes arbitrary expressions
and synthesises a format from whatever they turn out to be. Type flows *outward* there. Harmless
today; it is the one place the propagation rule is false.

---

## 3. Settled across all three sources

Grouped by area, deduplicated. Section references point back to the source notes.

### 3.1 Blocks, `if`, and loops

- **Blocks produce values; `if` only selects.** One rule instead of one per construct. `match` and
  any future selection form inherit it. *(M2 §1.1)*
- **`give` is the block-value keyword**, over `yield`, `break`-with-value, and Rust's bare
  expression. *(M2 §1.2)*
- **`give` may appear only as the last statement of a block.** A block either ends in `give` and has
  that type, or has no `give` and is `void`. Mandatory-value and no-dead-code with **zero flow
  analysis**, for the same structural reason spec §4.3 gave for `return`. `give;` spells unit.
  *(M2 §1.3 — the load-bearing rule)*
- **Scoping:** `return` exits the function; `give` targets the nearest enclosing block. Nesting
  composes with no extra machinery. *(M2 §1.4)*
- **Only block-like forms enter statement position** — `statement = let | set | builtin |
  block_expr`. Principle 6 survives; `x + 1;` stays unwritable; there is exactly one `if`.
  *(M2 §1.5)*
- **`void` becomes an inhabited unit type.** `if` without `else` gets an implicit `void` else.
  Erased at codegen — zero bits, declaration emits nothing. `void` as a `@print` argument is a type
  error. *(M2 §2)*
- **`if` form:** mandatory parens (they pre-empt the struct-literal ambiguity Rust and Go both
  patch around), mandatory braces, no `elif`. *(M2 §3.1)*
- **`if` arms match exactly, uniformly** — no LUB, no position-dependent rule. `if (c) { give 5; }
  else { }` is a type error, which catches the forgotten `give` for free. *(M2 §3.2)*
- **A diverging arm is rejected** rather than typed `i32|void`. The user writes a guard clause.
  Bottom type deferred, not rejected. *(M2 §3.3)*
- **One `for`, three forms:** `for { }` infinite, `for (cond) { }`, `for (let x: T in xs) { }`.
  One token of lookahead separates them. C's three-clause header rejected. *(M2 §4.1, §4.4)*
- **Only `for { }` can produce a value**; the other two are permanently `void` because they may run
  zero times. *(M2 §4.2)*
- **`for { }` is not `for (true) { }`** — the braceless form is structurally non-completing, which
  keeps the reachability predicate dumb. Java's constant-condition special case is the wart being
  avoided. *(M2 §4.3)*
- **Loop iteration binds the element; the index is a second binding in the header**, not a builtin —
  a builtin returning a different value per call is hidden mutable state. The header is a binding
  list, **not** destructuring. The binding is a fresh copy per iteration. *(M2 §4.5)*

### 3.2 Operators and places

- **Compound assignment adopted** — stays a statement, so `y = (x += 1)` remains unwritable. No
  implicit conversion, unlike C's. No `++`/`--`. **No `/=` for integers** (`/` has no integer
  signature); the integer form is `//=`. No `+=` on `c8`. *(M2 §6.1)*
- **`set`'s target becomes a place grammar** — `IDENT { "." IDENT | "[" expr "]" | deref }`. One
  production; arrays, fields and eventual deref all arrive free. Assignment stays a statement.
  *(M2 §6.2, and independently re-derived in the references thread)*
- **Bitwise binds tighter than comparison.** C's `a & b == c` wart, which Ritchie acknowledged, does
  not need inheriting. *(M2 §6.3)*
- **Comparison is non-associative** — `a < b < c` is a syntax error, narrowing the production rather
  than adding a check. Same technique as spec §4.4's single unary minus. *(M2 §6.4)*
- **Short-circuit `&&` / `||` is semantics, not optimisation**, because funC traps: `b != 0 && a //
  b > 0` is the only way to guard a division. The operational semantics must say so. *(M2 §6.5)*
- **`//` and `%` are one semantic primitive** — Euclidean divmod — with two projections. Matters
  when tuples or multiple returns arrive. *(M2 §6.6)*

### 3.3 Types

- **Families:** `i8/16/32/64`, `u8/16/32/64`, `f32/f64`, `c8/c32`, the `b` family, `bool`, `usize`.
  *(M2 §7.1)*
- **The `b` family is bit patterns**, a real family and not an alias for `u8`. Bitwise and equality
  yes; arithmetic and ordering no. `b1` is not `bool`. Shifts are heterogeneous — `(bN, u8) → bN` —
  the first operator whose operand types differ, and spec §6.1's table has no shape for it.
  *(M2 §7.2)*
- **`[]b8` is the storage type**, not `[]u8`. Deliberately departs from Zig, Rust and Hare, none of
  which has a `b` family. *(M2 §7.3)*
- **`@cast` (value-preserving, may trap) is split from `@bitcast` (representation-preserving, never
  traps, widths must be identical).** *(M2 §7.4)*
- **`bool` emits as `uint8_t` 0/1**, and the two-value invariant is funC's to maintain: `!` emits as
  `x == 0`, and `@bitcast` from `b8` to `bool` is illegal. *(M2 §7.5)*
- **`usize`/`isize` naming**, sign as a uniform prefix. `usize` is the **one** deliberate exception
  to spec §2.1's host-independence, and must be written as such. `isize` has no use case yet.
  *(M2 §7.6)*
- **Integer overflow traps, at every width**, via the promotion tiers — compute wider and
  range-check for `i8`/`i16`/`i32`, precondition tests for `i64`. `<stdckdint.h>` rejected (C23).
  *(M2 §7.7)*

### 3.4 `const` and references

- **`&T` is the reference type constructor**, not `*T`. `&` also spells address-of. *(refs thread)*
- **`const` occupies two independent positions**: replacing `let` it constrains the **binding** (a
  place property, not part of the type); inside a type it constrains the **pointee** (part of the
  type). Mirrors Rust's `mut`.
- **`const` is opt-in, and enforced** — a mutable binding never mutated is a compile error.
- **Transitivity is enforced at the `&` site.** `&c` where `c` is a const place *is* `&const T`.
  That is a typing rule about the expression, and is not subsumption.
- **Weakening happens at the boundary, not in the expression.** `&y` where `y` is mutable is `&i32`
  everywhere; what varies is whether a boundary accepts it as `&const i32`. Principle 3 holds
  because the expression's type never moves.
- **Permission-only weakening passes under read-only constructors, never writable ones.** With only
  writable references in the language, that reduces to **depth zero only**: `&&T` must not be
  accepted where `&&const T` is expected. Variance; the Java covariant-array trap.

### 3.5 Diagnostics and emission

- **Errors only. No warning tier.** The diagnostic set is part of the language definition; a warning
  cannot appear in an operational semantics. Consequence: no check can be trialled, and an
  acknowledged-exception mechanism (Zig's `_ = x;` shape) becomes a requirement rather than a
  nicety. *(refs thread — `D-024`)*
- **Emitted C targets C99.** This resolves `D-020`'s deferral as an actual decision. Forces `bool`
  as `uint8_t`, forbids `<stdckdint.h>`, and rules out `_Generic` a second time. *(M2 §8)*
- **Generated C is dumb and verbose** — temporaries over nesting, statements over expressions,
  explicit branches over cleverness. The guard rail is against the *abuse* instinct, not against
  minimalism. Protects spec §10.5's `#line`-and-gdb story. *(M2 §8)*
- **`@` means compile-time interaction with the compiler**, which closes the namespace. This puts
  `@print` **outside** it — its `@` is a placeholder for missing language features (byte output,
  type introspection, code generic over a type), and it should become funC source once those exist.
  `len` is likewise not an `@` builtin. *(M2 §9)*

---

## 4. Milestone plan

Supersedes the generics note's M1–M7 sketch, which underestimated milestone 2 by a large factor
(§6.4) and omitted references entirely (§6.3).

### M2 — control flow, `const`, arrays

**Theme:** the language becomes able to express a program with a shape.

| Area | Content |
|---|---|
| Control flow | `if`/`else` as expression; blocks produce values; `give` and its last-statement rule; `void` as unit; the `block_expr` statement production |
| Loops | all three `for` forms; `break`, `continue`; reachability for both |
| Bindings | `const` — **binding axis only**; the never-mutated error |
| Types | `bool`; comparison operators; `usize`; arrays |
| Operators | compound assignment; place grammar for `set`; bitwise-tighter-than-comparison; non-associative comparison; short-circuit semantics |
| Checker | flow analysis replacing spec §4.3's structural return rule |
| Builtins | `len` |

**Explicitly out of M2:** references and the `const` *type* axis; structs; user-defined functions;
the `b` family and the wider integer/float families; overflow trapping; `f64`; `c32`.

**Why `const` splits across two milestones.** The binding axis works on scalars and needs nothing
else. The type axis (`&const T`), transitivity at the `&` site, and the boundary weakening rule all
presuppose references, which are M3. Shipping half of `D-025` in M2 is coherent — but the spec must
say the type axis is *deliberately* absent, or the binding keyword will be read as the whole feature.

### M3 — references, structs, functions

**Theme:** the language becomes able to express a program with parts.

| Area | Content |
|---|---|
| References | `&T`, `&const T`; address-of; deref; the deref slot in the place grammar |
| `const` | the type axis; transitivity at `&`; boundary weakening at depth zero |
| Structs | fields; field access in places; `@print` over composites (the first real exercise of spec §9.4's recursive walk) |
| Functions | user-defined functions, parameters, calls, `return` beyond `main` |
| Checker | the acceptability seam; symbol table with scopes (`D-021`'s revisit trigger) |
| Types | tagged unions and `@inj`, if they land here rather than M4 |

**Why structs are here and not M2.** The method-attachment question — Rust-style traits versus
Go-style interfaces — must be settled before structs land, and it cannot be settled without knowing
whether generics are coming. Since generics are planned, structs wait for that fork to be decided.
This is the right sequencing and was the user's own reason.

### M4 — generics

**Theme:** the language becomes able to express a program that is general.

Leaning, per the generics note: substitution rather than compile-time evaluation; closed constraint
sets structured so open is a replacement not a rewrite; bounds on operations rather than
declarations; associated types rather than higher-kinded types; monomorphization; no automatic
destruction.

**Sequencing inside M4:** unconstrained containers first, constraints second. `push`, `pop`, `len`
and `at` never inspect the element, so building containers first keeps the two separable and avoids
the wrong conclusion that containers need a constraint system.

### Unscheduled

The `b` family, the wider numeric families, overflow trapping, `c32`, bit slicing, optionals, the
iterator protocol. Each is independent of the M2–M4 spine and can land wherever it earns its way in.

---

## 5. What each milestone forces

Not optional. These arrive whether or not they are asked for.

**M2 forces:**

| Forced | Because |
|---|---|
| `bool` and comparison operators | `if` and `for (cond)` need a condition type. Spec §6.3 already defers comparison to "phase 2" — this is that phase. |
| `usize` | Array indexing and `len` both return or consume it. |
| Real flow analysis | Spec §4.3's "block ends in exactly one `return`" cannot survive `if`. The relaxation is already flagged there as needing to be deliberate. |
| A reachability predicate | `break`, `continue`, and `return` inside branches are all non-local exits and land in one mechanism. Decide them together. |
| The place grammar | `set a[i] = v` is not expressible without it. |
| A decision on `give` | If `if` ships as a statement in M2 and gains value-production later, that is a widening and safe. But the last-statement rule is cheap now and expensive to retrofit into an existing `if`. Recommend shipping it with M2. |

**M3 forces:**

| Forced | Because |
|---|---|
| Types become a **tree** | `&T` contains a `T`. A flat enum tag cannot hold a child. Type equality becomes a recursive walk. `STYLE.md` §3.8 already reserves `type.h`. |
| The acceptability seam | Boundary weakening is not equality. The checker's core question goes from symmetric to directional. |
| Canonicalization decisions | Whether two independently built `&const i32` nodes are one object. Keys **must include qualifiers**. Retrofitting means finding every site that built a type by hand. |
| Structural vs nominal identity | Decided implicitly by whatever structs do unless decided explicitly first, and it silently determines instantiation caching at M4. |
| A `@bitcast` prohibition | `&T` and `&const T` are representationally identical and equal width, so `@bitcast` between them satisfies M2 §7.4's rule and defeats `const` entirely. Needs the same explicit ban M2 §7.5 gave `b8 → bool`. |

**M4 forces:**

| Forced | Because |
|---|---|
| Instantiation as a worklist to fixed point | Seen-set separate from the pending queue, or mutually referential generics re-enqueue forever. |
| A depth cap | Polymorphic recursion makes the instantiation set infinite. Under errors-only this is a hard error and needs its acknowledged-exception story designed with it. |
| Name mangling with a reverse table | C has one flat namespace, no overloading, and a short guaranteed-significant identifier length. Diagnostics must stay readable. |
| Emission ordering | Instantiations need a dependency sort. |
| Qualifier-multiplied instantiation sets | `List<&i32>` and `List<&const i32>` are two monomorphizations and two emitted C types. |

---

## 6. Conflicts between the three sources

### 6.1 Subsumption — resolved, not conflicting

The M2 note §5 rejects subsumption, §3.2 says not to reach for bidirectional checking, and §3.3
reserves "the one subtyping relation" for the bottom type. The references thread adopts boundary
weakening. **These do not actually collide**, and the distinction is worth stating in one sentence
because it is easy to lose:

- M2 rejected subsumption **over values** — where an arm's own type would change, determined by
  inference rather than by what is written. That objection stands, and structural union typing stays
  rejected.
- Boundary weakening changes **no value and no expression type**. `&y` is `&i32` before and after.
  What is checked is whether a boundary accepts it.

The honest formulation, replacing "funC's first subtyping relation": **funC admits no rule that
changes a value, and permission-only rules only where nothing can be written.** The bottom type, if
it lands, is a second permission-only rule — so M2 §3.3's "the one subtyping relation" needs
rewording either way.

The better framing for `D-025` is **capability-based**: a reference carries a permission set, and
you may hand out a weaker permission than you hold but never a stronger one. Same rule, honest
justification.

### 6.2 Bidirectional checking — partially reversed

M2 §3.2 rules it out on the grounds that both arms synthesise independently and get compared. That
works only while there is nothing to weaken. Once a boundary rule exists, comparing arms to each
other is the wrong operation, and pushing the expected type inward is the right one — which is the
checking direction of bidirectional typing.

M2's actual objection was against using it as a **fix for missing inference**. That objection
survives intact. The mechanism arrives anyway, by a different door, and §3.2 needs a caveat rather
than a reversal.

### 6.3 References had no milestone

The generics note's sequence — bool/if, functions, structs+arrays, generics — contains no pointers
or references anywhere, yet M4's containers need growable storage, which needs them. This roadmap
fixes it by placing references in M3 explicitly.

### 6.4 The generics note's M2 row was a large underestimate

It read "bool, comparison, `if`, flow analysis." The actual M2 note is that plus loops, `give`,
`void`-as-unit, compound assignment, places, operator precedence changes, the whole `b` family, four
integer widths, `usize`, overflow trapping at every width, and the C99 floor. Any sequencing built
on the old row is wrong by a wide margin. §4 above splits it.

### 6.5 `D-020` is already closed

The generics note's open point 8 asks whether `D-020` moves. M2 §8 already decides it: C99 floor,
explicitly superseding the deferral. Remove the open point; keep the observation that monomorphized
output must still be C99, which is a real constraint on M4.

### 6.6 `@store` — rejected twice, independently

The references thread rejected an `@store(p, v)` builtin because it only defers the place grammar.
M2 §9 kills it a second time and more cleanly: `@` means compile-time interaction, and a store is
runtime. Two independent reasons, same answer.

---

## 7. Open items

### 7.1 Blocking spec-v2

| # | Item | Note |
|---|---|---|
| 1 | The `give`-last-statement rule | Load-bearing for all of §3.1 and currently exists only in notes. |
| 2 | `statement = ... \| block_expr` | Without it there are still two `if`s. |
| 3 | Compound assignment: own AST node or parse-time expansion | Affects the node inventory. Expansion costs diagnostic anchoring (the error points at a `+` nobody wrote) and stops being free once the left side is `a[i]`, which C evaluates once. |
| 4 | Whether `give` ships with M2 or later | §5. Recommend with. |

### 7.2 Recommended, not ruled

| # | Item |
|---|---|
| 5 | Bitwise tighter than comparison |
| 6 | Non-associative comparison |
| 7 | Place grammar before arrays |
| 8 | `continue` alongside `break`, decided together |
| 9 | Shift count ≥ width: trap, for consistency with division |

### 7.3 Deferred with triggers

| # | Item | Trigger |
|---|---|---|
| 10 | Bottom type for diverging arms | The spec §4.3 relaxation — i.e. M2 |
| 11 | Labelled blocks and labelled `give` | Same. Meanwhile `ident:` stays reserved. |
| 12 | Value extraction from `for { }` | When a value-producing loop is wanted |
| 13 | Reverse range in `for` | Arrays. It is the fix for the unsigned reverse-loop hazard, which `isize` is **not**. |
| 14 | Destructuring / patterns | Its own merits, never via a loop header |
| 15 | `isize` | A use case. None exists. |
| 16 | Arbitrary-width `bN`, bit slicing | If slicing lands. Bit 0 = LSB; ranges by significance, not memory order. |
| 17 | Traits vs Go-style interfaces | **Before structs land — M3.** Largest architectural fork on the roadmap. |
| 18 | Iterator protocol for `for ... in` | Traits existing. Built-in iterables until then. |
| 19 | Optionals / nullable / neither | Unscheduled |
| 20 | `&T` vs `&const T` default | Before M3 begins |
| 21 | One seam or two — `satisfies(Type, Constraint)` and `acceptable(Actual, Expected)` | Both are directional boundary questions and both must return evidence rather than `bool`. Answer once, before either is built. |
| 22 | Structural vs nominal type identity | Before structs, or it gets decided by accident |
| 23 | `c8` ↔ `c32` is a decode, not a cast | When `c32` lands |
| 24 | `//`/`%` across four integer widths | Monomorphisation arriving in the runtime before the language |

---

## 8. Decision entries owed

### 8.1 The numbering problem — resolve before writing any entry

Three claims on the same IDs:

- `CLAUDE.md`'s closing line cites `D-023` for why that file exists. No such entry is in
  `DECISIONS.md`.
- The M2 note owes ~17 entries "continuing from `D-022`" — which would consume `D-023` upward.
- The references thread drafted `D-024` (errors only) and `D-025` (references and `const`).

**Allocate the block explicitly before anything is written**, or two entries will end up with one
ID. Suggested: write or retire `D-023` first, then allocate the M2 block and the references entries
from the same list in one sitting.

### 8.2 Entries owed, by area

Each has a genuine `Rejected` field.

| Entry | Rejected |
|---|---|
| `give` as the block-value keyword | `yield`; `break`-with-value; Rust's no-semicolon form |
| Blocks produce values, `if` selects | Per-construct value rules |
| `give` only as last statement | Uniform `give` (needs `give;` everywhere); free placement (needs flow analysis now) |
| `void` as an inhabited unit type | Two context-distinguished forms of `if` |
| `if` arms match exactly | RHS-only rule; LUB; bidirectional checking as an inference fix |
| Diverging arm rejected | `i32\|void`; bottom type (deferred, not rejected) |
| Mandatory parens in `if` | Braces-only — loses to struct-literal ambiguity |
| One `for`, three forms | C's three-clause header |
| Loop index as a second binding | `@index` / `@next` — hidden mutable state |
| Explicit union injection | Structural union typing; value-level subsumption |
| The `b` family | `byte` as a `u8` alias; no byte type; a lone 8-bit `byte` |
| `[]b8` as storage | `[]u8`, per Zig/Rust/Hare |
| `@cast` split from `@bitcast` | One cast builtin |
| `usize`/`isize` naming | `size`/`ssize` |
| Overflow traps at every width | Wrap-defined; no sub-32-bit arithmetic; `<stdckdint.h>` |
| **Emitted C targets C99** | C11; C23; leaving `D-020` deferred |
| `@` is compile-time interaction only | `@` as a general intrinsic namespace |
| **Errors only, no warning tier** | A warning tier; start-as-warning; Zig's invisible exemptions |
| **References and the two-axis `const`** | `*T`; `@store`; inferred constness; copy-in/copy-out; binding immutability propagating into the type |
| Boundary weakening, capability-framed | Value-level subsumption (still rejected); explicit `&const y` spelling at every call site |

---

## 9. Where this is most likely to go wrong

1. **M2 is large.** Control flow, loops, flow analysis, `bool`, comparison, arrays, `usize`, places,
   compound assignment, and two precedence changes. If it needs splitting, the natural seam is
   control flow first, arrays and places second — `usize` and `len` follow arrays, not `if`.
2. **The numbering collision produces a duplicate ID** unless §8.1 is done first.
3. **`@bitcast` silently defeats `const`** unless prohibited explicitly at M3. It satisfies every
   condition M2 §7.4 imposes.
4. **The two seams get invented separately** — one in the checker for weakening, one for constraints
   — and unifying them afterwards is a rewrite of both.
5. **Structural vs nominal identity gets decided by accident** at M3, and silently determines
   instantiation caching at M4.
6. **`//` gets typed as a comment opener.** It happened during the M2 session, to the person who
   decided `//` is integer division. The diagnostic should recognise "`//` followed by prose to end
   of line" and say *"`//` is integer division in funC; comments start with `#`"* — worth an
   error-production entry when parser diagnostics are written.
