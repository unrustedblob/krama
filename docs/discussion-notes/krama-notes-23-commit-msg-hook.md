# krama-notes-23 — the `commit-msg` hook

**Dates:** 2026-11-09 → 2026-11-22 · **Branch:** `tool/commit-hook` · **Session:** 23 (D-044)
**Covers:** steps 1–5 of the hook handover, the `pre-push` range rule ahead of step 6, and the
detours they forced: git's cleanup rules, reachability, regex, encoding, ruff configuration.

---

## 1. Why the hooks came first

Two untyped commits reached `ground-up`, and its merge reached `main` as
`Merge branch 'ground-up' ground-up: add the /ground-up learning track` — git's default line kept,
the real subject written directly under it with no blank line, so git joined them. Under D-041 pushed
history is final; both mistakes stay. The hook exists to stop that class of error before a commit
exists.

## 2. Language and placement

- **Python, standard library, under `tools/hooks/`**, per STYLE.md §3.1. Weighed against the
  `/ground-up` sparring languages: **C** was the only serious alternative (its toolchain is already
  required; POSIX `regex.h` and `popen` cover the needs), rejected because a compiled hook needs a
  build step and an unbuilt hook is silently skipped. Go, C++, Rust, Zig and OCaml would each make a
  toolchain mandatory just to commit.
- Wired by `git config core.hooksPath tools/hooks`, once per clone. The file is extensionless,
  starts with a shebang, and its executable bit is tracked (`100755`).
- **Known gap:** `core.hooksPath` is not cloned; a fresh clone runs no hook and says nothing. Raised
  twice, not yet answered.

## 3. What git hands the hook, and what it does with the message

- `commit-msg` gets one argument, a path to the message file. Exit 0 accepts; anything else rejects.
- The hook sees the message **before** git's cleanup. Git's default `--cleanup` strips `#` lines
  only when an editor ran; with `-m` or `-F` it only trims whitespace. Git signals this to every
  commit hook: `GIT_EDITOR=:` when no editor will open (`git help githooks`; `git help commit`,
  `--cleanup`). The hook mirrors it: comment lines are dropped only when `GIT_EDITOR` is not `:`.
- Comment lines are lines whose **first character** is `#`; an indented `#` survives git's cleanup,
  so the hook tests the raw line. Blank lines are tested after `rstrip`; the stored subject is
  `rstrip`ped only — git removes trailing whitespace, not leading.
- **Subject = first paragraph to git.** A body line directly under the subject is joined into it.
  Rejected outright with its own message rather than joined and then failing the length check,
  because an error should name the real cause.
- Empty message: rejected by the hook, not left to git, because `--allow-empty-message` would let it
  through. One exit code (1) for every rejection.
- Encoding: git writes the message as UTF-8 unless `i18n.commitEncoding` says otherwise; `open()`
  without `encoding=` uses the locale. The hook states `encoding="utf-8"` so its reading matches
  git's writing on any machine.

## 4. Length

Over 72 rejects; 51–72 warns; up to 50 is silent. Sources: Tim Pope (2008) — about 50 for the subject,
72 for the body because `git log` indents by four in an 80-column terminal and email quoting needs room;
Chris Beams (2014) — 50 as a rule of thumb, 72 where GitHub truncates. 9 of 93 existing subjects were
over 50, which made warning a tolerable noise level. Boundary tests (50, 51, 72, 73) caught an
off-by-one (`> 73`) that tests at 54 and 76 had missed.

## 5. Type and scope

- **Kind from git's state, never from the subject.** A merge is in progress when `MERGE_HEAD` exists
  (`git rev-parse -q --verify MERGE_HEAD`: 0 yes, 1 no, anything else is an error and reported
  loudly). `MERGE_MSG` was tried and rejected: a conflicted cherry-pick writes it too. `MERGE_HEAD`
  appears for clean and conflicted merges only.
