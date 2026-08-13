# funC — C Coding Standards

**Version:** 1.0 · **Date:** 2026-08-12
**Status:** Final for milestone 1. Amend by addition when a rule is actually needed.
**Target standard:** C23 (`-std=c23`, pinned)

Rules only. Rationale for individual decisions lives in `DECISIONS.md`; language rules live in
`funC-spec-v1.md`.

Sections marked **TBD** are not yet decided — do not invent a convention for them, raise it instead.

---

## 1. Formatting

Enforced by `.clang-format`. Do not hand-format against it.

| | |
|---|---|
| Indent | 2 spaces, never tabs |
| Column limit | 100 |
| Braces | Attached, same line, including function definitions |
| Single-statement bodies | Always braced |
| `case` labels | Not indented relative to `switch` |
| Pointer | `char *p` — bound to the declarator |
| `const` | West: `const char *p` |
| Alignment of consecutive declarations | Off |

Run `make fmt` before committing. Never commit a file clang-format would change.

### 1.1 Reading declarations

`const` binds to what is immediately on its left; with nothing on its left it binds rightward. Read
right to left.

```c
const char *p;        /* pointer to const char   — pointee immutable */
char *const p;        /* const pointer to char   — pointer immutable */
const char *const p;  /* const pointer to const char */
```

The "clockwise/spiral rule" is wrong for some declarations. Use right-to-left, or `cdecl`.

---

## 2. Naming

| Kind | Style | Example |
|---|---|---|
| Exported struct / enum tags | `PascalCase` | `struct Token`, `enum TokenKind` |
| File-local struct / enum tags | `PascalCase_` | `struct ParserState_` |
| Exported functions | `snake_case`, module-prefixed | `arena_alloc`, `parse_additive` |
| `static` functions | `snake_case`, **no** prefix | `new_binary`, `peek` |
| Variables, fields, parameters | `snake_case` | `node_count`, `src_len` |
| Enum members | `SCREAMING_SNAKE`, prefixed | `TOK_IDENT`, `ND_BINARY` |
| Macros | `SCREAMING_SNAKE` | `FUNC_LEXER_H` |
| File-scope constants | `SCREAMING_SNAKE` | `MAX_NESTING` |

### 2.1 Rules

- **Length tracks scope.** A three-line scope wants `i`, `p`, `n`. Anything crossing a function
  boundary or living in a struct must explain itself. Long names in tight scopes are noise.
- **Module prefix on every non-static function.** C has no namespaces; the prefix is the only
  mechanism. `arena_alloc`, `lex_next`, `parse_expr`.
- **`static` functions take no module prefix.** Internal linkage means there is nothing to collide
  with, so the prefix is noise. The short name is itself the marker: reading a `.c`,
  `new_binary(...)` is visibly file-local, `ast_new_binary(...)` visibly is not.
- **File-local types take a trailing underscore.** A struct or enum defined in a `.c` and not
  exported is `struct ParserState_`. Trailing underscore is not reserved by the standard. Applies to
  types only — file-local *variables* are already marked by `static`.
- **Never end an identifier with `_t`.** POSIX reserves that suffix.
- **Never begin an identifier with `_`.** Reserved for the implementation.
- **Functions are verb phrases.** `parse_expr`, not `expr_parser`.
- **Predicates read as questions.** `is_keyword`, `has_span`. Return `bool`.
- **Keep a small consistent vocabulary.** Once `tok` always means the current token, reading gets
  faster. Prefer a short conventional name used everywhere to a descriptive name used once.

### 2.2 Case in prefixes

`snake_case` concerns the separator, not the flattening of proper nouns. A language or product name
keeps its own capitalization; everything around it is lowercase and underscore-separated.

Transpiler source uses lowercase module prefixes: `ast_`, `arena_`, `lex_`, `parse_`. An uppercase
leading token in C conventionally signals a macro, and that signal is already spent (see the table
above).

Emitted runtime code is a separate codebase with its own convention: `funC_trap`, `funC_div_i32`.

### 2.3 No `typedef` on structs or enums

