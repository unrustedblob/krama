# Compiler construction

Organised by pipeline stage. Every shape sketch is pseudocode.

## Contents

**Scanning:** maximal munch · keyword post-filtering · perfect hashing · string interning · token
lookahead buffer
**Parsing:** cascading recursive descent · Pratt parsing · left-recursion elimination · panic-mode
recovery · synchronization sets · error productions
**AST and positions:** source span tracking · line table + binary search · index-based node
references · side table for annotations
**Checking:** symbol table with scope chain · bidirectional type checking
**Codegen:** constant folding · `#line` directives
**Testing:** differential testing · property-based testing · delta debugging · random program
generation · small-step operational semantics

---

# Scanning

## Maximal munch

**Problem:** the scanner reaches `/` and must decide whether it is looking at a division operator or
the first half of something longer. Decide too early and `//` scans as two divisions.

**Shape:**

```
at each position, try the longest token that could start here
only fall back to shorter candidates when the longer one fails to match
```

The rule generalises: attempt three-character operators before two, two before one. It is why
scanners are usually written as a switch on the first character with nested lookahead, rather than a
flat table.

**Read:** the term is standard in the Dragon Book and in every scanner-generator's documentation.
Its famous failure case is C++'s `>>` in nested templates, where maximal munch gives the wrong
answer and the standard had to carve out an exception.

**Related:** longest-match rule, lexer lookahead, trie-based tokenization.

---

## Keyword post-filtering

**Problem:** a scanner with a dedicated state machine per keyword is enormous and must be edited
every time a keyword is added.

**Shape:**

```
scan the identifier rule, greedily, without caring whether it is a keyword
once the identifier's extent is known, look it up in a keyword table
if found, emit the keyword token; otherwise emit an identifier
```

This is why "keywords are reserved identifiers" is the near-universal language design choice — it
falls directly out of this being the cheap implementation.

**Read:** the approach in *Crafting Interpreters* ch. 16, and in chibicc's tokenizer.

**Related:** perfect hashing, string interning, contextual keywords (what you get when you *don't*
do this).

---

## Perfect hashing (gperf)

**Problem:** the keyword table lookup happens once per identifier in the entire source file. A
linear scan over 40 keywords is 40 string comparisons per identifier.

**Shape:**

```
given a fixed, known-in-advance key set, precompute a hash function
    that maps each key to a distinct slot with no collisions
lookup becomes: hash, index once, compare once to confirm
```

The "perfect" part means no collisions are possible, so no probing is needed.

**Read:** GNU `gperf` generates these; its manual explains the construction. Most real compilers use
a hand-tuned hash keyed on length and first character instead, which is nearly as good and requires
no build-time tool.

**Cost:** requires the key set to be fixed at build time, and adds a generator to the build.
Premature until profiling says the keyword lookup matters.

**Related:** minimal perfect hashing, FNV-1a, length-plus-first-char dispatch.

---

## String interning

**Problem:** the same identifier appears 500 times in a file. Comparing identifiers means `strcmp`
every time, and any table keyed by identifier hashes the bytes on every lookup.

**Shape:**

```
maintain a set of unique strings
to intern: look the string up; if present return the stored copy, otherwise insert and return it
thereafter, every occurrence of that identifier is the same pointer
comparison becomes pointer equality
```

**Read:** used by essentially every serious compiler. Lisp's symbols are this idea as a language
feature. Java's `String.intern` exposes it directly.

**Cost:** the interning table itself must live as long as any interned string, and interning costs a
hash on first sight.

**Related:** intern pool, symbol table, atoms, flyweight pattern.

---

## Token lookahead buffer

**Problem:** the parser needs to see the next token to decide, but consuming it means it is gone if
the decision goes the other way.

**Shape:**

```
hold one (or k) already-scanned tokens in a small buffer
peek returns the buffered token without consuming
advance returns it and refills the buffer from the scanner
```

For a grammar needing only one token of lookahead, the buffer is a single variable.

**Read:** *Crafting Interpreters* ch. 6 keeps `current` and `previous`. The general form is what
LL(k) parsing means by k.

**Related:** LL(1), backtracking, ring buffer, packrat parsing (memoized unbounded lookahead).

