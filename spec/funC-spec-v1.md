# funC — Language and Transpiler Specification, v1 (Milestone 1)

**Status:** Closed for milestone 1. Implementation may begin against this document.
**Date:** 2026-08-12
**Supersedes:** design notes sessions 01–06 (retained as the decision record).

> This is the reference document. Where it disagrees with the session notes, this document wins.
> Items still undecided are isolated in §13 and none of them block the AST.

---

## 1. Overview

funC is a small, explicitly-typed imperative language that transpiles to C. Milestone 1 covers a
single `main` function, three scalar types, arithmetic expressions, and printing.

### 1.1 Design principles

These are the load-bearing commitments. Everything in this document follows from them.

1. **No implicit conversion, anywhere.** There is no coercion rule in the language. Operators have
   declared signatures; operand types must match a signature exactly or the program is rejected.
2. **Explicitness at the boundary.** Every declaration carries a type. Every function carries a
   return type, including `void`. Every function ends in an explicit `return`.
3. **One lexical form, one type.** A literal's type is determined by how it is written, never by
   context.
4. **Semantics before representation.** A type's meaning constrains its operations. `c8` denotes a
   character, so arithmetic on it is meaningless and therefore illegal.
5. **Defined behavior over host behavior.** Where C leaves something undefined, funC either forbids
   it statically or checks it at runtime. funC has no undefined behavior.
6. **Statements are not expressions.** Assignment produces no value. Whole classes of bug are
   unrepresentable rather than diagnosed.

### 1.2 Reference example

funC:

```
fn main(): i32 {
    let x: i32 = 0;
    let y: i32 = 0;
    set x = 5;
    set y = x + y;
    @print(x, y);
    return 0;
}
```

Emitted C:

```c
#include <stdint.h>
#include <stdio.h>

int main(void) {
    int32_t x = 0;
    int32_t y = 0;
    x = 5;
    y = x + y;
    printf("%d, %d\n", x, y);
    return 0;
}
```

---

## 2. Types

| funC | Emitted C | Definition |
|---|---|---|
| `i32` | `int32_t` | 32-bit two's complement signed integer |
| `f32` | `float` | IEEE-754 binary32 |
| `c8` | `uint8_t` | Unsigned; ASCII, values 0–127 |
| `void` | `void` | No value. Legal only as a return type. |

### 2.1 Notes

- Widths are **fixed by the specification**, not inherited from the host. `<stdint.h>` is always
  included.
- `c8` is unsigned by definition — a negative character is meaningless. This closes C's
  implementation-defined `char` signedness entirely; bare `char` is never emitted.
- funC's `c8` is ASCII only. A byte above 127 in a character literal is a **lexical error**, not a
  silent truncation.
- `f32` inherits IEEE-754 semantics, including ±inf and NaN from float division by zero.

### 2.2 Deliberately absent from milestone 1

`bool`, `f64`, `i8`/`u8`/`i16`/`u16`/`i64`/`u64`, `c32`, structs, arrays, pointers, strings.

`c16` is **permanently excluded**: UTF-16 is a legacy encoding and a UTF-16 code unit is not a
character (anything outside the BMP occupies a surrogate pair), which contradicts principle 4.

---

## 3. Lexical grammar

### 3.1 Tokens

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

char_escape = "\" ( "n" | "t" | "\" | "'" | "0" ) ;
letter      = "a".."z" | "A".."Z" | "_" ;
digit       = "0".."9" ;
```

### 3.2 Trivia

```
comment     = "#" { any_char_except_newline } newline ;
whitespace  = " " | "\t" | "\r" | "\n" ;
```

Both are discarded by the scanner and do not reach the parser.

### 3.3 Scanning rules

- **Maximal munch.** `//` must be attempted before `/`, or `5 // 2` scans as two divisions.
- **`#` for comments, not `//`.** funC uses `//` as an operator, so it cannot also open a comment.
  This is Python's resolution of the same collision.
- **No `--` token.** `++` and `--` do not exist in funC. `- -x` scans as two `MINUS` tokens (and is
  then rejected by the parser, §4.4).
