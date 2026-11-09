# Krama — Aside: designing `/ground-up`
 
**Date:** 2026-09-29 to 2026-09-30
**Status:** Discussion record. **Not normative.** An aside: it records how the learning process was
designed, not the transpiler. It sits outside the `krama-notes` numbering, and entries it produced
carry the Session tag `A-01`.
 
> Nothing here is binding. Where this note and `CLAUDE.md` / `DECISIONS.md` / `STYLE.md` /
> `COMMITS.md` disagree, those win. The rule itself is CLAUDE.md §8; the rationale is D-042 and
> D-043.
 
> **Relabelled 2026-11-08.** This note first drafted its two decisions as D-039 (`/ground-up`) and
> D-040 (CLAUDE.md tracked). Those IDs went to the harness decisions on `m1/cli-args` before
> `m1/ground-up` landed, so the pair is now owed and numbered when it lands (`krama-notes-22` §9).

> **Owed decisions, landed 2026-11-09** on the `ground-up` branch:
>
> | Owed | Decision | ID |
> |---|---|---|
> | DR-OWED-A01-01 | `/ground-up`: a parallel learning track | **D-042** |
> | DR-OWED-A01-02 | `CLAUDE.md` is tracked in the repository | **D-043** |
>
> The design landed revised from this note: the track belongs to no milestone, each packet runs on
> its own branch `learn/<topic>/<packet>`, and everything lives under `learn/` with the ledger at
> `learn/README.md` (D-042). Where §8 and §12 below say otherwise, D-042 wins.
 
It interrupts `krama-notes-18-case-manifest`. `m1/cli-args` commit 5 is still open, and its first
question is still unanswered (§11).
 
---
 
## 1. How to read this
 
- **§2** — the intent, in the programmer's words.
- **§3** — scoring: two axes, anchor tables, the trigger.
- **§4** — the guide set, the check set, and the gate between them.
- **§5** — the languages, and how they were chosen.
- **§6** — levels and sample problems.
- **§7** — the dry run and what it changed.
- **§8** — where it lives: `learning/`, STYLE.md, commits, branches, the wiki.
- **§9** — documents changed, and the order of work.
- **§10** — the mentor-or-driver question.
- **§11** — open items.
- **§12** — conclusions.
---
 
## 2. Intent
 
`/ground-up` replaces `/babymode`. The build is getting into the weeds, so concepts need to be
solid rather than borrowed. The means: short project-based exercises (around 200 lines) through
which a concept is established, then checked.
 
Principles the programmer set:
 
- **Benevolent dictator.** Claude may *request* a detour; the programmer decides. A decline is
  recorded, not erased (honour code).
- **Deviation-based conceptual absorption.** Sparring in languages other than C, so fitting a
  concept into foreign idioms forces understanding of the concept rather than of its C spelling.
  Assessment questions bound the deviation. Closest named ideas: interleaving and desirable
  difficulties (Bjork).
- **Domains of interest.** Databases, POSIX OS work, compilers, low-level memory, tuned data
  structures, algorithms — kept to the difficulty the concept needs.
- **Free, high-quality reading**, credited later in the README as "what I used to learn", with what
  was gained from each item.
- **A written summary to clear.** The *Road to Krama* blog.
- **Krama means discipline** — the process is meant as a practice, not a patch.
---
 
## 3. Scoring
 
### 3.1 Concept scale — per concept, with evidence stated
 
| Score | Evidence |
|---|---|
| 1–2 | Recognises the term |
| 3–4 | Follows an explanation or a worked example |
| 5–6 | Explains it back correctly, unprompted |
| 7–8 | Applies it to a problem shaped differently from the example |
| 9–10 | Compares it with alternatives and says when each is wrong |
 
A number without evidence cannot be argued with, so the evidence always comes with it.
 
### 3.2 Trigger — below 7, everywhere, loose
 
- **Below 7 on a concept → request `/ground-up`**, in brainstorming, active work, or a question.
- Kept **loose on purpose**: a request costs one sentence of judgement and can be declined. A tight
  trigger (only for concepts the current work depends on) would hide gaps the programmer might have
  wanted.
