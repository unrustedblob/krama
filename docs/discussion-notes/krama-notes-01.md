# Krama Transpiler — Design Notes, Session 01

**Date:** 2026-08-11
**Status:** Pre-implementation. No code written.
**Scope of session:** Project framing, milestone 1 scope, spec implications of the initial example.

---

## 1. Project aims (as stated)

1. **C refresher.** Transpiler chosen as a project large enough to exercise real C: memory ownership, tagged unions, string handling, error propagation.
2. **Testing, measurement, tooling.** Make, gdb, perf, sanitizers.
3. **Spec formalization, verification, documentation.**

**Working agreements:**
- No code generation for the transpiler itself. Concept explanations may use snippets or point to existing sources.
- User writes all code; review happens via repomix export pasted into chat.
- Each session summarized into notes (this document).

---

## 2. The seed example

```plain
fn main(): int {
    let x: int = 0;
    let y: int = 0;
    set x = 5;
    set y = x + y;
    print(x,y);
    return 0;
}
```

Target C:

```c
int main() {
    int x = 0;
    int y = 0;
    x = 5;
    y = x + y;
    printf("%d, %d\n", x, y);
    return 0;
}
```

*(Note: the semicolon after `printf(...)` was missing in the original example. Flagged because
hand-written expected-output files are exactly where this class of bug hides.)*

---

## 3. Findings from the example

### 3.1 `print` is a type-directed builtin — raised by Claude

The format string `"%d, %d\n"` does not appear in the source. It is synthesized from the *static
types* of the arguments. funC as specified has no variadic function declarations and no generics,
therefore `print` cannot be an ordinary function — it is a builtin whose code generation requires
argument type information.

**Consequence:** milestone 1 requires a type checker (or at minimum a typed AST pass), unless
`print` is deliberately restricted.

**Escape hatch identified:** define `print` as int-only for milestone 1 and hardcode `%d` per
argument. This is acceptable but must be recorded as an explicit spec restriction, not left as an
implementation accident.

**Unresolved:** which path to take. See open questions.

### 3.2 `let` / `set` separation is a deliberate design win — raised by Claude, from user's design

Separating declaration (`let`) from assignment (`set`) yields two properties:

- The parser can distinguish declaration from assignment **without a symbol table**.
- Assignment is a **statement, not an expression**, which structurally eliminates `if (x = 5)`
  and chained `a = b = c`.

Recommendation: state these as intentional language properties in the spec rather than leaving them
as incidental syntax.

**Naming risk noted:** `let` conventionally implies immutability (Rust, ML). funC's `let` is
mutable. Either accept the divergence explicitly or plan for `let` / `let mut`.

---

## 4. Architectural fork: AST vs. syntax-directed translation

**The option:** funC and C are near-isomorphic for this subset. Direct emission of C text from the
parser, with no intermediate tree, would work and would be less code.

**Claude's position:** take the AST. Three reasons, all tied to the stated aims:
- `print` needs type information, which means a pass over a structure.
- Aim 3 (spec conformance) requires an artifact to check the implementation against; a tree is that
  artifact.
- A tree-walking interpreter over the same AST is the highest-value verification tool available
  here (see §6).

**Acknowledged cost:** this is paying for future capability, not present need. Syntax-directed
translation is the faster path to a working milestone 1 in isolation.

**Resolution:** pending user decision, but Claude recommends AST.

---

## 5. C implementation decisions recommended before writing code

All three raised by Claude.

1. **Source spans on every token from day one** — line, column, byte offset. Retrofitting source
   locations is the most common regret in hand-written compilers. Needed for diagnostics *and* for
   `#line` directives (§7).

2. **Arena allocator for the AST.** A transpiler is a batch process: allocate, never free, single
   teardown at exit. ~40 lines. Teaches alignment and pointer arithmetic (aim 1) and removes a whole
   class of lifetime bug. Reference: Chris Wellons' arena allocator writeups.

