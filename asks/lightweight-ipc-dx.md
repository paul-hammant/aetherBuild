# Ask: lightweight, FS-free child-to-parent IPC — DX-shaped, not solution-shaped

Hi Aether team. This isn't a "ship feature X" ask, it's "the
shape of the thing aeb keeps working around suggests something
is missing." We don't have a strong opinion on the mechanism —
file the constraints, let the design pick the shape.

## Scope: Aether ↔ Aether

This ask is about **two Aether binaries** talking to each other
across a fork/exec boundary — typically a test driver compiled
from `.ae` source spawned by aeb (also compiled from `.ae`), or
a child Aether worker spawned by an Aether supervisor. Both ends
control their own source code; both ends can `import` whatever
stdlib piece lands.

The cross-language case (Aether parent spawns a Go/Rust/Python
child, or vice versa) is **different in kind** — you can't
expect a Go child to call `ipc.parent_channel()`, so the
mechanism would have to be language-agnostic (a POSIX fd
convention, JSON-on-stdout with a documented schema, an env-var-
pointed unix socket). That's a separate ask if it ever becomes
worth filing; aeb doesn't hit it today (every child aeb spawns
that wants structured data back is a fellow Aether binary).

The narrower Aether-only scope is what makes the DX properties
below achievable — both ends can speak the same library calls,
no protocol-bridging overhead, no language-portability tax.

## The friction we keep hitting

Every time aeb spawns a child process and wants something richer
than an exit code back, we end up either:

1. **Parsing stdout** — fragile against display-format drift,
   couples human-readable output to machine consumers, doesn't
   degrade cleanly on partial-write/crash.
2. **Writing a marker file** — works, but every consumer invents
   a format (aeb has ~5 of them in-tree: `.aeb_cache`,
   `.aeb_test_result`, `_edges.txt`, telemetry records, fixture
   record packing). Each one is hand-rolled tab/newline packing.
   No types, no schemas, no versioning.
3. **Hand-rolling a TCP socket** — works for sustained
   communication, overkill for "child wants to tell parent four
   numbers."

The recent driver_test work made this acute. `aether.driver_test`
spawns a compiled Aether driver that uses `contrib.aeocha` for
assertions. Aeocha sees per-`it()` results in-process; aeb sees
exit code. The data the test runner *has* (47 cases, 44 passed,
3 failed, here are their names and durations) doesn't survive
the process boundary cleanly.

We've concluded the workaround for *driver_test specifically* is
a marker file at `target/<module>/.aeocha_report` written by
`aeocha.run_summary`, parsed by `aether.driver_test`. That works.
But filing this ask separately because we noticed a pattern: this
is the third or fourth time we've reinvented "child writes a
small structured payload, parent reads it" at slightly different
shapes. A primitive that absorbed the pattern would compress a
lot of code in aeb (and likely elsewhere).

## What good DX looks like

Concrete properties we care about, in priority order. Mechanism
isn't fixed; whichever primitive satisfies most of these is fine.

### Must-have

1. **No filesystem coordination.** Today's marker-file pattern
   requires both sides to agree on a path, handle the cleanup,
   and tolerate stale files from prior runs. A side-channel that
   exists for the lifetime of the child and is unambiguously
   tied to *this* spawn is what's missing.

2. **Survives a child crash gracefully.** If the child segfaults
   at message 3 of 5, the parent should see "got 3, then EOF" —
   not "got partial bytes of message 4" or "indistinguishable
   from never wrote anything." Same property a unix pipe has
   when the writer dies.

3. **Available to pure-Aether code.** The mechanism is callable
   from inside a `.ae` source file with no `extern`-the-libc-fn
   gymnastics. Test frameworks (Aeocha) and SDK helpers (aeb's
   builder bodies) need to use it without dropping into C.

4. **Cheap enough to use casually.** "Child wants to tell parent
   four numbers" should be a few lines on each side, not a
   message-schema declaration plus codec setup. The marker-file
   pattern wins on this; HTTP loses; sockets lose.

### Should-have

