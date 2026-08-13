# funC Transpiler — Design Notes, Session 04

**Date:** 2026-08-12
**Status:** Pre-implementation. No code written.
**Scope:** Resolution of the `/` question; `@cast` design; drafted milestone-1 EBNF and lexical
grammar.

---

## 1. Settled this session

| Item | Decision |
|---|---|
| Comment syntax | `#` line comments. `//` remains integer division. |
| `/` signature | `(float32, float32) → float32` **only**. Integer operands are a type error. |
| `//` signature | `(int32, int32) → int32` only. |
| Casting | Explicit only, via a builtin. Deferred to phase 2. |
| `char8` | Unsigned, ASCII 0–127, emits `uint8_t`. |

### 1.1 The `/` resolution

The user's position, restated: funC permits **explicit casting only**. `int32 / int32` is a type
error; the author must write a cast to obtain float operands. This is option A from session 02,
reached independently.

Claude's operator-signature framing (session 03 §3.2) is therefore unnecessary — there is no
promotion to justify, because `/` simply has no integer signature. Simpler than the proposed
resolution.

### 1.2 Two consequences

**The float32 / float64 precision question is eliminated.** It existed only because
`int32 / int32` needed a result type. That expression is now illegal. When the author later writes
`@cast(float32, x) / @cast(float32, y)`, the target type is named explicitly, so any precision loss
is authored rather than inflicted. Session 03 §3.3's first open question is closed with no decision
required — a concrete instance of strictness removing spec surface rather than adding it.

**Casting remains deferred, as the user originally sequenced it.** Phase 1 needs no cast operator:
integer division is `//`, float division is `/`, and no phase-1 expression requires a conversion.

---

## 2. `@cast` design — raised by Claude

### 2.1 The notation is missing an operand

The user wrote `@cast(int32, float32)` — two types and no value. A cast requires something to
convert. Candidate forms:

| Form | Precedent | Assessment |
|---|---|---|
| `@cast(float32, x)` | Zig's `@as(T, value)` | Target type first. Matches the `ident : type` order already used in `let` and function signatures. **Claude's recommendation.** |
| `@cast(x, float32)` | — | Value first; reads as "convert x to float32." |

The `@` sigil for builtins already matches Zig's convention.

### 2.2 Consistency question: `print` vs `@print`

If `@` marks intrinsics, `print` is one — it performs type-directed code generation and is not a
callable function. As currently specified funC would have `print(x, y)` alongside `@cast(float32, x)`,
which is the same category of construct in two different notations.

**Options:** adopt `@print(x, y)` (Zig-consistent), or retain `print` and record the inconsistency
as deliberate. Rust makes the same distinction visible via `!`.

**Open.**

### 2.3 Lex `@` generically

`@cast` should **not** be a lexer keyword. Lex `@` followed by an identifier as a single `BUILTIN`
token and resolve the name during type checking. Adding `@print`, `@sizeof`, `@ord`, `@chr` then
costs no lexer change — and several such builtins are foreseeable.

### 2.4 Cast legality is a separate table, and one entry is a trap

`float32 → int32` is **undefined behavior** in C when the value is NaN, infinite, or outside
`int32` range — undefined, not implementation-defined. It does not reliably truncate; x86 yields
`INT_MIN`, other targets vary.

That conversion therefore requires the same treatment as division by zero: a helper that checks and
aborts. The trapping mechanism from session 03 §7 now has a second customer, which is corroborating
evidence that the mechanism is correct.

Additional decisions required when the cast table is drafted:

- Rounding mode for `float32 → int32`. C truncates toward zero; round-to-nearest is an alternative.
- Whether `int32 → char8` range-checks against 0–127.
- Whether identity casts (`@cast(int32, x)` where `x : int32`) are a no-op or an error.

---

## 3. Remaining blocker

**`//` and `%`: truncating or flooring?** They must agree, so that
`a == (a // b) * b + (a % b)` holds.

| | `-7 // 2` | `-7 % 2` | Emission |
|---|---|---|---|
| Truncating (C) | `-3` | `-1` | bare `/` and `%`; free |
| Flooring (Python) | `-4` | `1` | helper for both |

Flooring is the consistent choice given that `//` is the "discrete division" operator and borrows
Python's spelling — but it costs a helper and a divergence from the host language that the
differential interpreter must model correctly. Truncating is free and matches C.

This is a semantics decision, not a cost decision. **Open.**

---

## 4. Milestone 1 lexical grammar (draft)

### 4.1 Token classes

```
IDENT       = letter { letter | digit | "_" } ;
INT_LIT     = digit { digit } ;
FLOAT_LIT   = digit { digit } "." digit { digit } ;
CHAR_LIT    = "'" ( char_escape | ascii_printable ) "'" ;
BUILTIN     = "@" IDENT ;

KEYWORD     = "fn" | "let" | "set" | "return" | "print"
            | "int32" | "float32" | "char8" | "void" ;

PUNCT       = "(" | ")" | "{" | "}" | ":" | ";" | "," | "="
            | "+" | "-" | "*" | "/" | "//" | "%" ;

char_escape = "\\" ( "n" | "t" | "\\" | "'" | "0" ) ;
letter      = "a".."z" | "A".."Z" | "_" ;
digit       = "0".."9" ;
```

### 4.2 Trivia

```
comment     = "#" { any_char_except_newline } newline ;
whitespace  = " " | "\t" | "\r" | "\n" ;
```

Both discarded by the scanner. Every emitted token carries a span: line, column, byte offset
(session 01 §5).

### 4.3 Lexical notes

