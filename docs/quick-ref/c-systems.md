# C systems idioms

Entries are alphabetical within no particular order of importance. Every shape sketch is
pseudocode; translate it yourself.

## Contents

- Alignment rounding
- Arena / bump allocation
- Exhaustive `switch` without `default`
- Fat pointer / string slice
- Flexible array member
- `goto` cleanup ladder
- Static assertion
- Table-driven dispatch
- Tagged union
- Two-phase allocate
- X-macro

---

## Alignment rounding

**Problem:** an allocator that hands out addresses at arbitrary offsets will eventually return a
pointer that is legal for `char` and undefined behaviour for `double`. On x86 it usually works
anyway, which is worse — the bug ships and surfaces on a different machine.

**Shape:**

```
to round an address up to alignment A, where A is a power of two:
    add (A - 1) to the address
    clear the low bits: mask off (A - 1)
```

The mask trick works only because A is a power of two, which every natural alignment is.

**Read:** the `alignof` operator gives you A for a type. Wellons' arena articles do the rounding in
two lines and explain why. Chapter on alignment in any of the systems-programming texts covers the
undefined-behaviour side.

**Related:** padding, `max_align_t`, over-aligned types, false sharing.

---

## Arena / bump allocation

**Problem:** thousands of small, short-lived allocations — AST nodes being the classic case. Each
`malloc` costs bookkeeping, each `free` is an opportunity for a use-after-free or a double-free, and
tracking which subtree owns which node is a design burden with no payoff for a process that exits
in 40ms anyway.

**Shape:**

```
hold a list of blocks, plus a cursor and capacity for the current one

allocate(size, alignment):
    round the cursor up to the alignment FIRST
    then test whether the rounded cursor plus size fits in the current block's remainder
    if it does not fit: malloc a fresh block and link it, then bump into that
        (never split an allocation across two blocks)
    if the request is larger than the default block size: give it its own dedicated block,
        or the fit test never succeeds and you loop
    remember the rounded cursor, advance it by size, return what you remembered

free(pointer):
    do nothing

teardown:
    walk the block list, releasing each
```

Two details that look minor and are not. **Round for alignment before the fit test**, not after —
reversing them hands back a misaligned pointer that works on x86 and traps on ARM. And **growing by
chaining a fresh block never moves the existing ones**, so pointers already handed out remain valid
for the arena's lifetime. That is the property that makes plain child pointers safe in an AST, and
it is why block chaining rather than `realloc` is the standard arena design.

The larger insight is that **a batch process does not need to free**. Ownership questions disappear
because there is exactly one owner: the arena.

**Read:** Chris Wellons, *Arena allocator tips and tricks*. Ryan Fleury, *Untangling Lifetimes: The
Arena Allocator*. GNU `obstack` is the venerable implementation.

**Cost:** peak memory is retained until teardown. Fine for a compiler, wrong for a long-running
server that must reclaim.

**Related:** region-based memory management (the academic name), pool allocator (fixed-size cousin),
scratch arenas, `alloca`.

---

## Exhaustive `switch` without `default`

**Problem:** you add a new variant to a tagged union and there are eleven places that switch on the
tag. You find nine of them. The other two silently do nothing until a user reports something strange
six weeks later.

**Shape:**

```
switch over the tag with a case for every variant
provide no default label
compile with warnings that flag an unhandled enumeration value, and treat warnings as errors
```

The compiler now enumerates the eleven sites for you. A `default:` label that "handles the rest"
destroys this — it makes every future variant silently handled.

**Read:** `-Wswitch` is on by default in GCC and Clang under `-Wall`; `-Wswitch-enum` is stricter and
fires even when a `default` exists.

**Cost:** you must genuinely list every case, including the ones where the answer is "nothing" —
write them out with an empty body and a `break`.

**Related:** exhaustiveness checking, sum types, `__builtin_unreachable`.

---

## Fat pointer / string slice

**Problem:** you want to talk about a substring of a file you have already read into memory.
Copying it with `strdup` means an allocation and an ownership question per token. Not copying it
means the substring is not NUL-terminated, and every `str*` function in the standard library will
run off the end of it.

**Shape:**

```
represent a string as a pair: a pointer into the source buffer, and a length
comparison compares lengths first, then bytes over the length
printing uses a length-bounded format, never the plain string format
never pass one to a function that expects NUL termination
```

**Read:** essentially every modern C codebase that handles text. `printf`'s precision-on-`%s` form
takes a length. Rust's `&str` and Go's `string` are the same idea with the compiler enforcing it.

**Cost:** the slice is only valid while the source buffer is. That is a lifetime dependency, and it
is the one you must actually think about — the reason the source buffer is usually read once and
held for the whole run.

**Related:** string view, `string_view`, non-owning reference, interning.

---

## Flexible array member

**Problem:** a header followed by a variable number of trailing elements — a node with N children,
a message with a payload. Doing it as a header plus a separately allocated array costs two
allocations and one pointer chase.