- **`FLOAT_LIT` requires digits on both sides of the point.** `1.` and `.5` are rejected. This keeps
  `1.` from colliding with a future member-access `.` and upholds principle 3.
- **Keywords are recognized by post-filtering `IDENT`**, not by separate scanner states.
- **`@` is scanned generically.** `@` followed by an identifier produces one `BUILTIN` token. The
  name is resolved during type checking, not in the scanner. Adding `@cast`, `@is`, `@sizeof`
  requires no scanner change.
- **Every token carries a span:** line, column, and byte offset. Non-negotiable — required for
  diagnostics and for `#line` emission (§10.3).

### 3.4 Literal typing

| Lexical form | Type |
|---|---|
| `INT_LIT` | `i32` |
| `FLOAT_LIT` | `f32` |
| `CHAR_LIT` | `c8` |

There is no inference and no context-sensitivity. `let x: f32 = 0;` is a **type error** —
`0` is `i32`. `0.0` is required.

---

## 4. Syntactic grammar (EBNF)

Written in iterative form, matching the recursive-descent implementation. Associativity is not
encoded by the grammar and is specified separately in §5.

```
program        = { function } ;

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
unary          = [ "-" ] primary ;
primary        = INT_LIT | FLOAT_LIT | CHAR_LIT | IDENT | "(" expr ")" ;

type           = "i32" | "f32" | "c8" | "void" ;
```

### 4.1 Left recursion

The conventional presentation of `additive` is left-recursive:

```
additive = additive ( "+" | "-" ) multiplicative | multiplicative ;
```

Recursive descent **cannot execute this** — the function would recurse without consuming a token.
The iterative form above generates the same language and is what the parser implements. The
iterative form is normative for funC; the associativity it fails to encode is supplied by §5.

### 4.2 `program = { function }`

The grammar admits multiple functions. Milestone 1 restricts this **in the type checker**, not the
grammar (§12). Rationale: the grammar describes funC rather than funC-milestone-1 and so remains
stable; the restriction produces a real diagnostic instead of "expected EOF"; and a checker layer is
required regardless, since the grammar cannot express "must be named `main`."

### 4.3 `return` as block terminator

`block` requires exactly one `return_stmt`, positioned last. This delivers mandatory-return and
no-dead-code with zero flow analysis.

**This is a milestone-1 convenience.** It must be relaxed to real flow analysis when `if` arrives.
Recorded so the relaxation is deliberate rather than accidental.

### 4.4 Unary minus is single, not recursive

`unary = [ "-" ] primary` permits at most one prefix minus per operand.

- `-x` — legal
- `-(-x)` — legal (the parenthesized expression is a primary)
- `- -x` — **syntax error**; parenthesization required
- `a - -b` — **legal.** The additive loop consumes the binary `-`, then `unary` consumes the prefix
  `-`. No ambiguity arises. *Stated explicitly because "single unary minus" is easily misread as
  banning this.*

### 4.5 Lookahead

The grammar requires **at most one token of lookahead** throughout. With user-defined calls out of
scope, `primary` needs no lookahead past the current token.

### 4.6 Properties guaranteed by the grammar

Assertable in the spec because they are structural, not enforced by rules:

- **Assignment is a statement.** `if (x = 5)` and `a = b = c` are unrepresentable.
- **`let` requires an initializer.** Uninitialized-variable UB cannot be expressed.
- **Every block ends in exactly one `return`.** Dead code after `return` is unrepresentable.
- **Every statement ends in `;`; every block is brace-delimited.** No offside rule, no dangling-else.

---

## 5. Precedence and associativity

| Level | Operators | Associativity | Arity |
|---|---|---|---|
| 1 (loosest) | `+` `-` | left | binary |
| 2 | `*` `/` `//` `%` | left | binary |
| 3 | `-` | n/a | unary prefix |
| 4 (tightest) | literals, `IDENT`, `( )` | n/a | primary |

Parser functions map one-to-one onto levels 1–4 (§10.2).

---

## 6. Type system

### 6.1 Operator signatures

Any operand pair not listed is a **type error**. There is no promotion, no coercion, no fallback.

