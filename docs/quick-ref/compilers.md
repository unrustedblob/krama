# Compiler construction

Organised by pipeline stage. Every shape sketch is pseudocode.

## Contents

**Language design:** structured program theorem · labelled break and labelled blocks · iterator
protocol vs. built-in range · refutable vs. irrefutable patterns
**Scanning:** maximal munch · trivia attachment · keyword post-filtering · perfect hashing · string
interning · token lookahead buffer
**Parsing:** cascading recursive descent · Pratt parsing · left-recursion elimination · panic-mode
recovery · synchronization sets · error productions · non-associative precedence · expression
restrictions
**AST and positions:** source span tracking · line table + binary search · index-based node
references · side table for annotations
**Lowering and IR:** desugaring · A-normal form / three-address code
**Type systems:** unit type · bottom type · strong typedef · bitcast · arbitrary-width integer types
**Checking:** symbol table with scope chain · bidirectional type checking · subsumption and least
upper bound · normal completion and reachability
**Codegen:** constant folding · `#line` directives · zero-sized types and unit erasure
**Testing:** differential testing · property-based testing · delta debugging · random program
generation · small-step operational semantics

---

# Language design

## Structured program theorem (Böhm–Jacopini)

**Problem:** "which control-flow constructs does the language actually need?" reads like a taste
question. It is not — the minimal set is a published result, which turns the shopping list for a
core language into a fact rather than an argument.

**Shape:**

```
sequence + selection + iteration  ==  any computable function

everything else is derived:
    for, do/while, switch, break, continue, early return
```

The practical use is as a stopping rule. A core with one selection form and one iteration form is
known to be complete, so anything further is added for ergonomics and can be argued about on those
terms alone.

**Read:** Böhm and Jacopini, *Flow Diagrams, Turing Machines and Languages with Only Two Formation
Rules*, CACM 1966. Knuth's *Structured Programming with go to Statements* (1974) is the essential
counterweight and the more useful of the two for anyone actually designing a language. Dijkstra's
*Go To Statement Considered Harmful* is the polemic that made the result famous and says the least.

**Cost:** the construction can require introducing extra state variables, so "expressible" and
"expressible without contortion" are different claims. The theorem is cited as licence to ban `goto`
far more often than it supports — that reading is the index's warning, not the paper's argument.

**Related:** reducible control flow graphs, single-entry single-exit regions, Knuth's counterargument.

---

## Labelled break and labelled blocks

**Problem:** `break`, `continue`, and any block-value keyword bind to the nearest enclosing
construct. When the exit or the value belongs to an *outer* one — a search three levels deep that
has found its answer — nearest-enclosing cannot express it, and the workaround is a flag variable
tested at every level on the way out.

**Shape:**

```
outer: {
    for (x in xs) {
        for (y in ys) {
            if (match(x, y)) { break outer (x, y); }
        }
    }
    give none;
}

the label names a target; the exit is non-local
every block between here and the target has its remainder made unreachable
```

**Read:** Zig's labelled blocks — `blk: { break :blk value; }` — are the most complete version,
since one mechanism carries both the exit and the value. Rust's labelled `break 'a value` is the
same idea restricted to loops and `loop` blocks. Java (JLS §14.15) has labelled break for loops
without the value half, which is the older and narrower form.

**Cost:** two decisions that nearest-enclosing never raised. Labels occupy a namespace that must be
kept separate from variables, which is why Zig and Rust both mark them syntactically rather than
letting a bare identifier serve. And a non-local exit feeds the reachability predicate, so it cannot
land before that analysis exists.

**Related:** normal completion analysis, structured non-local exit, `goto` with a restricted target.

---

## Iterator protocol versus built-in range

**Problem:** `for x in thing` has to decide what `thing` may be. Either a fixed set of built-in
types the compiler knows how to walk, or anything implementing an interface — and the second answer
cannot be taken before the language has interfaces, generics, and a way to name an associated
element type.

**Shape:**

```
built-in:   for x in xs
              compiler knows arrays, slices, maps, strings, and nothing else
              no user extension; the loop is a primitive

protocol:   for x in xs   desugars to
              it = xs.into_iter()
              loop { match it.next() { Some(x) => body, None => break } }
              any type implementing the trait works
