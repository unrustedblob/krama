# Krama Transpiler — Design Notes, Session 05

**Date:** 2026-08-12
**Status:** Pre-implementation. No code written.
**Scope:** Division definition correction; trapping mechanism clarified; runtime prelude introduced;
grammar revised for `@print`, mandatory return, and removal of user-defined calls.

---

## 1. Settled this session

| Item | Decision |
|---|---|
| Type names | Rust style — `i32`, `f32`. `char8` → `c8` proposed (§5.1). |
| `@cast` argument order | `@cast(x, f32)` — "cast x as f32". |
| `print` | Becomes `@print`. `@` denotes intrinsics generally. |
| `return` in void functions | Mandatory. |
| User-defined function calls | Out of milestone 1 scope. `main` only. |
| Comparison operators on `c8` | Legal. Prior art: Rust. (Requires `bool` — phase 2.) |
| Division by zero | Trap. Mechanism clarified in §3. |

The user's rationale for `@` is recorded: it establishes an extensible intrinsic namespace —
`@type`, `@comp`, `@is(i32)`, `@derive` were named as plausible future members.

---

## 2. CORRECTION: Euclidean and floored division are different — raised by Claude

The user answered "Euclidean/standard math division. Flooring it is." These are two distinct
definitions that disagree.

**Euclidean** (the division algorithm from number theory): for any `a` and `b ≠ 0` there exist
unique `q, r` with `a = bq + r` and **`0 ≤ r < |b|`**. The remainder is always non-negative.

**Floored** (Python): the remainder carries the **sign of the divisor**.

They coincide whenever the divisor is positive — which is why they are easily conflated. They
diverge only on negative divisors:

| Definition | `7 // -2` | `7 % -2` | `-7 // 2` | `-7 % 2` |
|---|---|---|---|---|
| Truncated (C) | `-3` | `1` | `-3` | `-1` |
| Floored (Python) | `-4` | `-1` | `-4` | `1` |
| Euclidean (math) | `-3` | `1` | `-4` | `1` |

All three satisfy `a == (a // b) * b + (a % b)`. They differ in whether the sign lands on the
quotient or the remainder.

**Guidance:** if the motivation was "standard math," the answer is **Euclidean** — a non-negative
remainder is what makes modular arithmetic and array indexing behave. If the motivation was "matches
Python," the answer is **floored**. The emitted helper costs the same either way; this is purely a
semantics choice.

**Reference:** Raymond Boute, *The Euclidean definition of the functions div and mod* — the
canonical argument for Euclidean, and short.

**Status: open.** Still the blocker for the type table and the operational semantics.

---

## 3. Trapping clarified — user's hardware concern resolved

**User's question:** is trapping a hardware trap requiring per-architecture code, and does it force
UB into the spec?

**Answer: no to both.** The trap is a **software check in the emitted C**, executed *before* the
division:

```c
static inline int32_t funC_div_i32(int32_t a, int32_t b) {
    if (b == 0)                     funC_trap("division by zero");
    if (a == INT32_MIN && b == -1)  funC_trap("division overflow");
    /* ... */
}
```

Because the check precedes the operation, the faulting instruction is never reached. No signals, no
handlers, no architecture-specific code, no UB. Portable C throughout.

### 3.1 Why this is the right call — the hardware divergence

What happens *without* a check is exactly the argument for having one:

- **x86** — `idiv` raises `#DE`; the OS delivers `SIGFPE`; the process dies.
- **ARM** — `sdiv` does not trap. It returns 0 for division by zero and wraps silently on
  `INT_MIN / -1`.

Same source, same C compiler, two entirely different observable behaviors. This is why the C
standard declines to define the case. The software check collapses that nondeterminism into a
single defined, portable outcome — one that can be stated in the operational semantics and tested
end to end.

### 3.2 Terminology for the spec

"Trap" in funC means **software-checked abort**, not a hardware trap. Worth stating explicitly or a
future reader will assume signals are involved.

### 3.3 Failure mechanism

`abort()` versus `exit(1)`:

