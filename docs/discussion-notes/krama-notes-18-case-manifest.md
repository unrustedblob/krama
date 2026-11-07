# Krama — The case manifest: keys, exit names, and expected output

**Date:** 2026-09-29
**Status:** Discussion record. **Not normative.** Part 1 of the manifest discussion; §9 lists what is
queued and what part 2 picks up.

> Nothing here is binding. Where this and `DECISIONS.md` / `STYLE.md` / `COMMITS.md` /
> `krama-spec-v1.md` disagree, those win.

Continues `krama-notes-17-harness-handoff` §8, which named the manifest keys as the first open
decision for commit 5. D-036 fixed the manifest's *shape* and deliberately left the keys; this
session settles them.

---

## 1. How to read this

- **§2** — the case table and the key set.
- **§3** — `return_code`: full names, `UNKNOWN`, unmapped exit values.
- **§4** — where the name-to-number map comes from: reading the header.
- **§5** — program-side metadata, parked.
- **§6** — `stderr`: two states, both checked.
- **§7** — `expected`: three states and the `#` commands.
- **§8** — sample cases.
- **§9** — queued records, open, part 2.
- **§10** — conclusions.

---

## 2. The case table

Cases are an **array of tables**, `[[case]]`, one per case. The driver enumerates them; no case is
numbered, so deleting one renames nothing (notes-14 §2).

| Key | Values | Validation |
|---|---|---|
| `name` | string | unique within the manifest; reported as `stage/case` |
| `args` | array of strings | may be empty (the no-argument case) |
| `return_code` | full `KRM_EXIT_*` name | must be in the parsed header map; `KRM_EXIT_ERR_UNKNOWN` rejected |
| `stderr` | `true` / `false` | `true` = non-empty, `false` = empty |
| `expected` | `"#derive"` / `"#none"` / `"<path>.expected"` | see §7 |

**Every key is required.** A row in a five-row table costs nothing and makes each case say what it
checks. The useful consequence: the set of **required** keys and the set of **allowed** keys are the
same set, so validation is one comparison — a case's keys must *equal* that set. A missing key and a
typo (`stdderr`) fail the same check.

**No defaults or group table.** D-036 and notes-14 §7 left one open; with every key required there
is nothing for it to supply.

---

## 3. `return_code`

**Full enum names** (`KRM_EXIT_ERR_INPUT`), not short class names. It is a program's exit class and
should read as the code the program hit; prettier short names buy nothing. Full names also match the
header exactly, which is what §4 reads.

**`UNKNOWN` is two different situations:**

1. **A manifest asserts `KRM_EXIT_ERR_UNKNOWN`.** The manifest is wrong. Validation error; nothing
   runs.
2. **The program exits with `UNKNOWN`'s value.** A failed case — and it fails naturally, because no
   case may expect it.

**Exit values outside the map** — `1` (sanitizer fired, D-035), `134` (`abort()`, notes-17 §5) — are
also failed cases. How much the report explains them ("got 1 (sanitizer)") is open.

---

## 4. The map: read the header

**Chosen: the driver reads `src/exit_codes.h`** and builds the name → value map itself. X-macros
were ruled out.

It is a handful of steps in Python. The edge cases are tests for the parser, not reasons against it:

```
names = {}
next_value = 0                              # C: first implicit enumerator is 0
for each line in header:
    strip comments first                    # `KRM_EXIT_ERR_INPUT,  // path is wrong`
    if line does not start with KRM_EXIT_:  # not KRM_EXIT_ERR_ — SUCCESS would be lost
        continue
    split name / optional "= value"; drop trailing comma
    if a value is present but is not a plain integer:
        FAIL LOUDLY                         # never skip a half-understood line
    value = explicit value, else next_value
    names[name] = value
    next_value = value + 1
