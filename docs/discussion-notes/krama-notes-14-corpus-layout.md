# Krama — Corpus layout: the manifest, the driver, and per-stage files

**Date:** 2026-09-23
**Status:** Discussion record. **Not normative.** §7 lists what is queued for `DECISIONS.md` and for
`krama-spec-v1.md`.

> Nothing here is binding. Where this and `DECISIONS.md` / `STYLE.md` / `krama-spec-v1.md`
> disagree, those win.

Continues `krama-notes-13-argv-exit-codes.md`, which left the corpus layout open. Companion to
`krama-notes-12-test-harness.md` (Sandler's suite) and `krama-notes-11-lexer-design.md`.

---

## 1. How to read this

- **§2** — what belongs in a committed case file, and what does not.
- **§3** — why a table at all: the cases with no input file.
- **§4** — TSV considered, TOML chosen; the Python floor.
- **§5** — per-stage manifests and the walk.
- **§6** — fail-fast versus selection; the driver as a development tool.
- **§7** — queued records, open.
- **§8** — conclusions.

---

## 2. Inputs are not results

The first sketch was a CSV with `Step, Desc, Expected, Actual, Pass/Fail`. Three problems, each
worth keeping in mind for any future table:

- **`Actual` and `Pass/Fail` are results, not inputs.** In a committed file they force the driver to
  rewrite what it just read: every run dirties the working tree, `git status` fills with noise, and
  a clean-tree gate can never pass. Results go to stdout (or a gitignored report); the committed
  file holds inputs only.
- **`Step` implies an order the cases do not have.** They are independent and all of them run.
  Numbered identity also rots when a case is inserted or deleted — the same argument that killed the
  branch counter (notes-11 §7). Cases are named.
- **`Desc` is redundant** when the name carries it.

What survives is what the driver actually needs: **name, invocation, expected exit, stderr
expectation** — and later, a path to an expected dump.

---

## 3. Why a table at all

The finding that shaped everything else: **three of this branch's six cases have no input file.**
"No argument", "two arguments" and "a directory as the argument" are defined entirely by their
invocation. A pairs-on-disk layout has nowhere to put them — there is no `.krm` to pair with. A
table has a natural slot, because the invocation *is* the case.

**The pairs exist either way.** An expected token dump is multi-line text, so it can never live in
a cell; the column can only hold a *path*. So the real question was never "table or pairs" but
**"is the pairing explicit or by convention?"**

| | Explicit (manifest row) | Convention (directory) |
|---|---|---|
| Content cases | One near-identical row per case; the name repeated in three columns | Adding a case is adding files; no manifest edit, no merge conflict |
| Irregular cases | Natural — every row differs | No place for a case with no input file |
| Invocation | Written down, greppable | Inferred from the path |

**Decided: one mechanism, explicit.** A manifest covers every case. The deciding argument was not
verbosity but **inference**: tying behaviour to a folder name means renaming the folder changes what
the tests do, and a change to either the folder name or an option name then touches the other.

**Important distinction.** What was rejected is the invocation being *inferred*, not the invocation
being *shared*. A defaults or group table that many cases inherit is still explicit and greppable,
and TOML's arrays of tables make that natural.

**Expected dumps stay as sibling files on disk.** TOML multi-line strings could hold a dump inline;
deliberately not done. Inline dumps make a wrong token a diff in the manifest instead of a one-line
diff in a small file, grow the manifest to thousands of lines, and contradict spec §11.1's `.krm` +
`.expected` pairs. **The manifest names paths.**

---

## 4. Format and language

### 4.1 Python over shell

Spec §11.1 says "Zero dependencies. A `tests/` directory of `.krm` + `.expected` pairs and a shell
driver." The driver moves to **Python**, on the author's fluency: a driver that cannot be maintained
is worse than a dependency. Conditions:

- **Standard library only.** The constraint §11.1 was protecting against is `pip install`, not the
  interpreter.
- **Invoked as `python3`**, explicitly, from the Makefile — the bare `python` alias is the
  version-drift trap.
- **A build never needs Python.** `all` must work with no interpreter present; the version check
  lives in the driver or the `test` target, never the default path. The gcc-14 / C23 floor is a
  *build* requirement; this is a *test* requirement, and they should not merge.

### 4.2 TSV considered

A tab-separated manifest was the first shape, and remains the fallback:

```
# name          args                    exit     stderr
no_args         -                       USAGE    nonempty
extra_args      a.krm b.krm             USAGE    nonempty
missing_file    tests/does_not_exist    INPUT    nonempty
dir_as_file     tests                   INPUT    nonempty
valid_file      tests/cases/hello.krm   OK       -
empty_file      tests/cases/empty.krm   OK       -
```

CSV was rejected outright: real CSV has quoting rules, embedded commas and embedded newlines, and
shell has no parser for it (`IFS=,` breaks on the first quoted field). TSV parses in one line of
either language. Its weakness is `args` as a single field, which the shell word-splits, so no
fixture path may contain a space.

### 4.3 TOML chosen

Two things it buys over TSV:

- **`args` as an array of strings** — the word-splitting problem disappears, paths with spaces work,
  and `subprocess.run` takes the list directly.
- **Structure and comments**, which JSON cannot give.

Costs, accepted:

- **A Python 3.11 floor.** `tomllib` is standard library only from 3.11 (October 2022); Ubuntu
  22.04's 3.10 has none. Accepted as consistent with the existing gcc-14 / C23 floor — backward
  portability was already left behind. The driver should **check the version at start** and fail
  with a clear message rather than a traceback on `import tomllib`.
- **Validation is now the driver's job.** TSV's fixed column count made a malformed row obvious.
  With named keys, a typo like `stdderr` is silently ignored and the case asserts nothing. So the
  driver **rejects unknown keys and requires the required ones**. This is the concrete form of the
  "understanding between the file and the driver"; roughly ten lines, and what stops a fixture
  passing vacuously.

### 4.4 Columns, whatever the format

- **Symbolic exit names** (`OK`, `USAGE`, `INPUT`), not numbers. This fixes the duplication trap
  from notes-13 §7: the header's numbers then appear in exactly one other place — a small mapping in
  the driver — instead of in every case.
- **An explicit "nothing" marker** rather than an empty field, so a blank never means "I forgot".
- **`stderr` is `nonempty` or nothing, never the text** (notes-13 §3).
- **No `stdout` assertion this branch.** It arrives with the lexer, as a path to a sibling
  `.expected`.

---

## 5. Per-stage manifests

**One manifest per stage** (`tests/argv/…`, `tests/lex/…`), not one for the suite.

- A targeted run loads one small file; a full run is an `os.walk` of `tests/` executing every case.
  Many small parses are not a cost worth optimising against the subprocess work.
- **Discovery needs no registry.** A stage directory with its own manifest is picked up by the walk.
  Adding a stage is adding a directory.

Three details:

- **Sort the walk.** `os.walk` order is not guaranteed across filesystems; unsorted output makes
  diffs jump around and makes runs hard to compare between commits.
- **Resolve paths relative to the manifest**, not the repo root, and keep the manifest inside its
  stage directory. Then moving or renaming a stage is a rename with no edits inside.
- **Report as `stage/case`.** Names need only be unique within their manifest, and the prefix says
  where to look.

---

## 6. Fail-fast, selection, and the driver as a tool

"Fail fast" means different things at two levels, and they were settled differently:

| Level | Behaviour | Why |
|---|---|---|
| The gate | Stops at the first failing stage | Make stops at the first non-zero recipe line (notes-09 §9) |
| The driver | Runs **every** case, reports `N / Total passed`, exits non-zero if any failed | Stopping at the first failure hides the rest and turns one debugging session into twenty |

What the author wanted during development is **selection**, not fail-fast: while writing the lexer,
neighbouring cases do not matter; pre-commit, everything runs and everything is listed. So the
driver takes optional filters — a stage, or a case name — with the full walk as the default that
`make check` invokes.

- **A filter never changes the verdict:** non-zero if any *selected* case failed, and the report
  says what ran (`3 / 3 passed (lex)` versus `3 / 3 passed`), so a filtered run cannot be mistaken
  for a clean full one.
- **A filter matching nothing is an error**, not a pass. `0 / 0 passed` exiting `0` on a typo'd name
  is exactly the vacuous success to design out.

**Consequence worth noting:** the driver is now a development tool, not only a gate. That raises the
bar on its output — a failure line must carry the case name, what was expected and what happened,
without a manual re-run to find out.

---

## 7. Queued records, open

### Queued (adds to notes-11 §9 and notes-13 §8)

6. **`krama-spec-v1.md` §11.1 amendment** — two points: the driver is Python, not shell; "zero
   dependencies" restated as *standard library only, no third-party packages*. Largest queued record
   so far, and the first spec change.
7. **DECISIONS entry — corpus layout.** One TOML manifest per stage; invocation explicit per case;
   expected dumps as sibling files; symbolic exit names; driver validates keys.
8. **Toolchain floors recorded together** — gcc 14 / C23 for builds, Python 3.11 for tests, with the
   note that `all` must not need Python.

### Open

- The manifest's exact key names and the defaults/group table shape.
- Where the argv cases' directory sits (`tests/argv/`?) and what the empty-file and hello fixtures
  are called.
- **Next:** the branch's commit plan.

---

## 8. Conclusions — what we are going ahead with

1. **One mechanism:** a TOML manifest per stage, covering invocation-defined and content cases
   alike. Committed files hold **inputs only**.
2. **Invocation explicit per case**, never inferred from a directory name; shared settings may be
   inherited from an explicit defaults or group table.
3. **Expected dumps stay as sibling `.expected` files**; the manifest names paths.
4. **Cases are named, not numbered**; no ordering implied.
5. **Symbolic exit names** in manifests; the numeric mapping lives only in the exit-code header and
   one place in the driver.
6. **stderr asserted as non-empty**, never by wording.
7. **Driver in Python 3.11**, standard library only, invoked as `python3`, version checked at start;
   `all` never requires it.
8. **Driver validates** required and unknown keys, so no case can pass vacuously.
9. **Full run is a sorted `os.walk`** of `tests/`; paths resolve relative to the manifest; failures
   report as `stage/case`.
10. **Driver runs every selected case** and reports `N / Total passed`; filters select, never
    change the verdict; a filter matching nothing is an error.
11. **Spec §11.1 is amended** for the driver language and the dependency wording.
