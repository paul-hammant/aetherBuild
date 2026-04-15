# Aether runtime needs for aeb migration

## Problem

Migrating `aeb` from bash to Aether is blocked by two runtime issues:

1. **String memory leak in loops** — `string_concat` + `string_substring` in loops allocate without freeing. A simple replace-all loop OOMs the machine. This is the root cause of the `this_hangs_pauls_nuc` branch crash.

2. **No native string operations** — `string_replace_all`, `string_split`, line iteration all require `os_exec("sed ...")` or `os_exec("echo ... | sed -n Np")` which forks a shell per call. For the DAG discovery phase (scanning files, extracting deps, building topo-sort input), this means O(files * deps) subprocess forks — unusably slow.

## What's needed

A single C function in the Aether runtime:

```c
// Replace all occurrences of `from` with `to` in `str`
// Returns a new string (caller-managed or arena-allocated)
const char* string_replace_all(const char* str, const char* from, const char* to);
```

Exposed to Aether as:

```
extern string_replace_all(s: string, from: string, to: string) -> string
```

This would unblock:
- `encode_name` — currently uses `os_exec("sed ...")`, could be pure Aether
- `resolve-dag` — currently abandoned, could be ported if string building doesn't OOM
- Full `aeb` migration — the remaining bash is mostly string/line processing

## Secondary needs

- `string_split(s, delimiter)` → list — avoids `os_exec("echo ... | sed -n Np")` per-line parsing
- Arena/pool allocation for strings in loops — or at minimum, free intermediate strings

## Current workarounds

- `os_exec("sed ...")` for string transforms (works but forks per call)
- `os_exec("bash -c '...'")` for complex text processing (works but fragile quoting)
- Keeping line-oriented processing in bash (scan, topo-sort, dep extraction)

## What's already ported

With the workarounds above, two pieces of `aeb` are in Aether:
- `tools/gen-orchestrator.ae` — generates orchestrator source (bounded os_exec calls)
- `tools/transform-ae.ae` — transforms .ae files for linking (bounded os_exec calls)
- `tools/resolve-imports.sh` — BFS import resolution (stays in bash, called by transform-ae)

The boundary is: Aether handles per-file transforms, bash handles bulk text scanning.