- **`abort()`** raises `SIGABRT`, produces a core dump, and drops into gdb at the fault site —
  directly serving aim 2. Exit code 134 remains testable.
- **`exit(1)`** is cleaner and gives a conventional exit code.

**Recommendation:** write a diagnostic to stderr, then `abort()`.

---

## 4. New project component: the runtime prelude

The trapping helpers have to live somewhere. Current and foreseeable members:

- `funC_trap(msg)` — diagnostic + abort
- `funC_div_i32`, `funC_mod_i32` — zero and overflow checks, plus the Euclidean/floored adjustment
  once §2 is decided
- `funC_f32_to_i32` — the cast check (session 04 §2.4), phase 2

**Two placement options:**

| Option | Description | Assessment |
|---|---|---|
| Inline prelude | Emit as `static inline` at the top of every generated `.c` | Self-contained. `cc out.c` works with no link flags. **Recommended for milestone 1.** |
| Separate `funcrt.c` | Link against a runtime library | Worthwhile only once multiple translation units exist. |

Note this interacts with the `#line` directive plan (session 01 §7): prelude lines have no funC
source position, so `#line` emission must begin *after* the prelude.

---

## 5. Consequences of the naming and scope decisions

### 5.1 `char8` under Rust-style naming

Rust's character type is `char` with no width suffix, because a Unicode scalar value has exactly one
width. funC's explicit-width rule and the Rust naming style pull apart here:

- **`c8`** — consistent with `i32` / `f32`, no prior art.
- **`char`** — familiar, but breaks the explicitness rule for exactly one type.

**Recommendation:** `c8`, with the deviation from Rust noted in the spec.

### 5.2 `@cast(x, f32)` takes a type as its second argument

`arg_list = expr { "," expr }` does not cover builtin calls, since the second argument is a type,
not an expression.

**Recommended handling:** define `builtin_arg = type | expr`. The parser distinguishes by token
(type names are keywords); the **type checker** enforces each builtin's actual signature. This also
covers `@is(i32)` and a future `@sizeof(T)` with no further grammar work.

### 5.3 Mandatory return has a free enforcement mechanism in milestone 1

With no branching in the language yet, `return` can be made the *grammatical terminator* of a block:

```
block = "{" { statement } return_stmt "}" ;
```

This yields mandatory-return **and** no-dead-code with zero flow analysis. It must be relaxed to
real flow analysis once `if` arrives, but costs nothing now.

### 5.4 `@print` dissolves the dedicated statement production

`print_stmt` is replaced by a generic `builtin_stmt`. The good error messages that motivated the
dedicated production are not lost — they move to the type checker, which is where argument types are
available anyway.

### 5.5 `...` inferred declarations do not conflict with the literal rule

Noted for the record. The one-type-per-lexical-form rule (session 02 §2.6) fixes `0` as `i32`
regardless of context. `let x: ... = expr` merely copies the type `expr` already has. Different
mechanisms, no tension. Not phase 1.

---

## 6. CORRECTION: the Q5 answer contradicts its stated reason — raised by Claude

User answered: "Recursive. if `- -x` encapsulates that code has to be written as `-(-x)`, single if
not."

`-(-x)` is legal under **both** forms — `(-x)` is a parenthesized primary, and any primary may
follow a unary minus. The forms differ only on the *unparenthesized* case:

| Form | Grammar | `-(-x)` | `- -x` |
|---|---|---|---|
| Single | `unary = [ "-" ] primary` | legal | syntax error |
| Recursive | `unary = "-" unary \| primary` | legal | legal |

The stated reason — that parenthesization should be required — describes the **single** form. The
answer given was "recursive."

**Status: open.** Needs the user to confirm which was intended.

---

## 7. Revised milestone 1 lexical grammar

