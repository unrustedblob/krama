# funC — Milestone 2 Design Discussion

**Date:** 2026-08-20
**Status:** Discussion record. Not normative. Input to `funC-spec-v2.md`.
**Context:** Written during a break from Arena implementation. No code produced; language surface only.

> This document records what was settled, what was rejected and why, and what remains open.
> Nothing here is binding until it lands in the spec. Where this note and a future spec disagree,
> the spec wins.

---

## 0. How to read this

Sections 1–9 are the settled design, ordered roughly as they would appear in a spec. Section 10 is
the open list. Section 11 lists the `DECISIONS.md` entries owed, each of which has a genuine
`Rejected` field because a real alternative was considered and lost.

Two framings recur and are worth stating once:

- **Types have semantics, and those semantics define the legal operations.** This is spec principle
  4 taken further than milestone 1 needed. It is the rule that produced the `b` family, that keeps
  arithmetic off `c8`, and that decides every type question below. It must stay intact.
- **Three kinds of atomicity, kept separate.** *Semantic* atoms are what the operational semantics
  needs a rule for and should be minimal. *Surface* atoms are what the programmer types and may be
  generous. *Emission* atoms are whatever C does well. Collapsing these is where most language
  design goes wrong — a minimal surface produces a Turing tarpit, and a maximal core produces a
  semantics nobody can verify. Connect the two with rewrites.

**The self-hosting test**, used throughout to decide language versus library: *can this be written
in funC itself, once the primitives exist?* If yes, it is library. If no, it is language.

---

## 1. Statements, expressions, and blocks

### 1.1 Blocks produce values; `if` only selects

The value-producing construct is the **block**, not `if`. `if` selects between two block
expressions. `match` and any future selection form inherit the behaviour with no additional rule.

This is the single most load-bearing decision in the note. It replaces one rule per construct with
one rule total.

### 1.2 `give`

The keyword is **`give`**.

```
let z: i32 = if (y < x) { give x - y; } else { give y - x; };
```

**Rejected:**

- *Rust's expression-without-semicolon.* Explicitly not wanted — the value should be marked by a
  keyword, not by the absence of punctuation.
