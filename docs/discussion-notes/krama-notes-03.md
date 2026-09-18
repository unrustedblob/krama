# Krama Transpiler — Design Notes, Session 03

**Date:** 2026-08-12
**Status:** Pre-implementation. No code written.
**Scope:** Review of user's session 02 responses; resolution of the `/` contradiction; char
semantics; transpiler-gate justification; drafted type judgment table.

**Document note:** the uploaded response file (`response-session2_md.txt`, 431 lines) contains its
full content twice — lines 216–431 repeat lines 1–215. No content was lost; flagged for cleanup.

---

## 1. Settled this session (no further discussion needed)

| Item | Decision | Source |
|---|---|---|
| Format synthesis | Recursive walk over type tree | User confirmed |
| Type errors are a distinct diagnostic class | Confirmed — user's earlier "fail on parsing" was wording | User |
| `let` RHS | Full expression | User |
| `%` on floats | Type error, banned | User |
| Fixed-width types | Yes; funC surface syntax is explicit (`int32`, not `int`) | User |
| Literals | One type per lexical form, no inference | User |
| Cast operator | Deferred until type system layer 1 is proven | User |
| Float print format | `%g` | User |
| Expression parsing | Cascading (one function per precedence level) | User |
| EBNF form | Iterative, with a separate associativity table | User |
| AST vs. syntax-directed | AST | User |
| `print` | Dedicated statement production | User |
| Char escape set | Minimal to start | User |

User's rationale on cascading parsing is worth recording: the "cons" Claude listed (more functions,
manual rewiring) were reframed as **benefits** — C-like minimalism keeps the operator count low, and
the explicit per-level structure aids formal verification. Accepted; this is a coherent position,
not a concession.

---

## 2. BLOCKER: `//` collides with line-comment syntax — raised by Claude

**Not previously identified by either party.**

C, C++, Rust, Go, and Zig all use `//` to open a line comment. funC has committed to `//` as integer
division. A lexer cannot support both: `x // y` and `// comment` are indistinguishable at the
character level without unbounded lookahead.

Python avoids this only because it uses `#` for comments, leaving `//` free.

**Options:**

| Option | Comment syntax | Notes |
|---|---|---|
| A | `#` line comments | Python's resolution. One lexer branch. **Claude's recommendation.** |
| B | `--` line comments | Haskell, Ada, Lua. Costs a lookahead against unary minus / decrement. |
| C | `/* */` only | Works, universally disliked. |
| D | Rename the operator | `div`, `%/`, `~/` (Dart). Loses the visual Python affinity. |

**Must be resolved before the scanner is written.**

---

## 3. Resolved contradiction: `/` on integer operands

### 3.1 The contradiction

User's answer to Q1 stated `/` "works for both integer and floating with casting allowed," and that
generated C would carry `(type)` casts. But `int32 / int32 → float32` requires promoting both
operands, which is implicit conversion — the rule the user banned. This is effectively option C from
session 02, which the user had rejected.

### 3.2 The resolution — raised by Claude

A distinction worth carrying into the spec, because it dissolves the problem:

- **Implicit conversion** is a *general subsumption rule*: "wherever a `float` is expected and an
  `int` appears, insert a coercion." It applies everywhere and is what makes C arithmetic hard to
  reason about.
- **An operator signature** is a *specific typing judgment for one operator*. Declaring
  `/ : (int32, int32) → float32` introduces no general coercion. Division simply has that type.

funC can therefore have exactly the behavior the user wants while keeping the no-implicit-conversion
rule fully intact. Mixed operands (`int32 / float32`) remain a type error — there is no rule that
would make them agree.

**Falls out correctly:** `let x: int32 = 5 / 2;` is a type error, because `/` yields float. This is
precisely the class of bug the strictness rule exists to catch.

**Status:** proposed, awaiting user confirmation.

### 3.3 Two consequences requiring decisions

**Precision.** float32 carries a 24-bit mantissa; `int32 / int32 → float32` is lossy above roughly
1.67e7. If `/` is meant to be the "accurate enough" operator, float32 partially undercuts that.
Either declare `/ : (int32, int32) → float64` (adds `float64` to the type set) or accept and
document the loss.