---

# Parsing

## Cascading recursive descent

**Problem:** encoding operator precedence in a hand-written parser.

**Shape:**

```
one function per precedence level, loosest at the top
each function calls the next-tighter one for its operands,
    then loops while it sees an operator at its own level
the tightest level parses literals, identifiers, and parenthesised expressions
```

Left associativity comes from the loop; right associativity comes from recursing into the same
level instead of looping.

**Read:** *Crafting Interpreters* ch. 6. chibicc uses this throughout — an existence proof that it
scales to all of C.

**Cost:** one function per level means a deep call chain for a simple literal, and around a dozen
near-identical functions in a full language. That is the pressure that leads to Pratt.

**Related:** Pratt parsing, precedence climbing, LL parsing.

---

## Pratt parsing / precedence climbing

**Problem:** cascading descent's function-per-level becomes a dozen functions of boilerplate, and
adding an operator means editing the chain.

**Shape:**

```
give every operator a binding power (precedence as data, in a table)

parse(minimum binding power):
    parse a prefix/primary expression as the left side
    while the next token is an infix operator whose binding power >= minimum:
        consume it
        parse the right side with the appropriate minimum for associativity
        combine into a binary node
    return the left side
```

Left associativity: recurse with `power + 1`. Right associativity: recurse with `power`. That single
`+ 1` is the entire difference, which is the elegance people mean when they praise it.

**Read:** Vaughan Pratt's 1973 paper. *Crafting Interpreters* ch. 17 — same author and same language
as its ch. 6 cascading version, making it the cleanest available A/B comparison. Matklad's *Simple
but Powerful Pratt Parsing* is the best modern write-up.

**Related:** precedence climbing (the same algorithm, discovered separately), operator-precedence
parsing, binding power.

---

## Left-recursion elimination

**Problem:** the natural way to write a left-associative rule is `A = A op B | B`. A recursive
descent parser implementing that literally calls itself as the first thing it does, having consumed
nothing, and recurses until the stack ends.

**Shape:**

```
rewrite  A = A op B | B
as       A = B { op B }

parse one B, then loop consuming (op B) pairs, folding left as you go
```

The two grammars accept the same language. What is lost is that the rewritten form no longer
*encodes* associativity — the loop supplies it, so associativity must be stated separately in prose.

**Read:** standard; the Dragon Book §4.3 is the canonical treatment. Every recursive-descent tutorial
performs this rewrite, often without naming it.

**Related:** left factoring, EBNF repetition, LL vs LR (LR parsers handle left recursion natively,
which is much of their appeal).

---

## Panic-mode recovery

**Problem:** the parser hits a syntax error on line 3 and stops. The user fixes it and discovers an
error on line 7. Six iterations later they are still going.

**Shape:**

```
on error:
    report it
    set a flag suppressing further reports (to avoid cascades from the same mistake)
    discard tokens until reaching one that plausibly starts a new construct
    clear the flag and resume parsing normally
```

The suppression flag matters: without it one real error produces nine bogus follow-on errors and the
output is worse than stopping.

**Read:** *Crafting Interpreters* ch. 6 implements this in about twenty lines under the name
`synchronize`. Dragon Book §4.1.4 for the taxonomy of recovery strategies.

**Related:** synchronization sets, error productions, cascading errors, error cascade suppression.

---

## Synchronization sets (FIRST / FOLLOW)

**Problem:** panic-mode recovery has to decide where to resume. Resume too early and the parser
errors again immediately; too late and real errors are skipped.

**Shape:**

```
FIRST(rule)  = the set of tokens that can begin that rule
FOLLOW(rule) = the set of tokens that can legally appear immediately after it

to recover inside a rule: discard tokens until reaching something in FOLLOW of the enclosing rule,
    or in FIRST of a sibling construct
```

In a hand-written parser these sets are usually implicit — "skip to the next semicolon or closing
brace" is an informal FOLLOW set — but knowing the name tells you what you are approximating and
lets you check it.

**Read:** Dragon Book §4.4. Any parser-generator manual, since generators compute these
mechanically.

**Related:** LL(1) construction, nullable rules, predictive parsing.

