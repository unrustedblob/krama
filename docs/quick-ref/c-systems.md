# C systems idioms

Entries are alphabetical within no particular order of importance. Every shape sketch is
pseudocode; translate it yourself.

## Contents

- Alignment rounding
- Allocate-or-die (`xmalloc`)
- Always-on assertion
- Arena / bump allocation
- Atomic file replacement
- Exhaustive `switch` without `default`
- Fat pointer / string slice
- Flexible array member
- Designated initializers for sparse tables
- `do { } while (0)` statement macro
- `goto` cleanup ladder
- Mark / release (arena savepoint)
- Overflow-safe fit test
- Size classes and slab allocation
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

## Allocate-or-die (`xmalloc`)

**Problem:** an allocation failure has exactly one sensible response in this program — say so and
stop — but the return value forces every call site to write the branch anyway. A hundred `if (!p)`
blocks, all identical, all untested, guarding a condition that on a batch process with a modern
allocator essentially never fires.

**Shape:**

```
xmalloc(size):
    p = malloc(size)
    if p is null:
        message to stderr
        exit non-zero          // not abort — there is no bug to inspect
    return p                   // never null; callers do not check
```

The convention is a promise in the signature: the function either returns valid memory or does not
return. Once that holds, "can this be null" stops being a question anyone asks, and the header
comment says so explicitly so no one adds a defensive check later.

**When it does not apply:** a library, which cannot know whether its caller wants to retry, degrade,
or die, and must hand the failure up. This is an *application* convention. The distinction is the
whole argument — propagating in an application means every function grows a branch to serve a
policy that was never in doubt.

Keep it separate from an assertion failure, which reports a bug and wants `abort()` and a core
dump. Out of memory is an environment failure: the input is fine, the program is fine, the machine
said no. Same three lines on screen, different exit mechanism, and conflating them costs you the
distinction exactly when you need it.

**Read:** GNU libiberty's `xmalloc`; git's `xmalloc` in `wrapper.c`, which also handles the
`malloc(0)` case explicitly. Most compilers ship one under some name.

**Related:** allocate-or-die's cousin `xstrdup`; the arena, which absorbs the same decision one
level down so that individual node allocations never face it.

---

## Always-on assertion

**Problem:** `assert` from `<assert.h>` compiles to nothing under `NDEBUG`. Release builds are
exactly where a corrupted internal invariant is most expensive and least observable, and that is
precisely where the standard mechanism removes the check.

**Shape:**

```
ASSERT(cond, msg):
    do:
        if not (cond):
            assert_failed(#cond, msg, __FILE__, __LINE__, __func__)
    while (0)

assert_failed(...):            // [[noreturn]]
    message to stderr
    abort()                    // SIGABRT, core dump, gdb lands at the fault site
```

Four things this shape is doing, three of which force the macro:

- `#cond` stringifies the condition, so the report names the invariant rather than saying an
  assertion failed.
- `__FILE__` / `__LINE__` / `__func__` expand at the *call site*. A function can only ever report
  its own position.
- The handler is `[[noreturn]]`, which keeps flow analysis accurate — no spurious
  maybe-uninitialized or missing-return warnings after an assertion.
- `do { } while (0)` makes the expansion one statement, so an unbraced `if`/`else` around it still
  parses.

**Cost, accepted deliberately:** the check exists in every build. That is an AND and a
perfectly-predicted branch, which is nothing against real work — but it means the mechanism is for
genuine invariants, not for things you would merely like to double-check. If one ever lands
somewhere hot enough to measure, hoist the check rather than weakening the mechanism.

**Read:** Linux's `BUG_ON` and `WARN_ON`; LLVM's `llvm_unreachable`, which is instructive because it
does the *opposite* under `NDEBUG` — it becomes an optimizer hint that the path is impossible, which
turns a caught bug into undefined behaviour. Knowing that trade exists is the point.

**Related:** static assertion (compile-time, no runtime existence at all), and the
diagnostic-versus-assertion split — an assertion means *this program* is wrong, a diagnostic means
the *input* is wrong, and substituting either for the other is a defect.

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
mark / release below, which is how scratch arenas reclaim without a `free`, and `alloca`.

---

## Atomic file replacement

**Problem:** a program that writes its output directly to the target path leaves a partial file
behind when it dies mid-write. What remains looks like a valid artifact and is not, so whatever
consumes it next fails confusingly — a truncated generated `.c` produces a syntax error rather than
an obvious "the generator crashed".

**Shape:**

```
write everything to a temporary in the same directory
flush and close it
rename(temp, target)          // atomic; the target is the old file or the new one, never a mix
```