```

**Read:** the Go spec's "For statements with range clause" is the built-in answer, unchanged and
uncontroversial for fifteen years. The Rust reference on `IntoIterator` gives the protocol answer
with the desugaring written out explicitly — worth reading as an instance of the sugar/core split as
much as of iteration.

**Cost:** the protocol answer drags in its entire dependency chain — traits or interfaces, generics,
and a monomorphisation or boxing strategy for them. That is a compilation-architecture decision, not
a loop feature. The built-in answer forecloses nothing: a protocol can be added later and the
built-in types retrofitted onto it.

**Related:** monomorphisation, associated types, desugaring, external vs. internal iteration.

---

## Refutable versus irrefutable patterns

**Problem:** `let (a, b) = pair` and `let (a, 0) = pair` look like one feature and are two. The
first cannot fail — every pair has two components. The second can, and admitting it obliges the
language to say what happens on failure, plus exhaustiveness checking wherever that failure is meant
to be handled rather than fatal.

**Shape:**

```
irrefutable — always matches, legal in a binding position
    let (a, b) = pair
    for (k, v) in map
    fn f((x, y): Point)

refutable — may fail, legal only where failure has somewhere to go
    match v { Some(x) => ..., None => ... }
    if let Some(x) = v { ... }

rule: binding positions accept irrefutable patterns only
      exhaustiveness checking applies to refutable positions
```

**Read:** the Rust reference, "Patterns", §refutability, states the split as a normative rule and is
the clearest short treatment. Wadler's *Views: A way for pattern matching to cohabit with data
abstraction* sets out the tension between patterns and encapsulation. Maranget's *Compiling Pattern
Matching to Good Decision Trees* is the implementation, and carries the exhaustiveness algorithm.

**Cost:** destructuring arrives looking free — a tuple type and a binding form. What follows is not:
refutable patterns, match expressions, exhaustiveness checking over user types, and a diagnostic
that can say *which* case is missing. Adopt destructuring on its own merits, never as a side effect
of a loop header or a multiple binding.

**Related:** exhaustiveness checking, decision tree compilation, `switch` without `default` as the
degenerate case.

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

## Non-associative precedence (`%nonassoc`)

**Problem:** `a < b < c` is legal C and means something almost nobody intends. A precedence level
written as a loop admits chains; the same level written as an optional single rejects them at parse
time, before types are involved at all.

**Shape:**

```
left-associative — a loop
    additive   = mul { ( "+" | "-" ) mul }

non-associative — an optional single
    comparison = additive [ ( "<" | ">" | "<=" | ">=" ) additive ]
```

The technique generalises past comparison: any operator whose chained form is meaningless rather
than merely surprising belongs at a non-associative level. Narrowing the production makes the bug
unrepresentable instead of diagnosable.

**Read:** POSIX yacc's `%nonassoc` declaration is where the term comes from and is worth reading
alongside `%left` and `%right` to see the three as one mechanism. The Rust reference — "comparison
operators cannot be chained" — is the modern statement. Python deliberately does the opposite, where
`a < b < c` means `a < b and b < c`, and is the dissent worth understanding before choosing.

**Cost:** the rejected expression is *type-correct* in a language with `bool`, so the diagnostic has
to explain the design rather than report a mismatch.

**Related:** single non-recursive unary prefix, precedence declarations, ambiguity resolution by
production narrowing.

---

## Expression restrictions

**Problem:** a language with brace-delimited blocks and brace-delimited struct literals cannot parse
`if x { }` unambiguously — `x { }` may be a struct literal with the `if` still awaiting its block,
or `x` may be the condition and `{ }` the block. The grammar is genuinely ambiguous, not merely
awkward, and lookahead does not help because both parses are complete.

**Shape:**

```
the ambiguity
    if point { }        # `point {}` a struct literal? or `point` the condition?

fix A — restrict the expression grammar in that position
    if_expr = "if" expr_no_struct_literal block

fix B — require parentheses, and the ambiguity cannot arise
    if_expr = "if" "(" expr ")" block
```

**Read:** the Rust reference, "Expressions", on the struct-expression restriction, and the Go spec's
parsing-ambiguity paragraph under "Composite literals". Both languages took fix A and both document
it as a wart. Swift has the same collision with trailing closures.

**Cost:** fix A makes the expression grammar context-dependent — an expression legal in one position
is illegal in another, and every error message then has to explain that. Fix B costs two characters
and is what C already did by accident.

**Related:** dangling else, grammar ambiguity, context-dependent productions, offside rule
collisions.

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

# Lowering and IR

## Desugaring (sugar/core split, lowering)

**Problem:** every convenient surface form — compound assignment, `for`, `else if` chains — adds a
case to the type checker, the interpreter, the code generator, and the written semantics. The cost
of a feature is not one case; it is feature count times pass count.

**Shape:**

```
core  = the minimal set of forms the semantics has rules for
sugar = everything else, defined as a rewrite into core