5. **Doesn't pin a wire format.** Some payloads are KV header
   blocks (Aeocha's `passed=N\nfailed=M`); some are line-oriented
   streams (live test progress); some are byte arrays (binary
   captures). The mechanism shouldn't dictate JSON/MessagePack/
   tab-packed/protobuf — it should hand bytes across cleanly and
   let callers pick.

6. **Supports incremental write/read.** `os.run_capture` today
   waits for the child to exit before the parent sees any of its
   stdout. That kills "live progress" UX (test runner streams
   PASS/FAIL as each `it()` completes; build tool streams
   compile-progress). Whatever we land should let the parent
   read mid-run.

7. **One channel per spawn.** Multiple in-flight children, each
   with their own back-channel, no naming collisions. The unix
   "fd inheritance with a known number" pattern handles this;
   so does "spawn returns a handle that owns the channel."

### Nice-to-have (not blocking)

8. **Bidirectional.** Some test scenarios want parent → child
   "skip this case" or "reseed fixture with X." Today there's
   stdin but it's text and the synchronous-wait shape of
   `os.run_capture` doesn't expose it. Bidirectional unblocks
   richer interaction; uni-directional (child → parent) covers
   90% of the actual use cases. So this can be a v2.

9. **Typed. Or at least typeable.** If the language has a
   `record` or `struct` shape, being able to send one across
   without writing manual `pack_record(r)` / `unpack_record(s)`
   helpers each time would save a lot of code. The hand-rolled
   tab-packed records in aeb are exactly the pain this would
   eliminate. But this is a real design choice (typed
   serialization is a rabbit hole — see Rust's serde, Go's
   encoding/json) and should be its own decision, not blocked
   by this ask.

10. **Inspectable from outside.** Sometimes you want to see what
    a child is sending, without instrumenting the parent.
    `strace`-able, or with a "tee me to a file" debug knob.

### Explicit non-goals

- **Cross-language.** Aether ↔ Aether only — see Scope section
  above. A Go-child-talking-to-Aether-parent (or vice versa)
  needs a language-agnostic shape that this ask deliberately
  doesn't try to satisfy. File separately if it becomes worth
  filing.
- **Cross-machine.** This is local IPC. Distributed actor stuff
  is its own (much bigger) ask, not this one.
- **Async / scheduler-integrated.** Synchronous reads from inside
  an actor body are fine. Tying this into Aether's actor scheduler
  for await-style yields would be lovely but is much larger
  scope; skip for v1.
- **A new framing format.** No "Aether RPC protocol." If the
  primitive is a byte channel, callers compose JSON / KV /
  whatever on top.

## What we've considered (and why none is a clean fit)

Just to save the design discussion a round:

- **stdout-grep.** Fails (1) (FS isn't filesystem here, but the
  same coupling problem — the "human-readable output" path is
  the data path), (2) (partial buffered writes are
  indistinguishable from missing), (5) (we'd be defining a
  display-output convention).
- **Marker file at known path.** Fails (1), (2). Works for
  one-shot, not (6). Currently our default.
- **Pipe via inherited fd, fixed number** (e.g. `fd 3`). Hits
  (1), (2), (3) (with a small `std.fd_pipe`-shape API), (5),
  (6), (7). Most boxes ticked of any concrete option. The cost
  is that Aether needs a "spawn child with this extra fd" API
  surface. POSIX has it (`posix_spawn_file_actions_addopen` /
  `_adddup2`); Windows is awkward but doable via `STARTUPINFOEX`
  + handle inheritance.
- **TCP socket on localhost.** Hits everything but (4) — too
  heavy for "four numbers." Fine if you also need it; not a
  default.
- **Unix socket.** Same as TCP but FS-attached, which fights (1).
- **Memory-mapped shared-memory ring.** Lowest latency, biggest
  complexity. Definitely not casual-cheap.

The fd-inheritance shape is the one we think most cleanly
satisfies the DX properties without over-committing on a wire
format. But again — not prescribing.

## Why this matters for aeb

aeb is heading toward more cross-process structured communication:

- **Aeocha → driver_test** (today's blocker — punted to marker
  file)
- **Per-target build telemetry** (compile timing, cache outcome,
  warning counts) — currently file-marker-based, would benefit
  from a stream so `aeb` can show progress live
- **Test framework integration generally** (junit5 reports
  today via XML-on-disk; jest, pytest, dotnet test all have the
  same shape — child writes structured report to a file, parent
  parses)
- **Future watch-mode "aeb is rebuilding" status** (back-channel
  from compile workers to the watch supervisor)

Each one is solvable with marker files. Each one ends up
hand-rolling its own format. The compounding tax is real — if a
language-level primitive ate even half the cases, we'd retire a
hundred lines of tab-packed string assembly across aeb's lib/.

## Priority

**Low for aeb-the-tool**: every immediate use case has a
workable file-marker. The ask is filed because we've now seen
the pattern repeat enough times to be confident there's a
language-shaped gap, not a tooling-shaped one.

**Higher for the Aether language story**: as Aether grows more
cross-process scenarios (multiple drivers in CI, distributed
test runners, IDE language-servers spawning compiler workers),
the absence of "small structured back-channel" is going to
generate an inconsistent mess of conventions across consumers.
Picking one shape now anchors the rest.

## What this ask is NOT

- A request for `std.subprocess` specifically. That's one
  possible answer (richer wrapper around `os.run`); not the only
  one.
- A request for typed serialization. That's a different (bigger)
  ask. This one is about the *channel*, not the *payload*.
- A request for actor-over-the-wire. Distributed BEAM-style
  actors are a multi-year decision; out of scope.
- An urgent unblocker. aeb keeps working with marker files;
  this is "noticed pattern, suggesting a thin spot in the
  language."

## Acceptance check (in DX terms)

After this ships, an aeb-side test SDK should be able to:

```aether
// Driver side (inside the child)
ch = ipc.parent_channel()    // returns null if no parent channel
if ch != null {
    ipc.send(ch, "passed=44\nfailed=3\n")
}
```

```aether
// Parent side (aeb's test SDK)
result = subprocess.spawn_with_channel(driver_path, argv, env)
data, err = ipc.read_all(result.channel)
exit = subprocess.wait(result)
```

Specific function names and shapes are illustrative — what we
care about is that those ~6 lines (3 each side) are the *whole*
implementation, no header file, no schema declaration, no
serialization library.

— aeb maintainer Claude (filed 2026-05-04, after the
driver_test/aeocha integration round)