Write `struct Token`, not `Token`. The keyword at the use site says it is a compound type.

Exception: function pointer types, where `struct` does not apply and a typedef is the only readable
form.

Note that the reference codebases (chibicc, clox) do typedef. Translate while reading.

---

## 3. Files and headers

### 3.1 Project layout

```
README.md          project overview
PROJECT.md         current state and work log
Makefile
src/               all sources and headers, flat
tests/             .func / .expected pairs and the driver
spec/              language and transpiler specification
docs/              session notes, DECISIONS.md, STYLE.md, COMMITS.md
tools/             scripts supporting the docs and build — stdlib only
build/             object files and binaries — gitignored
```

`spec/` is kept separate from `docs/` deliberately: the spec is normative, the notes are the record
of how it got that way. When the spec loses context, the notes supply it.

`tools/` scripts use the Python standard library only — no virtualenv, no dependency file, nothing
to bootstrap on a fresh clone. Same reasoning as the zero-dependency test suite.

Flat `src/` with headers beside their sources. The `include/` versus `src/` split exists for
libraries that install public headers; funC is a binary with no public API, so there is nothing to
separate. One `-Isrc` covers it.

Objects build out of tree into `build/`, so `src/` stays free of artefacts and `make clean` is
`rm -rf build`.

### 3.2 Module layout

One module is one `.h` / `.c` pair. A `.c` gets a header only when another translation unit needs
something from it — not for symmetry. `main.c` has no header.

**No project-wide umbrella header.** Each module includes exactly what it uses.

### 3.3 Include guards

Traditional guards, not `#pragma once`.

```c
#ifndef FUNC_LEXER_H
#define FUNC_LEXER_H
/* ... */
#endif // FUNC_LEXER_H
```

Name is `FUNC_<MODULE>_H`. No leading underscore. Comment the `#endif` with the macro name.

### 3.4 Headers are self-contained

Every header compiles standalone: it includes what it uses and forward-declares what it needs. No
header may depend on the includer having included something first.

### 3.5 Include order in `.c` files

```c
#include "lexer.h"    /* this file's own header, first */

#include <stdint.h>   /* C standard headers */
#include <stdio.h>

#include "arena.h"    /* project headers */
#include "diag.h"
```

Own header first is a self-test for §3.4. Blank line between groups; `clang-format` sorts within
groups but will not move entries between them.

### 3.6 Opaque or transparent

**Opaque** when the struct has an invariant a caller could break. Forward-declare the tag in the
header, define it in the `.c`, expose functions only.

```c
/* arena.h */
struct Arena;
struct Arena *arena_create(size_t cap);
```

**Transparent** when it is plain data with a fixed shape that consumers must read directly.

Decided so far: `struct Arena` opaque. Tokens, AST nodes, spans transparent.

### 3.7 Function bodies do not belong in headers

Bodies live in `.c` files.

If a body ever must be in a header, it is **`static inline`** — never bare `inline`. In C, a plain
`inline` definition provides no external definition; if the compiler declines to inline it, the link
fails unless exactly one translation unit supplies `extern inline`. `static inline` has no such
rule.

### 3.8 Where shared types live

A type gets its own header when it has real API surface, or when it is small but touched everywhere.
Otherwise it goes in `common.h`.

Do not create a file named `types.h` — the type checker's representation of funC types will need
`type.h`, and the two would be permanently confusable.

---

## 4. Linkage

### 4.1 `static` by default

**Every function and file-scope variable not declared in a header is `static`.** If the prototype is
not in the `.h`, the definition gets `static`. Mechanical, greppable, no judgement required.

### 4.2 No mutable globals

No non-`const` file-scope or global variables. State that must be shared goes in a struct passed by
pointer.

`static const` file-scope tables are data, not state, and are expected — format tables, precedence
tables, keyword lists.

### 4.3 No `extern` variables

Do not declare variables `extern` in headers. Do not write `extern` on function prototypes; a
prototype in a header already has external linkage.

---

## 5. Types

### 5.1 Selection