| Operator | Operands | Result | Emission |
|---|---|---|---|
| `+` | `(i32, i32)` | `i32` | native `+` |
| `+` | `(f32, f32)` | `f32` | native `+` |
| `-` | `(i32, i32)` | `i32` | native `-` |
| `-` | `(f32, f32)` | `f32` | native `-` |
| `*` | `(i32, i32)` | `i32` | native `*` |
| `*` | `(f32, f32)` | `f32` | native `*` |
| `/` | `(f32, f32)` | `f32` | native `/` |
| `//` | `(i32, i32)` | `i32` | helper (§7) |
| `%` | `(i32, i32)` | `i32` | helper (§7) |
| unary `-` | `(i32)` | `i32` | native `-` |
| unary `-` | `(f32)` | `f32` | native `-` |

### 6.2 Explicitly illegal

- **`/` on integers.** `5 / 2` is a type error. `/` is true division and has no integer signature.
  Integer division is `//`. *(Consequence: `let x: i32 = 5 / 2;` is rejected — `/` yields `f32`.)*
- **`//` and `%` on floats.** Integer-only. C's `%` has no float form; funC does not add one.
- **All arithmetic on `c8`.** Principle 4. `'a' + 'b'` is a type error. This also removes C's
  `char + char → int` promotion problem by making the expression unwritable.
- **Every mixed-type operand pair.** `i32 + f32`, `i32 / f32`, `c8 == i32`, all rejected.

### 6.3 Comparison operators

Legal on `c8` (prior art: Rust). Ordering on ASCII 0–127 is code-point order and well-defined.

**Deferred to phase 2** — comparison yields `bool`, which does not exist in milestone 1.

### 6.4 Diagnostics

Type errors are a **distinct diagnostic class from syntax errors**, reported after a complete parse.
The parser has no type information; `set y = x + z` is syntactically identical regardless of operand
types.

This separation is load-bearing for verification: an operational semantics stating "well-typed
programs do not get stuck" is only meaningful if typing is a separate judgment over the AST.

---

## 7. Division semantics — Euclidean

### 7.1 Definition

For `a` and `b ≠ 0`, `//` and `%` yield the unique pair `(q, r)` satisfying:

```
a == q * b + r
0 <= r < |b|
```

The remainder is **always non-negative**, regardless of the sign of either operand.

### 7.2 Comparison

| Definition | `7 // -2` | `7 % -2` | `-7 // 2` | `-7 % 2` |
|---|---|---|---|---|
| Truncated (C) | `-3` | `1` | `-3` | `-1` |
| Floored (Python) | `-4` | `-1` | `-4` | `1` |
| **Euclidean (funC)** | **`-3`** | **`1`** | **`-4`** | **`1`** |

All three satisfy the first identity; they differ in whether the sign lands on quotient or
remainder. funC chooses Euclidean because a non-negative remainder is the mathematically standard
definition and makes `i % n` safe as an index without a guard.

### 7.3 Implementation

Derives from C's truncating `/` and `%`: take C's remainder, and where negative, add `|b|` and
adjust the quotient by one in the direction of `b`'s sign.

**Edge case:** computing `|b|` overflows when `b == INT32_MIN`. The helper must handle this without
ever forming the absolute value.

### 7.4 Uniqueness

Given `a` and `b ≠ 0`, exactly one `(q, r)` satisfies both constraints in §7.1. **Any implementation
satisfying both is correct by theorem**, not by testing. This makes property testing (§11.3) a
stronger guarantee for `//` and `%` than differential testing.

---

## 8. Runtime behavior and trapping

### 8.1 Trapping is a software check

"Trap" in funC means **software-checked abort**, not a hardware trap. The check is a branch in the
emitted C, executed *before* the operation, so the faulting instruction is never reached.

No signals. No handlers. No architecture-specific code. No undefined behavior. Portable C.

### 8.2 Why — the hardware divergence being avoided

Without a check, behavior is host-dependent:

- **x86** — `idiv` raises `#DE`; the OS delivers `SIGFPE`; the process dies.
- **ARM** — `sdiv` does not trap. It returns 0 for division by zero and wraps silently on
  `INT_MIN / -1`.