self-check: KRM_EXIT_SUCCESS == 0 and KRM_EXIT_ERR_USAGE == 2    # the values D-035 fixes
```

- **The filter is `KRM_EXIT_`, not `KRM_EXIT_ERR_`** — success is spelled `KRM_EXIT_SUCCESS`.
- **The self-check** is the cheap guard: a misread header fails before any case gets a wrong verdict.
- **Implicit values** are the real work — only `SUCCESS` and `USAGE` carry explicit values today.
- D-037 rejected hand-rolled *TOML* for silent misreads. The answer here is strictness: one known
  pattern, and anything matching the prefix but not the pattern is an error.

**What it changes.** The header comment and D-035's consequences both say the driver *duplicates*
the numbers. That stops being true: the header comment is rewritten, and the change is owed a
DECISIONS entry (§9).

---

## 5. Program-side metadata — parked

The idea: under a test macro, `kramac` writes a metadata block the harness can read. Parked, for
three reasons:

- **Wrong build.** The harness runs `kramac-san`, which is **not** built with `-DKRAMA_TEST_SUITE`
  (notes-16 §2). It would need a fourth build or a redefinition of the macro.
- **No channel for it.** C has only `stdin`, `stdout`, `stderr` — no `stdlog`. On stderr, stderr is
  never empty, and the non-empty assertion passes vacuously.
- **Nothing to say.** The exit code already carries the class; a second channel saying the same
  thing can only disagree with it.

The richer failure information it reached for already has homes: the diagnostic sink for the user's
program (D-033), and `__FILE__` / `__LINE__` on the assertion side for kramac's own bugs
(notes-17 §4). Neither is something the harness asserts on.

---

## 6. `stderr`

Two states, **both checked**:

- `true` — stderr must be **non-empty**. An error was reported.
- `false` — stderr must be **empty**. A success that writes to stderr is wrong.

No "don't care" state. It survives warnings: a valid program with a warning is `KRM_EXIT_SUCCESS`
with `stderr = true`. Wording is never asserted (notes-13 §3).

---

## 7. `expected`

### 7.1 Three states

| Value | Meaning | Driver |
|---|---|---|
| `"#derive"` | the `.krm` in `args`, with `.krm` swapped for `.expected` | compare stdout against it |
| `"<path>.expected"` | an explicit file — e.g. a future `--version` case with no `.krm` | compare stdout against it |
| `"#none"` | there is no expected file | no stdout comparison |

**Derivation is the pairing convention**, not inference: spec §11.1 and D-032 already make fixtures
`.krm` / `.expected` pairs, and differently named halves would be hard to match by eye. Sandler's
suite works the same way where it can (`foo.c` → `foo`).

### 7.2 The `#` commands

A value beginning with `#` is a **command**, not a path:

```
if value starts with "#":
    must be "#derive" or "#none", else manifest error
else:
    must end in ".expected", else manifest error
```

- **Why a sigil.** On POSIX only `/` and NUL are illegal in a filename, so no symbol is guaranteed
  to be a non-path. A fixed prefix gives the driver a reliable pattern; the cost is that no fixture
  path may begin with `#`, which is effectively free.
- **Strict prefix.** An unknown command (`#derve`) is an error, never a path.
- **The TOML trap.** Unquoted, `#` starts a comment: `expected = #none` loses the rest of the line
  and the key vanishes. The all-keys-required check (§2) reports it as missing.
- **"For now."** A new command is a deliberate extension; until then anything else is rejected.

### 7.3 Rules

- **`#derive` needs exactly one `.krm` in `args`.** Zero or more than one is a validation error; such
  a case must give an explicit path.
- **`#none` is "no expected file"**, read with the key name. `#skip` / `#ignore` were considered and
  rejected: they say a file might exist and is being disregarded, which is the wrong claim.

### 7.4 stdout is lenient

**stderr is the error channel and is always checked, both ways. stdout is checked only against an
`.expected` file;** without one, the exit status is the claim.