lower(node):
    CompoundAssign(place, op, rhs) -> Assign(place, Binary(op, Read(place), lower(rhs)))
    ForLoop(init, cond, step, body) -> Seq(init, While(cond, Seq(body, step)))
    _ -> node, children lowered

pipeline: parse -> lower -> check -> { codegen, interpret }
```

Where the rewrite happens is its own decision. In the parser it is cheapest and least visible; as a
separate pass between parse and check it costs a tree walk and keeps phase separation honest.

**Read:** the rustc dev guide on HIR lowering is the most legible modern implementation. GHC's
desugarer and Simon Peyton Jones's *The Implementation of Functional Programming Languages* are the
tradition it comes from. Landin coined "syntactic sugar" in *The Next 700 Programming Languages*.

**Cost:** two failure modes, both silent. A rewrite that duplicates a subexpression changes how many
times it evaluates — `a[f()] += 1` is the canonical case. A rewrite that synthesises nodes destroys
the spans diagnostics point at, so the user gets an error on an operator they never typed. Both are
handled at the rewrite site or not at all.

**Related:** macro expansion, HIR, Core, source-to-source translation, hygiene.

---

## A-normal form / three-address code

**Problem:** a nested expression tree gives every analysis and optimisation pass an arbitrary shape
to walk. Normalising so that every operation takes only trivial operands — variables and literals —
gives each pass one uniform form to handle.

**Shape:**

```
a + b * c

becomes

t0 = b * c
t1 = a + t0
```

**Read:** Flanagan, Sabry, Duba and Felleisen, *The Essence of Compiling with Continuations* (1993),
for ANF. The Dragon Book §6.2 for three-address code, which is the same idea in classical imperative
dress. SSA is what results from additionally numbering each assignment uniquely.

**Cost:** named, real, and not for a C backend — C accepts nested expressions, so normalising before
emission buys nothing and costs readability in the generated output. It becomes necessary when
targeting an IR that only takes atomic operands, such as QBE or LLVM. The *style* it describes —
temporaries over nesting, statements over expressions — is worth keeping in generated C even when
the form is not, because it is what keeps the output debuggable.

**Related:** SSA, continuation-passing style, quadruples, temporaries.

---

# Type systems

## Unit type (nullary tuple, `()`)

**Problem:** does an expression that produces nothing have *some* type, or *no* type? The answer
decides whether the language needs one `if` or two. If "no value" is itself a type with exactly one
value, then statement-position `if` is just an expression of that type and no second construct is
required.

**Shape:**

```
unit is a type with exactly one value, carrying zero bits

    type(block with no value-producing terminator) = Unit
    type(if c { } else { })                        = Unit
    type(while c { })                              = Unit

without it, every construct needs two forms:
    one usable as a statement, one usable as an expression
```

**Read:** the Rust reference on the unit type `()`, with the fact that Rust has exactly one `if` as
the payoff. ML's `unit` and Haskell's `()` are the same construct in the tradition it comes from.
Pierce, *Types and Programming Languages*, §11.2, treats it as a base case when building a type
system up from nothing.

**Cost:** the type must be erased at code generation or it costs real loads and stores, and it is
unspellable in a target language whose `void` is uninhabited — `let x: unit` has no direct C
translation. Easily confused with the bottom type, which has zero values rather than one and behaves
oppositely in a checker.

**Related:** zero-sized types, bottom type, `void` as a return type only, tuple arity.

---

## Bottom type (uninhabited type, `never`, `!`)

**Problem:** a branch that diverges — returns, aborts, loops forever — produces no value, but a type
checker comparing branch types demands one. Without a type for "control does not reach here", every
`if` with an early return in one arm is rejected, and the language grows an ad-hoc exception in the
typing rule instead.

**Shape:**

```
type(return e) = Never
type(abort())  = Never

Never is a subtype of every type, so it disappears at the join:
    join(Never, T) = T
    join(T, Never) = T
    join(T, U)     = error unless T == U

