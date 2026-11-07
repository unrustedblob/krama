# Krama — `Closes:` covers one issue, not a list
 
**Date:** 2026-11-08
**Status:** Discussion record. **Not normative.** Records a doc defect found while closing r001's
issues; the fix lands in COMMITS.md. Decision IDs are assigned only if one is recorded.
 
> Where this and `COMMITS.md` disagree, `COMMITS.md` wins until it is amended — but the claim below
> is wrong in practice, so do not rely on it meanwhile.
 
---
 
## 1. What happened
 
The fix commit for review r001 (`d350b75`) carried one trailer:
 
```
Closes: #16, #17, #18, #19, #20
```
 
On reaching `main`, GitHub closed **only #16** (a `closed` / `COMPLETED` event). #17–#20 show the
same commit as **`referenced`**, and stayed open. It is not a timing issue: the events were checked
after the push.
 
## 2. Why
 
GitHub binds a closing keyword to the **one** issue reference that follows it. In a comma list, the
first number is a close action and the rest are plain mentions, which only link.
 
**Confirmed against GitHub's documentation** ("Linking a pull request to an issue"): for multiple
issues, *"use full syntax for each issue"*, with the example
`Resolves #10, resolves #123, resolves octo-org/octo-repo#100`. Nothing there says one keyword
covers a list. The same page confirms closing keywords work in commit messages, closing the issue
when the commit merges into the default branch.
 
## 3. What is wrong in the docs
 
- **`docs/COMMITS.md`** (the repository copy) describes multi-issue comma lists for `Closes:`. That
  claim is the defect.
- **The project-context copy of COMMITS.md** does not contain the claim — its *Trailers* section
  shows only single-issue `Closes: #12`. So the context copy is **behind** the repository. Both get
  the corrected text; mirroring the repository's paths (`aside-notes-04` §7) is what stops this
  drift.
- **`PROJECT.md` Current** says r001 is "#16–#20, closed". Only #16 was closed by the commit. True
  once #17–#20 are closed by hand; worth a line in the next log entry either way.
D-031's own reference was right on what it checked — the colon and capitalisation are accepted. The
comma-list form was never checked against the source.
 
## 4. Proposed COMMITS.md change
 
In *Trailers*:
 
- **One issue per closing keyword.** Write one trailer line per issue:
  ```
  Closes: #16
  Closes: #17
  ```
 
  Repeated trailer keys are ordinary Git trailer convention, each line is one keyword bound to one
  issue, and each stays greppable. The inline form `Closes: #16, closes #17` also works per the
  docs, but mixes a trailer with prose.
- **`Refs:` may stay a list.** It never closes anything, and every listed number is still a mention,
  which links. `Refs: D-003, D-011` is unaffected (not a GitHub reference at all).
- **Add a check to the appendix's step 7:** after pushing `main`, confirm each `Closes:` issue shows
  `closed`, not `referenced` — `gh issue view <N>` or the issue page.
**Verify on first use.** The one-line-per-issue form is what the docs imply, not yet something
observed in this repository. The next multi-issue fix commit is the test.
 
## 5. Open — the programmer's call
 
1. **#17–#20:** close by hand now (`gh issue close 17 18 19 20 --reason completed`, offered by Claude
   Code, not run), or after COMMITS.md is fixed. The work is done and merged either way; a new commit
   only to re-close them would add history for no code change.
2. **Where the COMMITS.md fix lands:** its own small `docs` commit now, or folded into workflow 2.0's
   COMMITS.md edits (`aside-notes-03` §6), which already touch *Trailers*.
## 6. Lessons
 
- **Same shape as the chibicc check** (2026-09-18 Worked): a documented claim about someone else's
  tool, believed until it was observed. The colon was checked; the list was not.
- **Feeds workflow 2.0's `Refs:` point.** Trailer behaviour on GitHub is part of that decision;
  this is the first observed data point.
- **A `PROJECT.md` Friction candidate** for the next log entry.
