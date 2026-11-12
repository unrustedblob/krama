# Regular expressions: what each operator means, and how a match is found — packet

**Ledger:** `regex` / `regex` · score at write-up **5** · queued 2026-11-08, packet written 2026-11-13
**Branch:** `learn/regex/regex` (cut when the packet opens, not before)
**Surfaced by:** `m1/cli-args` (`krama-notes-20` §6), and again by `tool/commit-hook` step 4
**Scope:** reading + Q&A — no lab, no code
**Evidence at write-up:** walked COMMITS.md's ordinary-commit pattern piece by piece, correctly and
unprompted: anchors, the alternation group, `[a-z]` as a class, `+` and `?` as quantifiers, `\(` as
an escape. Then, on `fix(lex,parse,ast): x`, read `(,[a-z]+)?` as matching `,parse,ast` — `?` taken
as a repeat — and concluded it matched. Corrected with one prompt, then said `+` would give "any
number", which is `*`. Same family as `m1/cli-args`: an operator's meaning read from context rather
than from its definition. Has not yet said how a match fails, or chosen between `match`, `search`
and `fullmatch`.

---

## Brief

A regex looks like a description of the strings it accepts, and is read that way — which is how
`?` becomes "some more of these" and a missing `$` goes unnoticed. It is better read as a small
program: each operator does one exact thing, and the engine tries the alternatives in order,
backing up when one fails. The commit hook's step 4 checks subjects against three patterns; every
error message it prints depends on knowing why a pattern failed, not only that it did.

## Guide set

The aim, phrased as questions. Answer them in your own words in `notes.md` as the reading answers
them.

1. What do `?`, `*`, `+` and `{m,n}` each mean, exactly, in terms of how many times the thing before
   them may match? What *is* "the thing before them" — a character, a class, a group?
2. What separates a character class `[…]` from a group `(…)`? Which characters change meaning inside
   a class, and why does `+` inside `[a-z+]` mean a literal plus?
3. When part of a pattern fails, what does the engine do before it gives up? Describe backtracking
   on one concrete example.
4. What do `^` and `$` anchor to, and how do they interact with Python's `match`, `search` and
   `fullmatch`? Is there any string that `$` accepts that you would not expect?

## Reading

Read in this order. Each is free. Reachability was confirmed through search on 2026-11-13; the
pages could not be fetched from the session that wrote this packet, so if a link has moved, search
its title.

- **Python documentation, "Regular expression HOWTO"** —
  <https://docs.python.org/3/howto/regex.html>.
  The introduction written for the module the hook uses. Read *Simple Patterns* (metacharacters,
  repeating things) and *Using Regular Expressions* (`match`, `search`, `fullmatch`, match objects)
  first. Guide questions 1, 2 and 4.
- **Jan Goyvaerts, regular-expressions.info, "Repetition with Star and Plus"** —
  <https://regular-expressions.info/repeat.html>.
  Walks through what the engine does, step by step, as a quantified token matches and then gives
  characters back. The clearest free account of backtracking at this level. Guide question 3.
- **Python documentation, `re` module reference** — <https://docs.python.org/3/library/re.html>.
  Reference, not reading: the *Regular Expression Syntax* section for the exact definition of each
  operator, and the entries for `$`, `match`, `search` and `fullmatch`. Use it to check every answer
  to guide question 4 against the definition.

---

## Q&A

Answer each in `notes.md` **before** running anything. Then check with `python3 -c` or the REPL and
record whether the prediction held. A wrong prediction is the most useful line in the notes.

1. For each subject, say whether COMMITS.md's ordinary-commit pattern matches it, and if not, the
   first point where it fails:
   `fix(lex,parse): x` · `fix(lex,): x` · `fix(): x` · `fix(Lex): x` · `fixes: x` · `fix:x`
2. Rewrite only the scope part so that **any** number of comma-separated scopes is accepted. Then
   rewrite it so that **at least two** are required. Which single character changed each time?
3. `[a-z+]` and `[a-z]+`: give one string each accepts that the other rejects.
4. Using the same pattern with `^` and `$` removed, predict `match`, `search` and `fullmatch` on
   `"chore: x"` and on `"wip chore: x"`. Then put the anchors back and predict again.
5. With the anchors in, does `re.match` accept `"fix: x\n"` — a trailing newline? Find the sentence in
   the `re` reference that explains the answer, and say what it means for a hook that reads lines
   from a file.

---

## Bring back to the chat

1. Your answers to the Q&A, with every wrong prediction left in.
2. Guide question 3, explained on `fix(lex,parse,ast): x` — every point where the engine backs up,
   in order, until it gives up.
3. Which of `match`, `search` or `fullmatch` the hook should use, and why — in one paragraph that
   would survive someone removing the `$` from the pattern.

The check set is held back until the notes are reviewed (CLAUDE.md §8.5).
