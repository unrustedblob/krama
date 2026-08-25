# Data structures

Structures and the layout tradeoffs that make them worth naming. Every shape sketch is pseudocode.

## Contents

- Bitset
- Free list
- Growable array (amortized doubling)
- Hash table: chaining vs. open addressing
- Intern pool
- Robin Hood hashing
- Scope stack
- Struct-of-arrays (SoA)
- Tombstones
- Trie

---

## Bitset

**Problem:** set membership over a small dense universe — "which of these 64 flags are set", "which
registers are live". One `bool` per element wastes a byte on a bit.

**Shape:**

```
an array of machine words; element i lives at bit (i mod word bits) of word (i div word bits)
test:  shift a 1 into position, AND
set:   shift a 1 into position, OR
clear: shift a 1 into position, AND with the complement
union / intersection / difference of two sets: OR / AND / ANDNOT, word at a time
```

The set operations being single instructions per 64 elements is the reason dataflow analyses
represent their fact sets this way.

**Read:** any dataflow implementation. `std::bitset`, Rust's `bitvec`, and the Linux kernel's
`bitmap` API are readable references.

**Related:** bit flags, population count, dataflow analysis, dense sets.

---

## Free list

**Problem:** many same-sized objects with individual lifetimes. A general allocator's per-object
bookkeeping is pure overhead when every object is the same size.

**Shape:**

```
allocate a block of N slots
thread the free slots into a singly linked list — the "next" pointer lives *inside* the free slot,
    since a free slot's contents are meaningless
allocate: pop the head
free: push onto the head
```

Storing the link inside the free object costs zero extra memory, which is the trick worth
remembering.

**Read:** the classic slab-allocator design; the Linux kernel's SLAB paper by Bonwick is the
standard reference.

**Cost:** only works for uniform sizes, and gives back nothing to the system until the whole block
is released.

**Related:** pool allocator, slab allocator, object pool, arena (the sibling for non-uniform,
never-freed objects).

---

## Growable array (amortized doubling)

**Problem:** an array that must grow, without knowing the final size, without an O(n) copy on every
append.

**Shape:**

```
hold a pointer, a length, and a capacity
append: if length equals capacity, reallocate to capacity times a growth factor; then write
```

**The factor is the entire design.** Growing by a constant amount makes appends O(n) amortised.
Growing by a *multiple* makes them O(1) amortised, because the copies get exponentially rarer.
Common choices are 2 and 1.5; 1.5 can reuse previously freed blocks under some allocators, which is
why several standard libraries chose it over 2.

**Read:** any data structures text for the amortised analysis. Folly's `FBVector` documentation
argues the 1.5 case in detail and is the best write-up of that specific tradeoff.

**Related:** amortized analysis, `realloc`, capacity vs. length, small-buffer optimisation.

---

## Hash table: chaining vs. open addressing

**Problem:** two keys hash to the same slot. What happens next is the main design axis of hash
tables.

**Shape:**

```
chaining:         each slot holds a linked list of entries; collisions append to the list
open addressing:  each slot holds one entry; on collision, probe onward to another slot
                  (linear probing = try the next slot, and so on)
```

Chaining is simpler and degrades gracefully when full. Open addressing keeps everything in one flat
array, so probing walks contiguous memory and stays in cache — which in practice makes it
substantially faster despite the more complicated deletion story.

Open addressing requires a **load factor** limit — typically resize at around 70% full — because
probe lengths grow sharply as the table fills.

**Read:** *Crafting Interpreters* ch. 20 builds an open-addressed table and explains the choice.
Any algorithms text covers chaining first.

**Related:** tombstones, load factor, linear vs. quadratic probing, Robin Hood hashing, cache
locality.

---

## Intern pool

**Problem:** the storage side of string interning — you need a set of unique strings where lookup
returns the existing copy if present.

**Shape:**

```
a hash set keyed by the string's contents (hash and compare over the bytes)
intern(s): look up; if found return the stored string; otherwise copy s into the pool and return
           the copy
```

If the source text is held for the whole run, the pool can store slices into it rather than copies,
and the "copy" step disappears entirely.

**Read:** every compiler has one. Lisp's symbol table is the ancestor.

**Related:** string interning, flyweight, symbol table, arena-backed storage.

---

## Robin Hood hashing