- *`yield`* (Hare's spelling, exactly this meaning). Lost on collision: every reader arriving from
  Python, JavaScript or C# reads "generator" first. funC will never have coroutines, so the
  collision is theoretical — but the misreading is not.
- *`break` with a value* (Rust, Zig). Unifies with loop-as-expression later, but `break` means
  "abandon early" and a block's value is the normal path. Wrong register for the common case.

`give` was chosen for concision and low collision. It is an imperative verb, matching `let`, `set`,
`return`.

### 1.3 The placement rule — **this is the load-bearing piece**

**`give` may appear only as the last statement of a block.**

Consequences, all free:

- A block either ends in `give` and has that type, or contains no `give` and has type `void`.
- Mandatory-value and no-dead-code hold **with zero flow analysis**, for exactly the structural
  reason spec §4.3 gave for `return`.
- No `give;` noise on statement-position blocks — absence *is* the void case.

`give;` bare spells the unit value, symmetric with `return;`.

This rule extends the §4.3 zero-flow-analysis budget by a whole milestone. It is the thing to write
down first.

### 1.4 Scoping

| Construct | Scope |
|---|---|
| `return` | Function. Returns from the enclosing function regardless of nesting. |
| `give` | Nearest enclosing block expression. |

Nesting composes with no additional machinery:

```
give if (c) { give 5; } else { give 6; };
```

All three `give`s resolve by nearest-enclosing. This is a good regression case — if it ever fails to
type, something upstream is wrong.

### 1.5 Statement position

Spec §4 has no expression statement, and that is deliberate — it is what makes `x + 1;`
unwritable. One production is still required:

```
statement  = let_stmt | set_stmt | builtin_stmt | block_expr ;
block_expr = if_expr | loop_expr | block ;
```

Only block-like forms enter statement position, not arbitrary expressions. Principle 6 survives
intact, and there is exactly one `if`. Rust admits all expressions here and needs a disambiguation
rule as a result; funC does not.

---

## 2. `void` as an inhabited unit type

**Decided.** `void` is promoted from "legal only as a return type" (spec §2) to a real type with
exactly one value.

**Rejected:** two grammatical forms of `if` distinguished by context — statement-`if` and
expression-`if` as separate constructs. Defensible, and arguably more honest for a language whose
principle 6 says statements are not expressions. Lost because every future construct would then
arrive in two flavours and the count only goes up.

**Consequences:**

- `if` with no `else` is an implicit empty `else` of type `void`. A value-position `if` without
  `else` therefore fails on its own, with no special case.
- Loops are expressions of type `void` (§4.2 below).
- `let x: void = ...;` is well-typed funC with **no C spelling** — `void x;` is illegal C. This is
  the first place the AST and emitted C stop corresponding one-to-one.
- **Erasure at codegen.** A one-valued type carries zero bits: the declaration emits nothing, the
  assignment emits nothing, the `if` emits as a plain C statement. The differential interpreter
  (spec §11.2) must agree about what vanished.
- `void` as a `@print` argument is a **type error**. Spec §9.3's table has no row, and inventing one
  means printing nothing, which reads as a bug.

---

## 3. `if`

### 3.1 Form

```
if_expr = "if" "(" expr ")" block [ "else" ( block | if_expr ) ] ;
```

- **Parens mandatory and part of the production.** Not incidental. Written as `"if" expr block`,
  the parens would be a mere `primary` and both `if (x) { }` and `if x { }` would parse.
- **Braces mandatory.** No single-statement bodies. Matches the existing `.clang-format` posture.
- **No `elif`.** The `else if_expr` tail gives chains for free and stays unambiguous because braces
  are mandatory.
- **One `if`**, usable in statement and value position, given §1.5.

**Why the parens survive scrutiny.** Initially assessed as pure decoration, since mandatory braces
already remove the dangling-else. That assessment was wrong: without parens, `if x { }` is ambiguous
once struct literals exist — is `x { }` a struct literal, or is `x` the condition and `{ }` the
block? Rust and Go both carry a grammar restriction banning struct literals in condition position to
work around this. Mandatory parens sidestep it permanently, for free.

### 3.2 Typing

- Arm types must match **exactly**, in every position. No least-upper-bound, no subtyping, no
  common-supertype rule, no position-dependent rule.
- The rule is uniform rather than RHS-only. In statement position both arms lack `give`, both are
  `void`, and exact match holds trivially — the same rule doing nothing rather than a second rule.
- `if (c) { give 5; } else { }` is a **type error** — `i32` against `void`. This catches "forgot the
  `give` in one branch" for free. An RHS-only rule would have to silently discard the `5`.
- Do **not** reach for bidirectional type checking here. It is the usual answer to "what type should
  this `if` have", but funC does not need it: every literal has a fixed type by spec §3.4 and every
  binding is annotated, so both arms synthesise independently and are compared. Named so it can be
  ruled out on sight when reading references.

### 3.3 The diverging arm — deliberately rejected

```
let z: i32 = if (c) { give 5; } else { return 0; };
```

The `else` block has no `give`, so it is `void`, so this is **rejected**. It is a program people
write constantly.

**Rejected fix — typing `z` as `i32|void`.** Three problems:

1. It does not type-check under explicit injection (§5). A `void` would have to become an `i32|void`
   implicitly — a tag written that nobody wrote. That is principle 1.
2. It describes a state that cannot occur. The else branch returns from the function; `z` is never
   assigned. `i32|void` claims "z holds an i32 or nothing"; the truth is "z holds an i32, or we are
   not here". Those are different claims.
3. It infects everything downstream. Every later use of `z` must discriminate a union case that is
   statically impossible, and a reader asking "when is `z` void?" gets the answer "never".

**Adopted instead:** reject, and the user writes the guard clause.

```
if (!c) { return 0; }
let z: i32 = 5;
```

Same program, generally better style, costs the language nothing.

**Revisit at the spec §4.3 relaxation.** The alternative is a **bottom type** (`never`, `!`) — an
uninhabited type that is a subtype of everything, so a diverging arm imposes no constraint on the
join. It is cheaper than it looks: §4.3 already commits to real flow analysis when `if` arrives, and
"this arm diverges" is the same bit that analysis computes for mandatory-return. The cost is that it
is the one subtyping relation a language otherwise free of subtyping must admit — the honest
statement becomes "no coercions except from the type that has no values."

Cost asymmetry favours rejecting now: rejecting a program you later accept is recoverable;
accepting one you later reject is a breaking change.

**Write the trigger into spec §4.3 itself**, not only into a decision entry. That paragraph already
says the relaxation must be deliberate; this is now a second thing to reconsider at the same moment.

### 3.4 Labelled blocks — noted, not adopted

Nesting works today with no labels (§1.4). What labels would buy is *giving from a block that is not
the nearest one* — reaching past an enclosing block:

```
outer: {
    ...
    if (c) { give outer 5; }
    ...
}
```

Prior art: Zig's `blk: { break :blk v; }`, Rust's labelled `break 'a v`.

Two nuances to carry forward:

- **Labelled `give` is a non-local exit.** Statements after it in *every* block between here and the
  target become unreachable, so it lands in the same reachability machinery as `return`. Natural
  time is the §4.3 relaxation.
- **The label namespace is separate from the variable namespace.** Zig and Rust both mark it
  syntactically (`:blk`, `'a`) so a label and a variable of the same name cannot collide. Decide
  deliberately rather than letting it fall out of the parser.

**Action now:** none, except do not spend the syntax. `ident:` at statement position stays reserved.

---

## 4. Loops

### 4.1 One keyword, three forms

```
for { }                            # infinite
for (cond) { }                     # condition-tested
for (let x: T in iterable) { }     # iteration
```

One keyword, no header irregularity. The parenthesised clause is a plain `expr` in form 2 and a
binding list in form 3; neither needs internal `;`. Disambiguation is one token: after `(`, a `let`
means form 3, anything else means form 2.

Keep `let` in form 3. `for (x: i32 in xs)` is lighter but requires lookahead past the identifier;
`let` costs four characters and keeps the parse trivially LL(1).

`break` and `continue` become keywords. Both are non-local exits and land in the same reachability
bucket as `return`. Decide them together.

### 4.2 What each form's type is

| Form | Type | Why |
|---|---|---|
| `for { }` | value-producing | Cannot complete normally, so it is the only loop with a value to give |
| `for (cond) { }` | `void`, permanently | May execute zero times |
| `for (x in xs) { }` | `void`, permanently | May execute zero times |

Same reason Rust's `while` is unit and only `loop` can `break v`. "Loops are expressions" is true
and buys uniformity; it does **not** buy value-producing loops in general. Do not design `for (cond)`
as if it could `give`.

Extracting the value from `for { }` needs either labelled `give` (§3.4) or a `break v` form. Not
settled; not needed yet.

### 4.3 Why `for { }` is not `for (true) { }`

`for { }` **provably** never completes normally, structurally. `for (true) { }` requires the checker
to evaluate a constant condition to know the same thing — which is exactly the special case Java
carries in its reachability rules, and a known wart. Keeping the braceless form as a distinct
syntactic thing keeps the reachability predicate dumb.

Write this reason down or someone will "simplify" the form away.

### 4.4 Rejected: C's three-clause header

```
for (let i: i32 = 0; i < 10; set i += 1) { }     # NOT adopted
```

Three problems, ascending:

1. The init clause is a `let`, which is a *statement* terminated by `;` (spec §4.6). The header's
   `;` then does double duty — terminator or separator? — and the `let` reads as a statement escaped
   into a parenthesised expression.
2. The step clause is a `set` stripped of its terminator. Statements would appear in two different
   terminated-ness modes depending on position.
3. **The scope of `i` is a real question with a real wrong answer.** C got it wrong until C99. If
   `i` is loop-scoped, the header introduces a scope the braces do not delimit — the only place in
   funC where a binding is not visibly enclosed by the block that owns it.

C adopted the form because it had no better tool; it is a desugaring of
`init; while (cond) { body; step }`. Counting is form 3 over a range. Reverse and stride are range
constructions. What remains — genuinely irregular stepping such as `set i *= 2` — is `for { }` plus
an explicit `set` and a conditional exit.

Dropping it removes the only form that imported statement-in-expression-position and an
undelimited scope.

### 4.5 Iteration: what `in` binds

- **Element by default.** The index, when needed, is a **second binding in the header**, not a
  builtin:

  ```
  for (let e: i32, i: usize in array) { }
  ```

  This is Go's answer made explicit. It keeps every expression side-effect-free, which an
  `@index()` or `@next()` returning a different value each call would not — that is hidden mutable
  state, and spec §4.6 makes expressions pure.

- **The header is a list of bindings, not a destructuring.** No tuple type, no pattern syntax,
  nothing new in the type system. Only the second `let` is dropped; the general
  `let a: T, b: U = ...` form is a separate question and is **not** implied.

  *Why the distinction matters:* destructuring is the front half of pattern matching. Once
  `let (a, b) = ...` exists, `let (a, 0) = ...` is a natural next ask, and that is refutable
  patterns and exhaustiveness checking. Fine to want. Not fine to arrive there via a loop header.

- **The loop binding is a fresh copy per iteration.** `set` on it does not affect iteration. State
  it; do not leave it to the implementation.

- **Built-in iterables only.** Arrays and slices, possibly a vector or list later. No iterator
  protocol, no user extension, until traits exist. Go has lived there for fifteen years. Rust's
  `for` is sugar for a trait method and needs traits to exist first.

### 4.6 Traits — flagged, not designed

Leaning Rust-style traits over classes: behaviour attached to types without the type becoming a
container of behaviour. Consistent with spec §10.4's "no evaluation logic on nodes" at the language
level.

**Price to know up front:** traits need generics to be worth much; generics need monomorphisation or
boxing; monomorphisation is a whole compilation strategy — N copies of a function, one per
instantiating type — at which point codegen stops being a straight walk over the AST. That is the
single largest architectural change on the roadmap, larger than `if` or loops.

**Decide before structs land** whether the answer is Rust-style traits or Go-style interfaces. Go's
costs dramatically less and gives up rather less than people assume.

---

## 5. Unions — explicit injection

When tagged unions arrive, arm-type matching does **not** relax.

```
let z: i32|f32 = if (c) { give @inj(i32, 5); } else { give @inj(f32, 5.0); };
```

Both arms genuinely have the union type because the programmer named a constructor. Exact match
holds and every existing rule applies unchanged. Rust's `Some(x)` / `Ok(x)` is this.

**Rejected:** structural union typing, where the checker computes a least upper bound of the arms
and synthesises `i32|f32` (TypeScript, Ceylon). It works and is well understood. It lost because an
expression's type would then depend on inference rather than on what is written — the thing spec
§3.4 exists to prevent.

Also rejected implicitly: subsumption, where the expected type is pushed down into each arm and the
question becomes "is this acceptable here" rather than "do these match". Same objection.

Verbosity accepted deliberately. funC is a language written along the thought process; `@inj(i32, 5)`
says "this arm gives an i32 into a union of both", and a reader does not have to know a coercion
rule to see it.

**Type inference remains dropped**, considered and rejected several times. Revisit only when types
get verbose enough to hurt, and then via a spelling shortcut such as `@type_as` — which shortens how
a type is *named* without changing what type an expression *has*.

---

## 6. Operators

### 6.1 Compound assignment — adopted

`set x += 10;` and family. Adopted, and for a better reason than shorthand: it stays a *statement*,
produces no value, so `y = (x += 1)` remains unwritable and principle 6 holds.

C's `+=` hides an implicit conversion (`int x; x += 1.5;` compiles). funC's cannot — spec §6.1
signatures apply unchanged, so both sides must match exactly.

**No `++` / `--`.** Already excluded by spec §3.3. `set x += 1;` covers it.

**Asymmetries to state outright in the spec:**

- There is **no `/=` for `i32`**, because `/` has no integer signature (spec §6.2). The integer form
  is `//=`. State it, because `set x /= 2;` is what everyone types first.
- **No `+=` on `c8`** — no arithmetic on `c8` at all.
- `//=` and `%=` route through the Euclidean helper, like their binary forms.

**Open sub-decision: own AST node, or expanded at parse time** into
`Set(x, Binary(+, VarRef x, expr))`. Expansion is one node kind and every type rule falls out free.
Two costs:

- **Diagnostic anchoring.** After expansion the type error points at a `+` the user never wrote.
  Spec §12 is emphatic about spans anchoring to real syntax. Survivable by giving the synthesised
  node the `+=` token's span, but decide it deliberately.
- **The lvalue is a bare `IDENT` today.** When it becomes `a[i]` or `s.f`, expansion duplicates the
  left side; C explicitly evaluates it once. Free now — no side effects in expressions — and stops
  being free at exactly the milestone arrays arrive. That is a `Revisit if` line.

### 6.2 Places (lvalues) — the change that buys the most reuse

Today `set`'s left side is an `IDENT`. Make it a **place** — a restricted expression grammar
denoting a storage location rather than a value:

```
place    = IDENT { "." IDENT | "[" expr "]" | <deref> } ;
set_stmt = "set" place assign_op expr ";" ;
```

One production and one new judgment in the semantics ("evaluate a place to a location"), and arrays,
struct fields and eventual pointer dereference all arrive without a new statement form each. `set
a[i].x = 5;` works for free. Assignment stays a statement, so principle 6 is untouched.

C calls these lvalues; Rust calls them place expressions and the modern literature follows Rust.

### 6.3 Precedence — do not copy C

`a & b == c` parses as `a & (b == c)` in C. Ritchie acknowledged this as a mistake, kept only
because `&` predates `&&`. funC has no back-compatibility debt: **put bitwise tighter than
comparison** and the wart never exists.

### 6.4 Comparison should be non-associative

`comparison = additive [ ( "<" | ">" | "<=" | ">=" ) additive ]` — an optional single, not a loop.
`a < b < c` becomes a **syntax error** rather than a type error. Same technique as spec §4.4's
single unary minus: narrow the production rather than add a check. Rust does this; the searchable
term is yacc's `%nonassoc`.

Python deliberately does the opposite (chained comparison means `a < b and b < c`) and is worth
reading as the dissent.

**Not ruled by the user.** Recommended.

### 6.5 Short-circuit is load-bearing because of trapping

Normally short-circuiting is unobservable in a language with no side effects in expressions. funC
traps, so:

```
b != 0 && a // b > 0
```

is the **only** way to guard a division. Short-circuit evaluation is therefore semantics, not an
optimisation, and the operational semantics (spec §11.4) must say so explicitly.

### 6.6 A note on atoms: `//` and `%` are one primitive

Spec §7.4 says the pair `(q, r)` is unique. So there is **one** semantic primitive — Euclidean
divmod, `(i32, i32) → (i32, i32)` or trap — and `//` and `%` are its two projections. Costs nothing
to ignore today, since C makes you compute both and discard one. It starts mattering when funC gets
tuples or multiple return values, at which point `divmod` is the honest spelling and the operators
are sugar.

This is the shape to look for generally: two surface constructs, one semantic rule.

---

## 7. Types

### 7.1 The families

| Family | Members | Semantics |
|---|---|---|
| `i*` | `i8` `i16` `i32` `i64` | signed integers — quantities |
| `u*` | `u8` `u16` `u32` `u64` | unsigned integers — non-negative quantities |
| `f*` | `f32` `f64` | IEEE-754 floating point |
| `c*` | `c8` `c32` | characters. `c16` permanently excluded (spec §2.2) |
| `b*` | `b8` `b16` `b32` `b64`, possibly `b1`…`bN` | **bit patterns — machine representation** |
| — | `bool` | truth values |
| — | `usize` (and `isize` if ever needed) | host-width, the sole exception to fixed widths |

### 7.2 The `b` family

**Decided.** A distinct type family for machine representation, not an alias for `u8`.

**Rejected:**

- *`byte` as an alias for `u8`* (Go's `type byte = uint8`). Costs nothing, adds no checker work, and
  gives two names to one type — exactly the ambiguity principle 3 exists to eliminate.
- *No byte type at all; use `u8`.* Simplest. Accepts `u8` doing double duty as small-number and
  bit-pattern. Java's *signed* `byte` is the cautionary version of this at scale: `b & 0xFF` appears
  in every Java codebase that touches I/O.
- *`byte` as a single 8-bit type rather than a family.* This was the initial framing and it does not
  survive. `i`, `u`, `f` are families parameterised by width; a lone `byte` names a size, not a
  semantics. It also leaves 32- and 64-bit bit patterns — hashes, checksums, flag words — with no
  type, forcing bitwise back onto `u32` and defeating the point. C++'s `std::byte` has exactly this
  inconsistency and people notice.

**Prior art:**

| Source | Relevance |
|---|---|
| C++17 `std::byte` (P0298, Macintosh) | `enum class byte : unsigned char {}`. Bitwise ops only, no arithmetic, no implicit conversion; `std::to_integer<T>` to get a number out. The argument in the proposal is this one, near-verbatim. |
| VHDL `std_logic_vector` vs `numeric_std` | Same representation, different operations, explicit mandatory conversion. In industrial use since the early nineties, so the ergonomic verdict is in: everyone complains about conversion noise, nobody proposes removing it. |
| Erlang binaries / bitstrings | `<<A:8, B:24>>` — first-class bit-level construction and matching with explicit widths. Prior art for the slicing idea. |
| Go `byte`, Java `byte` | Counterexamples. The alias and the signed-number versions respectively. |

**Operations on `bN`:**

- Bitwise `&` `|` `^` `~` and shifts: **yes**. This retires the "is `&` on a signed integer
  meaningful" question entirely — `-1 & 7` was never well-formed, it was an `i32` asked to behave as
  a pattern. Same resolution as `'a' + 'b'`, three types later.
- Arithmetic: **no**.
- Equality: **yes**. Ordering: **no** — `<` presumes a numeric reading. First type in funC with
  equality but no order.
- `b1` is **not** `bool`. One bit versus one truth value; different semantics, different types. First
  place two types share representation *and* width, so `@bitcast` between them is legal and someone
  will do it.

**Shifts have heterogeneous operands.** `b32 << b32` is wrong — the shift *count* is a quantity, not
a pattern. Signature is `(bN, u8) → bN`. This is the first operator whose operand types differ, and
spec §6.1's table has no shape for it yet.

Shift count ≥ width is UB in C and must be defined or trapped by principle 5. Trap is consistent
with division; "yields all zeros" is defensible and branch-free. Either way it is a helper, not
native emission — the second operator family in that category.

Negative shift counts become **unrepresentable**, since the count type is unsigned. Of C's three
shift UBs: negative count is structurally impossible, count ≥ width is checked, and left-shift of a
negative value cannot occur because `bN` has no sign. All three closed.

### 7.3 `[]b8` is the storage type

**Decided.** Raw addressable memory is `[]b8`, not `[]u8`.

| Type | Means |
|---|---|
| `[]b8` | raw bytes, uninterpreted. What I/O returns. |
| `[]c8` | ASCII text |
| `[]u8` | an array of small non-negative numbers, because you meant numbers |

**The precedent deliberately departed from.** Zig, Rust and Hare all use `[]u8` for addressable
memory — but none of them has a `b` family. Their `u8` *is* the bit-pattern type, so `x & 0xFF` on a
`u8` is ordinary code in all three. They are precedent for the storage convention and precedent
*against* splitting patterns from unsigned integers at all. Taking `[]u8` while keeping the `b`
family would adopt their convention while rejecting the premise that makes it coherent — and would
put the conversion boundary at the single most-traversed point in the language (file read, socket
read, hash input, serialisation), maximising exactly the cost the prior art unanimously reports.

Asking "what are a file's contents?" in funC's own terms: not numbers — nobody adds two bytes of a
PNG. They are bit patterns until something interprets them. `[]b8` says that.

Cost accepted: unfamiliarity, and conversion at the point of interpretation rather than never.

### 7.4 `@cast` versus `@bitcast`

`b8` and `u8` are representationally identical; converting changes no bits. `f32 → i32` changes bits
and can trap (spec §8.3). Those are different operations and need different spellings.

| Builtin | Preserves | May trap | Width |
|---|---|---|---|
| `@cast` | **value** — meaning retained, representation may change | yes | may differ |
| `@bitcast` | **representation** — bits retained, meaning changes | never | **must be identical** |

`b8 ↔ u8` is `@bitcast`. So is `f32 ↔ b32`, which is how a float hash or an `isnan` check gets
written without library support.

**Equal-width requirement is what keeps `@bitcast` safe.** Rust's `transmute` is notoriously
dangerous precisely because it does not stop there. funC has no pointers yet and every scalar width
is known, so the restriction is enforceable.

Prior art: Zig's `@intCast`/`@floatCast` versus `@bitCast`; Rust's `as` versus `transmute`; LLVM
IR's `bitcast` versus the converting casts. Every one of them separates the two, because they have
different failure modes and a single spelling hides which risk was taken.

### 7.5 `bool`

- **Emitted as `uint8_t`, values 0 and 1.** Keeps the C floor low (§8).
- Operations: `==`, `!=`, and the logical operators. **No arithmetic, no ordering.** First type whose
  legal operations do not come from the numeric families — a new shape of row in spec §6.1.
- **The two-value invariant is funC's to maintain, not C's.** Nothing stops a `uint8_t` from holding
  7. It holds only as long as every construct producing a `bool` produces exactly 0 or 1:
  comparisons do; `!` must emit as `x == 0` rather than C's `!` (same result, but state it); and
  `@bitcast` from `b8` to `bool` must be **illegal**, since `b1` exists and is the right type for
  that intent. Assertion-worthy in the interpreter.
- C23's `bool` would carry the invariant automatically (conversion normalises non-zero to 1). Traded
  away for portability, knowingly.

### 7.6 `usize`

**Naming decided:** `usize` / `isize`, not `size` / `ssize`.

The sign is a **prefix**, uniformly, across every integer type. The suffix names the width — a
number for the fixed-width families, `size` for the host-width one. Nothing lies. `size`/`ssize`
breaks the sign-prefix uniformity, and the doubled `s` is a POSIX artifact with no principle behind
it. Rust and Zig both landed here.

**But state the exception explicitly in the spec.** Spec §2.1 is emphatic that widths are fixed by
the specification, not inherited from the host. `usize` is the one type whose width *is* the host's,
because addressable memory is a host property. Write it once as a deliberate exception with the
reason attached, or someone will later "fix" the inconsistency.

`isize` has **no use case yet** — its job elsewhere is pointer differences and funC has no pointers.
Do not add it speculatively; a type with no use is a type nobody knows the rules for.

**The reverse-loop hazard is real and `isize` is the wrong fix.** STYLE.md §5.2 documents
`for (size_t i = n - 1; i >= 0; i--)` never terminating; funC users will hit the identical trap with
`usize`. The good fix is a **reverse range** in the `for` form, which is wanted anyway.

`usize` is needed when arrays arrive, not before.

### 7.7 Integer overflow — trap, at every width

**Decided.** Overflow traps. Consistent with spec §8.3's division trapping and required by
principle 5.

**The problem C creates.** funC's rule is `i8 + i8 → i8`. C's rule is integer promotion:
`int8_t + int8_t` promotes both to `int`, computes in 32 bits, and narrows only on assignment —
where it is *implementation-defined conversion*, not UB. So the naive emission
`int8_t c = a + b;` silently produces `-56` for `100 + 100`, with no trap and no diagnostic.

**This applies at every width, not just the small ones.** `INT32_MAX + 1` is UB in C today and spec
v1 says nothing about it. Whatever the rule is, it must be the same rule at every width or there are
two overflow stories.

**Implementation tiers** (all C99, no `<stdckdint.h>` — see §8):

| Width | Technique |
|---|---|
| `i8`, `i16` | C's own promotion does the work. Compute in `int`, range-check against `INT8_MIN`/`INT8_MAX`, trap outside. Cannot itself overflow. |
| `i32` | Same trick one level up: compute in `int64_t`, range-check, trap. |
| `i64` | No wider type available. Requires the classic **precondition** test — check the operands *before* the operation. Fiddly for multiply, well documented. |

One helper per width per operation, all trapping. The differential interpreter must implement the
check **independently** (D-006), not call the helpers.

*Rejected:* `<stdckdint.h>`. Correct semantics, but C23 — see §8. `__builtin_add_overflow` is the
pre-C23 spelling with the same semantics but narrows the floor to GCC/Clang instead.

### 7.8 Loose ends in the type families

- **`c8` ↔ `c32`.** Once both exist, conversion is a **decode**, not a `@cast`. It is a function.
- **Arbitrary-width `bN`.** If bit slicing (`var_b32.bits(...)`) is ever on the roadmap, the width
  question arrives with it: what type does a 5-bit slice have? A `b8` with the top three zeroed
  re-introduces exactly the untyped knowledge the `b` family eliminates. `b5` is the only answer that
  keeps the rule intact. Zig has `u1`–`u65535`; LLVM IR has `iN`. **Not for milestone 2** — codegen
  must lower non-power-of-two widths with masking and shifting on every load and store, and the type
  checker starts doing arithmetic on widths (`b3 ++ b5` is a `b8`). C's bitfields are the cautionary
  version: the feature without the type system, which is why nobody uses them portably.
- **Bit numbering, if slicing lands.** Universal convention is **bit 0 = LSB**; a 1-based sketch will
  surprise everyone. And ranges must be defined by **significance, not memory order** — memory order
  is endian-dependent, which principle 5 forbids.
- **`//` and `%` across four integer widths** is four helper instantiations each, or one generic
  helper. That is the monomorphisation question arriving early, in the runtime rather than the
  language.

---

## 8. Emitted C target — C99 floor

**Decided.** Emit the **simplest possible C**, using the fewest constructs. C99 is the floor. This
resolves `D-020` (previously Deferred) as an actual decision rather than a deferral.

**Why.** C's minimalism and flexibility are what made it the lingua franca — CPython, Ruby and Lua
all bootstrap on portable C for this reason. The portability promise made to funC users is worth
more than any construct a later standard offers.

**Immediate consequences:**

- `bool` emits as `uint8_t` 0/1 (§7.5) — now forced rather than chosen.
- No `<stdckdint.h>`; overflow uses the promotion tiers (§7.7).
- `_Generic` was already ruled out on other grounds (spec §9.4); now ruled out twice over.
- Independent of `D-019`, which governs only what compiles the transpiler (C23, unchanged).

**The guard rail this needs.** "We can abuse the features" is true and is also how a generated-C
backend degenerates: increasingly clever C — nested ternaries, comma operators, statement
expressions — until the output is unreadable and undebuggable, which costs spec §10.5's entire
`#line`-and-gdb story.

**Generated C should be dumb and verbose:** temporaries over nesting, statements over expressions,
explicit branches over cleverness. Verbosity is free — spec §10.5 already says do not pretty-print,
and `clang-format` is in the pipeline. It is the *abuse* instinct that needs the rail, not the
minimalism.

The formal name for "dumb and verbose" is A-normal form. funC does not need ANF against a C backend
— C accepts nested expressions — but the style it describes is what keeps generated C debuggable,
and it is what would make an eventual QBE move cheap rather than a rewrite.

---

## 9. The `@` namespace

**Sharpened.** `@` means **compile-time interaction with the compiler** — a question asked or an
assertion made at compile time. `@cast` is "trust me, I know what I am doing"; `@bitcast`, `@is`,
`@sizeof`, `@type`, `@derive`, `@inj` all fit. This makes the namespace closed rather than a
grab-bag, and it should be stated in the spec.

**Which puts `@print` outside it.** `@print` performs runtime I/O; only its format synthesis is
compile-time. Spec §9.1 already says it cannot be declared in funC "since funC has neither variadics
nor generics" — so its `@` is a **placeholder for a missing language feature**, not a member of the
intrinsic namespace.

**Retirement conditions for `@print`,** worth recording so the builtin does not ossify. The atoms
hiding under it are three separate things:

1. byte output,
2. compile-time type introspection (`@type` / `@derive`),
3. some way to write code generic over a type.

Once those exist, `@print` should stop being a builtin and become funC source. This is the
self-hosting test applied to funC's own standard library.

**`len` is correspondingly *not* an `@` builtin.** Hare's `len` is a keyword-level operator working
on any type with a known length — resolved at compile time, but denoting a runtime value. Same for
the loop index (§4.5): loop state belongs in the header, not in a magic name.

`len` returns `usize`, which is the third feature blocked on that type (with array indexing and the
loop index binding).

---

## 10. Open items

Grouped by what unblocks them.

### 10.1 Needed before writing spec-v2

| # | Item | Note |
|---|---|---|
| 1 | The `give`-only-as-last-statement rule (§1.3) | **Load-bearing.** Everything in §1–3 rests on it and it currently exists only in this note. |
| 2 | The `statement = ... \| block_expr` production (§1.5) | Without it there are still two `if`s. |
| 3 | Compound assignment: own node or parse-time expansion (§6.1) | Affects the AST inventory. |

### 10.2 Recommended but not ruled

| # | Item | Recommendation |
|---|---|---|
| 4 | Bitwise tighter than comparison (§6.3) | Adopt. Fixes a known C wart for free. |
| 5 | Non-associative comparison (§6.4) | Adopt. Makes `a < b < c` a syntax error. |
| 6 | Place grammar for `set` (§6.2) | Adopt before arrays. One production, large payoff. |
| 7 | `continue` alongside `break` (§4.1) | Decide together. |
| 8 | Shift-count-≥-width: trap or zero (§7.2) | Trap, for consistency with division. |

### 10.3 Deferred with a named trigger

| # | Item | Trigger |
|---|---|---|
| 9 | Bottom type for diverging arms (§3.3) | The spec §4.3 relaxation |
| 10 | Labelled blocks and labelled `give` (§3.4) | The same relaxation. Meanwhile `ident:` stays reserved. |
| 11 | Value extraction from `for { }` (§4.2) | When a value-producing loop is actually wanted |
| 12 | `isize` (§7.6) | A use case. None exists. |
| 13 | Reverse range in `for` (§7.6) | Arrays. It is the fix for the unsigned reverse-loop hazard. |
| 14 | Destructuring / patterns (§4.5) | Its own merits, never via a loop header |
| 15 | Arbitrary-width `bN` and bit slicing (§7.8) | If slicing lands. Not milestone 2. |
| 16 | Traits vs Go-style interfaces (§4.6) | **Before structs land.** Largest architectural fork on the roadmap. |
| 17 | Iterator protocol for `for ... in` (§4.5) | Traits existing. Built-in iterables until then. |
| 18 | Optionals / nullable / neither (§4, `NULL` set aside) | Milestone 3 fork, unnamed so far |

---

## 11. Decision entries owed

Each has a real `Rejected` field. Numbering continues from `D-022`.

| Decision | Rejected alternatives |
|---|---|
| `give` as the block-value keyword | `yield`; `break`-with-value; Rust's no-semicolon |
| Blocks produce values, `if` selects | Per-construct value rules |
| `give` only as a block's last statement | Uniform `give` requirement (needs `give;` everywhere); free placement (needs flow analysis now) |
| `void` promoted to an inhabited unit type | Two context-distinguished forms of `if` |
| `if` arms match exactly, uniformly | RHS-only rule; LUB / subsumption; bidirectional checking |
| Diverging arm rejected | `i32\|void` typing; bottom type (deferred, not rejected) |
| Mandatory parens in `if` | Braces-only, Rust/Go style — loses to struct-literal ambiguity |
| One `for`, three forms | C's three-clause header |
| Loop index as a second binding | `@index` / `@next` builtins — hidden mutable state |
| Explicit union injection | Structural union typing (LUB); subsumption |
| The `b` family | `byte` as `u8` alias; no byte type; lone 8-bit `byte` |
| `[]b8` as the storage type | `[]u8`, per Zig/Rust/Hare |
| `@cast` split from `@bitcast` | One cast builtin |
| `usize` / `isize` naming | `size` / `ssize` |
| Trap on integer overflow, every width | Wrap-defined; no sub-32-bit arithmetic; `<stdckdint.h>` |
| Emitted C targets C99 — **supersedes the D-020 deferral** | C11; C23; leaving it deferred |
| `@` denotes compile-time interaction only | `@` as a general intrinsic namespace |

---

## 12. Friction

**`//` typed as a comment opener.** Happened inside three lines of writing example funC, by someone
who had personally decided `//` is integer division (D-012, spec §3.3). Muscle memory from C is
strong and will not be trained away quickly.

This is the strongest available argument that the diagnostic for a stray `//` needs to be
excellent — it should recognise the pattern "`//` followed by prose to end of line" and say *"`//`
is integer division in funC; comments start with `#`"*, rather than reporting a type error on
whatever the prose scans as.

Worth an error-production entry when the parser's diagnostics get written.
