# Krama — Renaming funC, and the book

**Date:** 2026-09-19
**Status:** Discussion record. **Not normative.** Decided as `D-032`; carried out on branch
`m1/rename`.

> Nothing here is binding. Where this and `DECISIONS.md` / `COMMITS.md` disagree, those win.

---

## 1. How to read this

- **§2** — why funC had to go.
- **§3** — what makes a name usable, and the three checks.
- **§4** — the candidates, and why each failed.
- **§5** — Krama, and what it commits to.
- **§6** — rename scope: what changes, what is frozen.
- **§7** — the search traps. The most transferable part of the session.
- **§8** — the book.
- **§9** — settled, owed, open.

---

## 2. Why funC had to go

Three problems, the third fatal:

1. **A weak pun.** "fun C" was never the point of the project.
2. **Load-bearing capital.** `funC` only reads correctly with the capital C. Any lowercase context —
   paths, identifiers, extensions — loses the joke and the meaning at once.
3. **`func_` means *function*.** In a C codebase, in a transpiler whose own source is full of
   functions, the prefix reads as an abbreviation for the wrong thing. The name fought the domain.

The extension was the tell: `.func` next to `.c` looks like a file *of* functions.

---

## 3. What makes a name usable

Criteria, in the order they killed candidates:

- **A unique search string.** `<name> programming language` must find this project, not something
  else. For a project that intends to publish a book (§8), searchability is most of the name's job.
- **An unclaimed extension.** Editors, `.gitignore` patterns and syntax highlighting all key off it.
- **Reads as a noun.** "a `.krm` file", "the Krama compiler".
- **Does not look like a C thing.** The funC failure.
- **Types easily.** Lowercase, no ambiguous letter shapes.

**The three checks**, run per candidate before any attachment forms:

1. `<name> programming language`
2. `.<ext> file extension`
3. Is the GitHub repository name free?

---

## 4. Candidates, and why each failed

| Candidate | Appeal | Why not |
|---|---|---|
| `mouse` / `.mse` | A daughter's nickname. Personal, warm | Peter Grogono's **Mouse** (BYTE, 1979; book 1983; retro following) owns the search results permanently |
| `bug` | Pun on debugging | `.bug` is OpenBUGS's extension for saved execution images |
| `koa` | Short, clean | Koa.js — a 34k-star Node framework |
| `tav` | Hebrew/Phoenician "mark"; the last letter. `.tav` clean | `jkingstonc/tav` is an existing alpha language inspired by C, Go and Jai — **same category**, which is the worst kind of collision |
| `naja` | Cobra genus | `naja-js` (Nette AJAX). Also carried no connection to the project beyond sounding well |
| `anu` | Sanskrit अणु, "atom" — the smallest unit that still works. Exactly the design philosophy | A girl's name. So are `rita`, `nitya`, `lipi`, `vidhi`, `sadhana` — which removed most of the same family |
| `sutra` | **The closest miss.** Names the genre of maximally terse rule-texts meant to be expanded by a teacher — which is what the spec and the planned book already are | The Kama Sutra association, to be fielded indefinitely. Search also skews religious |
| `.str` (for Sutra) | Pun | Rejected on funC's own grounds: in a C codebase `str` means *string* to every reader. Also taken by game string tables and PlayStation video |
| `.su` | Short | GCC writes `.su` under `-fstack-usage` — the exact tooling this project lives in |
| `kerf` | **The runner-up.** The slot a saw blade leaves. Almost entirely unclaimed | See below |

### 4.1 `kerf`, and the story it told

Worth preserving, because the argument is good even though it lost.

A kerf has width. Every woodworker learns it early: measure, cut, and the piece comes out a blade's
width short, because the material the blade removes has to be accounted for. Cutting as though the
kerf has no width is the beginner's error.

