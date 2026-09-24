# Krama — The commit plan for the argv branch, and the ordering rule that fixed it

**Date:** 2026-09-23
**Status:** Discussion record. **Not normative.** §6 carries the queued records forward.

> Nothing here is binding. Where this and `DECISIONS.md` / `STYLE.md` / `COMMITS.md` /
> `krama-spec-v1.md` disagree, those win.

Closes the branch design begun in `krama-notes-13-argv-exit-codes.md` and continued in
`krama-notes-14-corpus-layout.md`.

---

## 1. How to read this

- **§2** — the first draft of the plan and why its order fails.
- **§3** — the catch-22, and which untested window to accept.
- **§4** — the settled sequence.
- **§5** — how commit 4 declares its gap.
- **§6** — queued records, open.
- **§7** — conclusions.

---

## 2. The first draft, and the rule it broke

Draft order: docs → test driver → Makefile → `main`. The reasoning was sound on its face: `main` is
small, and putting it last means everything before it is already in place to test it.

**What it collides with.** COMMITS.md Rule 1: *every commit on `main` builds and passes tests*,
enforced by `rebase --exec "make check"` at the merge gate — and it walks into **each logical
commit**, not just the merge. A driver landing before `main`'s argument handling asserts behaviour
that does not exist yet, so those commits fail the gate, the rebase stops there, and the branch
cannot merge as sequenced.

"Tests before the thing they test" is a good instinct that the per-commit gate makes impossible:
**red until the last commit is not an option.**

**Two dependencies the draft also hid:**

- **The exit-code header had no home.** The driver needs the symbolic names (`USAGE`, `INPUT`)
  mapped to numbers (notes-14 §4.4), so the header must exist *before or with* the driver, not
  inside the `main` commit at the end.
- **The Makefile splits in two.** `-Isrc`, `-Itests` (test build only), the sanitized `kramac`
  target and folding `all` into `check` can land at any point. **Wiring the harness into `check`
  cannot** land before the driver exists, or `check` fails on a missing file.

---

## 3. The catch-22

*"I can't test `main` without the test driver"* — and the half that was missed: **the driver can't
be tested without `main`.** Something has to be first, so the real choice is which untested window
to accept:

| First | Cost |
|---|---|
| Driver first | A tested-looking driver with no subject — and the driver's own correctness is what everything downstream rests on |
| `main` first | One commit where `main`'s new behaviour has no harness |

**`main` first is the lesser evil.** The window is one commit wide and closes in the next.

It is also not a gap in coverage the way it sounds: at commit 4 the gate still runs `fmt_check`, the
unit tests, and compilation of `main.c`. **Nothing is skipped or stubbed** — the harness simply does
not exist yet. That is a different thing from a disabled test.

And the branch merges only after 5, so **`main` never sees the gap as its tip**. Bisect can land on
commit 4 and finds a green tree there, which is all Rule 1 asks.

---

## 4. The settled sequence

| # | Commit | Why here |
|---|---|---|
| 1 | **Docs** — PROJECT.md, the queued DECISIONS entries, the spec §11.1 amendment | No code; the starting point. Largest single piece of writing on the branch |
| 2 | **`chore`** — rearrange the existing unit tests, if the layout needs it | Pure moves, no behaviour change, kept separate so the next diff is readable |
| 3 | **Makefile, part A** — `-Isrc`, `-Itests` on the test build only, sanitized `kramac` target, `all` folded into `check` | `main.c` compiles as it stands, so this is green |
| 4 | **Exit-code header + `main`'s argument handling** | The behaviour. The one-commit window (§3) |
| 5 | **Driver + argv manifest + fixtures + Makefile part B** (harness wired into `check`) | Closes the window; commit 4 is now under test |

Also settled: commits 4 and 5 stay **separate**. Merging them removes the window but produces a
diff large enough to hurt review, and loses the place to state the gap explicitly.

---

## 5. How commit 4 declares its gap

**In the commit body, not PROJECT.md's *Deliberately incomplete* table.** A row added in 4 and
deleted in 5 is churn in the record; the table is for state that persists on `main`. The commit body
is where someone bisecting onto that commit will look anyway.

Wording should distinguish "the harness does not exist yet" from "a test is disabled" — the gate at
that commit is honest, not weakened.

---

## 6. Queued records, open

### Queued (carried from notes-11 §9, notes-13 §8, notes-14 §7) — all land in commit 1

1. **STYLE.md §13** — sink trigger moves to "during the lexer stage, before it merges" (+ version
   bump).
2. **PROJECT.md** — *Next up* reordered, lexer first; hardcoded-path row retargeted; the untested
   dump in `main` recorded as deliberately incomplete.
3. **DECISIONS** — stop at the first lexical error; *Revisit when:* the first fixture needing two
   errors reported.
4. **The add-an-error procedure** — written *after* `LEX001` exists, so not commit 1.
5. **DECISIONS** — exit-code scheme: own enum, `KRM_EXIT_` prefix, `1` left unused (sanitizer
   collision), append-only.
6. **`krama-spec-v1.md` §11.1 amendment** — Python driver, not shell; "zero dependencies" restated
   as standard library only. **The first change to the spec.**
7. **DECISIONS** — corpus layout: one TOML manifest per stage, invocation explicit per case,
   expected dumps as sibling files, symbolic exit names, driver validates keys.
8. **Toolchain floors recorded together** — gcc 14 / C23 for builds, Python 3.11 for tests, with
   `all` never requiring Python.

### Open

- The manifest's exact key names and the defaults/group table shape.
- Where the argv cases live (`tests/argv/`?), and the fixture names — answered in commit 5, and it
  may require the commit 2 rearrangement.
- Whether the unit tests need rearranging at all.

---

## 7. Conclusions — what we are going ahead with

1. **Order is forced by COMMITS.md Rule 1**: every commit builds and passes `check`, including
   logical commits inside a merge. Tests cannot precede the behaviour they assert.
2. **`main` lands before the driver.** One commit where the new behaviour has no harness, accepted
   knowingly as the lesser evil.
3. **The sequence:** docs → unit-test moves → Makefile part A → exit-code header and `main` →
   driver, manifest, fixtures and Makefile part B.
4. **The Makefile splits**, because wiring `check` to a driver that does not exist would fail the
   gate.
5. **The exit-code header lands with `main`**, before the driver that needs its names.
6. **Commits 4 and 5 stay separate**; the gap is declared in commit 4's body, not in PROJECT.md.
7. **Commit 1 is next** — eight queued records, including the first spec amendment.