Use the type that says what the value **is**.

| Use | For |
|---|---|
| `size_t` | Sizes, counts, indices, results of `sizeof` and `strlen` |
| `int32_t`, `uint8_t`, … | funC *values*, where width is part of the semantics |
| `ptrdiff_t` | Pointer differences |
| `bool` | Predicates and flags — `<stdbool.h>` |
| `int` | `main`'s return, and C library returns passed straight through. Little else |
| bare `unsigned`, `short`, `long` | Never. Name the width |
| `char` | Only for C string data (`const char *`). Never for arithmetic |

**A fixed-width type in transpiler code signals "this is a funC value."** Preserve that signal —
internal counts and indices use `size_t`, not `int32_t`.

### 5.2 Unsigned arithmetic

`size_t` is unsigned. Two consequences that must be handled explicitly:

```c
for (size_t i = n; i-- > 0;) { ... }   /* correct reverse loop */
for (size_t i = n - 1; i >= 0; i--)    /* WRONG — never terminates */
```

- `n - 1` when `n == 0` is `SIZE_MAX`, not `-1`. Guard before subtracting from an unsigned zero.
- Never compare signed against unsigned. `-Wsign-compare` is an error in this project.

### 5.3 `bool`

Used freely in transpiler code. Unrelated to funC not having a `bool` type — that is the language
being implemented, this is the language implementing it.

`bool`, `true`, `false` are keywords in C23. Do not include `<stdbool.h>`.

Do not compare against `true` or `false`. Write `if (is_keyword(k))`.

Do not use `bool` as a return type for an operation with more than one distinct failure mode.

### 5.4 `const` versus `constexpr`

A `const`-qualified object in C is **not** a constant expression. It cannot be used as an array
size, a `case` label, or a bitfield width. This differs from C++ and catches people out.

**Use `constexpr` for compile-time constants.**

```c
constexpr size_t MAX_NESTING = 64;   /* usable as an array size and a case label */
const size_t max_nesting = 64;       /* not a constant expression */
```

`constexpr` is C23 and repeals the historic reason for `#define` constants. Prefer it over both
`#define` and `enum` hacks for scalar constants; `enum` remains correct for closed sets of related
values (§7).

`static const` file-scope *tables* remain correct and useful — they land in `.rodata` and can be
folded directly into the code.

### 5.5 Casts

Every cast is a place the compiler was overruled.

- No cast without a reason that could be stated out loud.
- **Never cast away `const`.**
- Comment any narrowing cast with why the discarded bits do not matter.

---

## 6. Structs

### 6.1 Initialization

Three-part rule, in order of preference.

**1. Initialize at declaration when a meaningful value exists.** The common case.

**2. When no meaningful value exists yet, move the declaration** to the point where one does. C99
permits declaration at point of use. Declare as late as scope allows.

**3. When neither is possible — the value is assigned on several branches and used after they
rejoin — declare bare and let the compiler prove every path assigns.**

```c
struct Node *result;              /* deliberate: every case assigns */
switch (tok->kind) {
  case TOK_INT_LIT:   result = new_int_lit(p);   break;
  case TOK_FLOAT_LIT: result = new_float_lit(p); break;
  case TOK_LPAREN:    result = parse_grouped(p); break;
}
return result;
```

**Never initialize merely to silence an uninitialized warning.** `-Wmaybe-uninitialized` /
`-Wsometimes-uninitialized` perform real flow analysis; writing `= {0}` or `= NULL` to quiet one
replaces a compile-time error with a silent zero-valued or null-dereference bug. If the warning
fires, either the logic has a hole or the declaration is in the wrong place. Fix that instead.

### 6.2 Designated initializers, always

```c
struct Token tok = { .kind = TOK_IDENT, .span = sp, .text = slice };  /* yes */
struct Token tok = { TOK_IDENT, sp, slice };                          /* no  */
```

- Self-documenting at the call site.
- **Omitted fields are zero-initialized** — a forgotten field cannot leave garbage.
- Adding a field to the struct does not silently reassign meaning in existing initializers.