- Every request states a **proposed scope** — reading only, or a sparring level and language. Not
  every gap needs a code path.
- **Request once**; re-request only on new evidence (a changed score, or a review showing the gap
  costing something).
**Worked example.** Pattern matching scored **6** during the language discussion: `a` and `b` read as
pattern bindings unprompted, but a constructor was taken for a call, and when `AND` / `OR` continue
was blurred. Under the loose trigger that is a request — a reading-only one (constructors, patterns,
destructuring). The programmer's view: it may fire, and a later spell of correct OCaml/Rust use can
close it by evidence (§3.4).
 
### 3.3 Language fluency — a second axis, never a trigger
 
| Score | Evidence |
|---|---|
| 1–2 | Reads it, with a reference open |
| 3–4 | Working code, built by adapting examples; not yet idiomatic |
| 5–6 | Chooses the common idioms without an example to adapt |
| 7–8 | Asks the right questions of each call's contract; reviews mostly find design points, not misuse |
| 9–10 | Explains why the language made a choice, against the others |
 
**Revised mid-session.** The first draft used "with the docs open" and "unaided" as markers — memory,
which neither party can observe and which is not the skill. The programmer's point: reading the
spec for `strtol` or `fgets` is part of the toolkit. The revision scores *which questions a call's
contract raises*; the reference is where they are answered. **Consulting a reference never lowers a
score.**
 
**Targets:** C 8 or above. C++, Zig, OCaml, Go, Rust 5–6 or above.
 
Switching speed is read from review findings tagged *concept* or *language*: when a concept is
sparred in a second language, a falling share of language findings means the language is getting out
of the way. Baselines come from the first reviewed exercise in each language.
 
### 3.4 Ledger statuses
 
`declined` · `queued` · `open` · `paused` · `cleared` · `closed-by-evidence`
 
`closed-by-evidence` closes a concept without a check set, because later work showed it used
correctly. It must cite those entries and carry a re-estimated score, so the closure is checkable
rather than a lapse of attention.
 
---
 
## 4. Guide set, check set, and the gate
 
### 4.1 Two question sets
 
The programmer offered two approaches: questions first as the guide (pretesting effect), or
questions after as a test (retrieval practice). The first risks the assessment measuring whether the
questions' answers were found. Settled on a split:
 
- **Guide set** — three or four questions up front: *the aim, phrased as questions*.
- **Check set** — held back until "done"; different questions, each tagged with the anchor level it
  probes.
**Scoring the check set:** the highest level answered solidly **with every level beneath it also
solid**. A correct 7–8 answer on a shaky 5–6 scores 5–6. Reasoning is shown per question.
 
### 4.2 The gate before the check set — recursive
 
Review first, then iterate — review, fix, review — until:
 
- no *concept* finding is open;
- every *language* finding that is **misuse** is fixed (a wrong construct, a reference taken for a
  copy) — correct usage, not fluent usage; **and**
- **for C only**, the code reviews at fluency **7**.
Requiring each language's full fluency target was rejected: a first OCaml or Rust detour would turn
into a fluency programme, and the concept would wait behind syntax — the "Rust situation" the
programmer wanted to avoid. It would also tie together the two axes that were separated on purpose.
 
**6 versus 7 in C**, shown on a toy function summing one integer per line: the 6 used the right idioms
(`fgets` loop, `strtol` end pointer) but missed `ERANGE`, trailing junk, signed overflow of the sum,
over-long lines, and `ferror` at loop end. The 7 asked each call's contract those questions, and its
review found only design points (one `-1` for five failures; reading and parsing tangled). The
overflow guard is the subtractive form already in the Friction log.
 
### 4.3 Exit
 
Check set at 7 or above, plus the programmer's written summary → Claude states the new score → the
programmer confirms the release. Below 7 the detour stays open. The summary's format is settled at the
first clearance.
 
---
 
## 5. Languages
 
**Final set: C, C++, Zig, OCaml, Go, Rust.** Statically typed, low-level-leaning, differing in
paradigm or memory model. Chosen by the lens each gives, not by syntax comfort.
 