The rename must be on the same filesystem, which is why the temporary goes in the target's directory
rather than `/tmp`. POSIX guarantees the replacement is atomic with respect to other processes: a
concurrent reader sees one version or the other.

If the data must survive a power loss rather than merely a crash, `fsync` the file before the rename
and the *directory* after it. For a compiler writing build output, crash-safety is the requirement
and the syncs are usually skipped as too expensive for what they buy.

**Why it beats a cleanup path:** unlinking the partial file in an error handler only works for
failures you routed through the handler. It does nothing for a signal, an `abort()`, or a
power cut. The temp-and-rename structure makes partial output unrepresentable rather than cleaned
up after.

**Read:** the POSIX `rename()` specification on atomicity; how editors implement save-without-
corruption; SQLite's atomic-commit documentation for the same idea taken much further.

**Related:** write-ahead logging; the general pattern of making an invalid intermediate state
unobservable rather than transient.

---

## Designated initializers for sparse tables

**Problem:** a table parallel to an enum — kind-to-string for diagnostics, a format table, a
precedence table — written positionally. It is correct on the day it is written and silently wrong
the day someone reorders the enum or inserts a member in the middle. Nothing warns; every lookup
just returns its neighbour's value.

**Shape:**

```
static const char *const names[] = {
    [TOK_FN]    = "fn",
    [TOK_LET]   = "let",
    [TOK_IDENT] = "identifier",
};
static_assert(sizeof names / sizeof names[0] == TOK_LAST_VALUE + 1, "table drifted");
```

The index is written at each entry, so order in the source stops mattering and reordering the enum
cannot misalign anything. Omitted entries are null rather than garbage, which is a checkable
condition rather than a silent misread.

Two things it does not do, both worth knowing:

- **It does not catch a missing entry.** Adding an enum member leaves a null hole; nothing warns.
  The length assertion catches the case where the array is sized off the enum, which is why the two
  are used together rather than separately.
- **It does not make the elements constant expressions.** In C, a `constexpr` object of aggregate
  type is not usable in a constant expression, and a `constexpr` object of pointer type must be
  initialized with a null pointer constant — so an array of string pointers cannot be `constexpr`
  at all. `static const char *const` is the correct spelling here and lands in `.rodata` regardless.

**Read:** C's initialization clause on designated initializers; the Linux kernel's syscall tables,
which are the canonical large-scale use.

**Related:** X-macro, which solves the missing-entry half by generating enum and table from one
list; table-driven dispatch, which is what the table usually feeds; static assertion.

---

## `do { } while (0)` statement macro

**Problem:** a multi-statement macro that is not wrapped breaks at the call site in ways the author
never sees. Braces alone are not enough — `if (x) MACRO(); else ...` becomes a syntax error, because
the trailing semicolon after a brace-block terminates the `if`. Naked statements are worse: only the
first one ends up inside the `if`.

**Shape:**

```
#define MACRO(a) do { first(a); second(a); } while (0)
```

The loop runs once and costs nothing — every compiler folds it away. What it buys is that the
expansion is a single *statement* which requires a terminating semicolon, so the macro behaves
exactly like a function call in every syntactic position.

Use it whenever the macro expands to statements. A macro that expands to an *expression* wants
parentheses instead, not this — and a macro that could have been a `static inline` function wants
neither.

**Read:** any kernel header; the C FAQ entry on multi-statement macros, which walks through the
`if`/`else` failure directly.

**Related:** the single-evaluation rule (each parameter used once, or say so in the comment);
statement expressions (`({ ... })`) as the GCC extension that returns a value, and is not ISO C.

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

## Mark / release (arena savepoint)

**Problem:** a phase allocates temporaries — a scratch buffer per node, a working list per pass —
whose lifetime ends long before the arena's. From the arena they are never reclaimed, so peak
memory becomes the sum of every temporary ever made rather than the largest live set. From
`malloc` they reintroduce exactly the ownership bookkeeping the arena was adopted to abolish.

**Shape:**

```
mark(arena):
    return {current block, current cursor}    // a value; nothing is mutated

release(arena, mark):
    // every pointer handed out since the mark is dead after this line
    restore the arena's current block and cursor from the mark
    for each block chained after the mark's block:
        either unlink and free it, or retain it with its cursor reset for reuse
```

The cursor alone is not a mark once the arena chains blocks — restoring it without also restoring
the block silently strands or reuses the wrong region. Marks nest **LIFO**: releasing an outer mark
invalidates every inner one, and there is no mechanism that will tell you it happened.