**Problem:** in an open-addressed table, some keys land in their ideal slot and others probe far.
The variance is what hurts — the worst-case lookup, not the average.

**Shape:**

```
track each entry's probe distance: how far it sits from its ideal slot
when inserting, if the entry being placed has probed further than the entry currently occupying
    the slot, swap them and continue placing the displaced one
```

Rich entries (short probes) give way to poor ones (long probes) — hence the name. The effect is to
equalise probe distances, cutting the worst case sharply.

**Read:** Pedro Celis' 1986 thesis introduced it. Emmanuel Goossaert's blog series is the most
readable modern explanation. Rust's standard `HashMap` used this before moving to SwissTable.

**Cost:** more complex insertion and deletion than plain linear probing. Worth it for tables under
sustained heavy load; overkill for a compiler's symbol table.

**Related:** open addressing, backward-shift deletion, SwissTable, hopscotch hashing.

---

## Scope stack

**Problem:** nested lexical scopes with shadowing, entered and left constantly, needing O(1) entry
and exit.

**Shape:**

```
a single flat array of (name, declaration) entries
a separate stack of watermarks — the array height at each scope entry

enter scope: push the current height
declare:     append to the array
lookup:      scan backwards from the top; the first match is the innermost binding
leave scope: truncate the array back to the watermark
```

Backward scanning gives shadowing for free — the innermost declaration is simply the one found
first. The linear scan sounds bad and is not, because scopes in real code are small and the array is
contiguous.

**Read:** *Crafting Interpreters* ch. 22. chibicc's scope handling is a compact readable variant.

**Related:** symbol table, lexical scoping, shadowing, de Bruijn indices.

---

## Struct-of-arrays (SoA)

**Problem:** you have a million uniform nodes and a pass that touches only one field of each. With
each node stored as a struct, every cache line loaded contains mostly fields the pass does not want.

**Shape:**

```
array-of-structs:  one array, each element a full node with all its fields
struct-of-arrays:  one array per field, all the same length, index i naming the same logical node
                   across all of them
```

A pass over one field then walks one dense contiguous array with no wasted bandwidth.

**Read:** Andrew Kelley's talk on data-oriented design in the Zig compiler is the most accessible
treatment, with measured numbers. Mike Acton's *Data-Oriented Design and C++* is the polemical
origin.

**Cost:** individual nodes are no longer inspectable as a unit — in a debugger you must read several
arrays at the same index. Passes that touch *all* fields of *one* node get worse, not better. It is
a win at scale and pure friction below it.

**Related:** array-of-structs, data-oriented design, cache locality, index-based node references,
hot/cold splitting.

---

## Tombstones

**Problem:** deleting an entry from an open-addressed table by clearing its slot breaks every probe
chain that passed through it. Entries after the hole become unfindable.

**Shape:**

```
mark the deleted slot with a distinct "was occupied, now empty" sentinel
lookups treat a tombstone as occupied and keep probing past it
insertions may reuse a tombstone slot
resizing discards tombstones, since the chains are rebuilt anyway
```

**Cost:** tombstones accumulate. A table churned heavily without resizing fills with them and probe
lengths grow even though the table is not full. Counting tombstones toward the load factor is the
usual fix.

**Read:** *Crafting Interpreters* ch. 20 implements tombstones and discusses exactly this
accumulation problem.

**Related:** open addressing, load factor, backward-shift deletion (the alternative — no tombstones,
harder code).

---

## Trie

**Problem:** lookup keyed by prefix, where you want to know as you consume characters whether any
key can still match.

**Shape:**

```
a tree where each edge is labelled with one character
a path from the root spells a prefix
nodes carry a flag marking whether the path so far is a complete key
```

**Why it belongs here:** maximal-munch tokenization is exactly a trie walk over the operator set.
Most hand-written scanners implement the trie implicitly, as nested switches on successive
characters — recognising that they are the same shape is what lets you decide whether to write it as
a table instead.

**Read:** *Crafting Interpreters* ch. 16 builds keyword recognition as an explicit trie and says so.
Sedgewick covers the general structure.

**Cost:** one node per character per distinct prefix is memory-hungry. Radix tries (path-compressed)
address this.

**Related:** radix tree, Patricia trie, maximal munch, DFA, Aho–Corasick (multi-pattern matching).