- **Ordinary commits: parse loosely, then validate.** A structural pattern captures type and scopes;
  separate checks name each problem. The first version checked symptoms (uppercase, underscore,
  digits) and fell through to *accept* whenever the strict pattern failed for a reason no check named
  (`fix(9)`, `fix(café)`, `fix(bar,car)`). COMMITS.md's *Scopes* section was the real rule: a closed
  vocabulary. One membership check replaced the symptom checks and closed the class. Also from that
  section: no scope on `build` or `learn`, no `test` scope on `test`, no repeats. The regex in
  COMMITS.md's hook note was looser than its own *Scopes* section; the note now describes structure
  plus table checks instead of quoting one pattern.
- Type and scope lists are hard-coded in the hook for now. A config file is planned after the
  descope trims the scopes; TOML (`tomllib`) is likely cheaper than an invented CSV-like format.
- **Merges:** one merge head only — an octopus merge writes several lines to `MERGE_HEAD`, and
  `rev-parse`/`--points-at` read only the first. Then the label shape (three patterns), then its
  identity: the label must name a local branch pointing at `MERGE_HEAD`. `git branch --points-at`
  is porcelain; the hook uses `git for-each-ref --points-at MERGE_HEAD --format=%(refname:short)
  refs/heads/`. Membership, not equality, so two branches on one commit both pass. Empty list (a
  mid-branch commit, or `origin/x` with no local branch) gets its own message. `--no-merged` was
  tried and rejected: it proves the label names *some* unmerged branch, not the one being merged.
- **Endings:** the text after `: ` must not start with a space and must not end in `. , ; : ! ?`; one
  character is allowed. Capitalisation after the colon was considered and dropped — house style with
  no functional reason (Beams even capitalises), and imperative mood cannot be checked anyway.

## 6. Git's own subjects (step 5)

- `wip` became a **type** in COMMITS.md *Types*: local-only, never pushed. The hook accepts `wip:` once
  the structure matches, with no further checks. `build:` had been borrowed for work in progress —
  which would have slipped past `pre-push`, whose check is on subjects starting `wip`.
- `fixup!`, `squash!`, `amend!`: all three are autosquash markers for rewriting **unpushed** history
  (Rule 3); none rewrites pushed history (Rule 4). Deferred: rejected by `commit-msg` until the
  workflow needs them; `pre-push` refuses them regardless.
- `Revert "…"`: COMMITS.md states no rule; deferred.
- `Merge branch '…'`: already rejected by the merge patterns.

## 7. The `pre-push` range (ahead of step 6)

- An all-zeros SHA is git's null OID: no object, so it cannot be a range endpoint (`rev-list` fails).
- **A branch is a pointer to one commit, not a container.** With `tool` cut from a local `main` two
  commits ahead of `origin/main`, pushing `tool` sends all three (`H1`, `H2`, `T`); `main..tool`
  checks only `T`. The handover's `main..local` rule was wrong.
- **The rule:** commits reachable from the local SHA and from none of the remote's tracking refs. One
  rule for new and existing branches; deletions (local SHA all zeros) are skipped. Tracking refs are a
  snapshot; stale ones only cause extra commits to be checked. COMMITS.md's `pre-push` note was
  corrected (`10788e1`).

## 8. Regex, in practice

- `?` is zero or one; `*` zero or more; `+` one or more. `(,[a-z]+)?` allows one extra scope, so
  `fix(lex,parse,ast)` fails — and when it does, the engine backtracks through every optional part
  before giving up.
- A repeated group keeps its last repetition: `([a-z])+` captures one letter; `([a-z]+)` the word.
- `[^…]$` constrains one character; `[^…]+$` constrains every character it consumes.
- **Count the minimum.** `.+\w` and `\S.*[^…]` both rejected one-character subjects.
  `(\S.*)?[^…]` works: greed tries the group first, backtracking drops it when nothing is left for the
  final class.
