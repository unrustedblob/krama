# Algorithms

Named algorithms reachable from compiler and systems tooling work. Every shape sketch is
pseudocode.

## Contents

- Euclidean division adjustment
- FNV-1a / djb2 hashing
- Fixed-point iteration
- Memoization
- Reservoir sampling
- Topological sort
- Union-find (disjoint set)
- Worklist algorithm

Binary search on offsets is covered in `compilers.md` under *Line table + binary search*, where it
actually arises. Graph coloring is listed on the roster but has no entry yet — it becomes relevant
only with register allocation.

---

## Euclidean division adjustment

**Problem:** C's `/` and `%` truncate toward zero, so the remainder takes the sign of the dividend.
A language wanting floored or Euclidean semantics must derive them from what the hardware gives.

**Background — the three definitions.** All satisfy `a == q*b + r`. They differ in where the sign
lands:

- **Truncated** (C, Java): `q` rounds toward zero; `r` takes the sign of `a`.
- **Floored** (Python, Ruby): `q` rounds toward negative infinity; `r` takes the sign of `b`.
- **Euclidean**: `r` is always non-negative, whatever the signs. `q` follows from that.

Euclidean's appeal is that a non-negative remainder makes `i % n` safe as an index without a guard,
and it is the definition used in mathematics.

**Shape:**

```
compute q and r using the host's truncating division
if r is negative:
    adjust r upward by the magnitude of b
    adjust q by one, in the direction that keeps a == q*b + r
```

**The trap:** "the magnitude of b" is `abs(b)`, and `abs` of the most negative representable integer
overflows — the positive counterpart does not exist in two's complement. A correct implementation
must never form that absolute value at all, branching on the sign of `b` instead.

**Uniqueness is the useful property.** Given `a` and `b != 0`, exactly one pair `(q, r)` satisfies
both `a == q*b + r` and `0 <= r < |b|`. So an implementation satisfying both constraints is correct
*by theorem*, not by testing — which makes property-based testing here a complete proof of
correctness rather than a sampling of it. That is rare and worth exploiting when it appears.

**Read:** Raymond Boute, *The Euclidean definition of the functions div and mod* (1992) — the
argument for this definition, and short. Daan Leijen, *Division and Modulus for Computer Scientists*
is the clearest side-by-side comparison of the three.

**Related:** floor division, modulo bias, two's complement asymmetry, `INT_MIN` overflow.

---

## FNV-1a / djb2 hashing

**Problem:** you need a string hash for a symbol table and do not want to depend on a library or
think hard.

**Shape (FNV-1a):**

```
start from a fixed offset basis
for each byte:
    exclusive-or the byte into the accumulator
    multiply the accumulator by a fixed prime
```

djb2 is the same shape with the operations reversed and different constants. Both are a few lines
and are memorable enough to write without reference.

**Read:** the FNV specification names the constants for each width. *Crafting Interpreters* uses
FNV-1a and explains why a compiler's needs are modest here.

**Cost:** neither resists adversarial input. If a hash table is exposed to untrusted keys — a web
server's headers, say — an attacker can force collisions and turn a table into a linked list. Use
SipHash there. For a compiler's symbol table, where the input is a source file, this does not apply.

**Related:** SipHash, hash flooding attacks, avalanche, MurmurHash, xxHash.

---

## Fixed-point iteration

**Problem:** an analysis where each fact depends on other facts that are themselves being computed
— liveness, reaching definitions, constant propagation. There is no order in which to compute them
once.

**Shape:**

```
initialise every fact to a starting approximation
repeat:
    recompute every fact from the current values of the others
until nothing changed in a full pass
```

Termination requires the values to move in only one direction through a finite lattice, which is
why dataflow analyses are always framed in terms of lattices — the framing is what guarantees the
loop stops.

**Read:** any dataflow analysis chapter; the Dragon Book §9.3 is the standard reference. The
underlying theorem is Kleene's fixed-point theorem.

**Related:** worklist algorithm, lattice, monotone framework, abstract interpretation.

---

## Memoization

**Problem:** a pure function is called repeatedly with the same arguments during a tree walk, and
recomputes each time.

**Shape:**

```
keep a table from argument to previously computed result
on call: return the stored result if present, otherwise compute, store, and return
```

The requirement is purity. Memoizing a function that reads mutable state or has side effects
produces wrong answers that are hard to trace, because the wrongness depends on call order.

**Read:** the term is universal. In parsing, memoizing over (rule, position) is what turns
backtracking recursive descent into linear-time **packrat parsing** — worth knowing as the
connection.

**Related:** packrat parsing, dynamic programming, caching, purity, referential transparency.

---

## Reservoir sampling

**Problem:** choose k items uniformly at random from a stream whose length you do not know in
advance and cannot store.

**Shape (for k = 1):**

```
keep one item as the current choice
on seeing the nth item, replace the current choice with probability 1/n
```

At the end, every item seen has probability exactly 1/n of being the choice. The proof is three
lines and worth doing once — it is the sort of result that seems impossible until it is obvious.

**Read:** Vitter, *Random Sampling with a Reservoir* (1985). Knuth vol. 2, algorithm R.

**Related:** streaming algorithms, weighted reservoir sampling, random program generation (picking a
random node from a tree in one pass).

---

## Topological sort

**Problem:** order a set of items so that every dependency precedes its dependents — and report a
cycle if no such order exists.

**Shape:**

```
depth-first search over the dependency graph
mark each node: unvisited, in-progress, or done
on finishing a node, push it to the front of an output list
encountering an in-progress node means a cycle — report it, do not loop forever
```

The three-state marking is the part people get wrong; two states cannot distinguish a cycle from a
diamond.

**Read:** any algorithms text. Kahn's algorithm is the alternative formulation using in-degrees,
which some find clearer and which yields the cycle set naturally.

**Related:** cycle detection, DAG, Kahn's algorithm, dependency resolution, module initialisation
order.

---

## Union-find (disjoint set)

**Problem:** maintain a partition of items into equivalence classes, supporting "are these two in
the same class" and "merge these two classes", both very fast.

**Shape:**

```
each item points to a parent; a root points to itself
find(x): follow parents to the root
    path compression: on the way back, repoint every node directly at the root
union(a, b): find both roots, point one at the other
    union by rank: attach the shorter tree under the taller
```

With both optimisations the amortised cost is inverse-Ackermann — effectively constant.

**Why it matters here:** it is the data structure underneath **type unification**. When a type
inference engine says "these two type variables must be equal", that is a union. Recognising this
is what makes Hindley–Milner stop looking like magic.

**Read:** Tarjan's analysis is the classic. For the type-inference connection, any presentation of
Algorithm W.

**Related:** Hindley–Milner, unification, equivalence classes, Kruskal's algorithm, path
compression.

---

## Worklist algorithm

**Problem:** fixed-point iteration recomputes every fact on every pass, including the vast majority
that cannot possibly have changed.

**Shape:**

```
maintain a worklist of items whose facts might have changed; start with all of them
while the worklist is non-empty:
    take an item, recompute its fact
    if it changed, add everything that depends on it to the worklist
```

Same fixed point, far less work. It requires knowing the dependency edges, which is the price.

**Read:** Dragon Book §9.6. The pattern appears far outside compilers — build systems, spreadsheet
recalculation, and reactive UI frameworks are all worklist algorithms wearing different clothes.

**Related:** fixed-point iteration, dependency graph, incremental computation, demand-driven
analysis.
