# Krama — Sandler's test harness, and what Krama's must do differently

**Date:** 2026-09-23
**Status:** Discussion record. **Not normative.** No decision taken; every gap named in §7 is parked.

> Nothing here is binding. Where this and `DECISIONS.md` / `krama-spec-v1.md` disagree, those win.

---

## 1. How to read this

- **§2** — how Nora Sandler's suite actually works.
- **§3** — stages, and the thing most people miss about them.
- **§4** — what carries over to Krama and what does not.
- **§5** — the dump format as a contract.
- **§6** — spans: why the byte offset earns its place in a dump.
- **§7** — the spec gaps this session surfaced. Parked, not decided.
- **§8** — settled, owed, open.

---

## 2. The Sandler suite, as it is

`test_compiler` is a Python script pointed at *your* compiler binary. Per chapter:

1. It runs your compiler over each `.c` file in that chapter's `valid/` and `invalid/` trees.
   `--chapter N` selects the chapter; earlier chapters run too unless `--latest-only` is given.
2. **Invalid programs** must make the compiler exit **non-zero**. The message text is not graded —
   only the exit status.
3. **Valid programs** must make the compiler exit **zero**. In a full run it then assembles, links,
   runs the produced binary, and compares against `expected_results.json`: the **exit code**, and in
   later chapters **stdout** as well.
4. The executable is expected next to the source with `.c` stripped — `foo.c` produces `foo`.

Exit codes are the host's, so a program's `return` value is truncated to 0–255. `return 256;` is
observed as `0`.

---

## 3. Stages — the part that is easy to miss

The script can stop the compiler partway with `--stage lex`, `--stage parse`, `--stage codegen`,
passing the matching flag through to the compiler. At those stages:

- No binary is produced and none is expected.
- A valid program is judged **only** on the compiler exiting `0`.
- An invalid program is only expected to fail at or after the stage its error belongs to. The
  invalid trees are grouped for this — `invalid_lex`, `invalid_parse`, and so on.

**The worked example.** `int main(void) { return 2 }` — missing semicolon — is expected to *pass*
under `--stage lex`. Every character in it is a valid token; the `;` is a demand of the grammar, and
the grammar is the parser's business. Nothing about the lexer's job is violated.

**The consequence.** At a stage boundary the suite treats the compiler as a black box: exit status
only, never *which* tokens were produced. A lexer emitting complete garbage while exiting cleanly
passes `--stage lex`.

---

## 4. What Krama's harness keeps, and what it changes

| Aspect | Sandler | Krama |
|---|---|---|
| End-to-end check | exit code + stdout | compile generated C with `cc`, run, diff stdout — spec §11.1 |
| Stage boundary check | compiler exit status only | **token/AST content compared against an expected dump** |
| Failure cases | non-zero exit | exit code **and stderr** — spec §11.1, trap tests |
| Fixtures | `.c` trees + `expected_results.json` | `.krm` + `.expected` pairs, shell driver, zero deps |

The one structural change is the stage boundary. Opening the black box — "given this program, the
tokens are exactly these" — is a strictly stronger claim than "the process exited 0", and it is what
makes a lexer bug visible at the lexer rather than three phases downstream.

Everything else is the same shape as spec §11.1 already describes; the stage pairs are an extra axis
on the existing `.krm` / `.expected` layout, not a different scheme.

---

## 5. The dump is a contract

Spec §11.1 rejects golden C-text comparison because it "breaks on whitespace and formatting choices
that carry no semantic weight." A token dump is golden text comparison — so it only escapes that
objection if the format is deliberately boring and stable. One token per line, something on the
order of:

```
KIND  lexeme  line:col
```

Properties worth keeping:

- **Plain `diff` reads it.** No dependency, consistent with §11.1.
- **Line-oriented.** A single wrong token is a one-line diff, not a reflowed blob.
- **It tests D-005 for free** — spans on every token become observable rather than asserted.