---

## Error productions

**Problem:** a common mistake produces a technically correct but useless message. A user writing
`=` where the language wants `==` gets "unexpected token", when the compiler could say exactly what
happened.

**Shape:**

```
add a grammar rule that matches the *incorrect* form
its action is not to build a node, but to report a specific, targeted diagnostic
```

The parser deliberately accepts something illegal in order to describe it well.

**Read:** Clang's diagnostics are built substantially on this, and are the reason its error messages
are so much better than historical GCC's. `-fdiagnostics-parseable-fixits` is the mature form.

**Cost:** every error production is a permanent addition to the grammar for a mistake you predicted.
Add them in response to real confusion, not speculatively.

**Related:** diagnostics quality, fix-it hints, error recovery.

---

# AST and positions

## Source span tracking

**Problem:** a diagnostic that says only "type error" is nearly useless. To underline the offending
expression, every node must know where it came from.

**Shape:**

```
every token carries a span: start offset, length (or end offset)
every node inherits a span covering all of its constituent tokens —
    typically the start of its leftmost token to the end of its rightmost
```

Storing byte offsets rather than line/column pairs is the usual choice: offsets are one integer, and
line/column is derivable on demand via a line table.

**Read:** Rust's `Span`, Clang's `SourceLocation` and `SourceManager`. The design consideration both
share is making the span small, since every node carries one.

**Related:** line table, source maps, `#line` directives, diagnostic rendering.

---

## Line table + binary search

**Problem:** you have a byte offset and need to print "line 47, column 12". Counting newlines from
the start of the file for every diagnostic is O(file) per diagnostic.

**Shape:**

```
once, at scan time: record the byte offset at which each line begins, in a sorted array
to resolve an offset: binary search that array for the greatest line start <= the offset
    line number = index; column = offset - that line start
```

The array is sorted by construction, which is what makes the binary search available for free.

**Read:** Clang's `SourceManager` does exactly this. Any text editor's buffer implementation faces
the same problem, often solved with a piece table for the mutable case.

**Related:** binary search on offsets, piece table, rope, interval lookup.

---

## Index-based node references

**Problem:** *not* arena growth in general — get this distinction right, because it decides whether
the technique is needed at all. A **block-chained** arena mallocs a fresh block and links it;
existing blocks are never moved or freed until teardown, so pointers into them stay valid for the
arena's lifetime, and plain child pointers are perfectly safe. Only a **realloc-based** arena, which
may move the whole region, invalidates outstanding pointers.

The real arguments for indices are elsewhere: **size** (a 32-bit index is half a pointer, so a
million-node tree halves the memory each traversal pulls through cache) and **anything that leaves
the address space** — serializing the AST, caching it to disk, mmapping it, sharing it across
processes. A raw pointer is meaningless outside the process that produced it; an index is not.

**Shape:**

```
store nodes in a growable array
a child reference is an integer index into that array, not a pointer
resolve an index to a node only at the moment of use
```

**Read:** Zig's compiler, Rust's `rustc` (arena plus index newtypes), and the data-oriented design
literature generally. Andrew Kelley's talk on data-oriented design in the Zig compiler is the most
accessible treatment of why this wins at scale.

**Cost:** indices are bare integers unless wrapped in distinct types, so nothing stops you passing a
type index where an expression index was wanted — a bug the compiler will not catch. Debugging is
worse too: following a child in a debugger becomes manual arithmetic rather than a click.

**When not to reach for it:** a first compiler with a block-chained arena and a tree measured in
thousands of nodes. Take it when there is a measured cache problem or a serialization requirement,
not preemptively.

**Related:** the wrapping problem above is what newtypes and generational indices exist to solve;
struct-of-arrays is the natural next step once nodes are index-addressed rather than
pointer-addressed, since indices make the parallel-array layout free.

---

## Side table for annotations

**Problem:** the type checker computes a type for every expression. Storing it as a field on the
node means the parser must leave a hole for it, and the AST is no longer immutable after parsing.

**Shape:**

```
keep a separate map (or array, if nodes are index-addressed) from node to computed type
the parser's output is never modified
consumers that need types consult the side table
```

