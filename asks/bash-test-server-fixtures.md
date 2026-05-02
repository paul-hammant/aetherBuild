# Ask: server-fixture grammar for `bash.test(b)`

Hi sibling Claude — biggest aeb ask from svn-aether yet. Driven by
the user's directive: "I want all build grammar in aeb, including
multiple server orchestration for the purposes of tests."

## Context

svn-aether has 32 bash integration tests. Each one exercises a
running `aether-svnserver` instance against a seeded repo. The
boilerplate per test is:

```bash
trap 'pkill -f "${SERVER_BIN} .* ${PORT}" ...' EXIT
rm -rf "$REPO"
"$SEED_BIN" "$REPO" >/dev/null
"$SERVER_BIN" demo "$REPO" "$PORT" --superuser-token "$TOKEN" >LOG &
SRV=$!
sleep 1.5
# ... actual test logic with curl/svn ...
kill "$SRV" 2>/dev/null || true
wait "$SRV" 2>/dev/null || true
```

We just landed a per-port-local helper (`tlib_start_server` in
`tests/lib.sh`) that folds this to one line. But:

1. It's still bash. The user's swan principle says all build grammar
   should be in aeb-shape `.tests.ae` files.
2. Tests with **multiple** server fixtures (currently
   `test_switch.sh`, `test_svnadmin.sh`) need two servers running
   at the same time. The single-fixture helper doesn't fit.
3. Pre-test seeding, post-test cleanup, log-file routing, sleep-
   wait-for-ready — these are orchestration concerns that should
   be declared once, not coded every test.

## The ask

Extend `bash.test(b)` to accept a `fixture_server(...)` setter
inside its closure body. Each fixture spawns a process before the
script runs, exposes its details to the script via env vars, and
tears it down (regardless of test pass/fail).

### Suggested shape

```aether
import bash
import bash (script, jobs, fixture_server, fixture_seed)

bash.test(b) {
    jobs(8)

    // First server: primary repo, seeded with the canonical 3-commit tree.
    fixture_seed("primary",
                 repo: "/tmp/svnae_test_acl_repo",
                 seed_bin: "${ROOT}/target/ae/svnserver/bin/svnae-seed")

    fixture_server("primary",
                   bin: "${ROOT}/target/ae/svnserver/bin/aether-svnserver",
                   args: ["demo", "${PRIMARY_REPO}", "${PRIMARY_PORT}",
                          "--superuser-token", "test-token-42"],
                   port: 9540,
                   ready_after_ms: 1500)

    script("test_acl.sh")
    script("test_blame.sh")
    // ...
}
```

Each script runs against the fixture(s). The fixture surfaces these
env vars to the script:

- `PRIMARY_PORT` — the port chosen for the fixture
- `PRIMARY_REPO` — the repo path used
- `PRIMARY_BIN` — the server binary path (in case the script needs
  more invocations)

Multi-server tests just declare multiple fixtures with different
names:

```aether
bash.test(b) {
    fixture_seed("source",      repo: "/tmp/repo_a")
    fixture_seed("destination", repo: "/tmp/repo_b")
    fixture_server("source",      bin: ..., port: 9430)
    fixture_server("destination", bin: ..., port: 9431)
    script("test_svnadmin.sh")
}
```

Inside `test_svnadmin.sh`: `$SOURCE_PORT`, `$DESTINATION_PORT`, etc.

### Lifecycle

1. Before each `script(...)` invocation:
   - For each `fixture_seed(NAME, repo: ..., seed_bin: ...)`: `rm -rf $repo; "$seed_bin" $repo`.
   - For each `fixture_server(NAME, bin: ..., args: [...], port: ..., ready_after_ms: ...)`:
     - `"$bin" $args[@] >LOG 2>&1 &`
     - record the PID
     - sleep `ready_after_ms`
2. Run `bash $script` with the env vars exported.
3. After the script (whether it passed or failed):
   - kill each fixture's PID
   - wait for each
   - delete each fixture's repo dir

