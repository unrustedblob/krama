# Krama — Driver pseudocode, parts 1–2: review record and error catalogue

**Date:** 2026-10-02 / 2026-10-03
**Status:** Discussion record. **Not normative.** §8 is where the next session starts; Appendix A
catalogues every error the pseudocode raises.

> Nothing here is binding. Where this and `DECISIONS.md` / `STYLE.md` / `COMMITS.md` /
> `krama-spec-v1.md` disagree, those win.

Continues `krama-notes-19-driver-shape` §8, step 4. The programmer wrote the pseudocode for parts 1–2
of `tests/harness/kramat.py`: pre-checks, reading `exit_codes.h`, loading and validating manifests.
Execution waits for step 3 (fixtures).

---

## 1. How to read this

- **§2** — the pseudocode's final shape.
- **§3** — what the review rounds changed, and why.
- **§4** — `exit_codes.h` becomes a format the driver depends on.
- **§5** — the name table and `--dump-exit-codes`, planned.
- **§6** — regex: a ledger track.
- **§7** — deferred, deliberately.
- **§8** — queued records, open, next session.
- **§9** — conclusions.
- **Appendix A** — every error, by level.

---

## 2. Final shape (parts 1–2)

```
pre-checks     root from __file__ → argparse (--stage / -s) → stage list + "all"
               → Python ≥ 3.11 → kramac-san exists → exit_codes.h exists
header         find `enum KRMExitStatus` → collect members → `}` ends it
               → clean keys → convert values → derive implicit → self-check
manifests      stages = all directories, or the one named
               → per stage: cases.toml exists → opens → decodes → not empty
               → root is `case`, a list of tables → per-case contract checks
```

- **`tomllib` is imported after the version check**, with a comment saying why — otherwise someone
  moves it back to the top. Other imports stay at the top.
- **`--stage all`** is what `make check` passes. No argument is still a usage error; a `TODO` says
  it becomes "all" later.
- **Every manifest error exits for now** (fail fast). Collecting all errors per manifest and
  continuing to the next stage (notes-19 §5.3) arrives with the runner, when there are real errors to
  collect. A `TODO` anchors the change.

---

## 3. What the review rounds changed

| Finding | Fix |
|---|---|
| Binary named `krama-san` | `kramac-san` |
| `STAGE_ROOT` "one level up" from `__file__` — off-by-one risk | Derived from `PROJECT_ROOT/tests/stage`; one anchor |
| Mixed argparse setup (option and positional) | `-s` / `--stage` taking one value |
| argparse ran after environment checks | Parsed first, so `--help` works without a build |
| `if stage != "all" \|\| stage not in valid_stages` rejected every stage | `&&` — negating "or" gives "and": **De Morgan's law** |
| `all` added to the case key set | Removed — `all` is a value of `--stage`, never a manifest key |
| `--stage all` then looked for `tests/stage/all/` | Its own branch: loop over every stage |
| Duplicate check on a case's *keys* (TOML already rejects those) | Duplicate *names* across cases |
| `codes = []` inside the loop | Initialised once |
| `line.strip` unassigned (strings are immutable) | `line = line.strip()` |
| Keys kept comma and spaces; stored under the whole line | Strip comment → remove comma → strip; store under `key` |
| Implicit values off by one when the first enumerator is implicit | "Next value" semantics: `value + 1` after explicit, `+= 1` after implicit |
| Unknown lines inside the enum skipped silently — a typo'd enumerator (valid C) shifts every later value | Inside the enum: blank and `//` skipped; anything not starting `KRM_EXIT_` is a run error |
| The new rule then failed on blank lines | Blank lines skipped before the rule |
| Reserved-1 check covered implicit values only | Moved into the self-check |
| `[case]` (single brackets) gives a dict; the loop walked its keys | Root value must be a list of tables |
| Missing contract rules | `.expected` suffix; `#derive` needs **exactly** one argument **ending** `.krm` |

**Settled along the way:**

- **Raising `AssertionError` explicitly is fine.** Only the `assert` *statement* is stripped under
  `python -O`.
- **The self-check covers what the header promises:** `SUCCESS == 0`, `USAGE == 2`, no value is 1,
  no two names share a value (the report must turn a number back into a name).
- **`parse_int` converts any valid C integer**, octal included: `023` is 19, never 23. Returns
  `None` when it cannot, which is a run error. Expressions such as `(1 << 2)` are not handled.
- **The compiler as a guard.** The harness needs a successful build, so the header is valid C and
  the parser can be lax about what the compiler rejects (`2F`, `0b2F`). It cannot be lax about what
  the compiler *accepts* but the parser misreads — octal, digit separators, suffixes, expressions,
  misspelled enumerators. Those must convert correctly or fail loudly.

---

## 4. `exit_codes.h` as a parsed format

The programmer's word: in this one file, style is **dogma**, not convention, because the driver
parses it. The rules the parser depends on:

1. The enum opens on a line beginning `enum KRMExitStatus`.
2. It closes on a line beginning `}`.
3. Inside it, every line is blank, a `//` comment, or one enumerator.
4. One enumerator per line, beginning `KRM_EXIT_`; a trailing `//` comment is allowed.
5. No preprocessor lines and no `/* */` comments inside the enum (STYLE.md §10.2).
6. Explicit values are integer literals only — no expressions.

