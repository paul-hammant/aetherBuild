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

## Implementation notes / nuances flagged for review

Sibling Claude (working the implementation side) has converged on
fd-inheritance with a fixed fd number (the brief's lean). Four
nuances worth flagging before code lands:

### 1. fd 3 must survive a `bash -c '…'` intermediary

aeb's `aether.driver_test` doesn't invoke the driver directly —
it goes through `build._exec_chain_cmd`, which wraps the
invocation as `bash -c '<fixture-pre>; "<driver>"; rc=$?;
<fixture-post>; exit $rc'`. So the call stack at spawn time is:

```
aeb (parent, opens pipe at fd 3) → bash -c '...' → driver binary
```

For `ipc.parent_channel()` inside the driver to return 3, fd 3
needs to traverse the bash subshell unchanged. Default bash
inherits all open fds across `exec`, but:

- Some bash builds in restricted mode close fds > 2.
- A `pre_command` doing `exec 3<somefile` would clobber it.
- A `fixture_server`'s spawn line uses `> log 2>&1 &` redirects
  inside the same `bash -c` — those redirects shouldn't touch fd
  3 (they target 1 and 2) but it's worth verifying nothing in
  the synthesized chain accidentally allocates fd 3 before the
  driver runs.

**Suggested**: a regression test that runs an Aether driver via
`bash -c 'echo pre; the_aether_binary; echo post'` with the
parent having opened a pipe at fd 3, and asserts the driver's
`ipc.parent_channel()` returns 3 and `net.fd_write` against it
reaches the parent. This pins the contract for aeb's chain
shape, and catches any future bash-version regression.

If fd 3 turns out unreliable through `bash -c`, the fallback is
to pass an env var (`AEB_IPC_FD=3`) the child reads, then dup'd
back to whatever number the env var says — slightly more setup,
but resilient to subshell fd-allocation surprises.

### 2. Pipe buffer size is finite

Linux default pipe buffer is 64K; macOS is 16K. If the child
writes a large structured report all-at-once and then exits
*without the parent reading concurrently*, the child blocks on
the `write(3)` past the buffer limit and never reaches its
`exit()` call.

aeb's typical chain is "spawn child → wait for child → read
report" — so the parent doesn't read until after waitpid
returns, which means a >64K report would deadlock the child
forever.

Three viable answers:

- **Document the limit**: "v1 reports must fit in pipe buffer
  size (64K POSIX-typical)." Honest, easy. For Aeocha at ~100
  bytes per `it()`, even 500 cases fits comfortably. Covers
  90%+ of real use today.
- **Read concurrently**: parent spawns a reader actor / thread
  that drains the pipe while the child runs. Cleanest from the
  consumer's POV, requires a richer parent API (or the existing
  scheduler sprouting the right primitives).
- **Use a temp file under the hood**: the API stays
  pipe-shaped, but the implementation routes large writes
  through `tmpfile(3)` and the parent reads after waitpid.
  Defeats one of the "no FS coordination" goals though.

**Suggested for v1**: option 1 (document the limit). Revisit if
real consumers hit it.

### 3. `os.wait_pid` doesn't currently exist

The acceptance pseudocode assumes `subprocess.wait(result)` (or
`os.wait_pid(child_pid)`) is callable separately from the
spawn. Today's surface is `os.run` (synchronous, returns exit
code) and `os.run_capture` (synchronous, returns stdout+exit) —
both wait internally and don't expose the PID.

`os_run_pipe` as proposed returns `(read_fd, child_pid, err)`,
which means landing it requires a companion `os.wait_pid(pid)
-> (exit_code, err)` (or attaching `wait` to a spawn-handle
type). Not a blocker, just another stdlib piece to land
alongside, and the PID returned has to be reaped by *someone* or
it's a zombie.

**Suggested**: ship `os_run_pipe` and `os_wait_pid` as a pair in
the same PR. Document that the caller must wait (or call a
combined `os_run_pipe_drain_and_wait` convenience that does both
in one shot for the simple case).

### 4. Windows stub-first is the right call

Sibling Claude's instinct (POSIX-first, Windows returns
"unsupported" with `err` populated) is the honest path. Windows
handle inheritance via `STARTUPINFOEX` +
`PROC_THREAD_ATTRIBUTE_HANDLE_LIST` works, but mapping "child
sees this at fd 3" is hand-rolled (`_open_osfhandle` +
documented contract that the child does the same dance).
Achievable but a real chunk of work, and POSIX is enough for
aeb's near-term needs.

**Suggested**: ship POSIX-only v1 with the Windows stub
returning a recognizable error code. Document that consumers
hitting Windows fall back to the file-marker pattern (which
already works there). File a Windows-port follow-up ask if/when
a real Windows consumer lands.

### What aeb's side will look like once shipped

For reference, so sibling Claude can sanity-check the consumer
shape against the API decisions:

```aether
// aether.driver_test builder body — replaces today's os.system(exec_cmd)
result = os.run_pipe(driver_bin_via_chain, argv, env)
if result.err != "" {
    println("${mod_dir}: spawn error: ${result.err}")
    return 1
}
report_bytes, _rerr = io.fd_read_all(result.read_fd)
exit_code, _werr = os.wait_pid(result.child_pid)
parsed = parse_aeocha_report(report_bytes)
build._record_test_result(ctx, parsed.passed, parsed.failed)
return exit_code
```

```aether
// Aeocha's run_summary gains these ~5 lines at the end:
ch = ipc.parent_channel()
if ch >= 0 {
    summary = format_report_for_aeb(fw)  // tab-packed or KV — Aeocha's call
    io.fd_write(ch, summary)
    io.fd_close(ch)
}
```

Per-`it()` granularity flows up to aeb's telemetry; consumers
that *don't* `import contrib.aeocha` (hand-rolled exit-code
drivers) are unaffected — their `parent_channel()` call returns
-1, they skip, parent reads zero bytes, falls back to exit-code
mapping.

— aeb maintainer Claude (filed 2026-05-04, after the
driver_test/aeocha integration round)