- **Cost, accepted:** an error path that starts writing junk to stdout goes unnoticed by any `#none`
  case. stdout content gets real coverage once `.expected` files arrive; a lenient check must not be
  read later as a strict one.
- **Consequence:** `main`'s file dump does not block this branch. The valid-file cases are `#none`
  and pass with or without it. Removing it stays with the lexer (notes-16 §7) unless done
  deliberately as cleanup.

---

## 8. Sample cases

Illustrative — not the real fixtures. Paths resolve relative to the manifest (D-036).

```toml
# 1. Valid file, no expected file: exit status and empty stderr are the check
[[case]]
name        = "valid_file"
args        = ["hello.krm"]
return_code = "KRM_EXIT_SUCCESS"
stderr      = false
expected    = "#none"

# 2. Valid file, expected file derived: stdout compared against hello.expected
[[case]]
name        = "valid_file_dump"
args        = ["hello.krm"]
return_code = "KRM_EXIT_SUCCESS"
stderr      = false
expected    = "#derive"

# 3. Invalid input, no expected file: path does not exist
[[case]]
name        = "missing_path"
args        = ["does_not_exist.krm"]
return_code = "KRM_EXIT_ERR_INPUT"
stderr      = true
expected    = "#none"

# 4. Invalid program, expected file derived (see below)
[[case]]
name        = "stray_byte"
args        = ["stray_byte.krm"]
return_code = "KRM_EXIT_ERR_<LEX>"   # placeholder: this class does not exist yet
stderr      = true
expected    = "#derive"
```

- **Case 2, explicit form:** `expected = "hello.expected"` — valid only because it ends in
  `.expected`.
- **Case 3** is a real case on this branch. `#derive` would be wrong here: the path has no pair on
  disk.
- **Case 4 is a question, not a settled example.** The class is a placeholder — D-035 adds a
  compile-error class only when there is something to reject. And under D-034 the lexer stops at the
  first error and reports on stderr: does an error run write *anything* to stdout (tokens scanned
  before the bad byte)? If not, case 4 is really `#none`, and invalid-with-expected has no use until
  a partial dump exists.

---

## 9. Queued records, open, part 2

### Queued

- **DECISIONS entry — the driver reads `exit_codes.h`.** Supersedes D-035's consequence that the
  numbers are duplicated in the driver. Rejected: a hand-kept map (drifts); X-macros; program-side
  metadata (§5). The header comment is rewritten in the same commit.
- **Manifest keys recorded** — §2's table and §7's rules, where D-036 said they would be settled
  "when the driver is written".

### Open

- How the report renders exit values outside the map (`1`, `134`).
- What an invalid program's `.expected` would contain, if anything (§8 case 4).
- Whether `main`'s dump is removed on this branch as cleanup or with the lexer.

### Part 2 — next session

- **Should validation check that a derived or explicit `.expected` file exists?** Leaning yes: a
  missing expected file is a manifest problem, not a test failure.
- Then the driver's pseudocode (notes-17 §8).

---

## 10. Conclusions — what we are going ahead with

1. **`[[case]]` array of tables**, every case named, none numbered.
2. **Five keys, all required;** required set = allowed set, so validation is one equality check.
   No defaults table.
3. **`return_code` holds full `KRM_EXIT_*` names.** Asserting `UNKNOWN` is a manifest error; exiting
   with it is a failed case.
4. **The driver parses `exit_codes.h`** — filter on `KRM_EXIT_`, derive implicit values, fail loudly
   on anything unrecognised, self-check `SUCCESS == 0` and `USAGE == 2`. Owed a DECISIONS entry.
5. **Program-side metadata parked;** X-macros out.
6. **`stderr` is strict both ways:** `true` non-empty, `false` empty.
7. **`expected` is `#derive`, `#none`, or a path ending `.expected`;** `#` marks a command, unknown
   commands are errors, `#derive` needs exactly one `.krm`.
8. **stdout is lenient:** compared only against an expected file; otherwise the exit status is the
   check.