That is this language's central commitment. C pretends signed overflow does not exist and lets the
compiler assume it away; `INT32_MIN / -1` pretends to be division. Krama stops and says what
happened. It refuses to cut on the wrong side of the line and call the result exact.

The name also stays honest about scope: a kerf is a *narrow* cut, not a new tool. Not a replacement
for C — a thin, deliberate slice of it.

**Lost to `krama`** only because `krama` describes both the artifact and the method, and is the more
personal choice. If the name is ever revisited, start here.

---

## 5. Krama

**क्रम — sequence, order, one step following another.**

- What a compiler *is*: lex, parse, check, emit, in order.
- What building one by hand has been.
- Short, typed easily, no religious or comic baggage, search results effectively unclaimed.

| Site | Form |
|---|---|
| Prose | `Krama` |
| Code, paths, prefixes, extension | `krama` |
| Sources, test pairs | `.krm` / `.expected` |
| Binary | `kramac` |
| Macros, include guards | `KRAMA_TEST_SUITE`, `KRAMA_<MODULE>_H` |
| Emitted runtime | `krama_trap`, `krama_div_i32`; deferred `krama_rt.c` |
| Fixture corpus | `tests/krama_src/` |

**`kramac`, not `krama` or `krm`.** The trailing `c` in `gcc`, `rustc` and `javac` denotes
*compiler*, not the C language; transpiling to C does not change what the program is.

**`KRAMA_` in code, not `KRM_`.** Two spellings of one name means every reader learns which is
which. The three-letter form earns its keep only where length is the point: the extension.

### 5.1 A section that lost its reason to exist

STYLE.md §2.2 existed to explain how a proper noun's internal capital survives inside `snake_case` —
i.e. how `funC_trap` is legitimate. An all-lowercase name leaves it nothing to do. Rewritten to the
rule that now matters: **`Krama` in prose, `krama` in code.**

A rename can dissolve a rule rather than update it. Read before substituting.

---

## 6. Rename scope

The rule that resolved every case:

- **Live documents change.** Spec, STYLE.md, COMMITS.md, README, CLAUDE.md, PROJECT.md's *Current*,
  note *titles*. They describe the project as it is now.
- **Records keep the old name.** Decided entries `D-001`–`D-031`, note *prose*, PROJECT.md's dated
  log entries, past review documents. They record a project that carried that name at the time;
  editing them would claim the name existed earlier than it did. Same reasoning as "never edit a
  decided entry in place".
- **Pointers are the exception, and change everywhere.** A reference to a *file*
  (`funC-spec-v1.md`, `funC-notes-09-…`) was updated even inside frozen entries. A stale pointer is
  a **dead link**, not a historical fact.

### 6.1 Consequences worth remembering

- **GitHub issue URLs** were rewritten (16 in PROJECT.md). Redirects survive a repository rename —
  **until a repository with the old name is created under the same account**, which drops them.
- **CLAUDE.md is gitignored.** It carries the name, was updated by hand, and nothing in the
  repository verifies it. A fresh clone does not receive it.
- **The working copy directory** was renamed too. This is independent of GitHub: git tracks contents
  and `.git` lives inside the folder, so `mv` is the whole operation — no remote change, no
  reconfiguration.
- **Two fixture directories and two extensions** (`tests/fc-src/` + `.fc`, `tests/funC_src/` +
  `.func`) existed before this entry. Collapsed into one of each.

---

## 7. Search traps

The transferable part. Each of these would have shipped a defect.

### 7.1 `git grep` searches contents, never filenames

Every sweep this session was blind to `tests/funC_src/funC2.func`, whose basename survived the
extension rename. The check for names is a different tool:

```
git ls-files | grep -i func
```

It reads the **index**, not the working tree, so it only tells the truth *after* staging.

### 7.2 Case-sensitivity is a tool, not a detail

In one codebase: `funC` (the name), `FUNC_TEST_SUITE` (the define), `__func__` (a C standard
predefined identifier), and hundreds of instances of the English word "function".

