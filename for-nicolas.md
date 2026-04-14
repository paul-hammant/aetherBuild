# For Nicolas — what `aeb` would love from Aether upstream

Hi Nicolas 👋

Paul here. I've been porting `aeb` (the Aether Build runner) from bash to
Aether, methodically, one function at a time. It's gone well — `aeb` is
now a ~36-line bash trampoline plus thirteen Aether-language tools under
`aetherBuild/tools/`. The bash bit only sets a few env vars and dispatches
to `tools/aeb-main` / `tools/aeb-init` / `tools/gcheckout`. Everything
else — argument parsing, DAG discovery, dep extraction, topo sort,
per-file compile, orchestrator generation, gcc link, exec — is Aether.

Along the way I already sent you one PR against `nicolasmd87/aether`:

- **`fix/heap-tracker-and-fs-glob-dotfiles`** — two codegen/runtime fixes
  with regression tests and CHANGELOG entries under `[0.51.0]`. Both were
  blockers for the aeb port; both should be useful for anyone compiling
  non-trivial Aether programs.
  - `fs_glob: walk_recursive matches dot-prefixed files` — `**/.build.ae`
    used to return zero results.
  - `codegen: lazy _heap_<var> declaration for late string reassignment`
    — `lib/build/module.ae`'s `_collect_file_list()` failed to compile
    without it.

This doc is a wishlist for the **next** batch of upstream Aether work that
would let `aeb` shrink further and run unmodified on Windows. All items
are small — mostly POSIX wrappers and one Windows backend port. I've
ordered them by value.

You can read the longer, more detailed version in
[`docs/aether-runtime-needs.md`](docs/aether-runtime-needs.md) in the
aetherBuild repo. This file is the "if you only have time for one
PR, do this first" summary.

## Priority 1 — `os_run` argv-based process launch

The biggest single win. Right now every tool builds shell command strings
like:

```aether
cmd = string_concat(aetherc, " --lib ")
cmd = string_concat(cmd, aeb_lib)
cmd = string_concat(cmd, " ")
cmd = string_concat(cmd, src_path)
cmd = string_concat(cmd, " 2>/dev/null")
os_system(cmd)
```

This has two big problems:

1. **Quoting bugs everywhere.** Paths with spaces, quotes, or `$` break
   silently. The tools paper over it with `\"...\"` but it's brittle.
2. **Windows doesn't have `/bin/sh`.** `os_system` on Windows runs through
   `cmd.exe` by default in most C runtimes, and `>/dev/null`, `&&`,
   `2>&1` don't work there.

What I'd love:

```aether
// Run a child process with an explicit argv and optional env.
// Returns the child's exit code. No shell in the middle.
extern os_run(prog: string, argv: ptr, env: ptr) -> int

// Same but captures stdout into a new string.
extern os_run_capture(prog: string, argv: ptr, env: ptr) -> string
```

`argv` would be an existing Aether list. `env` would be optional (null
inherits). On POSIX this is `fork` + `execvp` + `waitpid`; on Windows
it's `CreateProcessW` with an explicit command-line. No shell touches
either argument.

This alone fixes about 80% of the "Windows doesn't work" story for `aeb`
and eliminates a whole class of bugs on POSIX too. It'd also benefit
every other Aether program that launches subprocesses.

## Priority 2 — `fs_glob` Windows backend

`std/fs/aether_fs.c`'s recursive glob walker is POSIX-only right now
(it uses `dirent`/`fnmatch` under `#ifndef _WIN32`). `aeb`'s scan mode
and `aeb gcheckout` both call `fs_glob("./**/.*.ae")`, so on Windows they
currently return zero results.

A Win32 implementation using `FindFirstFileW` / `FindNextFileW` plus
either a small fnmatch port or a simpler glob matcher would unblock
`aeb` on Windows. The function contract stays the same; only the
`#ifdef _WIN32` branch needs filling in.

## Priority 3 — `aether_argv0()` + `os_execv()`

These two are what lets the bash trampoline disappear entirely.

```aether
// Path the OS launched the current process with (argv[0]).
// Platform-specific realpath may still be needed, but this gets us 90%.
extern aether_argv0() -> string

// Replace the current process with another. Returns only on failure.
// Same contract as POSIX execv — argv is a list.
extern os_execv(prog: string, argv: ptr) -> int
```

Once these exist, I can collapse `aeb` into a self-bootstrapping
`aeb.ae` that the user runs directly. The `#!/bin/bash` line goes away
and the whole tool becomes one compiled binary.

`os_getenv` already exists (thank you!) so that piece is done.

## Priority 4 — a small filesystem stdlib bundle

`tools/aeb-init.ae` still shells out to `ln -s`, `rm`, `readlink`, and
`test -L` for symlink management. These are all straightforward POSIX
wrappers:

```aether
extern fs_mkdir_p(path: string) -> int
extern fs_symlink(target: string, link: string) -> int
extern fs_readlink(path: string) -> string       // "" if not a symlink
extern fs_is_symlink(path: string) -> int
extern fs_unlink(path: string) -> int
extern os_which(name: string) -> string          // "" if not on PATH
```

These would let `aeb-init` and `aeb-link` stop shelling out for trivial
operations. Bonus: they'd give `aeb --init` a sensible Windows fallback
(use directory junctions via `CreateSymbolicLinkW | SYMBOLIC_LINK_FLAG_DIRECTORY`,
or fall back to copying the SDK tree when elevation isn't available).

## Priority 5 — audit-ready: `path_join` etc. (already exist!)

Not a new ask, just a note: `path_join`, `path_normalize`, `path_dirname`,
`path_basename`, `path_is_absolute` all already exist in `std/fs`. Every
`tools/*.ae` file in `aeb` currently uses `string_concat(x, "/")`
instead of `path_join` — that's an audit job on my side (maybe two
hours) and it's blocked on nothing. I'll do it myself once the Priority
1–4 work lands so the final `tools/*.ae` is platform-clean end to end.

## What I'm NOT asking for

Two things I'm keeping on my own plate:

- **VCS abstraction for `aeb gcheckout`.** `git sparse-checkout` is
  fine as a default; Mercurial / jj / svn support would be nice but
  I can ship that as an `.aebvcs` strategy file at the consumer-repo
  root without any upstream Aether change.
- **The gcc-vs-cl.exe choice in `aeb-link`.** That's an `aeb` concern,
  not an Aether stdlib concern. When Windows support happens I'll
  write a tiny per-platform link-line builder in `aeb-link.ae` itself.

## Rough ordering for you

If you have one afternoon: **P1 (`os_run`)**. That's the biggest single
win.

If you have a weekend: **P1 + P2**. That gets `aeb` running on Windows.

If you have a week: **P1 + P2 + P3 + P4**. That gets the bash trampoline
deleted, `aeb` running as a single binary on Linux/macOS/Windows, and
`aeb --init` using no shell at all.

None of these are big. They're mostly 20-50 line POSIX wrappers plus one
Windows backend for `fs_glob`. Happy to contribute PRs for any of them
if that's more useful than a wishlist — just let me know which you'd
prefer I pick up vs. which you'd rather write yourself.

And thanks for the `fs_glob` + heap-tracker PR review in advance — those
two fixes were load-bearing for the whole aeb port.

— Paul
