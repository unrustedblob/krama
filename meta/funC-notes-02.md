# funC Transpiler — Design Notes, Session 02

**Date:** 2026-08-11
**Status:** Pre-implementation. No code written.
**Scope of session:** Answers to session 01 open questions; type system consequences; expression
parsing strategy.

---

## 1. Decisions made this session

### 1.1 `print` — type-directed format synthesis (user, confirmed)

**Decided.** Format specifiers are synthesized from static types. The user's motivation: eliminate
hand-written `%d` entirely, and have composite types derive their printing from the composition of
their members.

**Design consequence raised by Claude:** implement format synthesis as a **recursive walk over the
type tree**, emitting format-string fragments and a flattened argument list, even though phase 1
only ever reaches leaf cases. A struct would later produce `"{x: %d, y: %f}"` plus `s.x, s.y` from
the same walk. Building it recursively now makes composite support nearly free later.

**Prior art named:** this is `deriving Show` (Haskell) / `#[derive(Debug)]` (Rust).

**Noted:** C11 `_Generic` is not needed for any of this. A compiler knows its types statically;
`_Generic` is the workaround for code that doesn't.

### 1.2 Type checking is strict — no implicit casting (user)

**Decided.** Operands of differing type are an error. User rationale: simpler to manage, and the
type checker doubles as a self-test mechanism and as an input to the formal verification approach.

**Correction issued by Claude — point of contention, resolved:**

User stated this "should fail on parsing." It cannot. The parser has no type information;
`set y = x + z` is syntactically identical regardless of operand types. The pipeline must be:

```
source → lex → parse → AST → type check (annotates AST) → codegen
```

Type errors are a distinct diagnostic class, reported after a complete parse. This separation is
load-bearing for aim 3: an operational semantics wants to state "well-typed programs do not get
stuck," which is only meaningful if typing is a separate judgment over the AST rather than fused
into parsing.

**Resolution:** phases kept clean and separate. Accepted.

### 1.3 `let` is a fixed sequence (user)

**Decided.** `let` → `identifier` → `:` → `Type` → `=` → `value`. Anything deviating is a syntax
error. Initialization is mandatory.

**Consequence:** uninitialized-variable UB is eliminated by construction. This is a spec property
worth claiming explicitly.

**To confirm:** is the right-hand side a full expression or a literal only? Assumed full expression
(e.g. `let y: int = x + 1;`).

### 1.4 Phase 1 type set: `int`, `char`, `float` (user)

**Decided.** Arithmetic types only. Operators `+ - * / % //`, where `//` is integer/quotient
division. Deliberate staging: get arithmetic solid before `bool` introduces `if`/`else` and
`switch`/`case`.

### 1.5 Return type annotation always required (user)

**Decided.** `fn f(): void { }` — no omission, including for void. User rationale: modern tooling
(cited: basedpyright auto-completing ~45% of annotations and adding missing headers) means strict
boundaries cost the author little.

### 1.6 C standard target — deferred (user)

**Deferred by user.** C99 is a small clean spec; C11 is modern but heavier. The phase 1 subset
applies to both. Decision to be made when a genuine fork appears.

**Forks Claude expects to surface later:** `_Bool` / `<stdbool.h>`, anonymous structs and unions
(C11), `_Static_assert` (C11). Note that `_Generic` is *not* a fork, per §1.1.

---

## 2. Consequences of strict typing over `int`/`char`/`float` — all raised by Claude

The no-implicit-casting rule is sound but now conflicts with C's defaults in several places. Each
needs an explicit spec decision, because C will silently do something different from what funC
claims.

### 2.1 `/` versus `//` on integers — the significant one

In C, `int / int` already truncates toward zero. If `//` maps to C's `/`, the two operators are
identical on integers and one is redundant. Options:

