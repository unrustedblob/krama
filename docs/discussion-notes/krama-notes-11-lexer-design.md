# Krama — Lexer first: design of the first iteration

**Date:** 2026-09-21
**Status:** Discussion record. **Not normative.** Nothing here is recorded in `DECISIONS.md` yet —
§9 lists what is queued.

> Nothing here is binding. Where this and `DECISIONS.md` / `STYLE.md` / `krama-spec-v1.md`
> disagree, those win.

---

## 1. How to read this

- **§2** — why the AST is not next, and the lexer is.
- **§3** — the token buffer: one allocation, and the bound that justifies it.
- **§4** — character classes versus token kinds; how a token ends.
- **§5** — errors: where they are caught, stop-at-first, the message format.
- **§6** — testing: fixtures, the path argument, the three deferred triggers this branch fires.
- **§7** — branch naming.
- **§8** — reconciliation with `krama-spec-v1.md` §3. **Read before starting the branch.**
- **§9** — queued records and open items.
- **§10** — references, with links. **§11** — conclusions. Jump targets.

---

## 2. Why the lexer, not the AST

The previous *Next up* was arena init → AST nodes → diagnostic sink → lexer. Reading chibicc's
`struct Node` and harec's `ast.h` side by side produced no decision: `next` versus `lhs`/`rhs`, one
node struct versus many — the structure of both is shaped by what the parser needs to hold, and the
parser does not exist yet.

The test for "what is next" is: which stage has every input either built or decided? Only the lexer
passes. The source buffer exists (`main` reads it); slices (D-004) and spans (D-005) are decided.
The AST's input — the token — does not exist yet. Designing the AST first is designing against an
unknown.

The arena is no longer tied to the next step. It was paired with AST nodes because block size
depends on node size. The lexer does not use it (§3).

---

## 3. The token buffer

### 3.1 Shape

The lexer owns **one direct allocation**: an array of tokens sized once from the file size, freed
once. This is STYLE.md §9.1 (a phase owns at most one direct allocation) and §9.3 (where a provable
bound exists, allocate the bound once and do not grow).

Producing the whole array up front is the batch model. It was chosen implicitly by this design,
not deferred.

### 3.2 The bound — and the first guess that failed

First guess: `n / 2 + 1`, assuming every token is followed by a separator byte. `x+1` breaks it:
3 bytes, 3 tokens, bound of 2. The third append overflows.

Correct bound: **`n + 1`**. The argument is not the example — `x+1` shows `n` is *reachable*, not
that it is a *ceiling*. The ceiling comes from two properties together:

1. every token covers **at least one byte** of source;
2. **no two tokens cover the same byte**.

So `n` bytes hold at most `n` tokens. EOF covers zero bytes — the `+ 1`. Budget it even before
deciding whether an EOF token exists: one slot too generous is harmless, one too tight is an
overflow.

### 3.3 What §9.3 then requires

- The invariant above is **documented at the buffer**.
- **Every append asserts** against the bound.
- The invariant is what a future change breaks silently. The likeliest culprit: a zero-width error
  token (e.g. an unterminated literal reported at EOF) would be a second zero-width exception and
  would need another slot.