a value of Never cannot be constructed, so the subtyping is sound:
the coercion site is unreachable by construction
```

**Read:** the Rust reference on the never type `!`, and RFC 1216 for the argument that introduced it.
Kotlin's `Nothing`, TypeScript's `never` and Scala's `Nothing` are the same construct. Pierce,
*Types and Programming Languages*, §15.4, on Bot.

**Cost:** it is the one subtyping relation a language otherwise free of subtyping must admit, so
"no coercions anywhere" stops being literally true — the honest statement becomes "no coercions
except from the type that has no values". Distinct from the unit type: unit has one value and is
erased at codegen, Never has none and is unreachable. Conflating them yields a checker that accepts
genuinely dead code.

**Related:** normal completion analysis, divergence, subtyping, uninhabited vs. zero-sized.

---

## Strong typedef (distinct type versus alias)

**Problem:** two things with identical representation and different meaning — a byte and a small
number, a user ID and a post ID, metres and feet — are interchangeable to the compiler if one is an
alias for the other. Every confusion between them is then a bug the type system was in a position to
catch and declined to.

**Shape:**

```
alias:    B refers to the same type as A
          a B is accepted anywhere an A is expected — no checking gained

distinct: B has A's representation and its own identity
          conversion in either direction is explicit and named
          B's legal operations are declared independently of A's

the test: does f(a) compile, where f expects B and a is an A?
          alias -> yes.  distinct -> no.  That is the whole difference.
```

**Read:** C++17's `std::byte` (proposal P0298, Macintosh) is the best-documented modern instance,
and its rationale — that `char` was doing double duty as character and as raw memory — is the
canonical argument for the split. Ada's derived types (RM §3.4) are the oldest thorough version.
Haskell's `newtype` and Rust's tuple-struct wrapper are the functional spelling. Go's
`type byte = uint8` is the deliberate counterexample.

**Cost:** every boundary crossing becomes an explicit conversion, and the verbosity lands hardest in
exactly the code that crosses most — parsing, serialisation, hashing. VHDL has enforced this split
between `std_logic_vector` and `numeric_std` since the early nineties, so the verdict is in: users
complain about the conversion noise and nobody proposes removing it.

**Related:** newtype, phantom types, units of measure, nominal vs. structural typing.

---

## Bitcast (type punning, `transmute`)

**Problem:** two conversions look alike and are not. One preserves the *value* and may change the
bits — integer widening, float to integer. The other preserves the *bits* and changes the meaning —
a float to its IEEE-754 pattern. They fail differently: the first can lose information or trap, the
second can only be wrong about intent. One spelling for both hides which risk was taken.

**Shape:**

```
cast(T, x)     value-preserving
               representation may change; may trap or lose precision
               widths may differ

bitcast(T, x)  representation-preserving
               bits retained, meaning changes; never traps
               REQUIRE sizeof(T) == sizeof(typeof(x))
```

The width requirement is the entire safety story. Without it the operation reads or invents bytes
that were never there.

**Read:** Zig separates `@bitCast` from `@intCast` and `@floatCast` and is the cleanest reference.
LLVM IR distinguishes the `bitcast` instruction from the converting casts for the same reason.
Rust's `std::mem::transmute` documentation is worth reading for the opposite lesson — it does not
stop at width equality and is correspondingly notorious.

**Cost:** in C the portable spelling is `memcpy`, not a pointer cast. Casting a `float *` to
`uint32_t *` and dereferencing violates strict aliasing and is undefined however universally it
appears in real code; the union trick is legal in C99 and not in C++. A language offering this needs
a builtin precisely because the obvious implementation is wrong.

**Related:** strict aliasing, `memcpy` as the portable pun, representation vs. value conversion.

---

## Arbitrary-width integer types

**Problem:** a bit-level slice five bits wide has no type in a language whose integers come in
8/16/32/64. Storing it in the next size up with the high bits zeroed puts the real width back in the
programmer's head, which is the untyped knowledge the type system was meant to capture.

**Shape:**

```
types u1 .. uN and i1 .. iN, width carried in the type

    slice(x: b32, 3..8) : b5

width arithmetic in the checker:
    concat(b3, b5) : b8

packed structs fall out:
    struct { flag: b1, kind: b3, len: b12 }   # 16 bits, layout stated not implied