Same source, same C compiler, two different observable behaviors. This is why C declines to define
the case, and why the check is required by principle 5. The check collapses that nondeterminism into
one defined, portable, testable outcome.

### 8.3 Trapped conditions

| Condition | Site |
|---|---|
| `b == 0` in `a // b` or `a % b` | integer division helper |
| `a == INT32_MIN && b == -1` | integer division helper (overflow) |
| NaN, infinite, or out-of-range value in `f32 → i32` | cast helper — phase 2 |

Float division by zero is **not** trapped. IEEE-754 defines it as ±inf/NaN and funC inherits that.

### 8.4 Failure mechanism

Write a diagnostic to `stderr`, then call `abort()`.

`abort()` raises `SIGABRT`, produces a core dump, and lands in gdb at the fault site — serving the
tooling aim directly. Exit code 134 remains testable.

### 8.5 Runtime prelude

The helpers (`funC_trap`, `funC_div_i32`, `funC_mod_i32`, later `funC_f32_to_i32`) constitute a
runtime component.

**Placement is deferred** (§13). Options are an inline `static inline` prelude in every generated
`.c` (self-contained; `cc out.c` works with no flags) or a separate linked `funcrt.c` (worthwhile
only with multiple translation units). No impact on the AST.

---

## 9. `@print`

### 9.1 Form

`@print` is a **builtin**, not a function. It performs type-directed code generation and cannot be
declared in the language, since funC has neither variadics nor generics.

`@` denotes the intrinsic namespace generally. Planned members: `@cast`, `@is`, `@type`, `@comp`,
`@derive`, `@sizeof`.

### 9.2 Output contract

- Arguments separated by `", "` — comma, single space.
- A single `"\n"` after the final argument.
- No prefix, no trailing space.

`@print(x, y)` where both are `i32` emits `printf("%d, %d\n", x, y);`.

### 9.3 Format table

| funC type | Emitted C type | Format |
|---|---|---|
| `i32` | `int32_t` | `%d` — see §13 |
| `f32` | `float` | `%g` |
| `c8` | `uint8_t` | `%c` |

`%c` takes an `int`; `uint8_t` promotes automatically via C's default argument promotions. No cast
needed.

### 9.4 Implementation shape — important for later

Format synthesis must be written as a **recursive walk over the type tree**, emitting format-string
fragments and a flattened argument list — even though milestone 1 only ever reaches leaf cases.

A struct would later produce `"{x: %d, y: %g}"` plus `s.x, s.y` from the same walk. Building the
recursion now makes composite support nearly free. This is `deriving Show` / `#[derive(Debug)]`.

C11 `_Generic` is **not needed and should not be used**. A compiler knows its types statically;
`_Generic` is the workaround for code that cannot.

### 9.5 Zero arguments

`@print()` is grammatically legal (§4, `builtin_args` is optional). It emits `printf("\n");`.

---

## 10. Implementation architecture

### 10.1 Pipeline

```
source → scan → parse → AST → type check (annotates AST) → codegen → C
                          └──→ interpret (§11.2)
```

Phases are strictly separated. The AST is the pivot: type checking annotates it, and both codegen
and the interpreter consume it.

**Decided against:** syntax-directed translation (emitting C straight from the parser). It would
work for milestone 1 and be less code, but `@print` needs types, aim 3 needs an artifact to check
the spec against, and the interpreter needs a tree.

### 10.2 Parser

**Cascading recursive descent** — one function per precedence level, mapping one-to-one onto §5.

Chosen over precedence climbing because the EBNF-to-code correspondence is literal and directly
checkable by eye, which serves the verification aim. The extra functions are acceptable given funC's
deliberately small operator set.

**Planned migration:** refactor to Pratt when comparison, equality, and logical operators push the
function count up — and **keep the old parser**. Parser A can then be differential-tested against
parser B on randomly generated expressions, asserting identical ASTs. A free verification harness
from a refactor that was happening anyway.

### 10.3 Memory and strings

