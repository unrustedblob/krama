# Krama Transpiler — Design Notes, Session 06

**Date:** 2026-08-12
**Status:** Pre-implementation. Specification closed for milestone 1. No code written.
**Scope:** Final blocking answers; `program` production resolved; differential interpreter
explained.

---

## 1. Settled this session — the blocking set is now empty

| Item | Decision |
|---|---|
| Division definition | **Euclidean** — remainder always non-negative |
| Unary minus | **Single** — `- -x` is a syntax error; `-(-x)` required |
| Character type name | **`c8`** |
| `program` production | **`program = { function }`**, restriction enforced in the checker |
| Trap mechanism | **`abort()`** after a stderr diagnostic |
| Runtime prelude placement | **Deferred** — no impact on the AST, which is the current focus |

---

## 2. Euclidean division — what it commits to

**Defining property, now stateable and testable:** for `b ≠ 0`, `a % b` lies in `[0, |b|)`.
Non-negative regardless of the sign of either operand.

**Practical payoff:** `i % n` is safe as an array index without a guard. This is precisely where
C's truncating `%` bites.

### 2.1 Helper derivation (called "helper1" by the user)

The emitted runtime helper derives from C's truncating `/` and `%`: take C's remainder, and when it
is negative, add `|b|` and adjust the quotient by one in the direction of `b`'s sign.

**Edge case flagged:** computing `|b|` overflows when `b == INT32_MIN`. The helper must handle this
without ever forming the absolute value. This is exactly the class of bug the trapping design exists
to catch.

### 2.2 Property tests that fall out of the definition

Over random `(a, b)` pairs with `b ≠ 0`:

- `a == (a // b) * b + (a % b)`
- `0 <= a % b < |b|`

Nasty inputs to include deliberately: `b` negative, `a` negative, both negative, `a == INT32_MIN`,
`b == -1`, `b == INT32_MIN`.

**Uniqueness theorem — why this matters more than it appears.** Given `a` and `b ≠ 0`, there is
*exactly one* pair `(q, r)` satisfying both properties above. Therefore any implementation
satisfying both is *the* correct implementation, by theorem rather than by testing. For `//` and `%`
specifically, property testing is a stronger guarantee than differential testing.

---

## 3. Unary minus — correction to the rationale

The user's answer (single) was correct; the stated reason was not.

`a - -b` is **unambiguous under both forms**. The additive loop consumes the binary `-`, then calls
`unary`, which consumes the prefix `-` and takes `b` as its primary. No precedence question arises.

What the single form actually rejects is **consecutive unary minuses on one operand**: `- -x`,
`a - - -b`. Nothing else changes.

**Spec note required:** state explicitly that `a - -b` remains legal, or a reader will interpret
"single unary minus" as having banned it.

---

## 4. `program = function` vs `program = { function }` — elaborated at user request

The question is where a *temporary* restriction should live.

### 4.1 Grammar-level (`program = function`)

- Parser is trivial: parse one function, expect EOF.
- The spec describes what is implemented today — honest, but the grammar becomes a milestone
  artifact rather than a language artifact, and churns every milestone.
- Error for a second function: "expected EOF, found `fn`". Accurate, unhelpful.
- **Does not save checker work.** `program = function` does not say the function is named `main`,
  returns `i32`, or takes no parameters. Checker rules are still required for all three. The result
  is grammar churn without eliminating the layer it was meant to avoid.

### 4.2 Checker-level (`program = { function }`) — chosen

- The grammar describes funC, not funC-milestone-1, and stays stable through growth.
- The parser is a loop — three lines, and it is the code that survives.
- Error becomes "user-defined functions are not yet supported," anchored to the span of the
  offending `fn`. A real diagnostic; diagnostics-as-specified-surface serves aim 3.
- **Cost:** the parser accepts a superset of what compiles.

### 4.3 Why the cost is already paid

The grammar cannot express "operand types must match" either. Syntactic validity and static validity
are already separate layers in funC's design. Placing "only `main` exists" in layer 2 costs nothing
structurally, because layer 2 exists regardless.

### 4.4 Accompanying artifact

A **milestone restrictions** list, recording what the checker rejects that the grammar permits.
Currently: multiple functions, non-`main` function names, parameters, `bool`, casts, calls. This
makes the restrictions explicitly temporary rather than buried in a grammar that would later need
editing, and provides a natural home for "not yet implemented" diagnostics.

Now incorporated into the v1 specification, §12.

---

## 5. Differential interpreter — explained at user request

### 5.1 What it is

One AST, two consumers:

1. **Codegen path** — emit C, compile with `cc`, run the binary, capture stdout.
2. **Interpreter path** — walk the AST directly inside the transpiler process, evaluating and
   printing as it goes.

Same input, two independent routes to an answer. Run both, diff stdout. Disagreement means one is
wrong — found without anyone having written down the correct output in advance.

### 5.2 Why it is worth building

- **Golden `.func` / `.expected` pairs require knowing the answer beforehand**, so they only cover
  programs someone thought to write. Differential testing pairs with a random program generator:
  emit thousands of arbitrary well-typed funC programs, run both paths, compare. No expected outputs
  needed at all.
- Prior art: **Csmith** found hundreds of real bugs in GCC and LLVM by exactly this method.
- It is the concrete form of aim 3. **A tree-walking interpreter is the operational semantics, made
  executable.** When the small-step rules are written on paper, the interpreter should read as a
  near-transliteration. "The transpiler is correct" then becomes "codegen agrees with the
  semantics" — a testable claim rather than an assertion.

### 5.3 Independence requirement — user's question, confirmed

**The interpreter must not call helper1.** Correct.

**Reason: common-mode error.** If both paths run the same code they agree by construction. A bug in
helper1 produces identical wrong answers on both sides, the diff comes back clean, and the suite
reports success while the language is broken — an elaborate apparatus verifying nothing about the
operator it was aimed at.

**Independence must be in the derivation, not the file layout.** Copy-pasting helper1's algorithm
into the interpreter is as bad as calling it: duplicating the reasoning duplicates any error in it.
The interpreter should be the obvious, slow, branchy version written straight from the definition;
the emitted runtime is the tight one built on C's truncating operators.

**Caveat carried from §2.2:** for `//` and `%` the property tests are the stronger guarantee, since
each implementation is checked against the definition rather than against the other. The
differential harness earns its keep on things lacking a clean characterizing property — evaluation
order, statement sequencing, scoping, and later control flow, where "does the whole program behave
the same" is the only available property.

### 5.4 Two traps when building it

**Float width.** If the interpreter holds `f32` values in a C `double` while emitted code uses
`float`, the paths disagree on rounding and phantom bugs follow. The interpreter must store `f32` as
`float`, rounding to 32 bits at the same points codegen does.

**Print must be byte-identical.** The comparison is on stdout, so the interpreter must produce
exactly what `printf` produces — same `%g` behavior, same separator, same trailing newline. Simplest
route: have the interpreter build the same format string codegen would and call `printf`. This
shares *formatting*, not *semantics*, so it introduces no common-mode risk on arithmetic.

### 5.5 Consequence for the AST — relevant now

`--interpret` becomes a mode of the transpiler binary, so the AST must be walkable by two
independent consumers. **Keep evaluation logic out of the AST nodes** — no function pointers hanging
off nodes. Both codegen and the interpreter should be plain switch-driven walks over the node tag.

---

## 6. Status

Specification for milestone 1 is **closed**. See `krama-spec-v1.md`.

Remaining open items are non-blocking and recorded in the specification, §13.

**Next:** baseline coding standards, then AST construction.