```

**Read:** the Zig language reference on integers, which has `u1` through `u65535` as first-class
types. LLVM's Language Reference on the `iN` type is where the lowering strategy is visible. VHDL
and Verilog have had this natively for decades because hardware description requires it.

**Cost:** codegen must lower non-power-of-two widths to something the machine has, meaning masking
and shifting on every load and store. C's bitfields are the cautionary version — the feature without
the type system — which is why bit order and packing are implementation-defined there and nobody
relies on them portably. Bit numbering must also be defined by significance rather than memory
order, or the feature becomes endian-dependent.

**Related:** bitfields, packed structs, bit slicing, endianness, `iN` lowering.

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

## Subsumption and least upper bound

**Problem:** two branches produce different types and the expression needs one. Either the checker
demands they match exactly and rejects, or it computes a common supertype and accepts. This looks
like a convenience question and is not — it decides whether an expression's type can be read off
what is written, or must be inferred from context.

**Shape:**

```
exact match:   type(if c {A} else {B}) = A, and error unless A == B

subsumption:   push the expected type down into each arm
               ask "is this acceptable here", not "do these agree"

LUB / join:    synthesise the smallest type both arms fit
               join(i32, f32) = f32?  i32|f32?  error?

the tell: under LUB, deleting a type annotation elsewhere can change
          this expression's type
```

The explicit alternative is injection — the programmer names a constructor, both arms genuinely have
the wider type already, and exact match applies unchanged.

**Read:** Pierce, *Types and Programming Languages*, §15.3, for the subsumption rule stated formally.
TypeScript's union types are the structural LUB in wide production use. Java's conditional-operator
typing rules (JLS §15.25) are the cautionary version — a table so intricate that few users can
predict its result.

**Cost:** LUB buys ergonomics and spends predictability. In a language whose selling point is that
every expression's type is visible in its syntax, it is a contradiction rather than a trade.

**Related:** bidirectional type checking, join and meet, variance, explicit injection.

---

## Normal completion and reachability

**Problem:** "every path returns a value" and "there is no dead code after a return" can be enforced
structurally at first — one `return`, positioned last — and that rule dies the moment `if` exists.
The replacement is not a search for `return` statements; it is a per-statement judgment of whether
control can reach the statement after it.

**Shape:**

```
completes_normally(stmt) -> bool

    return e              -> false
    abort()               -> false
    block [s1..sn]        -> completes_normally(sn), and each si must
                             complete normally for s(i+1) to be reachable
    if c { a } else { b }  -> completes_normally(a) || completes_normally(b)
    if c { a }             -> true      # no else: the false path falls through
    while c { b }          -> true      # may execute zero times
    everything else        -> true

a function body must NOT complete normally, unless its return type is unit
a statement following one that does not complete normally is unreachable — an error
```

**Read:** the Java Language Specification §14.22, "Unreachable Statements", is the best-written
version anywhere — normative, exhaustive, per-construct, and short enough to read in one sitting.
C# has the equivalent as reachability plus definite assignment. The Dragon Book covers the general
dataflow machinery, which is far more than this needs.

**Cost:** the predicate is deliberately syntactic and therefore conservative. `while (true) { }`
is treated as completing normally by a naive rule even though it cannot, which is why Java
special-cases constant conditions — and every such special case is a place two compilers disagree
about whether a program is legal. Keep the rule dumb and documented rather than clever.

**Related:** definite assignment analysis, bottom type as the same information carried in the type,
dead code elimination, mandatory return.

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

## Zero-sized types and unit erasure

**Problem:** a type with exactly one value carries no information, so storing it, passing it and
returning it are all pure overhead. A backend that treats it as an ordinary type emits loads and
stores of nothing; a target language that cannot express an object of that type — C's `void` — will
reject the output outright.

**Shape:**

```
size_of(unit) == 0

at codegen:
    declaration of a zero-sized local -> emit nothing
    assignment of a zero-sized value  -> emit the RHS for its effects, discard
    parameter of zero-sized type      -> drop from the emitted signature
    return of zero-sized type         -> emit `return;`
```

**Read:** the Rust reference on zero-sized types, and the Rustonomicon's ZST chapter for what breaks
— notably that allocating one must not return null. Rust's `()`, `PhantomData` and empty structs are
all ZSTs. Haskell's `()` and ML's `unit` are the same idea without the layout concern, since neither
promises a memory representation.

**Cost:** erasure means the emitted code no longer corresponds one-to-one with the source tree, so
anything comparing the two — a differential interpreter, a `#line` scheme, a source map — has to
agree about what vanished. Distinct from an *uninhabited* type, which has zero values rather than
one; the two are easy to conflate and behave differently in a checker.

**Related:** unit type, newtype erasure, `#line` directives, differential testing.

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