`= {}` is the C23 empty initializer and is correct where zero is the intended value. Prefer it
over the older `= {0}`.

**Do not use `memset` to zero a struct.** Designated initializers are clearer and generate the same
or better code. (`memset` additionally zeroes padding bytes, which initializers do not guarantee —
that only matters if whole structs are `memcmp`'d or hashed, which this project does not do.)

### 6.3 Passing and returning

| Convention | Use for |
|---|---|
| By value | Small value types with no identity — `struct Span`, string slices |
| By pointer | Anything mutated, anything with identity, anything large |
| `const` pointer | Read-only access to something too large to copy |

**Return small structs by value.** `struct Span span_merge(struct Span a, struct Span b);` is
better C than an out-parameter.

The threshold is the ABI's: on x86-64 SysV and ARM64, a struct up to **16 bytes** passes in
registers. Beyond that it goes through memory and by-value passing starts costing.

### 6.4 Size assertions on by-value types

Any struct passed or returned by value carries a compile-time size assertion.

```c
static_assert(sizeof(struct Span) <= 16, "Span must stay register-passable");
```

`static_assert` is a keyword in C23 — no underscore, no `<assert.h>` include.

**Always on — never gated behind `NDEBUG`.** It is evaluated by the compiler and emits no code, so
there is no runtime cost to remove and no reason for release builds to skip the check.

It guards the type, not the call site: it will catch the struct growing past the threshold, not a
large struct being returned by value elsewhere. That is the right place for it — the fix is either
to shrink the type or to change its convention.

See `DECISIONS.md` D-019 on the C standard target.

### 6.5 `const` on struct pointers is shallow

`const struct Node *n` prevents writing `n->kind`. It does **not** prevent writing through
`n->lhs` — that field is still a `struct Node *`, freely mutable.

C has no deep const. `const` on a pointer parameter documents intent for exactly one level. Do not
read it as a guarantee that the pointed-to graph is immutable, and do not rely on it for that.

### 6.6 Field ordering

Order fields by **decreasing alignment** — pointers and `size_t` first, then 4-byte, then 1-byte.
The compiler inserts padding to satisfy alignment and will not reorder fields for you.

Normally a micro-concern; not here. One `struct Token` per token and one node per AST node means
tens of thousands of each on a large input, so a 40-byte token that could have been 32 is a 25%
arena increase for nothing.

Do not enable `-Wpadded` globally — it fires on nearly every struct and gets tuned out.

---

## 7. Enums

### 7.1 Enums, not `#define`

Token kinds, node kinds, type kinds and similar closed sets are enums. `#define` constants are
textual, unscoped, untyped, and invisible in a debugger.

The tag field is the enum type, never `int`:

```c
enum TokenKind kind;   /* yes */
int kind;              /* no — discards exhaustiveness checking */
```

### 7.2 Flat, ordered by group

One flat enum per closed set. Order members so related kinds are contiguous, and express groupings
as range predicates rather than as nested enums or separate fields.

```c
static bool is_keyword(enum TokenKind k) { return k >= TOK_FN && k <= TOK_VOID; }
```

### 7.3 Underlying type

C23 permits a fixed underlying type: `enum TokenKind : uint8_t { ... }`. This is worth taking for
enums stored in high-volume structs (§6.6), where a one-byte tag beats a four-byte one.

**Caution — verify before adopting.** An enum with a fixed underlying type can hold any value of
that type, not only the listed enumerators, which may weaken the exhaustiveness warning §7.4
depends on. Test it: switch over such an enum with one case omitted and confirm `-Wswitch` still
fires. If it does not, keep the plain enum — the exhaustiveness check is worth more than three
bytes.

### 7.4 Switch exhaustiveness

**Do not write `default:` in a switch over a project enum.** A `default` suppresses `-Wswitch` and
converts a compile-time error into a runtime one. Adding a member should break the build everywhere
that member is unhandled — that is the point.

If a catch-all is genuinely needed, list every case explicitly and place the failure *after* the
switch.

### 7.5 No sentinel members

No `ND_COUNT` or `TOK_LAST` inside the enum — they become unhandled cases in every switch. Define
counts outside the enum, or size arrays with designated initializers.

### 7.6 Parallel tables

Where a table parallels an enum (kind-to-string for diagnostics, keyword lookup), guard against
drift with a compile-time assertion on the table's length. Do not rely on discipline.

**X-macros are deferred.** Revisit only if hand-maintained parallel tables become a real burden.

---

## 8. Functions

### 8.1 Prototypes

- `int f(void)`, never `int f()`. In C23 the two finally mean the same thing, so this is
  consistency rather than correctness — but keep it, since every reader with pre-C23 habits reads
  `f()` as "unspecified parameters".
- Parameter names in prototypes, matching the definition. They are documentation.
- Pointer parameters to data the function does not modify are `const`.
- Do not write top-level `const` on scalar parameters in prototypes. In C, top-level qualifiers are
  discarded when forming a function type, so `f(int)` and `f(const int)` are the same function. It
  is invisible to callers and enables no optimization — the parameter is already a private copy and
  the compiler can see the whole body. Writing it on the *definition* as a self-discipline marker is
  legal and permitted, but is for the reader, not the compiler.
- `restrict`, not `const`, is the qualifier that affects optimization. It concerns aliasing. Do not
  add it speculatively; it is a promise the compiler cannot verify.
- Use C23 attributes, not compiler extensions: `[[noreturn]]`, `[[nodiscard]]`, `[[maybe_unused]]`,
  `[[fallthrough]]`. Not `_Noreturn`, not `__attribute__((...))`.

### 8.2 Shared state

When several mutually recursive functions need the same state, pass a **context struct pointer as
the first parameter** rather than threading individual arguments through every signature.

When a group of functions is really one operation, its state stays in locals. Do not create a
context struct for a single function.

Context structs are created and destroyed by their phase's entry point, not by the caller. The
context type itself is normally opaque (§3.6).

### 8.3 Ownership

Every function that takes or returns a pointer must have documented ownership. See §9.5.

---

## 9. Memory

### 9.1 Allocation is phase-scoped

- **Allocators call `malloc`; nothing else does.** The arena — and any future container that manages
  its own memory — owns its internal allocation. Code that *uses* an allocator never calls `malloc`
  directly.
- **A phase owns at most one direct allocation.** If a phase needs a buffer whose lifetime ends when
  the phase does, it acquires it on entry and releases it on exit. One `malloc`, one `free`, both
  visible in the phase's entry point.
- **Zero is the expected number.** Most phases allocate from the arena and own nothing.
- **Adding a phase means declaring its allocation** before writing it: what it owns, where it is
  acquired, where it is released.

Review question, which must have a one-sentence answer for any allocation: *which phase owns this,
and where is its free?*

A phase wanting two owned allocations is usually two phases. Not always — but stop and check.

Where several acquisitions in one function can each fail, use a **`goto` cleanup ladder**: labels in
reverse order of acquisition, each falling through to the next, so every failure path unwinds
exactly what succeeded. This is the one accepted use of `goto` in this codebase.

### 9.2 The arena

Long-lived, structurally uniform data — the AST — comes from the arena. Allocate, never free, one
teardown at exit.

- **Nothing in the arena is freed individually.** No `free()` on a node, ever.
- **Fixed maximum alignment** for milestone 1: every allocation is aligned to `alignof(max_align_t)`.
  A per-type alignment parameter is a later refinement.
- **Growth is by chaining blocks**, never `realloc` — moving a block would invalidate every pointer
  already handed out. Block size doubles, **capped** at a few MB, then grows linearly.
- Alignment rounding must not overflow near the top of the address space. Compute the padding
  directly rather than rounding the address up. Assert that the alignment is a power of two.

**Known cost: ASan cannot see arena bugs.** AddressSanitizer detects use-after-free by watching
`free()`, which is never called. A stale pointer into arena memory reads live, valid, wrong data.
Manual poisoning (`ASAN_POISON_MEMORY_REGION` on block creation, unpoison per allocation) restores
most of this and is a planned follow-up, not milestone 1.

### 9.3 Fixed-capacity buffers

Where a provable upper bound exists, allocate the bound once and do not grow. A proof beats a
mechanism.

**Every fixed-capacity array asserts its bound on append, and documents the invariant that justifies
its size.** The assertion is not optional: the array is safe only while the invariant holds, and the
invariant is exactly the thing a future change breaks silently.

Do not `calloc` a loosely-bounded buffer. Under overcommit, an untouched allocation costs address
space rather than resident memory; zeroing touches every page and makes the waste real.

### 9.4 Growable buffers

Where no bound exists, use a growable array — pointer, length, capacity, reallocating by a
multiplicative factor on append.

**The growth factor is the design.** A constant increment makes appends O(n) amortized; a multiple
makes them O(1), because copies become exponentially rarer. 2 is the obvious choice; 1.5 is used by
several standard libraries because it can reuse previously freed blocks under some allocators.

Nothing may hold a pointer into a growable buffer while it is still growing. Hold an index.

### 9.5 Ownership documentation

Every function returning a pointer states the lifetime in its header comment, in one word:

| Word | Meaning |
|---|---|
| **arena** | Valid until teardown. Caller does nothing. |
| **borrowed** | Points into memory owned by the caller or by something longer-lived. |
| **owned** | Caller must free. |

Almost everything is one of the first two. `owned` should be rare enough that seeing it is a signal.

### 9.6 Out of memory

`malloc` returning NULL is neither a diagnostic nor an assertion (§11) — the input is fine and the
transpiler is fine. It is a third category: **environment failure**.

Print to `stderr` and `exit` non-zero. Not `abort()` — there is no bug to inspect in a core dump.

Do not propagate OOM as a return value. That would put a failure branch in nearly every function in
the codebase to handle a condition that, for a batch process on a modern system, essentially never
fires and has no useful recovery.

---

## 10. Comments

### 10.1 The default is no comment

Code should speak. A comment that restates what the code plainly does is noise, and worse, it rots
independently of the code it describes.

```c
// loop over the tokens          <- no. The for loop says this.
for (size_t i = 0; i < n; i++)
```

A comment earns its place when it closes a **logic gap** — something a reader cannot recover from
the code:

- **Why** this approach and not the obvious one.
- **Why** something that looks wrong is correct.
- An invariant the code depends on but does not state.
- A reference to the decision that produced it: `// Euclidean adjustment, see D-014`.

Prior art: the Hare compiler.

### 10.2 Style

- **`//` everywhere**, in both source and headers.
- **Doc comments are plain prose** above the declaration, in the header. No Doxygen markup, no
  `@param`, no `@return`. Restating a parameter's name is the same failure as §10.1's loop comment.
- clangd surfaces the preceding comment on hover with no markup and no configuration. The editor is
  the documentation viewer.
- **Every `.c` and `.h` opens with a one- or two-line file comment** naming what the module is. This
  is the one comment always worth writing, because it cannot be inferred from any single
  declaration.

### 10.3 Required content

Prose doc comments are unenforced by tooling, so one thing must be a rule rather than a habit:

**Every pointer-returning function documents its lifetime** as `arena`, `borrowed`, or `owned`
(§9.5). This is a review item.

### 10.4 Markers

Exactly two, and no others. A larger vocabulary is one nobody distinguishes.

| Marker | Means |
|---|---|
| `// TODO(ref): ...` | Work deliberately deferred |
| `// NOTE: ...` | Something a reader needs that the code cannot say |

Both are permitted in committed code and are not review findings.

A committed `TODO` should reference a decision ID or a milestone —
`// TODO(D-021): intern once the symbol table exists` is a plan; `// TODO: fix this` is a wish, and
will still be there in a year.

---

## 11. Assertions versus diagnostics

Two distinct mechanisms. Never substitute one for the other.

| | Diagnostic | Assertion |
|---|---|---|
| Means | The **input program** is wrong | The **transpiler** is wrong |
| Caused by | User input | A bug in this codebase |
| Example | Operand types disagree | A binary node with a NULL child |
| Response | Message with a span, controlled exit | Crash loudly |
| Expected? | Yes — normal operation | No — never |

An `assert` on user input means a malformed file crashes with no useful message. A diagnostic for an
internal invariant means silently continuing in a corrupted state. Both are review failures.

`static_assert` is a third thing again — a compile-time check with no runtime existence. It is not
gated by `NDEBUG` and is not related to either row above.

Details of the diagnostic mechanism: **TBD**.

---

## 12. Compiler-enforced rules

These warnings back the rules above and are errors, not warnings.

| Flag | Enforces |
|---|---|
| `-std=c23` | Pinned language standard — never left to the compiler default |
| `-Wall -Wextra` | Baseline |
| `-Wmissing-prototypes` | §4.1 — non-static function without a visible prototype |
| `-Wstrict-prototypes` | §8.1 — `f()` instead of `f(void)`. Weakened under C23; kept for style |
| `-Wsign-compare` | §5.2 — signed/unsigned comparison |
| `-Wswitch` | §7.4 — unhandled enum case (implied by `-Wall`) |
| `-Wconversion` | §5.5 — implicit narrowing |
| `-Wshadow` | Shadowed declarations |
| `-Wvla` | Variable-length arrays |
| `-Wmaybe-uninitialized` / `-Wsometimes-uninitialized` | §6.1 — read before assignment |
| `-Wfree-nonheap-object` | §9.1 — freeing something that was not malloc'd |

Also build a `-fsanitize=address,undefined` target and run it before every push.

**Milestone 1 build.** The Makefile carries these flags and nothing else — compile, link, clean,
sanitize. Target chains, format gates, and doc tooling are phase 2; they are aim-2 work and are
worth doing once there is something to gate.

---

## 13. Deferred, with triggers

Deliberately unwritten. Each is settled when the work reaches it, not before — a convention invented
ahead of its first real use tends to be wrong in a way nobody notices until it is load-bearing.

Raise these rather than inventing a convention on the spot.

| Item | Settle it when |
|---|---|
| Error handling mechanics — propagation, return conventions, diagnostic sink shape | Before the lexer. Spec §2.1 has the scanner emitting diagnostics. |
| The assertion mechanism — `assert` compiles away under `NDEBUG`, which would remove internal invariant checks from release builds | Same conversation as the above; they are two halves of "how does this program report that something is wrong". |
| Testing conventions — corpus layout, `.func` / `.expected` naming, driver contract | Once the lexer produces output worth testing. |
| Build tooling — target chains, format gates, hooks, doc scripts | Phase 2. |

Anything else that arises is an addition to this document, not a gap in it.

---

## 14. Review checklist

Quick pass before requesting review.

- [ ] `make fmt` produces no changes
- [ ] Builds clean under §12 flags
- [ ] Every non-header function is `static`, and `static` functions carry no module prefix
- [ ] File-local types carry a trailing underscore
- [ ] No mutable globals
- [ ] Every header compiles standalone; own header included first in its `.c`
- [ ] No `default:` in a switch over a project enum
- [ ] No identifier ending `_t` or beginning `_`
- [ ] `size_t` for counts and indices; fixed-width types only for funC values
- [ ] All struct initializers are designated
- [ ] Nothing initialized solely to silence an uninitialized warning
- [ ] By-value struct types carry a `static_assert` on size
- [ ] No unexplained casts; no cast away of `const`
- [ ] Assertions guard internal invariants only; user input produces diagnostics
- [ ] Every allocation has a one-sentence answer to "which phase owns this, and where is its free?"
- [ ] Every fixed-capacity array asserts its bound and documents the invariant sizing it
- [ ] Every pointer-returning function documents lifetime as arena / borrowed / owned
- [ ] No comment restates what the code already says
- [ ] Every file opens with a one-line module comment
- [ ] Every committed TODO references a decision ID or milestone