| Option | Semantics | Assessment |
|---|---|---|
| A | `/` is a **type error** on integer operands; `//` required | Most consistent with no-implicit-casting. `/` means true division, yielding float; ints don't support it. **Claude's recommendation.** |
| B | `/` truncates toward zero (C), `//` floors (Python) | Differ on negatives: `-7 / 2 == -3` but `-7 // 2 == -4`. Requires emitting a helper for `//`, not bare `/`. |
| C | `/` on ints promotes to float | Violates the user's own rule. Rejected. |

**Open:** also undecided is `//` on *floats* — floor-division-yielding-float (Python's
`3.5 // 2.0 == 1.0`), or a type error?

### 2.2 `%` on floats does not exist in C

C's `%` requires integer operands. Either ban `%` on float in funC (falls out of the type checker
naturally) or emit `fmod()` and carry `<math.h>` plus `-lm`. Claude recommends integer-only for
phase 1.

### 2.3 `char + char` promotes to `int` in C

If funC specifies `char + char : char`, the emitted C performs arithmetic at a different width than
funC's semantics claim, and overflow behavior diverges. Correct emission requires an explicit
narrowing cast: `(signed char)(a + b)`.

Flagged as an excellent early test case for the differential interpreter (session 01, §6).

### 2.4 `char` signedness is implementation-defined in C

Bare `char` is signed on x86 Linux and unsigned on ARM. Unacceptable for a language with formal
aspirations. **Recommendation:** fix funC's `char` as explicitly signed or unsigned in the spec, and
always emit `signed char` / `unsigned char` — never bare `char`.

### 2.5 Fixed-width types

C guarantees `int` is only ≥16 bits. **Recommendation:** define funC `int` as exactly 32-bit two's
complement, emit `int32_t` from `<stdint.h>`; define `float` as IEEE-754 binary32. Costs nothing now
and yields a spec that makes checkable claims.

Consequence: strictly correct printing of `int32_t` uses `PRId32` from `<inttypes.h>` rather than
`%d`. `%d` works on every realistic platform. Since format synthesis is table-driven, the pedantic
version is one column of a table — Claude leans toward taking it.

### 2.6 Literals

With no implicit casting, `let x: float = 0;` is a **type error** — `0` is an int literal, `0.0` is
required. Char literals (`'a'`) must enter the lexer, with an escape-sequence set decided
(minimally `\n \t \\ \' \0`).

**Recommended rule:** every literal has exactly one type determined by its lexical form. No
inference, no context-sensitivity.

### 2.7 An explicit cast operator will be needed

Not phase 1, but banning implicit conversion means there must eventually be *some* way to move an
`int` into a `float`. Reserve a syntax slot mentally (`as`, or `cast<T>(x)`).

### 2.8 Float print format

Undecided: `%f` (fixed, 6 decimals), `%g` (shortest reasonable), or `%.17g` (round-trippable).
Claude suggests `%g` as the friendliest default.

---

## 3. Expression parsing — elaboration requested by user

Both approaches are recursive descent; they differ in how precedence is encoded.

### 3.1 One function per precedence level (cascading)

One function per level, each calling the next-tighter level and looping on its own operators:

```
parse_additive       → handles + -,        calls parse_multiplicative
parse_multiplicative → handles * / % //,   calls parse_unary
parse_unary          → handles prefix -,   calls parse_primary
parse_primary        → literals, identifiers, ( expr )
```

Each body has the same shape: parse a tighter operand, then
`while (next token is one of mine) { consume; parse tighter operand; build binary node; }`.

**Pros.** The EBNF-to-code correspondence is literal — one production, one function. Directly serves
aim 3: grammar and parser can be placed side by side and eyeballed for conformance. Most readable
option for someone returning to the code later.

**Cons.** Each new precedence level is another near-identical function plus rewiring of the chain.
Adding comparison, equality, logical and/or, and ternary puts this at 8–10 lookalike functions.
Inserting an operator mid-chain means editing two functions.

### 3.2 Precedence climbing / Pratt

One function parameterized by a minimum binding power, driven by an operator table:

```
parse_expr(min_bp):
    left = parse_unary()
    while next is a binary op with bp >= min_bp:
        op = consume()
        right = parse_expr(op.left_assoc ? op.bp + 1 : op.bp)
        left = make_binary(op, left, right)
    return left
```

Associativity is the `+1`: bumping the minimum for left-associative operators prevents the recursive
call from swallowing another operator at the same level, so `a - b - c` groups as `(a - b) - c`.
Right-associative operators do not bump, so they nest rightward.

**Pros.** One function permanently. Adding an operator is a one-line table edit. Generalizes (as
Pratt parsing) to prefix, postfix, ternary, indexing, and call syntax via per-token handlers.

**Cons.** Grammar correspondence is no longer readable off the page — conformance becomes an
argument about the table rather than a visual check. Associativity takes more care to get right
initially.

### 3.3 Claude's recommendation

**Cascading for milestone 1.** Only four levels exist, the grammar correspondence directly serves
aim 3, and bugs are visually obvious.

**Then refactor to Pratt** when booleans and comparisons push the function count up — as a
deliberate exercise. **Keep the old parser.** Parser A can then be differential-tested against
parser B on randomly generated expressions, asserting identical ASTs. A free, high-quality
verification harness falling out of a refactor that was going to happen anyway.

### 3.4 EBNF trap flagged for the spec

The natural way to write these productions is left-recursive:

```
additive := additive "+" multiplicative | multiplicative
```

Recursive descent **cannot execute this** — `parse_additive` recurses immediately without consuming
a token. The standard rewrite is iterative:

```
additive := multiplicative { ("+" | "-") multiplicative }
```

Same language, but the second is what the code implements. **Spec decision required:** write the
EBNF left-recursively (mathematically conventional, encodes associativity) with a documented
transformation, or iteratively (matches the code, but associativity must then be stated separately
in a table).

**Claude's recommendation:** iterative form plus an explicit associativity table. Keeps spec and
implementation mutually checkable, which is the point.

### 3.5 Phase 1 precedence, tightest first

```
primary  →  unary -  →  * / % //  →  + -
```

All binary operators left-associative.

### 3.6 Reading

- *Crafting Interpreters* ch. 6 (cascading) and ch. 17 (Pratt) — same author, same language,
  cleanest available A/B comparison.
- Eli Bendersky, "Parsing expressions by precedence climbing" — canonical short comparison.
- **chibicc** uses cascading throughout — existence proof that it scales further than expected.

---

## 4. Open questions carried to session 03

1. `/` on integer operands: type error (option A) or truncating (option B)? And `//` on floats:
   floor-yielding-float, or type error?
2. `%` on floats: banned, or emit `fmod`?
3. `char`: signed or unsigned, fixed by spec?
4. Fixed-width types (`int32_t`, IEEE binary32) via `<stdint.h>`, or C's native `int` / `float`?
5. Float print format: `%f`, `%g`, or `%.17g`?
6. Division by zero: undefined, trapped, or checked?
7. Char literal escape set for the lexer.
8. Is `print` a dedicated statement production, or a call expression used as an expression-statement?
   (Claude recommends dedicated: smaller grammar, better errors, given funC has no other expression
   statements.)
9. EBNF form: left-recursive with documented transformation, or iterative with associativity table?
10. Carried from session 01, still open: AST vs. syntax-directed translation.
11. Carried from session 01: is `let`'s right-hand side a full expression? (Assumed yes.)

---

## 5. Next actions

- [ ] User answers §4.
- [ ] Draft EBNF for milestone 1 (blocked on Q9, Q8).
- [ ] Draft the `print` contract precisely — separator, trailing newline, per-type format table
      (blocked on Q4, Q5).
- [ ] Draft the type judgment table: for each operator, which operand type pairs are legal and what
      the result type is (blocked on Q1, Q2).
- [ ] Draft small-step operational semantics for the phase 1 subset.
- [ ] Decide AST vs. syntax-directed and begin the lexer.
