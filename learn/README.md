# Krama — Learning ledger

The parallel learning track. One row per packet taken through `/ground-up` (CLAUDE.md §8). This
file is an index. Each packet's evidence, reading and exercises live in its own folder,
`learn/<topic>/<packet>/`, whose `README.md` is the packet itself. The session story lives in
`PROJECT.md`, and the rules live in CLAUDE.md §8. Status meanings are defined in CLAUDE.md §8.6 and
are not repeated here.

Rows change status in place. A `declined` row is never removed (§8.3).

**Columns.** None of them is a closed set. Use the best fit, and add a word when nothing fits.

- **Topic** is the area a packet belongs to: `git`, `memory`, `types`, and so on. It is also the
  folder name. An exercise filed under one topic may still teach another.
- **Scope** is what the packet asks for: `reading`, `Q&A`, `lab` (commands to run, no program to
  write), `code`. Combine them with `+`.
- **Language** is filled in only when the scope includes `code`. Otherwise it is `—`.
- A packet's branch is its path: `learn/<topic>/<packet>` (COMMITS.md *Learning branches*).

| Topic | Packet | Scope | Language | Status | Score | Dates | Notes |
|---|---|---|---|---|---|---|---|
| memory | alignment | code | C | queued | — | queued 2026-10-10 | Bump allocator over a `static` buffer, outside Krama. Named first candidate by D-042; in Friction 2026-08-21 and 2026-09-17. Unassessed |
| portability | [portability-layers](portability/portability-layers/) | reading | — | open | 4 | opened 2026-10-10 | Language standard vs libc vs OS; surfaced asking about Windows dev (D-038) |
| git | [remote-tracking](git/remote-tracking/) | reading + lab | — | paused | 5 | opened 2026-10-21; on hold 2026-10-23 | Remote-tracking branches and non-fast-forward pushes; surfaced by workflow 2.0, Rule 4 scope (`aside-notes-02` §10). Nothing waits on it since D-041. On hold per `aside-notes-04` §12.1 — the one-active-detour conflict with `portability-layers` is unresolved |
| types | affine-types | reading | — | queued | 6 | queued 2026-11-04 | Non-copyable Handlers consumed by nested `with`, reached unprompted; not yet applied to lending or compared with linear types / borrowing. `aside-notes-05` §9, §14. Packet not yet written |
| types | local-type-inference | reading | — | queued | 5 | queued 2026-11-04 | Hand-annotated deferred resolution correctly; attributed it to Go rather than Rust. `aside-notes-05` §7.2, §14. Packet not yet written |
| regex | [regex](regex/regex/) | reading + Q&A | — | queued | 5 | queued 2026-11-08; packet written 2026-11-13 | From `m1/cli-args` (`krama-notes-20` §6): class taken for a group twice, `+` read as a quantifier inside a class, no end anchor. Exit at 7, like any concept (CLAUDE.md §8.5); the 5–6 target in `krama-notes-20` §6 predates the gate and is superseded. Assessed 2026-11-13 on `tool/commit-hook` step 4: explained COMMITS.md's pattern correctly, then read `?` as a repeat on `fix(lex,parse,ast): x`. Accepted; queued under the one-active-packet limit |
| git | [reachability](git/reachability/) | reading + lab | — | queued | 3 | queued 2026-11-11 | Branches as pointers and reachability. Modelled a branch as a container of its own commits: held `H1` outside `tool` when `tool` was cut from a `main` ahead of the remote. Followed range-as-subtraction and confirmed the null-OID failure by running it. Surfaced by `tool/commit-hook` step 6 (the `pre-push` range). Sits under `remote-tracking` |
| text | [character-encoding](text/character-encoding/) | reading | — | queued | 6 | queued 2026-11-12 | Encode vs decode, and who chooses. Asked unprompted what `open()` does when the declared encoding and the file's disagree; did not know git writes UTF-8 regardless of locale. Not yet compared strict-UTF-8 loud failure with single-byte silent failure. Surfaced by `tool/commit-hook` step 3 (`encoding=` in the hook). First assessed at 4 on a misread question, corrected to 6 |
| compilers | incremental-compiler-construction | — | — | declined | 6 | declined 2026-10-21 | Explained back and extended unprompted ("build drives the spec"); not compared with when it is the wrong approach. Declined: no formed language whose retrofitting cost could be weighed. `aside-notes-02` §11.1; PROJECT.md **Didn't**, 2026-10-20 → 2026-11-08 |