3. **String slices into a single source buffer**, not `strdup` per identifier. Represent identifiers
   as `{char *ptr; size_t len;}` pointing into the mapped/read source. Fewer allocations, no
   ownership questions, and forces care around non-NUL-terminated strings.

---

## 6. Aim 3 rescoped: what "verification" means here

**Point of contention — Claude pushed back on the framing.**

CompCert-style mechanized proof is a multi-year research program and would consume the project.
Claude proposed the achievable substitute, ordered by value per hour:

1. **Small-step operational semantics for funC, written on paper.** At six constructs this is
   roughly one page. This is the artifact that makes the spec real rather than prose — and it is
   also the prerequisite for any later mechanized proof, so nothing is wasted.

2. **Tree-walking interpreter over the AST, used for differential testing.** Interpret funC
   directly; separately compile the generated C and run it; compare stdout. This is the real
   correctness oracle and it scales as the language grows.

3. **Round-trip property test.** Generate random valid ASTs → pretty-print to funC → re-parse →
   assert structural equality. Cheaply catches parser/printer disagreement.

**Status:** proposed, not yet accepted by user.

---

## 7. Testing and tooling decisions

- **End-to-end over golden-file as the primary gate.** Compile generated C with `cc`, run it, diff
  stdout against expected. Golden C-text comparison breaks on whitespace and formatting choices that
  carry no semantic weight. Keep a small number of golden tests for codegen *shape* only.
- **Zero test dependencies.** A `tests/` directory of `.func` + `.expected` pairs plus a shell
  driver is sufficient.
- **`-fsanitize=address,undefined` as a Makefile target, early.**
- **Emit `#line` directives** into the generated C, mapping back to funC source. Result: gdb on the
  compiled binary displays funC. High payoff for aim 2, near-zero cost.
- **Do not pretty-print generated C.** Emit it ugly, pipe through `clang-format`.

### Point of contention: `perf`

**Claude's position:** premature. On a 10-line input, `perf` measures process startup, not the
lexer. It becomes meaningful only after a stress generator exists that can emit ~100k-line funC
files. Until then the measurement that matters is coverage and correctness.

**Proposed resolution:** defer `perf` until the stress generator exists; treat writing the generator
as the enabling task rather than dropping the aim.

### Point of contention: Claude Code boundary

User proposed offloading infrastructure to Claude Code "if it can do it better."

**Claude's position:** the Makefile, test runner, and gdb configuration *are* aim 2. Delegating them
donates away the stated learning objective. Suggested boundary: "anything I'd learn from, I write" —
which leaves only CI configuration as a plausible handoff. Repomix-into-chat for review remains the
working mode.

---

## 8. Open questions carried to session 02

1. `print`: type-directed format synthesis (requires type checker in milestone 1) or int-only
   hardcoded `%d`?
2. Is initialization on `let` mandatory? If yes, uninitialized-variable UB is eliminated by
   construction — a spec property worth claiming.
3. Milestone 1 type set: `int` only, or `int` + `bool`? Adding `bool` is what makes the type checker
   non-trivial.
4. Return type syntax for no-value functions: `: void`, or omit the annotation?
5. C standard target for emitted code: C99 or C11?
6. Expression operators in milestone 1 scope. Recursive descent with one function per precedence
   level, or precedence climbing?
7. File extension and any reserved-word list.
8. AST vs. syntax-directed — decision pending (§4).

---

## 9. Reference material (no generated code)

- **chibicc** (Rui Ueyama) — a C compiler written in C, built commit by commit. Closest available
  model for this project's structure and growth path.
- **Crafting Interpreters**, part III (clox) — scanner and parser idioms in C.
- **lacc**, **tcc** — complete small C compilers, for reading.
- Chris Wellons — arena allocator technique in C.
- The Dragon Book — out of scope at this stage.

---

## 10. Next actions

- [ ] User answers open questions in §8.
- [ ] Draft EBNF grammar for milestone 1 funC.
- [ ] Draft `print` contract precisely (separator, trailing newline, argument type restrictions).
- [ ] Decide AST vs. syntax-directed.
- [ ] Draft small-step operational semantics for the milestone 1 subset.