The phase separation becomes visible in the data layout rather than living only in a comment. It
also makes "run the type checker twice with different rules" trivially possible.

**Read:** `rustc` keeps typeck results in side tables keyed by `HirId`. The tradeoff is discussed in
most compiler-architecture writing under "AST annotation".

**Cost:** a lookup on every access rather than a field read. If nodes are index-addressed the side
table is a parallel array and the lookup is one index — cheap enough that the argument mostly
dissolves.

**Related:** attribute grammars, decorated AST, parallel arrays.

---

# Checking

## Symbol table with scope chain

**Problem:** resolving a name means finding the innermost declaration that is still in scope, with
inner declarations shadowing outer ones — and doing it without copying the outer scope on every
block entry.

**Shape:**

```
a stack of scopes; each scope is a map from name to declaration
entering a block pushes a scope; leaving pops it
lookup walks outward from the innermost scope, returning the first hit
```

A common optimisation collapses this into a single flat stack of entries plus a stack of
watermarks — entering a scope records the current height, leaving truncates back to it. Lookup is a
backward linear scan, which is fast in practice because scopes are small.

**Read:** *Crafting Interpreters* ch. 22 does the flat-stack version and explains the reasoning.
chibicc's scope handling is compact and readable.

**Related:** lexical scoping, shadowing, de Bruijn indices (the shadowing-free alternative),
environments.

---

## Bidirectional type checking

**Problem:** "infer the type of this expression" and "check this expression against an expected
type" are different questions, and conflating them makes both harder. Literal typing is the classic
case: whether `0` is an integer or a float depends on which question you are asking.

**Shape:**

```
two mutually recursive judgments:
    infer(expression) -> type            — synthesise a type from the expression alone
    check(expression, expected) -> ok    — verify the expression fits an expected type

the default rule: to check, infer and then compare
specific forms get their own check rules where inference alone is insufficient
```

A language that mandates annotations at every binding — no inference across statements — is one
where `infer` alone suffices. Naming the technique matters mainly because it tells you what changes
when inference is added later.

**Read:** Dunfield and Krishnaswami, *Bidirectional Typing* (2020) is the survey and is unusually
readable. The technique underpins most modern type checkers.

**Related:** type synthesis vs. checking, Hindley–Milner, unification, local type inference.

---

# Codegen

## Constant folding

**Problem:** an expression whose operands are all known at compile time is evaluated at runtime for
no reason.

**Shape:**

```
during a walk over the tree, when every operand of a node is a literal:
    evaluate the operation now
    replace the node with a literal holding the result
```

The subtlety is that the folder must produce **exactly** the result the target would have produced —
same width, same rounding, same overflow behaviour, same division semantics. A folder that computes
in a wider type than the target uses is a correctness bug that only appears at the boundaries.

**Read:** the term is universal. Any optimising-compiler text covers it as the first and simplest
optimisation.

**Cost:** must be suppressed for operations that trap or have observable side effects, unless the
trap is also reproduced at compile time as a diagnostic.

**Related:** constant propagation, partial evaluation, peephole optimisation, algebraic
simplification.

---

## `#line` directives

**Problem:** generated C compiles to a binary whose debug information points at the generated C.
The user, stepping through in a debugger, sees machine-produced code they never wrote.

**Shape:**

```
before emitting each construct, emit a directive naming the original file and line
the C compiler records those positions in its debug information instead of its own
```

The directive must not be emitted for code with no original source position — a runtime prelude, for
instance, belongs to no line of the source language.

**Read:** C standard §6.10.4. `cfront`, `bison`, and `lex` all do this; every transpiler that cares
about debuggability does. The modern web equivalent is source maps, which solve the identical problem
with a side file instead of inline directives.

**Related:** source maps, DWARF, debug information, `#pragma` line control.

---

# Testing

## Differential testing

**Problem:** a golden test requires knowing the correct answer in advance, so your test suite only
covers programs someone thought to write — and thought to compute the answer for by hand.

**Shape:**

```
build two independent implementations of the same semantics
run both on the same input
any difference in output is a bug in one of them, without anyone knowing the right answer
```