Wellons' variant avoids the explicit release entirely: pass the arena **by value**, so the callee
bumps a private copy of the header and the caller's cursor is untouched when it returns. The mark is
the copy and the release is the `return`, which makes the lifetime visible in the signature —
`f(Arena *perm, Arena scratch)` says which allocations survive. It needs a header cheap enough to
copy and a matching block-chaining policy.

**Read:** Ryan Fleury, *Untangling Lifetimes: The Arena Allocator*, where this is the temp/scratch
arena. Chris Wellons, *Arena allocator tips and tricks*, for the by-value form. gingerBill's
*Memory Allocation Strategies* part 3 arrives at the same place from the other direction and calls
the result a stack allocator. GNU `obstack` has had it as `obstack_free` since the 1980s.

**Cost:** it puts back a lifetime rule the plain arena had removed — a pointer's validity now
depends on a scope that is not visible at the use site, and ASan cannot see the violation because
nothing was freed. Adopt it when temporaries are a measured share of peak memory, not on
speculation.

**Related:** LIFO / stack discipline, which is the constraint marks impose and the reason the next
allocator up the family is called a stack allocator; `alloca`, the compiler-provided version of the
same shape with no way to bound it; region inference (MLKit), where a compiler places the marks for
you.

---

## Overflow-safe fit test

**Problem:** the natural way to ask whether a request fits is `used + size > cap`. On unsigned
types that addition wraps, so a sufficiently large `size` produces a small sum and the test reports
that it fits. The allocator then hands out a pointer past the end of its own block, and the failure
appears somewhere else entirely.

**Shape:**

```
// given the invariant used <= cap
if (size > cap - used)  → does not fit
```

The subtraction cannot wrap, because the invariant guarantees a non-negative result. Nothing is ever
added, so nothing can exceed the type's range. Where padding is involved, subtract it too, in two
steps rather than one sum:

```
if (padding > cap - used)            → no room even to align
if (size > cap - used - padding)     → no room for the object
```

The invariant is what makes it safe, so the invariant is worth asserting rather than assuming.

**Pointer form:** the same defect appears as `if (p + size > end)`, which is worse than an
overflow — in C, *forming* a pointer more than one past the end of an object is undefined
behaviour, whether or not it is dereferenced. The correct spelling computes the remaining span
instead: `if ((size_t)(end - p) < size)`. This is the concrete reason allocators that track a
`size_t` offset rather than a pointer have less to get wrong.

**Read:** CERT C INT30-C on unsigned wraparound; any allocator's size check, all of which are
written subtractively once someone has been bitten.

**Related:** alignment rounding, which has the same shape of hazard one level down — computing the
padding rather than rounding an address up, so nothing can wrap near the top of the address space.

---

## Size classes and slab allocation

**Problem:** an arena never reuses memory, which is correct when everything shares one lifetime and
wrong when objects die individually. But a general-purpose allocator carries per-object headers,
free-list searching, and fragmentation to serve request sizes that, in practice, are drawn from a
handful of fixed values.

**Shape:**

```
pool:         one free list, one fixed object size, allocation is a pop
size classes: several pools at fixed sizes; round the request up to the nearest class
slab:         one cache per type, objects pre-constructed, reuse preserves initialized state
```

The progression is worth having straight, because the three names get used interchangeably and are
not the same thing. A **pool** serves one size. **Size classes** (informally, buckets) are several
pools with rounding, which is the jemalloc and tcmalloc structure. A **slab** is per-*type* rather
than per-size, and its distinguishing feature is that a freed object keeps its constructed state so
reuse skips re-initialization — Bonwick's original point, and the reason the kernel uses it for
inodes and dentries.

All three trade the arena's bargain — no reuse, no bookkeeping, one teardown — for per-object reuse
at the cost of a free list. That is a different bargain, not a better one. Reach for it when objects
demonstrably die individually, not when the arena feels wasteful.

**Read:** Bonwick, *The Slab Allocator: An Object-Caching Kernel Memory Allocator* (USENIX 1994);
the jemalloc paper for size classes at scale.

**Related:** arena / bump allocation, free list, mark / release — four points on the same axis,
trading reuse granularity against bookkeeping.

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

**Read:** `static_assert` is a keyword in C23 — no underscore, no include, and the message operand
is optional. Two older spellings are worth recognising rather than writing: `_Static_assert` with
`static_assert` as an `<assert.h>` macro over it (C11), and, pre-C11, declaring an array with a
negative size when the condition failed.

**Related:** compile-time invariants, `sizeof` checks, layout assertions. Distinct from both a
runtime `assert` and a diagnostic — it has no runtime existence at all, so `NDEBUG` does not reach
it and there is nothing to gate.

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