**Rounding for the `//` / `%` pair.** C's `%` truncates toward zero: `-7 % 2 == -1`. If `//` floors
(`-7 // 2 == -4`), the pair no longer satisfies `a == (a // b) * b + (a % b)`. The choice must be
made once for both operators:

- **Truncating** — matches C, emits bare `/` and `%`, no helper needed.
- **Flooring** — matches Python, requires a helper function for both.

---

## 4. User query: "Is this over-engineering?"

**Claude's answer: no — but the justification needs trimming, not the feature.**

Not over-engineering because the cost is one row in a type table and no implementation complexity.
Over-engineering is when conceptual weight materializes as code weight; here it does not.

**Caveat on the rationale.** The CPU-cores example is about *domain*, and a type system cannot know
a number represents core counts. `//` does not encode that intent. What it encodes is "the result
must be a whole number" — real, but more modest than intent capture. Intent capture is the job of
newtypes (`type Cores = int32`), which belongs to a later phase. Keep the operator; narrow the
claim.

---

## 5. User query: "Does the transpiler gate make sense?"

**Claude's answer: yes, and the stated premise is partly wrong.**

### 5.1 What C does *not* prevent

The user's concern was that transpiling to C forecloses a custom memory model and concurrency
primitives. It largely does not:

- Arenas, refcounting, region allocators, pool allocators — all straightforwardly expressible in C.
- Concurrency — C11 `<stdatomic.h>` and pthreads provide real primitives.
- Prior art: GHC, Nim, Vala, Chicken Scheme, cfront (original C++), and early Go all shipped through
  C backends.

### 5.2 What C genuinely denies

- **Precise moving GC.** The C stack cannot be portably walked for roots. Workaround is a shadow
  stack, which costs performance and pollutes codegen.
- **Guaranteed tail calls.** Available only at the C compiler's discretion — not a semantics.
- **Stackful coroutines / green threads.** Requires `ucontext` or assembly; nonportable.
- **Custom calling conventions, precise layout control, SIMD intrinsics.**

None of these are near-term.

### 5.3 The stronger argument for the gate

The session-01 decision to build an AST means the escalation path costs only the backend. Frontend,
type checker, and operational semantics all survive when C emission is replaced by:

- **QBE** — a deliberately small SSA backend designed for exactly this transition; used by `cproc`.
- **LLVM IR** — heavier, more capable.

The gate is therefore not a dead end but the first of N backends behind a stable IR. This directly
answers the user's framing of the transpiler as "proving grounds."

---

## 6. `char` semantics — user's design accepted, with one correction

### 6.1 The user's position (recorded)

Types have two attributes: hardware representation and semantics. `char` denotes a *character*;
arithmetic such as `+` is therefore meaningless on it. Operations on `char` should be type-aligned
to what one would do with literal `'a'` / `'b'`. Unlike C, `char x = 'a'; char y = 'a'; x + y`
should not typecheck.

**Claude's assessment: this is a strong rule and Rust agrees with it** (`char` has no arithmetic).
It also resolves session 02 §2.3 (the `char + char → int` promotion problem) by making the
expression illegal rather than requiring a narrowing cast.

### 6.2 The correction: `++` / `--` contradicts the rule

The user proposed `++` / `--` reading as "next/previous char in sequence." This is the arithmetic
just banned, renamed:

- Successor in *what* sequence? For ASCII, `'z'++` yields `'{'` — that is `+1` on a code point, not
  a linguistic successor.
- For Unicode it is worse: code point order runs through unassigned points, surrogates, and
  combining marks. There is no "next character."
- Undefined at the boundary: what is `'\xFF'++` for an 8-bit char?

**Recommendation:** drop `++` / `--` on char. The coherent operation set is equality comparison plus
**explicit conversion to and from an integer code point** — Rust's `as u32` / `char::from_u32`. This
is exactly what the user's own `int8` / `uint8` proposal already gestures toward; make it the
sanctioned escape hatch.

**Separate open question:** is ordering comparison (`<`) legal on char? Defensible for ASCII,
meaningless for Unicode.

### 6.3 Correction on `char8` / `char16` / `char32`

`char16` should be cut permanently. UTF-16 is a legacy encoding (the Microsoft recollection is
`wchar_t`, 16-bit on Windows), and a UTF-16 code *unit* is not a character — anything outside the
BMP occupies a surrogate pair. `char16` therefore directly contradicts the "char is a character"
rule.

