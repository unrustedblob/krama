# Compiler construction

Organised by pipeline stage. Every shape sketch is pseudocode.

## Contents

**Scanning:** maximal munch · trivia attachment · keyword post-filtering · perfect hashing · string
interning · token lookahead buffer
**Parsing:** cascading recursive descent · Pratt parsing · left-recursion elimination · panic-mode
recovery · synchronization sets · error productions · declaration follows use · lvalue / place
expression
**AST and positions:** source span tracking · line table + binary search · index-based node
references · side table for annotations
**Checking:** symbol table with scope chain · bidirectional type checking · type constructor /
derived type · kinds · transitive const · interior mutability · variance · mutability polymorphism ·
constant expression vs. const-qualified object · effectively final · escape analysis ·
intraprocedural vs. interprocedural analysis · region-based memory management
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

## Trivia attachment

**Problem:** comments and whitespace must not reach the parser — a comment can appear between any
two tokens, so letting one into the stream means every lookahead site must tolerate it, and the one
site you forget is a bug that surfaces when someone comments in an unusual place. But discarding
them in the scanner destroys them permanently, and some tools need them back.

**Shape:**

```
the scanner recognises comments and whitespace (this stays a lexical decision)
rather than discarding them, it attaches each run to a neighbouring token as trivia
each token carries leading trivia and trailing trivia
the parser matches only on real tokens and never sees trivia at all
```

The token stream can then reproduce the source byte for byte. The resulting tree is called
**lossless** or **full-fidelity**, and is a *concrete* syntax tree rather than an abstract one.
Splitting leading from trailing trivia is what keeps an end-of-line comment on its own line instead
of migrating to the top of the next statement.

**Read:** Roslyn coined the term and documents the design well. rust-analyzer and Swift's libSyntax
are the same approach.

**Cost:** bookkeeping on every token, plus an arbitration rule for who owns an ambiguous run of
blank lines between two constructs.

**When to bother:** a compiler that only emits code should discard in the scanner and skip this
entirely. Build it if a formatter, a doc generator, or an editor refactoring is ever wanted — those
are impossible to retrofit once comments are gone, which is the real reason the choice matters
early. Note that round-trip *structural* testing is not a reason: structure survives discarding
comments.

**Related:** the abstract/concrete distinction here is the same one that makes a pretty-printer
re-derive parentheses from precedence rather than recover them from the tree, since an AST discards
grouping exactly as it discards comments.

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

## Declaration follows use (C declarator syntax)

**Problem:** C has no syntax for writing a type on its own. "Pointer to array of 25 int" only exists
smeared across a declarator with the identifier buried in the middle, so readers reach for the
"clockwise/spiral rule", which is wrong for some declarations. Designing a new language's type
syntax without knowing why C's looks like that tends to reproduce it by accident.

**Shape:**

```
C:        int *p;          — read as "the expression *p has type int"
          int (*a)[25];    — identifier in the middle; read outward

prefix:   p: *[25]int      — read left to right, no backtracking
```

C's rule is that a declaration mirrors the *use* of the thing declared. That is internally
consistent and is the reason `*` is a prefix in the declarator but the type constructor is
conceptually postfix. It also does not survive being moved to a prefix type grammar: `*[25]i32` can
no longer claim to mirror any expression, so a language choosing `*` after that point is choosing
familiarity, not coherence — which is a fine reason, just not C's reason.

Where C needs a type with no name attached — casts, `sizeof` — it uses an *abstract declarator*: the
same grammar with the identifier deleted. That is the language admitting it needs a name-free
spelling and declining to build a separate one.

**Read:** K&R §5.1. The C99 Rationale on declarator syntax. `cdecl` as a tool, and its existence as
evidence. Go's "Go's Declaration Syntax" blog post argues the prefix case directly.

**Cost:** none to know; the trap is assuming a prefix grammar inherits C's justification along with
its glyph.

**Related:** abstract declarator (the name-free spelling), right-to-left reading (the rule that
actually works), type constructor (what the prefix form makes explicit).

---

## Lvalue / place expression