- **Arena allocator for the AST.** A transpiler is a batch process: allocate, never free, one
  teardown at exit. Removes a whole class of lifetime bug and exercises alignment and pointer
  arithmetic directly.
- **String slices into a single source buffer.** Read the file once; represent identifiers as
  `{ptr, len}` pointing into it. No `strdup` per token, no ownership questions. Requires care with
  non-NUL-terminated strings.

### 10.4 AST constraints

- **No evaluation logic in AST nodes.** No function pointers on nodes. Both codegen and the
  interpreter must be plain switch-driven walks over the node tag, so they stay independent (§11.2).
- **Every node carries a span**, inherited from its tokens.

### 10.5 Codegen

- **Emit `#line` directives** mapping generated C back to funC source, so gdb on the compiled binary
  displays funC. Emission must begin *after* the runtime prelude, which has no funC source position.
- **Do not pretty-print.** Emit ugly C and pipe through `clang-format`.
- **`main` is special-cased.** C requires `int main(void)`, not `int32_t main(void)`. Identical on
  every real platform, but the standard names `int`.
- **Always emit `<stdint.h>` and `<stdio.h>`.** Never emit bare `char`.

---

## 11. Testing and verification

### 11.1 Test strategy

- **End-to-end is the primary gate:** compile the generated C with `cc`, run it, diff stdout against
  expected. Golden C-text comparison breaks on whitespace and formatting choices that carry no
  semantic weight.
- **A small number of golden tests for codegen shape only.**
- **Zero dependencies.** A `tests/` directory of `.func` + `.expected` pairs and a shell driver.
- **`-fsanitize=address,undefined` as a Makefile target, from the start.**
- **Trap tests check exit code and stderr**, both observable.

### 11.2 Differential interpreter

A tree-walking interpreter over the same AST, exposed as `--interpret`. Run both paths on the same
input and diff stdout.

**Why:** golden tests require knowing the answer in advance, so they only cover programs someone
thought to write. Paired with a random program generator, differential testing needs no expected
outputs at all. Prior art: Csmith, which found hundreds of real bugs in GCC and LLVM this way.

**It is also the operational semantics made executable.** The interpreter should read as a
near-transliteration of the small-step rules, making "codegen agrees with the semantics" a testable
claim.

**Independence is mandatory.** The interpreter must not call the codegen helpers, and must not
copy their algorithms — shared code or shared reasoning produces common-mode error, where both paths
are wrong identically and the diff comes back clean. The interpreter should be the obvious, slow
version written straight from the definition.

**Two traps:**
- **Float width.** The interpreter must store `f32` as C `float`, not `double`, rounding at the same
  points codegen does. Otherwise the paths disagree on rounding.
- **Byte-identical printing.** The interpreter should build the same format string codegen would and
  call `printf`. This shares *formatting*, not *semantics* — no common-mode risk on arithmetic.

### 11.3 Property tests

**Euclidean division**, over random `(a, b)` with `b ≠ 0`:

- `a == (a // b) * b + (a % b)`
- `0 <= a % b < |b|`

Include deliberately: negative `b`, negative `a`, both negative, `a == INT32_MIN`, `b == -1`,
`b == INT32_MIN`.

By §7.4 these two properties fully characterize correctness — a stronger guarantee than differential
testing for these operators.

**Round-trip:** generate random valid ASTs → pretty-print funC → re-parse → assert structural
equality. Cheaply catches parser/printer disagreement.

### 11.4 Formal verification — scope

CompCert-style mechanized proof is out of scope. The achievable substitute, in priority order:

1. **Small-step operational semantics for funC, written on paper.** At this size, roughly one page.
   This is what makes the spec real rather than prose, and it is the prerequisite artifact for any
   later mechanized proof — so nothing is wasted.
2. **The differential interpreter** (§11.2) as the executable form of those semantics.
3. **Property tests** (§11.3).

### 11.5 Performance measurement — deferred

`perf` on a 10-line input measures process startup, not the lexer. It becomes meaningful once a
stress generator emits ~100k-line funC files. **Writing the generator is the enabling task**, not
dropping the aim. Until then the measurements that matter are coverage and correctness.

