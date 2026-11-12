# Character encoding: encode, decode, and who chooses — packet

**Ledger:** `text` / `character-encoding` · score at queue **6** · queued 2026-11-12
**Branch:** `learn/text/character-encoding` (cut when the packet opens, not before)
**Surfaced by:** `tool/commit-hook`, step 3 — whether `open()` in the `commit-msg` hook should state
`encoding="utf-8"`
**Scope:** reading — no lab, no code
**Evidence at queue:** asked unprompted what happens when `open()` declares one encoding and the file
on disk holds another, which shows decoding understood as an interpretation chosen by the reader,
not a property the bytes carry. Did not know git writes commit messages as UTF-8 regardless of
locale (a fact about git, not the concept). Saw that `LANG=en_IN` resolved to UTF-8 only by asking
Python. Has not yet compared the two failure modes of a mismatch — a strict decoder failing loudly
versus a single-byte decoder failing silently — which is what separates 6 from 7.

---

## Brief

Text on disk is bytes. Turning bytes into characters (decoding) needs an encoding, and nothing in the
bytes says which one. Whoever reads chooses, and whoever wrote chose. The hook met this from three
sides at once: git chose UTF-8 when writing the message file, the locale chose an encoding for
Python's `open()` unless told otherwise, and `len()` counted characters rather than bytes. This
packet is about the model underneath that — and about the difference between a mismatch that
crashes and one that quietly produces wrong text.

## Guide set

The aim, phrased as questions. Answer them in your own words in `notes.md` as the reading answers
them.

1. What is the difference between a character, a code point, and a byte? Which one does Python's
   `len()` count on a `str`, and which on a `bytes`?
2. When text is written and read back, who chooses the encoding at each end, and where is that
   choice recorded — if anywhere?
3. Take UTF-8 bytes read as Latin-1, and Latin-1 bytes read as UTF-8. Which one raises an error and
   which one silently produces wrong text? Why is the asymmetry built into the two encodings?
4. Python's `errors=` parameter offers `strict`, `replace` and `ignore`. For a git hook, which is
   right, and what would each of the other two hide?

## Reading

Read in this order. Each is free and was checked reachable on 2026-11-12.

- **Joel Spolsky, "The Absolute Minimum Every Software Developer Absolutely, Positively Must Know
  About Unicode and Character Sets (No Excuses!)"** (2003) —
  <https://www.joelonsoftware.com/2003/10/08/the-absolute-minimum-every-software-developer-absolutely-positively-must-know-about-unicode-and-character-sets-no-excuses/>.
  The classic short introduction: why "plain text" doesn't exist and why every string needs its
  encoding known. Read it first for the history and the central claim.
- **Ned Batchelder, "Pragmatic Unicode"** (PyCon 2012) —
  <https://nedbatchelder.com/text/unipain.html>.
  The same idea in Python terms, as the "Unicode sandwich": decode at the boundary, work in text,
  encode on the way out. Short, and it is exactly the hook's situation — a file read at a boundary.
  Read second. Guide questions 2 and 4.
- **Python documentation, "Unicode HOWTO"** — <https://docs.python.org/3/howto/unicode.html>.
  The reference for `str` versus `bytes`, the codecs, and the `errors=` handlers. Read the sections
  on encodings and on reading and writing Unicode data; skip the rest. Guide questions 1, 3 and 4.
- **Nikita Prokopov, "The Absolute Minimum Every Software Developer Must Know About Unicode in 2023
  (Still No Excuses!)"** — <https://tonsky.me/blog/unicode/>. Optional. A modern follow-up to
  Spolsky that covers what `len()` cannot tell you: grapheme clusters, and why one "character" on
  screen can be several code points. Read it if guide question 1 leaves you wanting more.

---

## Bring back to the chat

Answer these in `notes.md`, then post them.

1. Explain, using the em dash `—` (UTF-8 bytes `E2 80 94`), what the hook would see with
   `encoding="utf-8"`, with Latin-1, and with ASCII. For each case, say whether the hook fails
   loudly, fails silently, or works.
2. Guide question 3, answered from the structure of the two encodings, not from having seen the
   result.
3. The hook reads a file that git wrote. Where should the hook's choice of encoding come from, and
   what breaks if that source and git disagree?

The check set is held back until the notes are reviewed (CLAUDE.md §8.5).