The cost, accepted knowingly: the format is now an interface. Changing a field order or a separator
invalidates every `.expected` file at once.

---

## 6. Spans in the dump

**The correction that mattered.** The byte offset is *not* the column, even in pure ASCII. The
offset counts from the start of the **file**; the column resets at every newline. For

```
fn main(): i32 {
    return 2;
}
```

`return` is at column 5 of line 2 but byte offset 21 — the 17 bytes of line 1 including its `\n`,
plus four spaces. They coincide only on line 1, and even there they differ by one if columns are
1-based and offsets 0-based.

**Where encoding actually bites.** The offset is unambiguous under any encoding: it counts bytes.
The **column** is the ambiguous one — bytes, code points, or display columns, and how wide a tab is.
Nor are Krama sources strictly ASCII even in v1: §3.2 lets a comment contain any character up to the
newline, so `# café` is already legal input.

**Why carry the offset anyway**, given it is derivable from `line:col` plus the file: the dump is
not checking arithmetic, it is checking **bookkeeping**. The lexer maintains the three counters
independently, and independent counters drift. Printing all three makes the drift a diff.

Concrete drift cases, all from §3.2:

- **`\r\n`.** Counting both bytes as newlines doubles every line number while offsets stay right.
  Counting neither collapses a Windows file onto line 1. Treating `\r` as plain whitespace bumps the
  column just before the newline resets it — harmless until a span covers it.
- **Tabs.** One byte, but 2/4/8 display columns depending on the editor. Clang reports byte columns;
  GCC exposes the choice via `-fdiagnostics-column-unit`. Both defensible; Krama must pick one.
- **The comment's trailing newline.** `comment = "#" { any_char_except_newline } newline ;`
  *consumes* the newline. If comment scanning eats it without bumping the line counter — because
  line counting lives in the whitespace branch — every line after a comment is off by one while the
  offset remains correct. Precisely the bug the offset column exposes.

---

## 7. Spec gaps surfaced — parked

None of these were decided. Each is owed a `D-` entry before the lexer is written, not now.

1. **No stage/dump flag exists.** Nothing in spec v1 says how to ask the compiler to stop after
   lexing and dump its tokens — `--dump-tokens`, `--stage lex`, or otherwise. The test layout
   depends on the answer.
2. **Column unit and base undefined.** §3.3 mandates line, column and byte offset on every token;
   it never says whether a column counts bytes, code points or display columns, nor whether it
   starts at 0 or 1. Tab width is the same question.
3. **Newline definition undefined.** §3.2 lists `\r` and `\n` as whitespace without saying what
   constitutes a line break — `\n` only, `\r\n` as a unit, or a lone `\r` as well.
4. **Comment at EOF.** The §3.2 rule *requires* a trailing newline, so a file ending `# done` with
   no final newline is, read strictly, a lexical error. Almost certainly unintended.

---

## 8. Settled, owed, open

**Settled (understanding, not decisions).**

- How the Sandler harness works, including stage semantics and the black-box property at stage
  boundaries.
- That Krama's harness keeps the end-to-end gate of §11.1 and adds content comparison at each stage.
- That a dump format is an interface, and the cost of that.
- That the byte offset is file-relative, that encoding ambiguity lands on the column rather than the
  offset, and that carrying all three counters is what makes bookkeeping drift observable.

**Owed.** Four `D-` entries, §7 items 1–4, due before the lexer.

**Open.** The dump's exact field set and separators. Whether stage fixtures live beside the
end-to-end `.krm` / `.expected` pairs or in a parallel tree. Whether the driver grows stage support
before or alongside the `make test` runner that `PROJECT.md` currently lists as deliberately
incomplete.

**Not in scope of this session.** Any change to `krama-spec-v1.md`. Current milestone-1 order is
unchanged: arena init in `main`, then AST node definitions, then the diagnostic sink, then the lexer.