---

## 12. Milestone 1 restrictions

Enforced by the **type checker**, not the grammar. Each should produce a "not yet supported"
diagnostic anchored to the offending span — not a syntax error.

| Restriction | Diagnostic anchor |
|---|---|
| Exactly one function | span of the second `fn` |
| That function must be named `main` | span of the identifier |
| `main` takes no parameters | span of the parameter list |
| `main` returns `i32` | span of the return type |
| No user-defined function calls | n/a — absent from the grammar |
| No `bool`, no comparison operators | span of the type or operator |
| No casts | span of the `@cast` builtin |
| Only `@print` is a recognized builtin | span of the `BUILTIN` token |

This list is a **temporary artifact** and shrinks each milestone. Keeping it explicit prevents the
restrictions from becoming quietly load-bearing.

---

## 13. Open items

None block AST construction.

| # | Item | Notes |
|---|---|---|
| 1 | `%d` vs `PRId32` for `i32` | `%d` works on every realistic platform. `PRId32` from `<inttypes.h>` is strictly correct for `int32_t`. Format synthesis is table-driven, so the pedantic version costs one column. Leaning `PRId32`. |
| 2 | Runtime prelude placement | Inline `static inline` prelude vs. linked `funcrt.c`. Deferred; no AST impact. |
| 3 | C99 vs C11 target | Deferred until a genuine fork appears. Expected forks: `_Bool`/`<stdbool.h>`, anonymous structs/unions, `_Static_assert`. `_Generic` is *not* a fork (§9.4). |
| 4 | `f32` print precision | `%g` chosen. `%.9g` would be round-trippable for binary32; revisit if the differential harness shows precision loss. |

---

## Appendix A — AST node inventory

Derived from §4. Provided as a checklist for construction, not as a prescribed layout.

**Every node carries a span.** No node carries evaluation logic (§10.4).

| Node | Fields |
|---|---|
| `Program` | list of `Function` |
| `Function` | name (slice), params, return type, `Block` |
| `Param` | name (slice), type |
| `Block` | list of `Stmt`, terminating `Return` |
| `Let` | name (slice), declared type, initializer `Expr` |
| `Set` | name (slice), `Expr` |
| `BuiltinCall` | builtin name (slice), list of `BuiltinArg` |
| `Return` | optional `Expr` |
| `BuiltinArg` | either a `Type` or an `Expr` |
| `Binary` | operator, left `Expr`, right `Expr` |
| `Unary` | operator (always negate), operand `Expr` |
| `IntLit` | value (`int32_t`) |
| `FloatLit` | value (`float`, not `double` — §11.2) |
| `CharLit` | value (`uint8_t`) |
| `VarRef` | name (slice) |

**Type-check annotations** (added in a later pass, not by the parser): every `Expr` node gains a
resolved type. Consider whether this is a field on the node or a side table — a side table keeps the
parser's output immutable and makes the phase separation visible, at the cost of a lookup.

**Note on parenthesization:** `( expr )` produces no node. The tree structure records the grouping.
If a funC pretty-printer is later built for round-trip testing (§11.3), it must re-derive
parentheses from precedence rather than recover them from the tree.

---

## Appendix B — Reference material

| Source | Relevance |
|---|---|
| **chibicc** (Rui Ueyama) | A C compiler in C, built commit by commit. Closest model for project structure and growth path. Uses cascading recursive descent throughout — existence proof that it scales. |
| **Crafting Interpreters**, part III (clox) | Scanner and parser idioms in C. Ch. 6 is cascading, ch. 17 is Pratt — same author, same language, cleanest available A/B. |
| **lacc**, **tcc** | Complete small C compilers, for reading. |
| **Chris Wellons** on arena allocation | The §10.3 technique. |
| **Raymond Boute**, *The Euclidean definition of the functions div and mod* | The §7 argument. Short. |
| **Csmith** | Random program generation for differential compiler testing (§11.2). |
| **QBE** (used by `cproc`) | Small SSA backend. The escalation path if C is ever outgrown. |
| The Dragon Book | Out of scope. |
