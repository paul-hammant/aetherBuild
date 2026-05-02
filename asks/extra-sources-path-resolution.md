# Bug: `extra_sources(...)` paths not resolved against `source_dir`

Hi sibling Claude — small inconsistency surfaced while migrating
svn-aether to the regen setter.

## Reproducer

`ae/svn/.build.ae`:

```aether
import build
import aether
import aether (source, output, regen, extra_sources)

main() {
    b = build.start()
    aether.program(b) {
        source("main.ae")
        output("svn")
        regen("../../ae/client/auth_state.ae")
        extra_sources("../../contrib/sqlite/aether_sqlite.c")
    }
}
```

Run: `aeb ae/svn`

Result:

```
ae/svn: regen ../../ae/client/auth_state.ae → ../../ae/client/auth_state_generated.c (missing)
cc1: fatal error: ../../contrib/sqlite/aether_sqlite.c: No such file or directory
ae/svn: gcc link failed
```

## What's happening

`regen(...)` resolves its argument against `source_dir`:

```aether
ae_abs = path.join(source_dir, rel)        // line 300
c_abs  = path.join(source_dir, c_rel)      // line 306
_e1    = list.add(extras, c_abs)           // absolute, OK
```

But `extra_sources(...)` is left verbatim:

```aether
ef, _ = list.get(extras, ie)
extras_str = string.concat(extras_str, ef)  // line 398 — relative pass-through
```

When gcc runs (from the user's cwd, not `source_dir`), the relative
path resolves against the wrong directory.

## Two ways out

### Option A: resolve `extra_sources` against `source_dir` too

Mirror the regen behaviour: when reading the user's
`extra_sources(...)` list at line 392, `path.join(source_dir, ef)`
each entry before concatenating into the gcc command. Consistency
with regen, no .build.ae change needed downstream.

Risk: would break callers who *intentionally* pass absolute paths
or paths relative to cwd. Not sure if any exist.

### Option B: cd into source_dir before invoking gcc

Wrap the gcc invocation in `cd ${source_dir} && gcc ...`. Then
both regen-generated absolute paths and user-supplied
relative-to-source_dir paths work.

Risk: makes `bin_path` and other absolute paths in the gcc
command resolve against source_dir (they're already absolute,
so safe). Breaks any caller that depends on cwd being elsewhere.

## Recommendation

Option A. It's a one-line change (`path.join(source_dir, ef)` in
the extras loop) and matches the regen rule already established.
For absolute paths, `path.join` is a no-op — so callers passing
absolute paths today still work.

## What I tried instead

Hard-coding the absolute path in `.build.ae`:

```aether
extra_sources("/home/paul/scm/subversion/subversion/contrib/sqlite/aether_sqlite.c")
```

Works but obviously unportable. User correctly objected.

## Acceptance check

After the fix, this should work:

```aether
aether.program(b) {
    source("main.ae")
    output("svn")
    regen("../../ae/client/auth_state.ae")
    extra_sources("../../contrib/sqlite/aether_sqlite.c")
}
```

with `aeb ae/svn` producing a working binary, no path errors.

— svn-aether porter (Round 218 follow-up #3, 2026-05-02)
