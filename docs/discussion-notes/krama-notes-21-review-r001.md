# Krama — Code review r001, Python 3.13, and reviews under workflow 2.0

**Date:** 2026-11-05 / 2026-11-07
**Status:** Discussion record. **Not normative.** Source notes for D-039 (in part) and D-040.

> Nothing here is binding. Where this and `DECISIONS.md` / `STYLE.md` / `COMMITS.md` /
> `krama-spec-v1.md` disagree, those win.

Continues `krama-notes-20-driver-pseudocode`. The programmer wrote `tests/harness/kramat.py`
(parts 1–2: pre-checks, header parsing, manifest validation). It was reviewed as **r001**
(`docs/review/m1/cli-args/r001.md`), filed as issues #16–#20, fixed, and committed on
`m1/cli-args`.

---

## 1. How to read this

- **§2** — minimum Python 3.13 (D-040).
- **§3** — the header parser stays light (reverses part of notes-20; D-039).
- **§4** — review r001: what was found and how it was resolved.
- **§5** — principles settled along the way.
- **§6** — reviews under workflow 2.0.
- **§7** — numbering, branch state, what's next.
- **§8** — conclusions.

---

## 2. Minimum Python 3.13 — D-040

**The trigger.** The code uses backslashes inside f-string expressions (`{'\n'.join(...)}`), which is
a SyntaxError before Python 3.12 (PEP 701). A syntax error stops the whole file compiling, so no
runtime version check can catch it — the same lesson as the late `tomllib` import, one level
earlier.

**Decision:** the minimum is **3.13**, the version the harness is written and tested on. Supersedes
D-037's 3.11.

- **Rejected: 3.12.** The technical minimum for PEP 701, but never run — declaring it claims support
  for an untested version.
- **Context (November 2026):** Python 3.15.0 released 2026-10-09; 3.10 reached end of life
  2026-10-01. Debian 13 ships 3.13; Ubuntu 24.04 ships 3.12; Ubuntu 26.04 ships 3.14 (sources
  disagree; hands-on guides report 3.14). Distribution defaults run one to three versions behind.
- **Revisit if:** a second machine or CI runs an older Python.

**Consequences:**

- The version check compares `sys.version_info` to a tuple — string comparison was wrong
  (`"3.9.1" < "3.11"` is `False`).
- The check fires only on 3.12; anything older fails at compile time. A comment says so.
- Spec §11.1 and the STYLE.md §13 Python row must state 3.13.

---

## 3. The header parser stays light — part of D-039

notes-20 §3 added an unknown-line rule and enum scoping because "until the dump exists, the Python
parser is the only check". The written code went lighter, and the programmer kept it that way:

- **Accepted gaps (r001 2.4–2.6):** no enum scoping; no unknown-line rule (a misspelled enumerator
  compiles and silently shifts later values); comments mentioning `KRM_EXIT_` and trailing comments
  are not handled.
- **Why:** the final check is the cross-check against `kramac --dump-exit-codes` (notes-20 §5);
  hardening the parser before then is work the cross-check replaces.
- **The window:** until `--lex` lands, a malformed header line can misread silently.
- **Tracked as #21** — *Harness: cross-check parsed exit codes against --dump-exit-codes*. The body
  stands alone (notes are not in the repository).
- **Owed:** a PROJECT.md *Deliberately incomplete* row and a line in D-039's consequences.

**Flag name settled:** `--dump-exit-codes` (not `--dump-error-codes`).

---

## 4. Review r001

Five issues, each a group of numbered findings, kept with their review IDs so GitHub's numbers map
back to the file.

| Issue | Findings | Resolution |
|---|---|---|
| #16 · 1 Version check | 1.1 minimum 3.11 → 3.13; 1.2 string comparison | Fixed; comment added |
| #17 · 2 `parse_c_int` | 2.1 digit order reversed; 2.2 `0x0F` fell into octal; 2.3 invalid digits accepted; 2.7 first implicit enumerator got 1 | Fixed — Horner's method; one prefix chain; `int(c, base)` per digit; "next value" tracking |
| #18 · 3 Contract drift | 3.1 unknown keys accepted; 3.2 non-table elements; 3.3 `#derive` stricter than decided; 3.4 empty `case` | Fixed; 3.3 aligned to "exactly one `.krm` among the args" |
| #19 · 4 Messages | 4.1/4.2 missing `f`, unbound `e`; 4.3 errors on stdout; 4.4 spellings | Fixed; type errors gained the `Case #` prefix |
| #20 · 5 Order/tidy | 5.1 args after env checks; 5.2 unsorted stages; 5.3 debug prints; 5.4 stale comment; 5.5 `all` shadowing | Fixed; 5.3 became `_ = exit_codes  # TODO(#21)` |

**A bug found in the fix round.** The first `parse_c_int` rewrite split characters into ranges
(`0`–`7`, then `8` or `A`–`F`) and rejected every `9`. The self-check missed it because the header's
explicit values are `0` and `2`; the issue's own test table had no `9` in it either. Lesson: test
inputs must exercise every digit and avoid symmetric values — `"0b101"` passed the reversed-digit
bug by being a palindrome.