**Shape:**

```
declare a struct whose final member is an array of unspecified length
allocate: size of the struct, plus element size times count
the trailing elements live immediately after the header, in the same block
```

**Read:** C99 §6.7.2.1. The pre-C99 version — declaring the array with length 1 or 0 — is called the
*struct hack* and is still visible in older code.

**Cost:** such a struct cannot be an array element, cannot be assigned by value, and cannot be
nested inside another struct except as the last member.

**Related:** struct hack, variable-length structures, tail allocation.

---

## `goto` cleanup ladder

**Problem:** a function opens a file, then allocates a buffer, then acquires a lock. Each step can
fail. Each failure must unwind exactly the steps that already succeeded. Written with nested `if`s
this reaches five levels of indentation and duplicates the cleanup at every exit.

**Shape:**

```
on each failure, jump to the label that unwinds everything acquired so far
place the labels in reverse order of acquisition, falling through one to the next
the success path falls into the same ladder, or returns before it
```

**Read:** the Linux kernel uses this pervasively and its coding style document defends it. It is the
one universally accepted use of `goto` in C.

**Related:** RAII (what C lacks), `__attribute__((cleanup))`, defer.

---

## Static assertion

**Problem:** your code assumes a struct is 16 bytes, or that an enum fits in a byte, or that the
node tag array has one entry per tag. Written as a comment, the assumption rots. Written as a
runtime check, it costs a branch and finds the problem too late.

**Shape:**

```
assert a compile-time-constant condition at file scope
the compiler refuses to build if it is false
```

**Read:** `_Static_assert` in C11, `static_assert` via `<assert.h>`. Pre-C11 the idiom was declaring
an array with a negative size when the condition failed — worth recognising in old code.

**Related:** compile-time invariants, `sizeof` checks, layout assertions.

---

## Table-driven dispatch

**Problem:** an if-chain, or a switch, that grows by one arm every time the language grows by one
operator. The logic and the data are tangled, so adding an operator means editing code in four
places rather than adding a row in one.

**Shape:**

```
define a table: one row per operator, columns for name, precedence, arity, emitted form
the parser and the code generator both index this table rather than branching on the operator
adding an operator means adding a row
```

**Read:** this is how most production compilers store operator information. Pratt parsers are
table-driven dispatch applied to precedence specifically.

**Cost:** indirection. A table lookup is harder to follow than a `switch` when reading, and when the
table has three rows it is not worth it.

**Related:** data-driven design, jump tables, X-macro (for keeping the table and an enum in sync).

---

## Tagged union

**Problem:** a value that is exactly one of several shapes — an AST node is a binary operation *or*
a literal *or* a variable reference, never two at once. A struct with all the fields present wastes
space and lets you read the wrong one.

**Shape:**

```
a struct containing:
    a tag: an enum naming which variant this is
    a union: one member per variant, holding that variant's payload

every read of the union goes through a switch on the tag
never read a member the tag does not name — that is undefined behaviour
```

Putting the tag first, before the union, is conventional and makes it inspectable in a debugger
regardless of variant.

**Read:** the shape of essentially every AST in every C compiler. chibicc's `Node`. clox's `Value`.
The theory name is *sum type* or *discriminated union*.

**Related:** sum types, variants, NaN boxing (packing a tag into unused float bits), exhaustive
switch.

---

## Two-phase allocate

**Problem:** you need to build a string or buffer whose final size you do not know until you have
produced it. Guessing and reallocating works but is fiddly; guessing too large wastes.

**Shape:**

```
phase 1: run the production logic with output disabled, counting bytes only
allocate exactly the counted size
phase 2: run the same logic again, this time writing
```

The trick is that both phases run *the same code*, parameterised by whether the destination is real
or null — otherwise the count and the fill can disagree, which is a nasty class of bug.

**Read:** `snprintf` with a null destination returns the length it would have written. That is this
idiom exposed by the standard library, and it is the canonical example.

**Related:** measure-then-fill, dry run, `vsnprintf` sizing.

---

## X-macro

**Problem:** an enum and its parallel string table drift apart. You add a token kind, forget the
name entry, and three weeks later an error message reads `(null)`.

**Shape:**

```
define a list macro whose body invokes a per-item macro once per item,
    passing the item's tag and its printable name

to build the enum:  define the per-item macro to expand to the tag; include the list
to build the table: redefine it to expand to the name string; include the list again
```

Written once, expanded twice, so the two cannot disagree.

**Read:** chibicc's token kinds. Documented under "X macro"; David Hanson's *C Interfaces and
Implementations* covers the family.

**Cost:** debuggers cannot see through it, and it is genuinely hard to read the first time. Widely
considered worth it for the enum/name-table case specifically and rarely elsewhere.

**Related:** code generation from a single source of truth, designated initializers (a lighter
alternative when only one artefact is generated).