| Language | Lens |
|---|---|
| C | The language of the work. Strict review against STYLE.md. |
| C++ | RAII and OOP — an alternative approach to the same manual-memory problems. |
| Zig | The allocator as an explicit interface; `comptime`; strictness — a gauge of what feels excessive, useful against Krama. Pre-1.0: pin the version first. |
| OCaml | A different way of thinking: algebraic data types and exhaustive matching. Compiler lineage (Rust's first compiler, Flow, Hack, Haxe, Rocq, the WebAssembly reference interpreter). Allows mutation when a problem needs it. |
| Go | The concurrency *model* — CSP (Hoare, 1978): share by communicating. A Unix pipe is almost a channel. Plus small implicit interfaces, and GC as a contrast to the arena. |
| Rust | Ownership — the one memory model otherwise missing — and industry weight. Starts at level 1 with single-owner problems; no ceiling. |
 
**The OCaml / Gleam decision.** The programmer worried about OCaml's syntax. The same `WHERE`-clause
evaluator (`age = 30 AND (id = 1 OR id = 2)`) was shown in OCaml, Gleam and Zig, traced end to end,
then OCaml's real pain points shown separately: `+.` for floats, `let … in` chaining, mandatory
parentheses around a nested `match`. Gleam reads most familiarly but has no mutation and no loops,
so cursor-and-buffer problems become lessons in the language rather than the concept. OCaml kept.
*Familiar isn't better* — the programmer's own framing.
 
**Go** was the doubt: C-like syntax, so what's new? Answered by framing it as the *model*, not a
convenience — and a planned three-way comparison of one pipeline: Go channels, C `fork` + `pipe`,
C pthreads with a mutex-guarded queue. Go stays. Correction on record: Claude said `kramac` "will stay
single-threaded"; the programmer rejected presuming that. D-026 already anticipates multi-file
compilation, the usual first meeting point with concurrency.
 
**Considered, not taken:** Hare (same paradigm as C; weakest assessment), TypeScript (types erased
over a dynamic runtime), Haskell (laziness is a detour), Swift (reference counting — a real
candidate), Ada/SPARK (proof of absence of overflow — a real candidate), Odin (overlaps Zig), Gleam
(above).
 
---
 
## 6. Levels and sample problems
 
| Level | Meaning |
|---|---|
| 0 | Trivial (a guessing game) |
| 1 | One file, data from stdin or hardcoded, no spec to read |
| 2 | Files involved; spec reading starts |
 
OCaml and Rust start at level 1 and level up where applicable.
 
**Level 1**
- **OCaml — RPN calculator.** Tokens as a variant; stack as a list; underflow and division by zero
  as values, not crashes. *Lens:* exhaustive matching; errors as return values.
- **Rust — in-memory KV store.** `SET` / `GET` / `DEL` / `COUNT` from stdin. *Lens:* ownership
  moving into the map, `&str` borrows, `Option` / `Result` — the gentlest honest meeting with the
  borrow checker.
- **Go — number pipeline.** Generator → K workers → collector, over channels; correct for any K, no
  leaked goroutines. *Lens:* CSP.
**Level 2**
- **C — page replacement.** 100 pages, 16 frames, FIFO and LRU over a trace file; diagnostics with
  line numbers. *Lens:* fixed-capacity bounds, file reading, STYLE.md §11 practised away from Krama.
- **Zig — pool allocator** behind `std.mem.Allocator`, replayed from a command file; double frees
  reported. *Lens:* the allocator as a first-class interface.
- **C++ — one-query SQL over CSV.** `SELECT … FROM … WHERE col <op> value`. *Lens:* RAII,
  invariant-establishing constructors, `const&` versus by value.
- **Go — append-only KV store** (Bitcask): log, offset index rebuilt at start, truncated last record
  skipped. *Lens:* `io.Reader` / `io.Writer`, errors as values, crash recovery.
---
 
## 7. The dry run
 
Simulated on a stand-in concept (endianness), so no real questions were spent: trigger at 4 →
decline branch (ledger row + **Didn't**) → accept branch (packet: guide set, reading, level-2 C
brief) → review with tagged findings → check set tagged 5/7/7/9 → partial 9 → score 7 → summary →
release.
 
What it surfaced, and how each was settled:
 
1. **Side-discussion concepts** — request anyway; loose trigger (§3.2).
2. **The in-flight packet** spans sessions — it lives as a file in the concept's folder, linked from
   the ledger row.
3. **Review before the check set** — yes, recursively; gate as §4.2.
4. **Does main work pause?** — ideally, but not mandatory. Honour code. A paused detour is named in
   `PROJECT.md` **Current**, under next up.
---
 
## 8. Where it lives
 
### 8.1 In the repository, not beside it
 
First settled as a separate sparring repository; reversed the next day. The indirection — PROJECT.md
linking into another repo, switching between two — costs more than the separation buys, and there
is nothing to hide. Now:
 
- `learning/<concept>/` — packet, exercises, summary. Best fit only; "what is obvious goes in the
  appropriate folder, otherwise a best fit is enough".
- `learning/LEDGER.md` — one ledger for all concepts.
- The Claude project context holds only the latest ledger and the active packet. Claude never
  reconstructs a prior score.
**Guardrails:** `make check` never builds anything under `learning/` (the D-037 principle — no Zig,
OCaml, Go or Rust toolchain in the gate); each exercise builds on its own; whether the format gate
sweeps sparring C is to be checked against the Makefile; `.gitignore` gains each language's build
output. CLAUDE.md §2: Claude writes `learning/` prose (packets, questions, reading lists, ledger rows),
never an exercise's code.
 
### 8.2 STYLE.md covers sparring C
 
Option (b), chosen over scoping STYLE.md to `src/` and `tests/`. CLAUDE.md §1 already said STYLE.md
governs "the C written in this repo", and practice code is where habits are built. A rule that means
nothing in a toy program (a `KRAMA_` guard on a non-module header) is raised as an amendment, not
skipped.
 
### 8.3 Commits and branches
 
- **New type `learn`**, never scoped — like `build`. `learn(<concept>)` was rejected: sparring work
  doesn't divide cleanly by concept (an exercise under memory may teach alignment), so the vocabulary
  would be open-ended. Changes to the `/ground-up` decision (DR-OWED-A01-01) or CLAUDE.md are `docs`.
- **Learning branches: `m<N>/learn-<concept>`**, e.g. `m1/learn-alignment`, for backward runs and
  paused detours that outlive their stage. A detour found during a stage stays on that stage's
  branch, so its merge shows the gap and its closing beside the code that needed it. The hyphen form
  was preferred to `m1/learn/alignment`: it fits the existing one-stage-name rule and the merge-hook
  pattern unchanged, and avoids the ref-path clash with any `m1/learn` branch.
### 8.4 The blog
 
The *Road to Krama* is the **GitHub wiki on Krama's repository**: free on public repositories,
markdown, its own git repository, so no rendered book in the code tree. GitLab's wiki was the only
close alternative (it allows private projects on the free tier). Private drafting is solved by
cloning the wiki and pushing only when a post is ready. The wiki exists only while the repository is
public.
 
### 8.5 In CLAUDE.md, pointed to from the instructions
 
The rule lives in CLAUDE.md §8, appended rather than inserted beside `/BURNOUT` so no existing section
citation renumbers. It is not a role — it has an exit — so the standing role holds. The project
instructions carry a one-line pointer, never a copy, because the instructions load every session and
CLAUDE.md does not. `/babymode` is retired. CLAUDE.md becomes tracked in git (DR-OWED-A01-02):
transparency, and evidence that the work was not generated.
 
---
 
## 9. Documents changed, and order of work
 
Uploaded to the project context 2026-09-30; to be applied locally on `m1/ground-up`. All four were
rebuilt from project copies — **`git diff` against local files before committing** (Friction,
2026-09-19).
 
| Document | Change |
|---|---|
| `CLAUDE.md` 1.1 | §8 `/ground-up`; §2 `learning/` line; §5 preamble; §7 close-of-session check; closing reference |
| `DECISIONS.md` | DR-OWED-A01-01 (`/ground-up`), DR-OWED-A01-02 (CLAUDE.md tracked); index rows; `A-NN` session note. D-023 stays `Decided` (refined, per D-024 / D-027 precedent) |
| `STYLE.md` 1.6 | §3.1 `learning/` row and paragraph |
| `COMMITS.md` | `learn` type and no-scope note; `docs` lists CLAUDE.md; learning-branch naming; hook regex; the `/ground-up` decision's ID in the intro |
 
Still owed on `m1/ground-up`, by the programmer: `.gitignore` (sparring build outputs; un-ignore
`CLAUDE.md`), an empty `learning/LEDGER.md`, and the §3.1 typo "assisstant interaction riles".
 
**Order:**
 
1. `m1/cli-args` — finish commit 5, merge.
2. `m1/ground-up` — the documents above, cut from the updated `main`. Cut earlier, its entries would
   take IDs ahead of `m1/cli-args`' and bump STYLE.md from 1.4, conflicting with `m1/cli-args` on
   merge.
3. `m1/fatal-cleanup`.
4. `m1/learn-alignment`, plus whatever a backward pass over the Friction log turns up.
5. The lexer.
**Backward-run candidates** from a first look at Friction: alignment arithmetic (scheduled since
2026-09-17, untouched); deriving a bound rather than guessing one (`n / 2 + 1`); "whose exit status
is this?"; subtractive fit tests (likely folds into alignment). Pattern matching (6) is a ledger
candidate from this session.
 
---
 
## 10. Mentor, or driver?
 
Asked directly. Read from the records only (Claude keeps nothing between sessions):
 
- **The programmer drives** the code (issue history shows reviews finding defects and the
  programmer fixing them, sometimes on a third attempt), the verification (the `-Wformat` claim
  tested and found wrong; the chibicc squash claim checked and found false), understanding
  arrived at firsthand (command-query separation found on re-reading, before review named it), and
  this session's design.
- **Claude's influence is large** in framing — many "Rejected" lists and technique names — and in
  document prose. Its own mistakes are on record too.
- **Evidence against "vibe coded"** is the trail of struggle — issues, Friction, experiments that
  caught a reviewer out — more than CLAUDE.md, which shows intent rather than compliance. The Road to
  Krama posts, in the programmer's voice, will be the strongest evidence.
- **Worth watching:** document voice. Where it matters, the programmer drafts and Claude reviews.
---
 
## 11. Open items
 
- **Clearance summary format** — settle at the first clearance.
- **Format gate and `learning/`** — confirm what `fmt_check` sweeps.
- **First ledger rows** — alignment; pattern matching at 6; the Friction backward pass.
- **Back on `m1/cli-args`:** should validation check that a derived or explicit `.expected` file
  exists? (notes-18 §9, part 2). Also open from notes-18: rendering exit values outside the map;
  what an invalid program's `.expected` holds; whether `main`'s dump goes now or with the lexer; the
  owed DECISIONS entries for the driver reading `exit_codes.h` and for the manifest keys.
---
 
## 12. Conclusions — what we are going ahead with
 
1. **`/ground-up` is a declinable detour, not a role**, governed by CLAUDE.md §8, pointed to from the
   project instructions. `/babymode` retired.
2. **Two axes.** Concept (triggers below 7, loose, with a proposed scope) and language fluency
   (tracked, never triggers). Evidence stated with every score. References never lower a score.
3. **Guide set up front, check set held back.** Check set issued only after reviews clear concept
   findings and language misuse, and C reaches 7. Clearing: 7 plus a written summary, confirmed by
   the programmer.
4. **Declines** go to the ledger and PROJECT.md **Didn't**; **paused** detours are named in
   **Current**. Honour code on both.
5. **Languages: C, C++, Zig, OCaml, Go, Rust.** Targets C 8+, others 5–6+.
6. **Everything lives in `learning/`**, one folder per concept, one `LEDGER.md`. The gate never
   builds it. STYLE.md covers its C.
7. **`learn` commit type, unscoped; `m<N>/learn-<concept>` branches** for work outside a stage.
8. **Road to Krama on the GitHub wiki**, drafted locally.
9. **CLAUDE.md tracked in git.** DR-OWED-A01-01 and DR-OWED-A01-02 record the why.
10. **Next:** finish `m1/cli-args`, then `m1/ground-up`, `m1/fatal-cleanup`, `m1/learn-alignment`,
    the lexer.