```
IDENT       = letter { letter | digit | "_" } ;
INT_LIT     = digit { digit } ;
FLOAT_LIT   = digit { digit } "." digit { digit } ;
CHAR_LIT    = "'" ( char_escape | ascii_printable ) "'" ;
BUILTIN     = "@" IDENT ;

KEYWORD     = "fn" | "let" | "set" | "return"
            | "i32" | "f32" | "c8" | "void" ;

PUNCT       = "(" | ")" | "{" | "}" | ":" | ";" | "," | "="
            | "+" | "-" | "*" | "/" | "//" | "%" ;

char_escape = "\\" ( "n" | "t" | "\\" | "'" | "0" ) ;
comment     = "#" { any_char_except_newline } newline ;
```

**Changes from session 04:** `print` removed from `KEYWORD` (now a `BUILTIN`); type names shortened
to `i32` / `f32` / `c8`.

Maximal munch on `//` before `/` still applies. No `--` token. `FLOAT_LIT` still requires digits on
both sides of the point. Every token carries line, column, and byte offset.

---

## 8. Revised milestone 1 EBNF

```
program        = function ;

function       = "fn" IDENT "(" [ params ] ")" ":" type block ;
params         = param { "," param } ;
param          = IDENT ":" type ;

block          = "{" { statement } return_stmt "}" ;

statement      = let_stmt
               | set_stmt
               | builtin_stmt ;

let_stmt       = "let" IDENT ":" type "=" expr ";" ;
set_stmt       = "set" IDENT "=" expr ";" ;
builtin_stmt   = BUILTIN "(" [ builtin_args ] ")" ";" ;
return_stmt    = "return" [ expr ] ";" ;

builtin_args   = builtin_arg { "," builtin_arg } ;
builtin_arg    = type | expr ;

expr           = additive ;
additive       = multiplicative { ( "+" | "-" ) multiplicative } ;
multiplicative = unary { ( "*" | "/" | "//" | "%" ) unary } ;
unary          = [ "-" ] primary ;                  (* or recursive — §6 *)
primary        = INT_LIT | FLOAT_LIT | CHAR_LIT | IDENT | "(" expr ")" ;

type           = "i32" | "f32" | "c8" | "void" ;
```

**Changes from session 04:**

- `program = function` — a single function, `main`, per the milestone 1 scope decision. (Alternative:
  keep `program = { function }` and have the *checker* reject anything but a lone `main`, which
  yields a better message than a syntax error. **Open.**)
- `call` production removed; `primary` no longer requires lookahead past the current token.
- `print_stmt` → generic `builtin_stmt`.
- `block` now requires a terminating `return_stmt` (§5.3).
- `builtin_arg` admits a type (§5.2).

### 8.1 Properties assertable in the spec

- Assignment is a statement, not an expression — `if (x = 5)` and `a = b = c` are unrepresentable.
- `let` requires an initializer, so uninitialized-variable UB cannot be expressed.
- Every block ends in exactly one `return`; dead code after `return` is unrepresentable.
- Every statement terminates in `;`; every block is brace-delimited. No offside rule, no
  dangling-else.
- The grammar requires at most one token of lookahead.

---

## 9. Associativity and precedence (unchanged)

| Level | Operators | Associativity | Arity |
|---|---|---|---|
| 1 (loosest) | `+` `-` | left | binary |
| 2 | `*` `/` `//` `%` | left | binary |
| 3 | `-` | — | unary prefix |
| 4 (tightest) | literals, `IDENT`, `( )` | — | primary |

---

## 10. Open questions carried to session 06

1. **Division definition: Euclidean or floored?** (§2 — still the blocker.)
2. Unary minus: single or recursive? The answer and its rationale disagree. (§6)
3. `c8` or `char` for the character type? (§5.1)
4. `program = function`, or `program = { function }` with a checker-level restriction? (§8)
5. Confirm `abort()` over `exit(1)` for the trap. (§3.3)
6. Confirm the inline runtime prelude over a linked `funcrt.c`. (§4)

---

## 11. Next actions

- [ ] User answers §10. Question 1 remains blocking.
- [ ] Finalize the type judgment table (session 03 §8) with `i32` / `f32` / `c8` naming.
- [ ] Draft the `@print` contract: separator, trailing newline, per-type format table.
- [ ] Draft small-step operational semantics for milestone 1, including the trapping rule.
- [ ] Begin the lexer — §7 is complete pending Q3.