**Problem:** assignment needs to say *where* to store, not *what* value to compute. As soon as the
left of `=` can be more than a bare identifier — `a[i]`, `s.f`, `*p` — the language has acquired a
second grammar running alongside the expression grammar, whether or not anyone designed one.

**Shape:**

```
place = IDENT { "." IDENT | "[" expr "]" | deref } ;

assignment = "set" place assign_op expr ";" ;

evaluating a place yields a location, not a value
evaluating an expression yields a value, not a location
```

Two properties make this worth naming rather than growing one statement form per target shape.
First, one production covers fields, elements and dereference together, so each new composite type
costs nothing. Second, keeping places as their own judgment in the semantics ("evaluate a place to a
location") is what lets assignment stay a statement while its target grows arbitrarily complex.

The hazard is compound assignment. `a[f()] += 1` must evaluate the place once; a naive desugaring to
`a[f()] = a[f()] + 1` evaluates it twice. C specifies single evaluation explicitly for this reason.
A language with no side effects in expressions gets it free — and stops getting it free at exactly
the moment calls become expressions.

**Read:** ISO C §6.3.2.1 (lvalues, arrays and function designators). The Rust Reference, "Place
Expressions and Value Expressions", which is where the modern terminology comes from. C++'s five
value categories are the cautionary version of letting this grow.

**Related:** value categories, addressability, compound assignment double-evaluation, desugaring
(where the double-evaluation bug is introduced).

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

## Type constructor / derived type

**Problem:** the moment a type can contain another type — pointer, array, struct, function — a flat
`enum TypeKind` stops working, and the checker's `==` on a tag stops being type equality. The
representation change is structural and retrofitting it means finding every site that built a type
by hand.

**Shape:**

```
a type is a tree:
    leaf        i32, f32, bool, ...
    constructor tag + arguments

    ptr(T)              one type argument
    array(T, N)         a type argument and a value argument
    func(T..., R)       many type arguments

type equality is a recursive walk over constructor and arguments,
not an integer comparison
```

The name matters because it separates two things beginners conflate. `ptr` on its own is *not* a
type — it is a function from types to types. `ptr(i32)` is a type. C's standard calls the results
**derived types** and treats them as types in full standing; what C lacks is a way to *write* one
without a declarator wrapped around it, which is why "a pointer isn't really a type in C" is a
common and wrong conclusion.

Array is the awkward member: it takes a type *and a value*, so the argument list is heterogeneous.
That makes arrays harder than single-parameter generics in this one respect, not a special case of
them.

**Read:** ISO C §6.2.5 on derived types. Pierce, *Types and Programming Languages* ch. 29 for the
general treatment. Clang's `ASTContext` for a production version with uniquing attached.

**Related:** kinds (the notation for "how many arguments and of what sort"), canonicalization
(making equal types one object), hash consing, const generics (value arguments taken seriously).

---

## Kinds / kinding

**Problem:** once types take arguments, some things that look like types are not types. `ptr` alone
cannot annotate a variable; `ptr(i32)` can. Without a word for that distinction, the checker either
allows nonsense or forbids it by accident.

**Shape:**

```
*         the kind of a type            i32, bool, ptr(i32)
* -> *    takes a type, returns a type  ptr, array-of
* -> * -> *                             pair, map-of

"kind checking" is type checking one level up:
    ptr(i32)     well-kinded
    ptr          not a type, only a constructor
    ptr(ptr)     ill-kinded — ptr is not a type
```

Most languages never expose kinds to the programmer, and a language that only ever *applies*
constructors never needs the machinery. It becomes load-bearing the moment something quantifies over
a constructor rather than a type — `T<U>` where `T` is a parameter — which is **higher-kinded
types**, and materially harder to infer. Knowing the word is mostly how you recognise that a design
sketch has wandered into it.

**Read:** Pierce, *TAPL* ch. 29–30 (System Fω). Haskell's `:kind` in GHCi is the cheapest way to see
kinds in action.

**Cost:** naming kinds in a small language's documentation usually costs more than it buys. Its
value here is diagnostic — it tells you which side of a hard line a feature sketch has landed on.

**Related:** higher-kinded types, associated types (the route that avoids them), type constructor,
dependent types (where a type takes a *value* argument, e.g. array lengths).

---

## Transitive const / deep const

**Problem:** `const` in C and C++ stops at one level. `const struct Node *n` prevents writing
`n->kind` but not `n->next->kind`, because `next` is an ordinary mutable pointer. A language that
wants "const all the way down" must decide *where* the propagation happens, and the obvious place is
wrong.

**Shape:**

```
shallow (C, C++):  const applies to the immediate object only

deep (D):          const propagates through every indirection reachable from it

the mechanism question — where is deepness enforced?

    at the binding    a const binding rewrites the types of what it holds
                      → the declared type and the actual type disagree

    at address-of     taking a reference to a const place yields a
                      const reference; no writable reference to a
                      const place can ever be obtained
                      → local rule, no rewriting, deepness falls out
```

The second form is the one that composes. It keeps binding immutability a property of the *place*
and pointee immutability a property of the *type*, two independent axes, without either silently
editing the other. Deepness then holds not because const spreads but because the writable capability
was never issued.

**Read:** D language specification, "Type Qualifiers — const and immutable". Zig's pointer
mutability table (`const x: *T` vs `var x: *const T`) is the clearest four-cell statement of the two
axes. C++'s `propagate_const` is the retrofit and shows the cost of not having it from the start.

**Cost:** deep const is a rule someone must keep enforcing, not a property that falls out — see
interior mutability.

**Related:** interior mutability (the documented escape hatch), variance (why the rule cannot pass
under a writable constructor), capability-based reasoning, bitcast (the other way to defeat it).

---

## Interior mutability

**Problem:** "immutable means nothing can change through this" is a rule with a hole in every
language that has one. Reference counting, memoization, and lazy initialization all need to mutate
through a shared read-only handle, so the language either provides a sanctioned hole or people cast
the qualifier away.

**Shape:**

```
outer type says: no writes through this reference
inner cell says: writes permitted, discipline enforced elsewhere

Cell / RefCell / atomics — the write is legal, the checking moved
from compile time to runtime (or to a proof the author supplies)
```

The point of naming it is not to build one. It is that "deep const" and "immutable" are claims about
what the *type system* enforces, and every mature language with such a claim also ships a documented
exception. Designing the guarantee without deciding the exception means the exception arrives later
as an unprincipled cast.

**Read:** *The Rustonomicon*, "Interior Mutability". Rust's `std::cell` documentation for the
sanctioned forms. C's `mutable`-equivalent is `const_cast`, which is the unprincipled version and
undefined when the object was genuinely const.

**Related:** transitive const, const-cast / `@constCast` (undefined when the original was const),
capability-based reasoning.

---

## Variance

**Problem:** a conversion that is obviously safe on its own is not automatically safe underneath a
type constructor. If `Cat` is acceptable where `Animal` is expected, is `array of Cat` acceptable
where `array of Animal` is expected? The intuitive "yes" is unsound, and the counterexample is one
function call long.

**Shape:**

```
covariant       T acceptable for U  =>  F(T) acceptable for F(U)
contravariant   T acceptable for U  =>  F(U) acceptable for F(T)
invariant       neither

rule of thumb:
    read-only positions may be covariant
    writable positions must be invariant

the counterexample, every time:
    accept F(T) where F(U) is expected
    the callee stores a legitimate U into it
    the caller reads it back believing it is a T
```

For qualifiers specifically this is why "const may be added" holds at the outermost level and
nowhere deeper. C states it directly: pointer-to-`T` converts to pointer-to-`const T`, but
pointer-to-pointer-to-`T` does not convert to pointer-to-pointer-to-`const T` — the qualifications
must match. Java made mutable arrays covariant, hit exactly this, and pays for it with a runtime
check on every array store forever.

**Read:** ISO C §6.5.16.1 on qualified assignment. Pierce, *TAPL* ch. 15.2. Java's
`ArrayStoreException` as the cautionary tale, and C#'s array covariance as the same mistake made
twice knowingly.

**Related:** subsumption (the rule variance constrains), depth-zero restriction, bottom type (the one
relation that is safe everywhere because there is no value to store).

---

## Mutability polymorphism (`inout`)

**Problem:** once `&T` and `&const T` are different types, a function that only *reads* through its
argument still has to pick one — and picking the read-only one is right but loses the caller's
mutability on the way out. The result is every accessor written twice, `get` and `get_mut`, differing
only in a qualifier.

**Shape:**

```
without it:
    fn first(xs: &const List) -> &const Item
    fn first_mut(xs: &List) -> &Item        — identical body

with it:
    fn first(xs: &inout List) -> &inout Item
    the qualifier is a parameter; the return borrows the argument's
```

This is a real gap rather than an ergonomic complaint: the two bodies are textually identical, so
the duplication is pure, and it multiplies with every accessor. D added a whole type qualifier for
it. Rust has not, and `get`/`get_mut` pairs are the visible consequence throughout its standard
library.

**Read:** D language specification, "Functions — Inout Functions". Rust's `std::collections` API as
the counterexample, where the duplication is right there in the method list.

**Cost:** it is a fourth qualifier interacting with the other three, and D's own documentation on the
transitivity rules is not short. Worth knowing the problem has a name before deciding to live with
the duplication.

**Related:** transitive const, variance, subsumption, generics (the general form of the same
parameterisation).

---

## Constant expression vs. const-qualified object

**Problem:** in C these are two different things wearing one keyword. A `const size_t n = 64;` is
**not** a constant expression, so it cannot size an array, label a `case`, or width a bitfield. C++
programmers hit this constantly. Separately, a language with array types in its type system needs to
say what expressions may appear in a type at all.

**Shape:**

```
const-qualified object   a runtime value that may not be written
constant expression      a value the translator can compute

C23:
    constexpr size_t N = 64;   usable as an array size and a case label
    const size_t n = 64;       not a constant expression

in a language with [N]T, the question is which expressions
may occupy N:
    literals only          simplest; no arithmetic in types
    constant expressions   requires a compile-time evaluator
```

The second half is the part that bites language design. `[25]i32` puts a value inside a type for the
first time, and "does `[2+3]i32` parse" is not a syntax question — it asks whether the type checker
must evaluate expressions, which is the thin end of compile-time evaluation and eventually of
dependent types.

**Read:** ISO C §6.6 on constant expressions. C23's `constexpr` (N2645). Hare's three-way split —
`let` for mutable bindings, `const` for immutable ones, `def` for compile-time constants — is the
clean version of separating the two meanings.

**Related:** const generics, dependent types, multi-stage programming (`comptime`), constant folding.

---

## Effectively final

**Problem:** a variable that is never reassigned is already immutable in fact, so a language can
compute constness rather than requiring it to be declared. That works, and it is the wrong trade —
knowing why is what stops the keyword being dropped as redundant.

**Shape:**

```
inferred:   scan the scope; if no assignment names it, treat as final

declared:   the programmer states it; the checker verifies

the asymmetry:
    inference derives the rule FROM the code, so the code is
    always consistent with what was inferred — a stray write does
    not make the program wrong, it makes the variable mutable

    a declaration is the only artifact that can be CONTRADICTED
```

Java computes this (a variable is *effectively final* if never reassigned, which is what lets it be
captured by a lambda) and kept the `final` keyword anyway. The inferred version cannot catch a
mistake; it can only describe one. The declared version also answers "does this still hold what line
4 gave it" in O(1) rather than a scan of the scope.

There is a third option worth knowing: Zig keeps `const` opt-in but makes a mutable binding that is
never mutated a **compile error**, so both spellings carry a contract without flipping the default.

**Read:** JLS §4.12.4 (effectively final). Zig's "local variable is never mutated" diagnostic and its
documented exemptions — globals, struct-scoped variables, and locals whose address is taken.

**Cost:** inference stops being sound as soon as writes can happen through an alias; see escape
analysis.

**Related:** escape analysis, transitive const, capability-based reasoning.

---

## Escape analysis

**Problem:** any claim of the form "nothing writes to this variable" or "this allocation does not
outlive this frame" is only true if no reference to it gets away — out through a return value, into
a parameter, onto a data structure that outlives the scope. Determining that is not a local question,
and assuming it is produces a check that is quietly unsound.

**Shape:**

```
for each allocation or binding:
    does any reference to it reach
        a return value?
        a parameter of a function that might store it?
        a longer-lived structure?

    no  -> it does not escape; stack-allocate, or trust the local claim
    yes -> it escapes; assume nothing
```

Two uses, opposite ends of the compiler. Optimisation: an object that does not escape can be
stack-allocated or scalar-replaced instead of heap-allocated — the main reason Java and Go can afford
so much allocation. Checking: a local rule like "never mutated" needs it as soon as an alias can be
formed, unless the language makes *taking a writable alias* its own syntactic act, in which case the
question collapses back to a local one.

That last clause is the useful part of knowing the name: it tells you which language design choices
let you avoid the analysis entirely rather than implement it.

**Read:** Choi et al., *Escape Analysis for Java* (OOPSLA 1999). Go's `-gcflags=-m` prints its escape
decisions, which is the cheapest way to see one working.

**Cost:** interprocedural and expensive, and conservative answers are the norm. If a design can avoid
needing it, that is worth more than implementing it well.

**Related:** intraprocedural vs. interprocedural analysis, region-based memory management, effectively
final, capability-based reasoning.

---

## Intraprocedural vs. interprocedural analysis

**Problem:** whether a check must look inside other functions decides its cost, its precision, and
whether it can run at all on incomplete programs. Designs slide from one to the other by accident,
usually while adding a feature that looks unrelated.

**Shape:**

```
intraprocedural   one function body at a time; calls are summarised
                  by their signatures
                  cost O(function), composes with separate compilation

interprocedural   follows calls into bodies
                  cost grows with the call graph; needs the whole program

the design lever: make signatures carry enough that call sites can be
resolved without opening the callee
```

The lever is the point. "Does this call write through my argument?" is interprocedural if the answer
lives in the callee's body, and intraprocedural if the parameter's declared type says so. A type
system is, among other things, a mechanism for turning interprocedural questions into local ones,
which is why adding a qualifier can be cheaper than adding an analysis.

**Read:** Nielson, Nielson and Hankin, *Principles of Program Analysis*, ch. 2 for the framing.
Rust's borrow checker is the large worked example of buying locality with signatures.

**Related:** escape analysis (the canonical interprocedural one), symbol table with scope chain,
whole-program optimisation, separate compilation.

---

## Region-based memory management

**Problem:** arenas work because the programmer knows a lifetime the language does not. Region
inference is the version where the *compiler* works it out — every allocation is assigned to a
region, regions nest, and a whole region is freed at once with no per-object bookkeeping and no
collector.

**Shape:**

```
annotate each allocation with a region variable
infer, per program point, which region an allocation belongs to
regions nest with lexical structure:

    letregion r in
        ... allocations tagged r ...
    end          -- entire region deallocated here

a value may not outlive its region — that constraint is the analysis
```

This is worth knowing for two reasons that are not "implement it". First, it is the formal account of
what an arena allocator does by hand, so it tells you what a manual arena is trading away: the
compiler is not checking that nothing outlives the teardown. Second, it is the ancestor of lifetime
and borrow checking — the question "does this reference outlive what it points at" is a region
constraint, and a language that adds references without answering it has reintroduced dangling
pointers no matter what its type syntax looks like.

**Read:** Tofte and Talpin, *Region-Based Memory Management* (Information and Computation, 1997).
Cyclone's regions are the C-shaped version. Grossman et al., *Region-Based Memory Management in
Cyclone* (PLDI 2002).

**Cost:** full inference is heavy machinery and famously produces bad diagnostics when it fails —
Cyclone's reputation. Most projects want the manual arena and an explicit rule about what may hold a
pointer into it.

**Related:** arena / bump allocation (the manual version), escape analysis, lifetimes and borrow
checking, capability-based reasoning.

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