Only `char32` (a Unicode scalar value) fully satisfies the rule. `char8` satisfies it only if
defined as **7-bit ASCII**, not as a UTF-8 code unit.

**Recommendation for phase 1:** exactly one char type — `char8`, **unsigned**, defined as ASCII
0–127, emitting `uint8_t`. Unsignedness follows directly from the semantics (a negative character is
nonsense), which **closes session 02's open question 3** without further deliberation. Defer
`char32`; drop `char16`.

---

## 7. Division by zero — user's sketch has a bug

User proposed emitting a guard along the lines of `if (x) z = y / x;`.

**Problem:** this silently leaves `z` at its previous value. A wrong answer with no diagnostic is
worse than the crash it was meant to prevent.

**Claude's recommendation: trap.** Emit a helper (`funC_div_i32(a, b)`) that checks and aborts with
a message on stderr.

Rationale tied to the project aims:

- The operational semantics can state it cleanly — evaluation halts with an error, rather than being
  undefined or getting stuck. A testable claim.
- End-to-end testable: exit code and stderr are both observable.
- Constant-folding the check away when the divisor is a nonzero literal is a clean, self-contained
  optimization exercise for a later phase.

**Second trap caught by the same helper:** `INT32_MIN // -1` overflows and is undefined in C; it
faults on x86. Classic bug, easy to miss.

**Float division by zero:** do nothing. IEEE-754 already defines it as ±inf / NaN. State that funC
inherits IEEE-754 semantics for float and the question closes.

---

## 8. Drafted type judgment table (phase 1)

Provisional, contingent on §3.3 and §6.3 decisions. Any pair not listed is a type error.

| Operator | Operand types | Result | Emitted C |
|---|---|---|---|
| `+` `-` `*` | `(int32, int32)` | `int32` | native |
| `+` `-` `*` | `(float32, float32)` | `float32` | native |
| `/` | `(int32, int32)` | `float32` or `float64` — §3.3 | cast + native `/` |
| `/` | `(float32, float32)` | `float32` | native |
| `//` | `(int32, int32)` | `int32` | helper (trap; §7) |
| `%` | `(int32, int32)` | `int32` | helper (trap; §7) |
| unary `-` | `(int32)` | `int32` | native |
| unary `-` | `(float32)` | `float32` | native |
| `==` `!=` | `(char8, char8)` | `bool` — phase 2 | native |
| `<` etc. | `(char8, char8)` | open — §6.2 | — |

**Explicitly illegal in phase 1:** all arithmetic on `char8`; `%` and `//` on float; every mixed-type
operand pair.

**Print format table:**

| funC type | Emitted C type | Format |
|---|---|---|
| `int32` | `int32_t` | `%d` or `PRId32` |
| `float32` | `float` | `%g` |
| `char8` | `uint8_t` | `%c` |

---

## 9. Open questions carried to session 04

1. **Comment syntax** — `#`, `--`, or `/* */` only? (Blocks the scanner; §2.)
2. `/` on integer operands → `float32` (lossy) or `float64` (adds a type)? (§3.3)
3. `//` and `%`: truncating (C, free) or flooring (Python, needs a helper)? Same answer for both.
   (§3.3)
4. Confirm the operator-signature framing resolves the implicit-conversion objection. (§3.2)
5. Drop `++` / `--` on char in favor of explicit code-point conversion? (§6.2)
6. Is `<` legal on `char8`? (§6.2)
7. Confirm `char16` cut, `char32` deferred, `char8` = unsigned ASCII. (§6.3)
8. Confirm trapping division. (§7)
9. Type name style: `int32` / `uint32` / `float32` (Odin-like) or `i32` / `u32` / `f32` (Rust-like)?

---

## 10. Next actions

- [ ] User answers §9. Questions 1–3 are the blocking set.
- [ ] Finalize the type judgment table (§8).
- [ ] Draft EBNF for milestone 1 — iterative form plus associativity table. Unblocked once §9 Q1 is
      answered.
- [ ] Draft the `print` contract: separator, trailing newline, per-type format table.
- [ ] Draft small-step operational semantics for the phase 1 subset, including the trapping rule for
      division.
- [ ] Begin the lexer.