- `count * sizeof(struct Token)` gets an overflow guard (CERT INT30-C, D-028's subtractive style).

### 3.4 Where the free lives — open

On the first branch there is no parser; freeing after the dump is fine. Once a parser exists, "end
of the phase" cannot mean lexer exit — the parser reads tokens after the lexer returns. Do not write
the free anywhere that bakes in an answer.

---

## 4. How a token ends

### 4.1 Two tables, not one

- **Character classes** — what the scanner *reads*: letter, digit, operator character, punctuation
  character, whitespace, quote.
- **Token kinds** — what the scanner *emits*.

They overlap but are different lists. Whitespace has a class and **no** kind. The mismatches below
all come from treating one as the other.

### 4.2 A separator is not a character

A token ends wherever the next character **cannot continue it**. `x` ends at `+` because `+` cannot
be part of an identifier — not because `+` "separates". Hence the four whitespace-equivalent
fixtures (§6.2).

### 4.3 The first character picks the rule

```
look at the current character
  letter or _   -> identifier rule: consume while letter, digit, or _
  digit         -> number rule:     consume digits, then whatever a number may contain
  '             -> char rule:       consume until the closing '
  whitespace    -> skip
  ...
```

The rule alone decides where the token ends. The "context" that seemed to be needed
(`in_char_literal`, …) is not a flag on the token: it is *which branch of the scanner is running*.
That is why `1.5` is not a problem — the `.` is met inside the number rule, which can accept it. A
future `5u32` suffix would be the number rule accepting more, not a new mode.

`'` does end the preceding token — the issue was what it *starts*: inside a char literal, bytes that
would otherwise end a token do not.

### 4.4 Maximal munch

At each position, take the longest token that could start there; fall back only when the longer
match fails. It decides `/` versus `//`, and later `=` versus `==` if such operators arrive. **Spec
§3.3 requires it** (§8).

### 4.5 First-draft kinds

Identifier, operator, punctuation, integer / float / char literal, and a kind for **a byte that
matched no rule**, covering exactly that byte (keeps §3.2's invariant). Naming per STYLE.md §2.1:
`TOK_` prefix, literal suffix last (`TOK_INT_LIT`, `TOK_FLOAT_LIT`, `TOK_CHAR_LIT`). Not `DBL`:
`f32` is IEEE binary32 — a kind named after `double` misstates the type. See §8 for what the spec
adds (`BUILTIN`, keywords) and how it groups operators.

---

## 5. Errors

### 5.1 Identifier validity is the lexer's job

Treating unknown bytes as identifier characters was the first idea, on the assumption the parser
would catch them. Trace `let x$y: i32 = 1;`: the lexer emits `x$y` as `TOK_IDENT`, and the parser
only asks whether an identifier makes sense **in that position**, never whether it **is** one. The
program parses. `$` might reach `cc` — a compiler extension there — far from the source, with the
wrong error.

**Each phase rejects what it is the first to be able to see.** A stray byte is visible only at
character level.

With the first-character rule, `x$y` is *not* a bad identifier: the identifier rule ends `x` cleanly
at `$`, and `$` then matches no rule. The message describes an unexpected character, not an invalid
identifier.

### 5.2 Stop at the first error — for now

The lexer stops at the first lexical error. Continuing is cheap *in the lexer* (skip the byte, carry
on) — it is the parser where continuing is hard — so this is a decision, not a necessity.

The choice is coupled:

| | Stop at first | Record and continue |
|---|---|---|
| Sink | A function: print, exit non-zero | A data structure that collects |
| Unknown-byte kind | Barely earns its place — at most a carrier for span and slice | Keeps the stream intact past the bad byte |

**Revisit when:** the first fixture that needs two lexical errors reported. It announces itself —
that fixture fails under stop-at-first. The hard step is one → many, not two → three
(zero-one-infinity rule), so once it exists, any count follows.

Two habits keep the switch cheap:

- Emit the report from **one place** — the sink later replaces one call site.
- Write each invalid fixture with **exactly one** lexical error — its `.expected` is then identical
  under both approaches and survives the switch.

### 5.3 Message format

```
file:line:col: error: LEX001: message
<the whole source line>
<caret line pointing at the column>
```

- **No spaces around the location colons.** Editors (Vim quickfix, Emacs compilation mode, VS Code
  problem matchers) parse `file:line:col:` as written.
- **`error` stays before the code** — some matchers (VS Code's GCC one) require a severity word
  there.
- **Codes are colon-free.** `LEX:001` collides with the location colons. C-like prior art: GCC and
  Clang do not number errors; MSVC does, colon-free (`C2065`). Rust adds `rustc --explain <code>` —
  the long-term ideal, documented per code.
- **Codes help testing.** Invalid fixtures can match on code + location, not wording, so rewording
  a message does not break every fixture.
- **Finding the line:** spans carry line, column and byte offset (spec §3.3), and the source buffer
  is alive. Scan back from the offset to the previous newline and forward to the next — or to the
  end of the buffer.
- **Tabs:** build the caret line by copying tabs from the source line, or the caret lands wrong.
- **Unprintable bytes** (including each half of a UTF-8 `é`): print as an escape, `\xC3`, never raw.
- Wording along the lines of "unexpected character".

### 5.4 The add-an-error procedure

A "steps to add an error" record — code, message, documentation entry, fixture, `.expected` line.
**Written from `LEX001` as it is actually done, then tested by following it exactly for the second
error.** Gaps and noise show up in that second pass. It later extends to other phases as a common
core (registry, docs format, fixture pattern) plus a section per phase — recovery is trivial in the
lexer, hard in the parser, and cascading in the type checker.

---

## 6. Testing

### 6.1 Many fixtures, not one program

One updated `test.krm` cannot express either of the things this branch must prove:

- **Whitespace equivalence** is a relationship *between* files — four inputs, identical kinds and
  text, differing only in spans.
- **Isolating failures** — one program exercising every rule says *that* it failed, not *which rule*.

### 6.2 Fixture checklist (fixtures are written by the author, not Claude)

- The four whitespace-equivalent programs: spaced, collapsed (`let x:i32=10;`), padded, one token
  per line.
- Every rule once, each ending against a different kind of neighbour.
- Boundary cases: `1.5`; a char literal whose content would otherwise end a token (a quoted `;` or
  space). See §8 for spec-driven additions.
- The unknown byte, alone and inside an identifier (`x$y`).
- An **empty file** — nothing but EOF. The cheapest test of the `+ 1`.
- A file whose **last token runs into end of file** with no trailing newline.
- An **unterminated char literal** — the zero-width-error case; likely what forces the sink design.

### 6.3 `main` takes a path

Several fixtures need `main` to take **one path argument**. Check `argc` before touching `argv[1]`;
missing path → usage line, non-zero exit. PROJECT.md's hardcoded-path row said "due with arena
initialization" — that trigger is stale; the row retargets to this branch.

### 6.4 This branch fires three deferred triggers

None blocks the first rule; all three gate the merge.

- **STYLE.md §13 testing conventions** — "once the lexer produces output worth testing". The
  `.expected` token-dump format is **deferred** by choice: a crude first version is acceptable and
  independent of everything else.
- **D-025 `CHECK`** — second consumer arrives with the lexer unit tests; answered while writing them.
- **The diagnostic sink** — moved into this stage (§9); stop-at-first (§5.2) is what gets settled.

---

## 7. Branch naming

A milestone-plus-counter scheme (`m1/lex:1/…`) was considered and dropped:

- `:` is **illegal in a Git ref name** — the branch would be rejected.
- D-030 fixes `m<N>/<stage>`; changing it needs a superseding entry.
- On merit: `main`'s merge commits already record order, and a counter goes stale when a stage is
  reordered or dropped. A stage name saying what it *adds* carries the phase: `m1/lex-slices`, then
  e.g. `m1/lex-kinds`. Not `read` — reading already exists in `main`, and names are permanent.

---

## 8. Reconciliation with `krama-spec-v1.md` §3

The spec **is** in the project files. During the session Claude twice said it was not, and the
design above was worked out without checking it. This section reconciles. Where they differ, the
spec wins.

**Characters the draft missed:**

- **`#` opens a comment** (§3.2, D-012) — discarded to end of line. Absent from the draft's table,
  it would be an unknown byte and every commented file would fail with `LEX001`.
- **`@` starts a `BUILTIN`** (§3.1, §3.3): `@` followed by an identifier is **one** token, name
  resolved in type checking. The spec's own reference example (§1.2) uses `@print`.
- **`\r` is whitespace** (§3.2).

**Characters the draft had that milestone 1 does not:** `<` `>` `&` `|` `^` `!` `[` `]` `"`, and
`.` anywhere except inside a `FLOAT_LIT`. Strings are absent from milestone 1 (§2.2). In m1 these
are all unknown bytes.

**Rules the spec already fixes:**

- **Maximal munch is required**: `//` before `/` (§3.3). With "all rules in the first branch", `//`
  is in the first branch.
- **Keywords by post-filtering `IDENT`** (§3.3). `let` as `TOK_IDENT` is an interim state; the
  filter is due before the lexer stage completes.
- **`FLOAT_LIT` needs digits on both sides**: `1.` and `.5` are rejected (§3.3). New fixtures.
- **`CHAR_LIT`** is one printable ASCII char or an escape from `\n \t \\ \' \0` (§3.1). A byte above
  127 in a char literal is a **lexical error** (§2.1). New fixtures.
- **`PUNCT` groups operators and punctuation together** (§3.1). The draft's `TOK_OP` / `TOK_PUNCT`
  split is finer than the spec's category — allowed, but a deliberate choice. STYLE.md's examples
  (`TOK_LPAREN`) point toward one kind per symbol eventually.
- **Testing** (§11.1): a shell driver over `.krm` + `.expected` pairs; trap tests check exit code
  and stderr. Relevant when `.expected` is settled.
- **Appendix B** is the spec's reading list; §10 below extends it.

---

## 9. Queued records, owed, open

### Queued records

1. **STYLE.md §13** — the sink trigger moves from "before the lexer" to "during the lexer stage,
   before it merges". A real alternative was weighed → DECISIONS entry + STYLE.md version bump.
2. **PROJECT.md** — *Next up* reordered, lexer first; the hardcoded-path row retargeted to this
   branch; the Log's `m1/arena-init` vs `m1/ast-nodes` question is closed by this reordering.
3. **DECISIONS.md** — stop at the first lexical error. *Revisit when:* the first fixture that needs
   two lexical errors reported.
4. **The add-an-error procedure** — written after `LEX001` exists, not before.

### Owed

- Fold `all` into `check` — necessary the moment `main.c` calls the lexer.

### Open

- Whether an EOF token exists (the `+ 1` is budgeted either way).
- Where the token buffer's `free` lives once a parser exists.
- The `.expected` token-dump format (deferred, §6.4).
- **Next half of the discussion:** the high-level commit plan for the branch — path argument in
  `main`, `TokenKind`, the token array, and onward.

---

## 10. References

Useful **now** is marked; the rest is for later milestones.

| Source | Link | Useful for, now |
|---|---|---|
| **Crafting Interpreters** (Robert Nystrom) | [craftinginterpreters.com](https://craftinginterpreters.com/) | **Now.** Ch. 4 *Scanning* (Java, but the rule-per-first-character shape) and ch. 16 *Scanning on Demand* (the same scanner in C). Caution: clox compiles in a single pass with no AST — the design D-002 rejected. Use it for lexing idioms, not structure. Ch. 17 later (Pratt, D-009); ch. 20 later (interning, D-021). |
| **chibicc** (Rui Ueyama) | [github.com/rui314/chibicc](https://github.com/rui314/chibicc) | **Now.** `tokenize.c`. Translate while reading: typedefs and file-scope globals throughout, both forbidden by STYLE.md §3 and §8.2. `struct Node` later, with the parser. |
| **Writing a C Compiler** (Nora Sandler) and its test suite | [norasandler.com/book](https://norasandler.com/book/) · [github.com/nlsandler/writing-a-c-compiler-tests](https://github.com/nlsandler/writing-a-c-compiler-tests) | **Now.** Corpus layout (`valid/`, `invalid_*/`) and the runner's per-stage mode (`--stage lex`). It checks accept/reject by exit status only — not tokens, not messages — so it informs layout and driver contract, not the `.expected` format. |
| **harec** (the Hare compiler, in C) | [git.sr.ht/~sircmpwn/harec](https://git.sr.ht/~sircmpwn/harec) | Later. A C-like language with no implicit conversions (D-007) and tagged unions (M3). `ast.h` once the parser needs it. Already cited by STYLE.md for comments. |
| **C Interfaces and Implementations** (David Hanson) | [github.com/drh/cii](https://github.com/drh/cii) | Later. *Arena* chapter (D-003, D-026 when mark/release reopens); *Atom* chapter is string interning (D-021). |
| **Chris Wellons** on arenas | [nullprogram.com — Arena allocator tips and tricks](https://nullprogram.com/blog/2023/09/27/) | Later — D-026. |
| **Check** (C unit testing, fork per test) | [libcheck.github.io/check](https://libcheck.github.io/check/) | Later. Prior art for the process-per-case driver — the only way to test that `FATAL_PATH_ABORT` aborts (D-029). |
| **Csmith** and Yang et al., *Finding and Understanding Bugs in C Compilers* (PLDI 2011) | [github.com/csmith-project/csmith](https://github.com/csmith-project/csmith) | Later, but read early — spec §11.2's differential testing is milestone 1's hardest design problem. |
| **cproc** + **QBE** (Michael Forney) | [git.sr.ht/~mcf/cproc](https://git.sr.ht/~mcf/cproc) · [c9x.me/compile](https://c9x.me/compile/) | Later. The escalation path past emitting C; also the cleanest small C in this list. |
| **Clang diagnostics** | [Clang Internals Manual — diagnostics](https://clang.llvm.org/docs/InternalsManual.html) | When the sink is designed — the collected-sink end of the spectrum. |
| **GCC diagnostic format** | [GCC — Diagnostic Message Formatting Options](https://gcc.gnu.org/onlinedocs/gcc/Diagnostic-Message-Formatting-Options.html) | **Now.** The `file:line:col: error:` shape adopted in §5.3. |
| **Rust error index** | [doc.rust-lang.org/error_codes](https://doc.rust-lang.org/error_codes/error-index.html) | The long-term ideal for documented error codes (`rustc --explain`). |
| **Git ref-name rules** | [git-scm.com/docs/git-check-ref-format](https://git-scm.com/docs/git-check-ref-format) | Why `:` is illegal in a branch name (§7). |
| Terms to search | [Maximal munch](https://en.wikipedia.org/wiki/Maximal_munch) · [Zero one infinity rule](https://en.wikipedia.org/wiki/Zero_one_infinity_rule) | **Now** (§4.4) · for §5.2's trigger. |

Links were written from memory and not all were opened this session — verify before relying on
them.

---

## 11. Conclusions — what we are going ahead with

1. **The lexer is next**, not the AST. The arena is no longer tied to the next step.
2. **Branch:** `m<N>/<stage>` per D-030, no counter — e.g. `m1/lex-slices`. The first branch takes
   **all the rules** discussed; the rebase splits commits per rule.
3. **`main` takes one path argument**, validated via `argc`, usage + non-zero exit if missing.
4. **Token buffer:** one `malloc`, one `free`, sized for **`n + 1`** tokens. The invariant (≥ 1 byte
   per token, no shared bytes, EOF the only zero-width token) is documented at the buffer, asserted
   on every append; the size multiplication is overflow-guarded. The `free`'s final home stays open.
5. **Two tables:** character classes (read) versus token kinds (emitted). Whitespace and comments
   have classes, no kinds.
6. **The first character picks the rule; the rule decides the end.** Maximal munch for `//`.
7. **Kinds:** `TOK_`-prefixed, literal suffix last, float named for `f32`; plus a kind for a byte
   matching no rule. Keywords stay `TOK_IDENT` until post-filtering lands, before the lexer stage
   completes. Reconcile with spec §3 first (§8).
8. **Identifier validity is the lexer's.** `x$y` → `x`, then an unexpected `$`.
9. **Stop at the first lexical error**, reported from one place, exit non-zero. Revisit at the first
   fixture needing two errors. One error per invalid fixture.
10. **Format:** `file:line:col: error: LEX001: message`, then the full source line and a caret line;
    tabs copied, unprintable bytes escaped.
11. **Error codes** colon-free per phase (`LEX001`), documented; the add-an-error procedure is
    written from `LEX001`, tested on the second.
12. **Many small fixtures**, one concern each (§6.2 plus §8's additions).
13. **Deferred:** `.expected` format (crude first version acceptable); `CHECK` (answered while
    writing the unit tests); EOF token existence.
14. **Next session half:** the branch's high-level commit plan.