**Where to record them:** the header comment being rewritten in commit 5 (it still says the driver
*duplicates* the numbers), and D-039's consequences. Whoever edits the header next sees the rules
before breaking them.

---

## 5. The name table and `--dump-exit-codes` — planned, not commit 5

**The idea.** A C table of names, indexed by value, printed by a kramac flag. The harness compares
the dump to its own parsed map — name and value must both agree.

**Why it earns its place:**

- **Cross-check.** It catches what neither side catches alone, such as
  `[KRM_EXIT_ERR_INPUT] = "KRM_EXIT_ERR_OOM"`, which compiles.
- **`to_string` for free** — the report needs names, and so will kramac's own messages.
- **It checks `parse_int`**, since the compiler computes the real values.

**Design as discussed:**

- **`src/exit_codes.c`**, holding the table and a print function.
- **No X-macros.** The table is written by hand: a small maintenance cost, accepted.
- **Designated initializers** (`[KRM_EXIT_ERR_INPUT] = …`) make index = value true by construction;
  unlisted indices, including `[1]`, are null.
- **Checks:** length is `UNKNOWN + 1` (`static_assert`, compile time); index 1 is null and *every
  other index is not* — that catches a class added to the enum but forgotten in the table.
- **`constexpr` — verify first.** As remembered, C23 allows a `constexpr` pointer object only with a
  null value, so an array of strings would be `static const char *const`. Unverified: test it with
  the compiler.
- **A flag, not a second `main`.** `--dump-exit-codes` runs the print and exits before reading any
  source. It ships in the real binary — public interface, so a spec and decisions matter. Prior art:
  query flags such as `gcc -dumpversion`, `clang -print-targets`.
- **The output is a contract** — for example one `NAME VALUE` per line, exiting `SUCCESS`.
- **Lands with `--lex`**, when `main` gains its option handling.

**Until then, the Python parser is the only check** — which is why the unknown-line rule (§3)
went in now rather than waiting for the dump.

---

## 6. Regex — a ledger track

Regex is classed as a **concept**, not a language: a formal notation with a small core, so a fluency
target is reachable. That distinguishes it from the Rust situation the `/ground-up` design rejected.

- **Target:** levels 5–6 (informally ELI5 → God tier).
- **Evidence:** about 80% success on a problem set, *and* reading a regex and saying what it does.
- **Branch:** decided when the work starts. Raised: a merge per finished packet keeps D-030's short
  branches; a long-lived branch would be a recorded exception.
- **Recorded in:** `learning/LEDGER.md` once `m1/ground-up` lands. Until then, here.

**Evidence from this session:**

| Observed | Example |
|---|---|
| Character class confused with a group, twice | `[\s*=\s*]`, `[\/\/]*` |
| `+` inside a class read as a quantifier | `[\w+\d+_]` |
| Searching mistaken for validating — no end anchor, partial matches pass | `KRM_EXIT_C == 4` still yielded a name |
| Quantifier miscount — `(…)+` demanded a second digit run | `= 0` and `= 2` silently lost |
| `\s` crossing lines in multiline mode | blank lines highlighted as part of matches |
| C23 digit separator taken as backtick | it is an apostrophe: `1'000` |
| Scope widened at once (hex, binary, separators) where the exercise asked for decimal | both bugs entered there |
| **Strengths** | tested against real input; read regex101's explanation panel; anchored start and used a lookahead in round 2; fixed each issue once named |

The regex itself was **parked** for commit 5: the parser now matches by prefix and splits.

---

## 7. Deferred, deliberately

| Item | Why deferred | Revisit |
|---|---|---|
| Collect-and-report for manifest errors | No runner, one stage | With the runner |
| `SUCCESS` / `USAGE` presence check | A missing name raises `KeyError` — loud already; the programmer wants names derived from the C side | With the name table |
| `parse_int` for expressions | The header uses literals only (§4 rule 6) | First non-literal value |
| Multi-line comments and macros inside the enum | STYLE.md §10.2; none exist | When they would |
| Enum dump cross-check | Needs `main`'s option handling | With `--lex` |
| Per-case timeout | notes-19 §7 | First hang |

---

## 8. Queued records, open, next session

### Additions to commit 5's records

- **D-039 consequences:** §4's format rules; the planned name-table cross-check (§5).
- **`exit_codes.h` header comment:** §4's rules, replacing "the driver duplicates these numbers".
- **PROJECT.md *Deliberately incomplete*:** fail-fast manifest errors; presence check; `parse_int`
  limits — alongside notes-19 §8's rows.
- **`TODO`s in the driver:** no-argument → all; collect-and-report; multi-line comments and macros
  in the enum.

### Future decisions

- **`--dump-exit-codes`:** a DECISIONS entry and a spec note when it lands (§5).
- **Ledger row — regex** (§6), on `m1/ground-up`.

### Next session

