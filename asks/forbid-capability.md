# Idea: build-time capability forbid-list for `aether.program(b)`

Self-filed by aeb Claude after a question from Paul (round 218
follow-up #2). Not driven by a downstream port — capturing the idea
so it doesn't get lost. **Not blocking anything; not asking for
implementation today.**

## What this is

A build-time setter that fails `aeb` if the binary would need a
capability the user has explicitly forbidden. Distinct from runtime
sandboxing — this is about what the binary's link surface is allowed
to contain, not what it's allowed to do when it runs.

## Sketch

```aether
aether.program(b) {
    source("main.ae")
    output("readonly-svn")

    regen("ae/repos/blobfield.ae")
    regen("ae/wc/pristine.ae")        // auto-detects --with=fs
    regen("ae/client/auth.ae")        // auto-detects --with=net

    forbid_capability("os")           // hard error if any regen
                                      // entry requires --with=os
}
```

`_run_regen_pass` resolves caps for each entry (auto-detect or
`regen_with`), then before invoking aetherc, intersects the resolved
cap set with the forbid list. If non-empty, hard-fail with:

```
ae/svn/main.ae: capability 'os' is forbidden in this binary
  but ae/svnserver/dump.ae requires --with=os.
```

Allowed values: `net`, `fs`, `os` — same set aetherc currently
gates. Repeated calls accumulate (`forbid_capability("net");
forbid_capability("os")`).

## Why this is distinct from runtime sandboxing

Two layers, often confused:

1. **`--with=net`** (compile time, `aetherc --emit=lib`): controls
   whether the resulting `.so` can expose net-capability stdlib
   symbols. Decided by whoever *builds* the binary.
2. **Runtime sandbox grants** (runtime, `aether_sandbox_check`):
   controls whether those symbols actually do I/O when called.
   Decided by whoever *invokes* the binary, via the embedding
   application's sandbox config.

`forbid_capability(...)` lives at layer 1. It says "this binary's
ABI surface must not contain X-capability symbols at all." Stricter
than runtime sandbox (which is per-invocation); useful as a
permanent property of the artifact.

Runtime sandbox grants would be the wrong thing to put in
`.build.ae` — they're a deployment concern, not a build concern.
That layer stays out of aeb.

## Use cases

- **Library binaries**: ship a `.so` that *must not* grow os deps.
  Catches accidental `import std.os` drift in transitive helpers.
- **Multi-team repos**: one team's binary can't accidentally
  compile in `net` without an explicit review (the diff that
  removes `forbid_capability("net")` is the review).
- **CI lints**: "if the regen footprint of this PR added a new
  capability, surface it." Naturally falls out of having
  `forbid_capability` declarations to compare against.
- **Reproducibility audits**: a binary's `.build.ae` declares its
  full capability footprint without grepping all transitive .ae
  files.

## What's NOT being asked

- A way to *grant* capabilities at runtime from `.build.ae`. That
  belongs at the embedding layer (`runtime/aether_sandbox.h`),
  not the build configurator. Build systems control linking, not
  invocation.
- Per-source allow-lists ("auth.ae may use net but blobfield.ae
  may not"). aetherc already enforces this — if `blobfield.ae`
  has no `import std.net`, it doesn't get net at the ABI level.
  Nothing for aeb to add.
- Allow-list shape (`allow_capabilities("fs")` whitelisting only
  `fs`). The negative form is more useful in practice — most
  binaries genuinely need most caps; locking out one or two is
  the realistic case.

## Why not now

- No downstream caller asked for it. svn-aether's current
  migration just needs `regen` + `regen_with` (which shipped).
- Aether's capability list may grow (`actor`? `time`?). Locking
  in aeb's understanding now risks drift.
- One sentence in the README ("explicit `regen_with(...)` makes
  the cap declaration visible per-source for code review")
  already covers (1) and most of (3) from the discussion that
  generated this idea.

## When to revisit

Three triggers, any one is reason to implement:

1. A real downstream wants this for one of the use cases above
   (library shipping, multi-team boundary, CI lint).
2. Aether grows a capability whose accidental inclusion is a
   real footgun (e.g. an `unsafe` cap that bypasses the actor
   model). Build-time forbid becomes load-bearing.
3. The auto-detect heuristic in `regen(...)` proves too permissive
   in practice — binaries quietly grow capabilities their authors
   didn't intend, and a forbid-list becomes the natural fix.

Until then: tracked here, not built.

## Implementation cost estimate

- New setter `forbid_capability(_ctx: ptr, cap: string)` + parallel
  list in `_builder` (mirrors `regen_caps`). ~10 lines.
- Check in `_run_regen_pass` after cap resolution, before aetherc
  invocation. ~10 lines.
- Doc entry. ~5 lines.

Total: small. The cost-of-shipping risk is the *contract* (which
caps are allowed values, what error message to emit, how to evolve
when Aether's cap list grows), not the code.

— aeb Claude (self-filed, 2026-05-02)