---

## 5. Principles settled along the way

- **The compiler as a guard, applied consistently.** Inputs the compiler rejects (`"0x"`, `"0b12"`,
  `"019"`) cannot reach `parse_c_int`, so it need not reject them. Only accepted-but-misreadable
  inputs matter (`"19"`, `"0x9"`, octal `023`). The name for documenting such assumptions instead
  of checking them is **design by contract**; the docstring should state the precondition.
- **Sorting is about the result, not the walk.** notes-14 §5 and §8.9 require a sorted stage list so
  runs compare between commits. `iterdir()` or `os.walk` both work once the names are sorted.
  Sorted means alphabetical, not pipeline order — fine while stages are independent.
- **Labels carry level and source.** `ManifestError: Stage x: TOMLDecodeError: …` — the first label
  says what happens (the stage is rejected), the second who detected it.
- **`case` elements must be tables.** Only an inline array (`case = [1, 2]`) produces a non-table;
  the message names the wrong syntax.
- **TODOs point at open issues, not notes.** Notes are not in the repository and have no state; an
  issue is reachable from the code and its closing is the signal to remove the TODO. A TODO
  pointing at a closed issue looks resolved when it is not.

---

## 6. Reviews under workflow 2.0

### 6.1 What r001 did

- **Review file** at `docs/review/<milestone>/<branch>/rNNN.md`: header table, one section per
  issue (title + verbatim body), *Not filed* (decided in review, resolved by records, records owed),
  *Filed as* (URLs).
- **Claude Code files the issues** with `gh issue create --body-file`, after showing every title for
  approval, then fills the *Filed as* table. No labels, no commits.
- **Follow-up records** go into the review file the same way (#21 added to *Not filed*).

### 6.2 The gap r001 exposed

The reviewed code was **never committed**. r001 describes a version of `kramat.py` that does not
exist in git, so its "Today" column cannot be reproduced. Committing before review gives the full
history: commit → findings as issues → fix commits closing them.

### 6.3 Generating the story from git

- **`Refs: #N`** trailer on each fix commit; `git log --grep "#N"` lists them in order.
- **`git log -L :function:file`** shows one function's history with only its diffs — the
  code → review → code story per finding. Python needs `*.py diff=python` in `.gitattributes`.
- **`gh issue view N --json`** supplies the finding's text.
- **Closing note:** when a review's issues all close, generate a short note from the issues and
  diffs and append it to `rNNN.md`.

### 6.4 Rules for the CLAUDE.md review process

1. Commit before review.
2. One finding per fix commit where practical.
3. `Refs:` trailer naming the issue.
4. Never rewrite reviewed commits — a squash or rebase breaks every link.
5. Generate a closing note when all of a review's issues close.

**Still open:** aside-notes-03 §5 point 3 — when `Refs:` / `Closes` trailers go in. `Closes #N` on a
feature branch only takes effect on merge to the default branch.

**Note on location:** under aside-notes-04 §7, `docs/` becomes the published Pages story arc, so
reviews there are public pages. Deliberate, or move to `story/review/`.

---

## 7. Numbering, branch, next

- **Numbering:** D-039 (driver parses the header), D-040 (Python 3.13). `m1/ground-up`'s entries
  stay owed — DR-OWED-A01-01 / -02 — and are numbered when they land; STYLE.md 1.6 here.
  *(Updated 2026-11-08: pre-assigned IDs dropped under the DR-OWED convention, `krama-notes-22` §9.
  D-041 went to workflow 2.0.)*
- **Branch:** continue `m1/cli-args` — the harness validates but runs no case and `make check` does
  not call it; merging would put a gate on `main` that cannot fail.
- **Commit 5 remaining:** records (step 6) → fixtures (step 3) → runner (part 3) → Makefile part B
  (step 5) → commit body (step 7).

**Records, in order:**

1. DECISIONS: D-039 (sources: notes-18 §4/§5/§9, notes-19 §8, notes-20 §3–§5/§7, this note §3),
   D-040 (this note §2; background D-037, notes-14 §4.3).
2. `exit_codes.h` header comment — format rules, no "duplicates"; STYLE.md 1.6 §13 Python row;
   spec §11.1 for 3.11.
3. PROJECT.md *Deliberately incomplete* — root two levels up; no per-case timeout; stdout unchecked
   under `#none` and `main`'s dump; the light header parser and #21. Then the log entry and Current.

---

## 8. Conclusions

1. **Minimum Python is 3.13** (D-040), the tested version; 3.12 rejected.
2. **The header parser stays light;** the gap is tracked as #21 and closes with `--dump-exit-codes`.
3. **r001's five issues (#16–#20) are fixed** and committed.
4. **Compiler as guard, consistently:** `parse_c_int` handles what compiles; the docstring states
   the precondition.
5. **TODOs point at open issues.**
6. **Reviews:** commit first, file issues from a review file, fix with `Refs:`, never rewrite,
   close with a generated note.
7. **The branch continues;** records next.