1. **Step 3:** is `krama_src/test.krm` the valid-file fixture or scratch? If scratch, what does the
   fixture in `tests/stage/args/` contain? Then fixture names for the six cases.
2. **Execution** — part 3 of the pseudocode, including the case-level errors in Appendix A.3.
3. **Build** — deferred-to-build questions: collect or stop, one exception per level, the late
   import, the STYLE.md §13 Python row.

---

## 9. Conclusions — what we are going ahead with

1. **Parts 1–2 are complete as pseudocode** and consistent with notes-18, notes-19 and §3 here.
2. **The header parser is line-based and scoped to the enum:** unknown lines are run errors; blank
   and `//` lines skipped; self-check on 0, 2, no 1, unique values.
3. **`exit_codes.h` is a parsed format** with written rules (§4).
4. **`parse_int` handles any C integer literal, octal included;** expressions are not yet handled.
5. **Fail fast on manifest errors** until the runner exists.
6. **The name table and `--dump-exit-codes`** are planned for the `--lex` work, not commit 5.
7. **Regex is a ledger concept,** target 5–6.

---

## Appendix A — Error catalogue

Levels per notes-19 §5.3: **run** stops everything; **manifest** rejects a stage; **case** is
counted and the rest run. "Label" is what the pseudocode names today. Consolidating to one exception
per level is a build-time decision (§8).

### A.1 Run errors — pre-checks and header

| # | Where | Condition | Label today | Notes |
|---|---|---|---|---|
| R1 | argparse | No arguments | `UsageError` (← `RuntimeError`) | Becomes "all" later (`TODO`) |
| R2 | argparse | Unknown option (`-k`, `--abc`) | raised by argparse | Exits **2**, the *driver's* status, unrelated to `KRM_EXIT_ERR_USAGE` |
| R3 | argparse | `--stage` without a value | raised by argparse | Exits 2 |
| R4 | Stage list | Value is neither `all` nor a directory under `tests/stage/` | `InputError` (← `RuntimeError`) | Also rejects `../` paths |
| R5 | Version | Python older than 3.11 | `AssertionError`, raised explicitly | Before `tomllib` is imported |
| R6 | Environment | `build/kramac-san` missing | `RuntimeError` | |
| R7 | Environment | `src/exit_codes.h` missing | `RuntimeError` | |
| R8 | Header | Cannot open the header | "Environment error" | |
| R9 | Header | Line inside the enum is not blank, `//`, or `KRM_EXIT_…` | `ValueError` (← `RuntimeError`) | Should name the line |
| R10 | Header | `parse_int` returns `None` | `ValueError` (← `RuntimeError`) | Parser needs an update |
| R11 | Self-check | `SUCCESS ≠ 0`, `USAGE ≠ 2`, any value is 1, or two names share a value | `AssertionError`, raised explicitly | |
| R12 | Self-check | `SUCCESS` or `USAGE` absent | *unlabelled* — `KeyError` | Deferred (§7) |

### A.2 Manifest errors — per stage, from the contract alone

*Today each one exits the run (§2).*

| # | Condition | Label today |
|---|---|---|
| M1 | `cases.toml` missing from the stage directory | "driver-manifest-error" (← `RuntimeError`) |
| M2 | Cannot open `cases.toml` | `OSError` |
| M3 | Not valid TOML | `TOMLDecodeError` |
| M4 | Empty — no cases | `ManifestError` (← `RuntimeError`) |
| M5 | A root key other than `case` | `ManifestError` |
| M6 | `case` is not a list of tables (`[case]` instead of `[[case]]`) | `ManifestError` |
| M7 | A case's keys ≠ `{name, args, return_code, stderr, expected}` — missing or unknown (`agrs`) | `ManifestError` |
| M8 | Wrong type: `name`, `return_code`, `expected` not strings; `args` not a list of strings; `stderr` not a boolean | `ManifestError` (to be collected per file) |
| M9 | `expected` starts with `#` but is not `#derive` or `#none` | `ManifestError` |
| M10 | `#derive` without exactly one argument ending `.krm` | `ManifestError` |
| M11 | Explicit `expected` not ending `.expected` | `ManifestError` |
| M12 | Duplicate `name` in the manifest | `ManifestError` |

### A.3 Case errors and failures — not yet in the pseudocode (part 3)

From notes-19 §4.2; listed so part 3 starts complete.

| # | Kind | Condition |
|---|---|---|
| C1 | Error | `return_code` is not a name in the parsed map |
| C2 | Error | `return_code` is `KRM_EXIT_ERR_UNKNOWN` |
| C3 | Error | The `.expected` file (derived or explicit) is missing on disk |
| F1 | Failure | kramac's exit code ≠ `return_code`'s value |
| F2 | Failure | `stderr = true` and stderr is empty, or `false` and it is not |
| F3 | Failure | stdout ≠ the `.expected` file (only when `expected` is not `#none`) |
| F4 | Failure | Exit value outside the map — `1` (sanitizer), `134` (abort); rendering open (notes-18 §3) |

**Errors** mean the case could not be judged — fix the fixture. **Failures** mean kramac did the
wrong thing. Both count against `N / Total`; both make the run exit non-zero.