- `$` matches at the end **or before a trailing newline**: `match` with `$` accepts `"foo1\n"`,
  `fullmatch` rejects it. With `rstrip` both behave, but if the strip were forgotten `fullmatch` fails
  loudly and `match` silently — so the hook uses `fullmatch`.

## 9. Ruff configuration — D-044

`ruff --show-settings` showed a user-level `~/.config/ruff/ruff.toml` at 100 columns while STYLE.md
said "default settings". The repository now carries `ruff.toml` with only its departures from the
defaults: `line-length = 100`, `target-version = "py313"`, `extend-include = ["tools/hooks/*"]` (ruff
skips extensionless files on directory runs). STYLE.md 1.8 names the file. Rejected: ruff defaults
(88 columns), the rule in prose with the user config (machine-dependent), and a full copied config
(boilerplate hides the decisions).

## 10. Reviews

- **r001** (against `eb17597`): no-scope crash (#23, fixed in `5b6accb`, closed by comment); strict
  failures accepted (#24); merge message on one line (#25). Filed by Claude Code from approved titles.
- **r002** (against `0380a11`, which carries `Closes:` for #24 and #25): reran in a scratch repo —
  forty commits, ordinary and merge, valid and invalid; all pass, no new issues. *Not filed*:
  accumulation stops at an invalid type; merge label outside a merge got the generic message (since
  fixed by naming the kind in every message); duplicate message prints tuples; double space after the
  colon (since fixed); label identity unbuilt (since built).
- **r003** is owed for everything after `0380a11`.
- `docs/review/tool/<name>/` was used for a tool branch; the review README's layout does not yet say so.

## 11. Assessments (CLAUDE.md §8.1)

| Concept | Score | Evidence | Outcome |
|---|---|---|---|
| Branches as pointers, reachability | 3 | Held `H1` outside `tool` because it was "not part of" the branch; later worried two changes could share a hash | Queued `git/reachability`, reading + lab |
| Character encoding | 6 | Asked unprompted what a declared-vs-actual mismatch does; did not know git writes UTF-8. First scored 4 on a misread question, corrected | Queued `text/character-encoding`, reading |
| Regex | 5 | Explained the COMMITS.md pattern correctly, then read `?` as a repeat. Later: found `$`'s newline case by experiment and compared `match`/`fullmatch` on it; explained the optional group's success as greed (backtracking was the rest) | `regex/regex` packet written, queued |

## 12. Process friction

- **Silent stale writes.** Three writes to the user's computer reported success and left old
  content: the ledger, D-044's entry, a COMMITS.md edit. Cause: a file copied into the outputs area
  and then edited could be picked up before the edit. Fixed by editing first and sending the finished
  file once from a fresh path, then reading it back. Now in the project instructions. It took two
  rounds to find: the first fix (a new path) treated the symptom.
- Claude stated twice that a hook version would crash, from a copy older than the one tested, and
  once claimed `fullmatch` did not depend on `rstrip`; both corrected by the programmer's evidence.
- Reviews that ran the hook in a scratch repo found nothing prediction had missed, and confirmed
  everything it had claimed — worth keeping for `pre-push`.

## 13. Side discussion — lighter alternatives to git

Git's core model (content-addressed objects plus refs) is small; the weight is in the porcelain.
`got` (Game of Trees) keeps git's format with a small command set; Jujutsu (`jj`) simplifies the model
(no staging area, working copy as a commit, undoable operations); Fossil is one binary and one SQLite
file, with no rebase; Pijul is patch-based. Krama stays on git because the review workflow (D-041)
runs on GitHub issues and `Closes:` trailers.

## 14. Owed

PROJECT.md **Current** rewrite and the branch's log entry; r003; step 6 (`pre-push`); the
*Deliberately incomplete* rows listed in `claude/handover-tool-commit-hook-step6.md` §6; the
COMMITS.md *Notes — deferred* heading; the review README's tool-branch path; STYLE.md §13's hooks row
and line 8's spec path; the fresh-clone `core.hooksPath` question.
