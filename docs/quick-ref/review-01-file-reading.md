# Review 01 — file reading in `main.c`

**Date:** 2026-08-14
**Scope:** First code review. `src/main.c` (read a funC source file, write it to stdout) and the
milestone-1 `Makefile`.
**Outcome:** Code correct after three rounds. Two build-system issues still open.

---

## 1. Defects found and fixed

| # | Defect | Why it mattered |
|---|---|---|
| 1 | `printf("%s", buf)` on a buffer with no NUL terminator | Heap overread. `malloc` returned exactly the file size; `%s` scans past the allocation until it finds a zero byte. Appeared to work because fresh pages are usually zeroed — luck, not correctness. ASan would flag it. |
| 2 | `fseek` failure printed a message then fell through | `ftell` ran on a stream in an unknown state. Every other error path used the ladder correctly, which is exactly why the odd one out was invisible. |
| 3 | `buf[file_sz] = '\0'` placed *before* the `if (!buf)` check | Null dereference on allocation failure; the check could never run. Introduced while fixing #1 — a fix creating a bug adjacent to the one it repaired. |
| 4 | `%ld` and `%lu` used for `size_t` | `%zu` is correct everywhere. `%lu` happens to work on LP64 because `size_t` *is* `unsigned long` there — the same class of host-dependence as C's `char` signedness. Passing an unsigned value to `%ld` is undefined behavior per C23 §7.23.6.1, not merely sloppy. |
| 5 | `int main()` | `(void)` per STYLE.md §8.1. C23 makes them equivalent so no warning fires; the rule is for readers with pre-C23 habits. |
| 6 | Empty file reported as an allocation failure | `file_sz == 0` meant `malloc(0)`, which the standard permits to return NULL. Fixed incidentally by allocating `file_sz + 1` for the sentinel. |

## 2. Errors in the review itself

**Claimed `-Wformat` catches signed/unsigned specifier mismatches. It does not.** `%ld` against a
`size_t` is the same width on LP64 and GCC accepts it silently. The flag that catches it is
**`-Wformat-signedness`** — GCC-only, no Clang equivalent, so it belongs in `GCCFLAGS`. Added, and
the defect then failed the build as expected.

Worth noting how this surfaced: the `%ld` was left in deliberately to test whether the enforcement
was real. That is the right instinct and it found a wrong claim in the review rather than a wrong
flag in the build.

`-Wformat` itself is enabled by `-Wall` and does not need naming explicitly. `-Wformat-signedness`
extends it and requires it active.

## 3. Build system findings — still open

**`CC ?= gcc` is inert.** Make predefines `CC` as `cc`, so `?=` sees it already set and skips the
assignment. The build ran `cc`. On Debian that symlinks to gcc so nothing broke, but the line does
nothing. To default to gcc while still allowing override:

```make
ifeq ($(origin CC),default)
  CC = gcc
endif
```

`$(origin)` distinguishes make's built-in default from a value the user set.

**`LLVMFLAGS` is dead.** The compile rule hardcodes `$(GCCFLAGS)`, so GCC-only flags go to every
compiler and the LLVM group is never referenced. `make CC=clang` will fail on
`-Wmaybe-uninitialized`, `-Wfree-nonheap-object`, and now `-Wformat-signedness`. Either wire the
conditional or drop the variable — two variables implying a mechanism that does not exist is worse
than one.

## 4. Decisions taken during review

**Sentinel: allocate `file_sz + 1`, write `'\0'` at the end.** Makes the buffer C-string-safe and
lets the scanner peek one past the last real byte without a bounds check. Belt-and-braces only — a
file containing an embedded NUL would still fool `%s`, so the length must still travel with the
buffer.

**`rewind` kept over `fseek(fp, 0, SEEK_SET)`.** It cannot report failure (returns `void`, clears
the error indicator), but a seek to start on a stream that just seeked to end successfully will not
realistically fail.

*Revisit condition — corrected during review:* the trigger is **not** compiling multiple funC files;
those are still one seekable file at a time. The trigger is a **non-seekable stream** — reading
source from stdin via a pipe. Then `fseek`, `ftell`, and `rewind` all fail and the entire
size-then-read strategy has to become read-until-EOF into a growable buffer.

**`printf("%s\n", buf)` left as-is.** Not byte-faithful: it stops at an embedded NUL and appends a
newline the file may already have. `fwrite(buf, 1, file_sz, stdout)` is exact. Left because this
print disappears once codegen exists.

## 5. Carried forward — preserve these

**`fclose` on write streams must be checked.** Harmless to ignore on a read-only stream. On an
*output* stream a failing `fclose` means buffered data never reached disk, and ignoring it is how
files get silently truncated. Codegen writes files, so the habit forms there. Candidate for a
STYLE.md rule once there is real write code to point it at.

**File reading becomes a module.** Not `main`'s job once a pipeline exists. Two design points
already visible:

- *The buffer and its length must travel together.* `file_sz` is currently a local that dies with
  `main`. As a function it needs both returned — a two-member struct (`char *data; size_t len;`) at
  16 bytes, register-passable. Exactly STYLE.md §6.3's by-value case, and it takes a `static_assert`
  on size per §6.4.
- *It is the codebase's only `owned` return.* Per §9.5 every pointer-returning function documents
  lifetime; everything else will be `arena` or `borrowed`. `owned` appearing anywhere should be a
  signal, so the comment matters here.

**The test fixture has an accidental bug.** `tests/fc-src/test.fc` contains `set c = 5;` where `c`
is never declared. Keep it — it is a ready-made fixture for the type checker's undefined-variable
diagnostic. Do not "fix" it.

## 6. What went right

- The `goto` cleanup ladder was correct on the first attempt: labels in reverse order of
  acquisition, `cleanup_alloc` falling through to `cleanup_file`, every failure path unwinding
  exactly what succeeded.
- `exit_status` initialised to failure and set to success only on the last line — the right default.
- `ferror` and `feof` distinguished rather than lumped into one "read failed".

---

## Next tasks

**Before committing**

- [ ] Add the file comment to `main.c` — `// Read funC source file and dump to stdout` is sufficient
- [ ] Optional: swap `printf("%s\n", buf)` for `fwrite(buf, 1, file_sz, stdout)`
- [ ] Confirm `make clean && make` is still clean

**Commit**

- [ ] `feat(main): read source file and echo to stdout` — body should note the sentinel decision
      (`+ 1` byte, NUL-terminated) and why

**PROJECT.md**

- [ ] Fill in the `Current` block — it is still placeholders
- [ ] Log entry for this session. Put the null-write-before-check bug (#3 above) in **Didn't** — a
      fix that introduced a neighbouring bug is exactly what that field is for
- [ ] **Friction:** note the `%ld` / `size_t` episode if the format-specifier rules felt unclear

**Build system**

- [ ] Decide: wire the `GCCFLAGS` / `LLVMFLAGS` conditional, or delete `LLVMFLAGS`
- [ ] Fix the `CC` default with `$(origin CC)` if defaulting to gcc actually matters

**Later — do not do now**

- [ ] STYLE.md addition: check `fclose` on write streams (once codegen writes files)
- [ ] Extract file reading into its own module returning a `{data, len}` struct by value
- [ ] `argv[1]` instead of the hardcoded path