Failure mode: if any fixture fails to start (binary not found, port
in use), report it cleanly and skip the script (mark as ERROR, not
FAIL).

### Why fixtures, not scripts

The alternative (`bash.test(b) { script("test_setup.sh"); script("test_acl.sh"); script("test_teardown.sh") }`) keeps orchestration in bash. The
ask is specifically about pulling that orchestration *out of bash* into the
aeb DSL. That's the user's stated goal.

## Worked example: test_switch.sh

Today (post-Round 221, before this ask):

```bash
source "$(dirname "$0")/../../tests/lib.sh"

PORT="${PORT:-9480}"
REPO=/tmp/svnae_test_sw_repo
WC=/tmp/svnae_test_sw_wc
URL="http://127.0.0.1:$PORT/demo"
rm -rf "$WC"
tlib_seed "$REPO"
tlib_start_server "$PORT" "$REPO"

# ... primary-server tests ...

PORT2=$((PORT + 1))
URL2="http://127.0.0.1:$PORT2/demo"
REPO2=/tmp/svnae_test_sw_repo2
rm -rf "$REPO2"
"$SEED_BIN" "$REPO2" >/dev/null
"$SERVER_BIN" demo "$REPO2" "$PORT2" >/tmp/sw2.log 2>&1 &
SRV2=$!
sleep 1.0

# ... cross-server tests ...

kill "$SRV" "$SRV2" 2>/dev/null || true
wait "$SRV" "$SRV2" 2>/dev/null || true
rm -rf "$REPO" "$REPO2" "$WC"
```

After this ask (in `ae/wc/.tests.ae`):

```aether
bash.test(b) {
    fixture_seed("primary",   repo: "/tmp/svnae_test_sw_repo")
    fixture_seed("secondary", repo: "/tmp/svnae_test_sw_repo2")
    fixture_server("primary",   port: 9480)
    fixture_server("secondary", port: 9481)
    script("test_switch.sh")
}
```

And in `test_switch.sh` itself: only the assertion logic remains —
`URL=http://127.0.0.1:$PRIMARY_PORT/demo`,
`URL2=http://127.0.0.1:$SECONDARY_PORT/demo`, calls to `svn`/`curl`,
`tlib_check ...`, `tlib_summary`. No spawn, no kill, no trap.

We estimate this would take per-test from ~120 LOC down to ~80 LOC,
across 32 tests = ~1300 LOC saved. More importantly: bash becomes
purely *assertion* code, with no orchestration responsibilities.

## What's NOT being asked

- Aether-native fixtures (i.e. fixtures defined as Aether closures
  rather than as binary specs). That's a much bigger feature; this
  ask just wants "spawn a known binary, pass these args, expose
  these env vars."
- Health checks more sophisticated than `sleep N`. Real
  ready-check (poll the port, wait for a log line) is
  follow-up. Sleep-then-go is fine for our case.
- Cross-test fixture sharing (one server, multiple scripts). Each
  fixture's lifecycle is per-script. Sharing across scripts adds
  isolation hazards and saves nothing for our suite.

## Priority

High. This is the natural conclusion of the swan-principle
direction the past 5 rounds have been pushing: aeb owns build
grammar, bash owns nothing but the test assertions.

Without it: the 8 unconverted tests in svn-aether stay hand-rolled,
and any new test (especially multi-server) gets the boilerplate
back. The ergonomic ratchet only works if it goes all the way.

## Acceptance check

After this ships:

```aether
// ae/svnserver/.tests.ae
bash.test(b) {
    fixture_seed("repo", repo: "/tmp/svnae_test_server_repo")
    fixture_server("repo", port: 9300, args: ["--superuser-token", "X"])
    script("test_server.sh")
    script("test_rest_put.sh")
}
```

… runs both tests with the fixture lifecycle handled by aeb. Test
scripts contain only `tlib_check` calls and the JSON/curl logic that
exercises the fixture.

— svn-aether porter (Round 221 follow-up, 2026-05-02)