- **`//` before `/`.** The scanner must attempt the two-character token first, or `5 // 2` lexes as
  two divisions. Standard maximal-munch.
- **No `--` token.** With `++` / `--` dropped (session 03 §6.2), `- -x` and `--x` both lex as two
  `MINUS` tokens. Harmless, but worth stating so it isn't later mistaken for a decrement.
- **`FLOAT_LIT` requires digits on both sides.** `1.` and `.5` are rejected. This keeps `1.` from
  colliding with a future member-access `.` and matches the one-lexical-form-one-type rule
  (session 02 §2.6).
- **`CHAR_LIT` is restricted to ASCII 0–127** per §1. A non-ASCII byte inside quotes is a lexical
  error, not a silent truncation.
- Keywords are recognized by post-filtering `IDENT`, not by separate scanner states.

---

## 5. Milestone 1 EBNF (draft)

Iterative form, per the user's session 02 decision. Associativity is stated separately in §6.

```
program        = { function } ;

function       = "fn" IDENT "(" [ params ] ")" ":" type block ;
params         = param { "," param } ;
param          = IDENT ":" type ;
block          = "{" { statement } "}" ;

statement      = let_stmt
               | set_stmt
               | print_stmt
               | return_stmt ;

let_stmt       = "let" IDENT ":" type "=" expr ";" ;
set_stmt       = "set" IDENT "=" expr ";" ;
print_stmt     = "print" "(" [ arg_list ] ")" ";" ;
return_stmt    = "return" [ expr ] ";" ;

expr           = additive ;
additive       = multiplicative { ( "+" | "-" ) multiplicative } ;
multiplicative = unary { ( "*" | "/" | "//" | "%" ) unary } ;
unary          = [ "-" ] primary ;
primary        = INT_LIT
               | FLOAT_LIT
               | CHAR_LIT
               | call
               | IDENT
               | "(" expr ")" ;

call           = IDENT "(" [ arg_list ] ")" ;
arg_list       = expr { "," expr } ;

type           = "int32" | "float32" | "char8" | "void" ;
```

### 5.1 Decisions embedded in this draft — each needs confirmation

1. **`return_stmt` makes the expression optional.** Given that `: void` must always be written
   (session 02 §1.5), consistency might argue for `return;` being *mandatory* in void functions —
   an explicit terminator matching the explicit annotation. Alternatively drop the optional form and
   require every function to end in a return. **Open.**

2. **`unary = [ "-" ] primary`** permits only a single negation. The recursive alternative,
   `unary = "-" unary | primary`, permits `- -x`. The recursive form is the more conventional
   grammar; the iterative form is arguably more honest about what anyone would write. **Open.**

3. **`primary` lists `call` before `IDENT`.** Both begin with `IDENT`, so the parser needs one token
   of lookahead on `(` to choose. Worth noting explicitly because it is the only place in the
   milestone-1 grammar requiring lookahead beyond the current token.

4. **`print` is a keyword and `print_stmt` is a dedicated production**, per the user's session 02
   decision. If §2.2 resolves toward `@print`, this production changes to consume a `BUILTIN` token
   and `print` leaves the keyword list.

5. **`call` is in the grammar but user-defined function calls are arguably out of milestone-1
   scope** — the seed example has only `main`. Including `call` costs little and makes the grammar
   usable for milestone 2; excluding it shrinks the type checker. **Open.**

6. **No `@cast` production**, per §1.2. It enters in phase 2 as another `primary` alternative.

### 5.2 Grammar properties worth asserting in the spec

- **Assignment is a statement, not an expression** (session 01 §3.2). `if (x = 5)` and `a = b = c`
  are unrepresentable — not rejected by a rule, but absent from the grammar.
- **`let` requires an initializer** by construction, so uninitialized-variable UB cannot be
  expressed.
- **Every statement terminates in `;`** and every block is brace-delimited, so no
  offside/indentation rules and no dangling-else problem (there is as yet no `if`).

---

## 6. Associativity and precedence table (milestone 1)

Required as a companion to the iterative EBNF, which does not itself encode associativity.

| Level | Operators | Associativity | Arity |
|---|---|---|---|
| 1 (loosest) | `+` `-` | left | binary |
| 2 | `*` `/` `//` `%` | left | binary |
| 3 | `-` | — | unary prefix |
| 4 (tightest) | literals, `IDENT`, `call`, `( )` | — | primary |

Parser functions map one-to-one onto levels 1–4, per the user's cascading decision (session 02 §3.3,
confirmed session 03).

---

## 7. Open questions carried to session 05

1. **`//` and `%`: truncating or flooring?** (§3 — the blocker for the type table and the
   operational semantics.)
2. `@cast` argument order: `@cast(float32, x)` or `@cast(x, float32)`? (§2.1)
3. `print` or `@print`? (§2.2)
4. `return` in void functions: optional, or mandatory? (§5.1.1)
5. Unary minus: single or recursive? (§5.1.2)
6. Are user-defined function calls in milestone 1 scope? (§5.1.5)
7. Type name style: `int32` / `float32` (assumed in this draft) or `i32` / `f32`?
8. Carried from session 03: is `<` legal on `char8`?
9. Carried from session 03: confirm trapping division by zero.

---

## 8. Next actions

- [ ] User answers §7. Question 1 is blocking.
- [ ] Finalize the type judgment table (session 03 §8) once Q1 lands.
- [ ] Draft the `print` contract: separator, trailing newline, per-type format table.
- [ ] Draft small-step operational semantics for the milestone-1 subset, including the trapping rule
      for division.
- [ ] Begin the lexer. The lexical grammar (§4) is complete enough to start once Q3 and Q7 are
      settled.