**Independence is the entire load-bearing assumption.** If the two share code, or worse share
*reasoning*, they fail identically and the diff comes back clean. This is called **common-mode
failure**, and it is the reason the second implementation should be the obvious, slow, naive one
written straight from the specification rather than a clever variant of the first.

**Read:** Csmith (Yang, Chen, Eide, Regehr, 2011) found hundreds of real bugs in GCC and LLVM this
way — the paper is a good read and unusually entertaining. John Regehr's blog covers the practice at
length.

**Related:** metamorphic testing, N-version programming, common-mode failure, oracle problem.

---

## Property-based testing

**Problem:** examples test the cases you thought of. The bug is in the case you did not.

**Shape:**

```
state a law the code must satisfy for all valid inputs
generate random inputs
assert the law
when it fails, shrink the failing input to the smallest one that still fails
```

The shrinking step is what makes the technique usable — a raw random failure is often a 200-element
input nobody can read.

Where a property set **uniquely characterises** the answer — where exactly one output can satisfy
all the stated laws — property testing becomes a stronger guarantee than differential testing,
because it is checking against the definition rather than against another implementation.

**Read:** Claessen and Hughes, *QuickCheck* (2000) — the original, and short. Theft and
theft-adjacent libraries do this in C. Hypothesis' documentation is the best modern explanation of
shrinking.

**Related:** shrinking, generators, invariants, metamorphic relations, fuzzing.

---

## Delta debugging

**Problem:** the random program generator produced a 4000-line input that crashes the compiler. The
bug is six lines somewhere in there and you cannot find them by reading.

**Shape:**

```
repeatedly:
    remove some portion of the input
    if the reduced input still fails in the same way, keep the reduction
    otherwise restore it and try a different portion
stop when no single removal preserves the failure
```

The result is *1-minimal*: no further single deletion preserves the bug. The "same way" test matters
— reducing to a program that crashes for a *different* reason is the classic failure mode.

**Read:** Zeller and Hildebrandt, *Simplifying and Isolating Failure-Inducing Input* (2002) — the
ddmin algorithm. C-Reduce is the production tool for C specifically and is language-aware, which
makes it far more effective than blind line deletion.

**Related:** test-case reduction, ddmin, C-Reduce, bisection, interestingness test.

---

## Random program generation

**Problem:** differential testing needs inputs, and hand-written ones only cover what was imagined.

**Shape:**

```
generate from the grammar, choosing productions randomly with tuned weights
constrain generation so the output is not merely syntactically valid but *well-typed*
    and free of undefined behaviour — otherwise every disagreement is uninteresting
```

The hard part is not generating syntax; it is generating programs whose behaviour is *defined*, so
that a disagreement between implementations is genuinely a bug rather than both being allowed.

**Read:** Csmith is the reference implementation and its paper explains the undefined-behaviour
avoidance in detail. Grammarinator and similar tools do the naive version.

**Related:** grammar-based fuzzing, swarm testing, generative testing, coverage-guided fuzzing
(AFL — a different and complementary approach).

---

## Small-step operational semantics

**Problem:** a specification written in prose can be ambiguous and cannot be tested against. "What
does this program mean?" needs an answer precise enough to check an implementation against.

**Shape:**

```
define a relation: one configuration steps to another
    a configuration is roughly (expression or statement, environment)
write one rule per syntactic form, giving the conditions under which it steps and to what
evaluation is the transitive closure: step until no rule applies
```

Two theorems are then stateable, and together they say "well-typed programs do not get stuck":

- **Progress** — a well-typed configuration either is a final value or can take a step.
- **Preservation** — if a well-typed configuration steps, the result is still well-typed.

This is the artifact that turns a specification from prose into something with content. It is also
the prerequisite for any later mechanised proof, so writing it is never wasted even if the proof
never happens.

**Read:** Pierce, *Types and Programming Languages*, ch. 3 and 8 — the standard introduction, and
the first chapters are genuinely approachable. Its *small-step* framing is what most modern work
uses. Big-step (natural) semantics is the alternative and is closer to how a tree-walking
interpreter is actually written.

**Related:** big-step semantics, progress and preservation, structural operational semantics,
denotational semantics, CompCert (the mechanised end of this road).