- `funC` cannot match `FUNC_TEST_SUITE`.
- `FUNC_` cannot match `__func__` — which is exactly why an uppercase, underscore-anchored pattern
  is safe on the source.
- **Survey with `-i`. Change with precise patterns.**

### 7.3 Substring damage

- `sed 's/func/krama/g'` turns **`function`** into `kramation`, and breaks `fatal.h`, STYLE.md and
  half the quick-refs.
- `sed 's/funC/krama/g'` turns **`cfunC`** into `ckrama`. Ordered substitutions, longest first, or
  exclude the file.

### 7.4 Lowercase spellings hide from a name sweep

`funcrt.c` (two spec sections) and `.func` matched neither `funC` nor `FUNC_`. Found only by
searching lowercase `func` separately, with "function" as accepted noise.

### 7.5 Numbers, before starting

341 matching lines → 159 pure "function" noise → ~180 real, of which **11 were code identifiers**.
Knowing the split before touching anything is what made the blind `sed` unnecessary.

### 7.6 Whole-file replacement undoes hand edits

A trimmed line in `D-032` was silently restored when a regenerated copy of the file — built from a
stale read — was pasted over it. Caught in review. Prefer targeted edits to a file already being
edited by hand.

---

## 8. The book

A **K&R-style walkthrough**: tutorial and reference manual bound together. Half of it already
exists — the spec *is* the reference manual.

**Written at milestone boundaries, not continuously.** Prose written against a moving language is
rewritten constantly, and that is time not spent on the compiler. A closed milestone is a frozen
subset by definition. Milestone 1 yields a real chapter: three scalar types, arithmetic, `@print`,
one `main`.

Two reasons it is worth doing rather than a distraction:

1. **Writing the tutorial is a design test.** A feature that is awkward to explain is usually a
   feature that is wrong. K&R's clarity partly reflects a language shaped by people who had to teach
   it.
2. **The examples should be the tests.** If every sample in the book is an actual `.krm` /
   `.expected` pair from the corpus, then `make check` proves the book still compiles. Examples that
   rot are the standard failure mode of language books; this is structural immunity.

**Shape:** `book/` in the repository, outside the build; one chapter per closed milestone; examples
drawn from the test corpus. The session notes are the raw material.

---

## 9. Settled / owed / open

### Settled

- The project is **Krama**; `.krm`, `.expected`, `kramac`, `KRAMA_`, `krama_`, `Krama`/`krama`
  (D-032).
- Rename scope: live documents change, records are frozen, pointers change everywhere.
- Fixtures consolidated to `tests/krama_src/`.
- The book is written at milestone boundaries, with corpus files as examples.

### Done this session

- `m1/rename` merged in three commits: COMMITS.md appendix, documentation rename, identifiers and
  fixtures.
- COMMITS.md gained the **command-sequence appendix** — the seven workflow steps as runnable
  commands.
- `D-032` recorded, superseding the `.func`/`.exec` pairing (D-025, D-030), the `FUNC_TEST_SUITE`
  define (D-024, D-026), and STYLE.md §2.2.

### Owed

- **Rename the GitHub repository**, then `git remote set-url origin …`.
- **Fold `all` into `check`** so the gate compiles `src/main.c`. Harmless only while `main.c` does
  nothing — which ends with the arena initialization.
- **`src/main.c`'s hardcoded absolute path** — now a *Deliberately incomplete* row. Goes when `main`
  takes a path argument.
- **Session numbering.** `DECISIONS.md`'s template carries `Session: ##` and it has never been
  filled in. Tying it to the note number is the cheapest fix: both increment together, and both are
  produced at the same moment.

### Open

- **Next branch shape:** `m1/arena-init` alone, or folded into `m1/ast-nodes`? They are coupled —
  block size depends on node size — but a working `main` end to end has its own value. Decide before
  naming the branch; the name is permanent once merged.
