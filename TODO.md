# Aether Build — TODO

## SDK function configuration

Today every SDK function takes just the context — zero config, pure convention:

```aether
build.javac(b)
build.kotlinc(b)
build.go_build(b, "c-shared", "libgonasal.so")
```

Three levels of configuration, progressively more expressive:

### Level 1: Zero config (done)

Convention handles standard layouts. No args needed.

```aether
build.javac(b)
```

### Level 2: Named args

Common overrides as named parameters. No trailing block needed.

```aether
build.javac(b, source: "17", target: "17")
build.kotlinc(b, jvm_target: "17")
build.go_build(b, mode: "c-shared", output: "libgonasal.so")
build.cargo_build(b, lib: "libvowelbase.so", profile: "release")
build.shade(b, main_class: "com.example.Main", output: "app.jar")
```

### Level 3: Trailing block DSL

Full control via Aether's `_ctx` invisible context injection — the same
mechanism as `sandbox() { grant_fs_read(...) }`.

```aether
build.javac(b) {
    source_version("17")
    target_version("17")
    annotation_processor("lombok")
    extra_flags("-Xlint:all", "-Werror")
    encoding("UTF-8")
}

build.kotlinc(b) {
    jvm_target("17")
    api_version("1.9")
    extra_flags("-Werror")
}

build.go_build(b) {
    mode("c-shared")
    output("libgonasal.so")
    flags("-ldflags", "-s -w")
    env("CGO_ENABLED", "1")
    tags("netgo")
}

build.cargo_build(b) {
    lib("libvowelbase.so")
    features("jni", "serde")
    profile("release")
    extra_flags("--jobs", "4")
}

build.tsc(b) {
    strict(true)
    target("ES2022")
    module_kind("NodeNext")
}

build.junit(b) {
    includes("**/*Tests.class")
    excludes("**/*IntegrationTests.class")
    jvm_args("-Xmx2g", "-ea")
    parallel(true)
    timeout(300)
}

build.mocha(b) {
    timeout(5000)
    reporter("spec")
    grep("unit")
}

build.shade(b) {
    main_class("com.example.Main")
    output("app.jar")
    exclude("META-INF/*.SF", "META-INF/*.DSA")
    relocate("com.google.common", "shaded.guava")
}
```

Each setter stores config in the `_ctx` map. The SDK function reads
the map after the block runs and translates to compiler flags.

### Implementation plan

1. Named args first — add optional parameters to existing SDK functions.
   `javac(ctx, source, target)` with defaults. Quick, no language changes.
2. Trailing block second — requires the SDK function to accept a block,
   run it to populate a config map, then use the config. Uses Aether's
   existing builder DSL + `_ctx` injection.
3. Each config setter is a function with `_ctx: ptr` as first param
   (invisible injection): `source_version(_ctx: ptr, ver: string)`.

## Runner improvements

### Target filtering (done)

`aeb <target>` builds only the named target and its transitive deps.

```bash
aeb java/applications/monorepos_rule          # just this + deps
aeb javatests/components/vowelbase            # auto-detects test
aeb --dist java/applications/monorepos_rule   # compile + package
```

### `scan()` grammar function — glob-based dep discovery

A DSL function inside a composite `.ae` file that expands to many
`build.dep()` entries via a glob pattern. Enables named bundles
composed from globs, without any CLI flag.

```aether
// .all_tests.ae at the repo root
import build

main() {
    b = build.start()
    build.scan(b, "**/*.tests.ae")
}
```

```aether
// .smoke-tests.ae
main() {
    b = build.start()
    build.scan(b, "javatests/components/**/*.tests.ae")
    build.scan(b, "csharptests/components/**/*.tests.ae")
}
```

```aether
// .integration.ae — compose scans with explicit deps
main() {
    b = build.start()
    build.dep(b, ".smoke-tests.ae")
    build.dep(b, ".release-builds.ae")
    build.dep(b, "end-to-end/.tests.ae")
}
```

`aeb .all_tests.ae` runs all matching tests plus their transitive deps.
Named targets become composable `.ae` files — no CLI flags, same DAG,
same topo sort, same sparse-checkout story via `gcheckout`.

**Implementation:**
- Extract `scan()` calls during the same grep pass as `dep()` in
  `aeb`'s `extract_deps`
- For each pattern, run `find . -path "./$pattern" -type f` (respecting
  `.aebignore`)
- Concatenate matched paths into the file's `FILE_DEPS` list
- BFS + topo sort handle the rest
- `build.scan()` is a runtime no-op (like `build.dep()` — both are
  statically extracted by the runner, not executed)

**Gotchas:**
- Glob must respect `.aebignore`
- Zero matches should be an error (typo protection)
- A file shouldn't match its own `scan()` pattern (self-ref loop)

### Parallel execution

Independent modules in the DAG can build concurrently. The visited map
needs thread-safe access (mutex or atomic). Aether actors are a natural
fit — one actor per module, message-passing for completion.

### ~~Affected-target detection~~ (done)

Shipped via `aeb --since <ref>` and `aeb --print-affected <ref>`.
Walks reverse-dep edges from changed-file owning targets. Source-
to-target ownership rule: nearest enclosing directory with a
dot-prefixed `.ae` build file. Multiple build files in one dir
all share ownership of source files in that dir (the round-218
multi-binary case).

Useful CI shape: `aeb --since main` (or `--since origin/main`)
runs only the targets affected by the PR's changes. Telemetry
shows what built; cache integration ensures hits stay hits even
when the broader CI pipeline rebuilds something else.

### Local content-addressed cache (partially done)

`lib/cache/` ships sha256+zlib content-addressable storage under
`$AEB_CACHE_DIR` (default `~/.aeb/cache`). Wired into:
- `lib/maven` — resolved classpath
- `lib/aether` — manual-path link binary
- `lib/java` — javac + javac_test classes trees (tar+zlib)

Other SDKs still use mtime-only (or no skip at all). Remaining wiring,
roughly in order of expected wall-time savings:

- `lib/kotlin` (kotlinc — closest in shape to lib/java)
- `lib/scala` (scalac — same shape)
- `lib/ts` (tsc)
- `lib/dotnet` (dotnet build)
- `lib/go` (go build — `go` already has its own cache; aeb wrap value lower)
- `lib/rust` (cargo — cargo already has its own cache; aeb wrap value lower)
- `lib/clojure`, `lib/python` (lower priority)

Each SDK's cache wiring is the same shape: hash inputs (sources +
classpath + flags + toolchain version) → probe `cache.get` →
restore artifact on hit, run on miss → `cache.put`. See `lib/aether`
and `lib/java` for the reference implementations.

### Remote build cache

The next layer up from local caching: share artifacts across
machines (Bazel, Gradle, Nx, Turborepo all do this). Implementation:
hash inputs → check remote store (S3, GCS, local server) → download
artifacts or build locally → upload result. The cache primitives in
`lib/cache/` are the substrate; needs a backend protocol and auth.

### ~~Build graph visualization~~ (done)

Shipped in commit `be2d97c`. `aeb --graph` (DOT default) /
`aeb --graph mermaid` for inline-Markdown output. Pipe DOT to
`dot -Tsvg` or paste Mermaid into a `\`\`\`mermaid` fence.

### Watch mode

`aeb --watch <target>` — rebuild on source file change using
inotifywait or fswatch. Useful for dev loops. Only rebuild affected
targets (combines with affected-target detection).

### User-defined builders

Currently all builders live in `lib/*/module.ae` shipped with aeb.
Users should be able to define project-local builders in their own
`.ae` files without forking the SDK. A `local_lib/` directory
alongside `.aeb/lib/` that the compiler searches first.

### Lockfiles for reproducible dep resolution

Maven/NuGet/npm versions can shift between builds (ranges, snapshots,
latest). A `dep.lock` file recording exact resolved versions ensures
reproducible builds. The resolver already computes the full closure —
just needs to write it out and check it on subsequent runs.

### Build telemetry (partially done)

Per-module wall-time + cache outcome rendered as a `[telemetry]`
block at the end of every build. The orchestrator (generated by
`tools/gen-orchestrator.ae`) records (label, type, wall_ms, cache)
per module into an in-memory list and hands it to
`build.render_telemetry` for the stdout renderer.

Cache outcomes wired in three SDKs (`lib/aether`, `lib/java`,
`lib/maven` indirectly via java); other SDKs report `n/a`.

What's left:

- **More cache integration → better telemetry**: each SDK that
  grows `cache.get`/`cache.put` integration also gets honest
  `[hit]`/`[miss]` reporting in telemetry for free.
- **Alternative renderers**: file dump (JSON, JSON Lines) for CI
  consumption; HTML/JS for a richer view (force-directed graph
  with node sizes by wall-time, colors by cache outcome). The
  records list is the single source of truth; renderers walk
  it without modifying.
- **Cross-build aggregation**: "what's our cache hit rate over
  the last 100 builds?" Needs persistent storage (per-build
  telemetry file under `target/_aeb/` or central `~/.aeb/log/`).
  Postpone until a real consumer asks.
- **Per-builder timing inside an SDK**: today telemetry sees
  per-target wall-time. Inside `aether.program(b)`, regen vs
  aetherc vs gcc isn't separated. Useful for SDK profiling;
  needs each SDK to emit phase markers. Defer.

#### Telemetry — the four-tier vision

Today's implementation is the degenerate case of a much larger
shape. As aeb grows parallel module execution, multi-process
worker fan-out, and remote build, the telemetry channel evolves
through four tiers. Each tier is its own session of work; the
*data model* is forward-compatible across all four (records can
gain fields without breaking renderers), but the *transport*
between informer and consumer must change at each step.

A node informs the graph "I started" and later "I ended,
extra-info...", with arbitrary other informers writing
interleaved between. The data model captures that explicitly
with two-sided events:

```
{ kind: "start", label: "ae/svnserver:seed", at_ns: ..., type: "build" }
{ kind: "end",   label: "ae/svnserver:seed", at_ns: ..., cache: "hit", ... }
```

The current "records" shape (one map per completed module) is
the synchronous fold of those events: start arrived → in-progress,
end arrived → record completed. When transport stays single-
process synchronous, folding can happen at append time and the
events are never materialised (today's case).

##### Tier 0 — single process, synchronous (DONE)

In-memory list, records appended as each module finishes, lost
on exit. The orchestrator (one process) is the only writer. No
synchronization needed. What this commit ships.

##### Tier 1 — single process, multi-thread

Triggered when aeb grows parallel module execution (already on
the TODO under "Parallel execution"). Multiple worker
threads/actors build independent DAG branches concurrently.

Transport: a `TelemetryActor` owns the records list. Workers
`send Started{label,...}` / `send Ended{label, cache, ...}`.
No locks; message-passing is the synchronization. Aether's
actor model is the natural fit.

Inflection point: the records→events refactor lands here. The
TelemetryActor's mailbox IS the event stream; it folds events
into records as messages arrive, hands the records to the
existing `render_telemetry` at session end. Renderer signature
unchanged.

`clock_ns()` is monotonic and meaningful between threads on the
same host (sub-microsecond drift). Wall-time semantics survive.

##### Tier 2 — multi process, same machine

Triggered when aeb spawns subprocess workers (e.g. parallel
`aetherc`/`javac`/`gcc` invocations from `tools/aeb-link.ae`,
or per-target subprocess fan-out). All workers share the
filesystem and the wall clock.

Transport: append-only file log at `target/_aeb/telemetry.jsonl`.
Each subprocess opens the file in `O_APPEND` and writes one
event per line. POSIX guarantees writes ≤ PIPE_BUF (4096 bytes
on Linux, 512 minimal POSIX) are atomic against concurrent
writers; events fit comfortably. No locks, no coordination.
Standard practice — Bazel BES, Buck2, Nx Cloud all use
append-only logs.

Workers find the log via env var (`AEB_TELEMETRY_LOG=...`)
inherited from the orchestrator. The orchestrator reads the
log post-build, folds events to records, hands to the renderer.

`clock_ns()` is monotonic per-process. Cross-process timing is
accurate to ~microseconds (different processes have their own
monotonic baselines). For "what's slow?" this doesn't matter.

##### Tier 3 — multi machine / VM / container

Triggered when aeb grows remote build execution. Workers run
on different hosts. No shared filesystem, no shared clock.

Transport: a network-reachable telemetry sink. Industry
standard is Bazel's Build Event Stream (BES) — gRPC streaming
of protobuf events, well-defined semantics for incomplete
builds, reusable consumers (BuildBuddy, EngFlow). Don't roll
own; adopt BES when the time comes.

Each event gains identity discriminators: `host`, `pid`, `tid`,
container/VM ID. The records→events refactor from Tier 1 means
adding fields is a no-op for renderers (they ignore unknown
keys).

Clock skew is real: cross-host timing requires either NTP-bound
wall-clock with accepted skew, or logical clocks (Lamport,
vector). Per-host monotonic order is preserved; cross-host
causal ordering is the harder problem. BES sidesteps by
standardising on UTC wall-clock with skew tolerance.

Backpressure: if the sink is slow or unreachable, workers can't
block on inform. Local buffer + async flush + drop-on-overflow
counter (Bazel's pattern).

##### Forward-compatibility checklist

The current Tier 0 implementation should not lock decisions
that constrain Tiers 1-3. Status:

- ✓ Records are `map<string, string>` (open-ended; can grow
  fields like `host`, `pid`, `cache`, `exit_code` without
  changing the renderer signature).
- ✓ Renderer takes a list and a total — same shape as
  events-folded-to-records will produce in Tier 1+.
- ✓ Cache markers are per-target files, readable across
  process boundaries — already Tier 2 compatible (a worker
  subprocess writing the marker is observable to the
  orchestrator parent).
- ✗ The events shape is not yet a thing. Tier 1's records→
  events refactor IS the event-protocol introduction. ~50
  LOC of orchestrator + actor + renderer; defer until the
  parallel-modules feature drives it.

##### Where each tier's preconditions live

| Tier | Blocked on | Estimated session work |
|---|---|---|
| 0 → 1 | Parallel module execution (TODO above) | ~half-session, riding the parallel-modules feature |
| 1 → 2 | Subprocess fan-out in aeb-link or per-SDK | ~half-session, tied to whichever feature spawns subprocesses |
| 2 → 3 | Remote build execution + BES adoption | Multi-session; the telemetry transport piece is the smaller part |

### Cross-compilation targets

Building for a different OS/arch than the host. Relevant for Go
(`GOOS=linux GOARCH=arm64`), Rust (`--target aarch64-unknown-linux-gnu`),
.NET (`-r linux-arm64`). DSL: `target_platform("linux-arm64")`.

### Hermetic dependency pinning

aeb currently trusts whatever version the resolver returns. For
production builds, pin every transitive dep to an exact version with
integrity hashes (like `package-lock.json` or `go.sum`). Detect drift
between lock file and resolved deps.

## Aether compiler issues to fix upstream

- [ ] `module` as a variable name silently breaks codegen — should be
      a reserved word or the codegen should handle it
- [ ] Module function return type inference fails when first `return`
      is a literal `0` — infers `int` instead of `ptr`. Workaround:
      return `map_get(m, "_null_")` to force ptr type.
- [x] `MAX_MODULE_TOKENS` was 2000, needed 20000 for the build SDK.
      (Fixed: bumped to 20000 in `aether_module.h`)
- [x] Module function return types not inferred across module boundaries.
      (Fixed: `lookup_symbol` → `lookup_qualified_symbol` in `typechecker.c`,
       with void/unknown guard to avoid regressing pure-Aether return types.
       Regression test added: `tests/integration/module_return_types/`)
- [ ] `const char*` vs `void*` warnings on every `map_put`/`list_add`
      call. The codegen should emit casts for `string` → `ptr` params.
- [ ] **macOS link step fails with duplicate symbol errors.** Root
      cause: the Aether compiler emits imported module functions (e.g.
      `rust_cargo_build`, `build__mkdirs`) into every translation unit
      without a `static` qualifier. GNU ld silently dedupes them via
      the `-Wl,--allow-multiple-definition` flag currently hard-coded
      in `tools/aeb-link.ae:294`. That flag is GNU ld only — Apple's
      ld64 rejects it, and `-multiply_defined,suppress` was removed in
      Xcode 15. Consequence: a full `./aeb` multi-module run fails
      with duplicate-symbol errors on macOS.
      Two fixes, in order of preference:
      (1) **Upstream compiler fix** — `compiler/codegen/` should either
      mark imported module functions `static` so each TU gets a private
      copy, or emit each function exactly once with `extern` declarations
      in the callers. This is the root-cause fix and unblocks macOS for
      every downstream tool, not just aeb.
      (2) **Local workaround in `aeb-link.ae`** — platform-gate the
      link flag so Linux keeps `-Wl,--allow-multiple-definition` and
      macOS drops it. Useful for small builds that happen to not have
      duplicate symbols, buys time until (1) ships.
      Until this is fixed, macOS users can run the unit tests
      (`./tests/run.sh`) but `./aeb` itself cannot link full builds.
- [ ] `maven.classpath` resolution fails when a test file imports
      `java` (which imports `maven`). Reproduces on `test_javac_cmd.ae`,
      `test_junit_cmd.ae`, `test_kotlinc_cmd.ae` via `./tests/run.sh`.
      Likely a qualified-symbol resolution issue across two levels of
      module imports.

## Build environment validation

Run before any module builds. Fail fast with install hints.

```aether
build.env(b) {
    tool("javac", ">= 21")
    tool("kotlinc")
    tool("go", ">= 1.24")
    tool("rustc", ">= 1.78")
    tool("cargo")
    tool("tsc")
    tool("node")
}
```

## Per-task sandboxing (phase 2)

Wrap SDK calls in sandbox grants:

```aether
build.javac(b) {
    sandbox() {
        grant_fs_read("src/**")
        grant_fs_write("target/**")
        grant_exec("javac")
    }
}
```

## ~~`aeb --init` documentation~~ (done)

Documented in README.

## Trailing-block DSL for remaining languages

All language SDKs now use `defer` functions with trailing-block DSL:

- [x] `javac()` / `javac_test()` — release, source, target, lint, encoding, etc.
- [x] `junit()` — jvm_args, extra
- [x] `kotlinc()` / `kotlinc_test()` — jvm_target, api_version, language_version
- [x] `go_build()` / `go_test()` — build_mode, output_file, tags, ldflags, race, env_var
- [x] `cargo_build()` — lib_name, profile, features, jobs
- [x] `tsc()` — strict, ts_target, module_kind, out_dir
- [x] `mocha()` — mocha_timeout, reporter, mocha_grep

## Test coverage gaps (this session, deliberately deferred)

The round-218 backfill (`tests/test_aether_*.ae`, `tests/test_bash_*.ae`,
`tests/test_file_to_label.ae`) covered the pure string-builders for
this session's SDK additions: `aetherc_emit_lib_cmd`,
`aether_link_cmd`, `bash_xargs_cmd`, `bash_runner_body`, plus the
mtime-driven `_regen_action` and the install-layout resolvers (with
filesystem fixtures). Three behaviours are still uncovered. None
blocks svn-aether; all need either a refactor or harness work that
wasn't worth doing in-session.

### `_run_regen_pass` integration

`_run_regen_pass` in `lib/aether/module.ae` orchestrates the full
.ae → _generated.c regen flow: derive paired paths, mtime-check via
`file.mtime`, resolve caps (explicit or auto-detect), invoke real
`aetherc --emit=lib` via `os.system`, append to `extra_source`,
hard-fail on aetherc error.

The pure parts are already factored out (`_paired_generated_c`,
`_regen_action`, `_detect_caps_from_content`, `aetherc_emit_lib_cmd`)
and unit-tested. The orchestration loop itself isn't testable
without either:

- Stubbing aetherc with a recording wrapper script that captures
  args. Doable; needs a small bash fixture under `tests/fixtures/`
  and a per-test PATH override.
- Or extracting a "planner" function that returns a list of
  `(ae_path, c_path, caps, action)` records without invoking
  aetherc, then testing the planner. Simpler but means the actual
  aetherc-invocation loop is still uncovered.

End-to-end smoke (`/tmp/aeb-regen-smoke`) covers the integration
today. Worth formalising into a `tests/integration/` directory if
this gap bites.

### `bash.test` parallel-mode end-to-end

`bash.test(b) { jobs(N) }` writes scratch files (item list, runner
script), invokes `xargs -P` via `os.exec`, parses stdout with
`_parse_xargs_output`. The string-builders (`bash_xargs_cmd`,
`bash_runner_body`) and the parser are unit-tested. The dispatch
loop isn't — would need a fixture tree of `test_*.sh` files plus
a way to assert the resulting parallel runtime ordering, which is
non-deterministic. End-to-end smoke (`/tmp/aeb-bash-smoke`) covers
this today.

### Three-copy `file_to_label` drift detection

The label-derivation logic exists in three places that must stay in
sync: `tools/file-to-label.ae`, `tools/gen-orchestrator.ae`,
`tools/aeb-link.ae`. Round-218 commit `41d5ffa` updated all three
together; nothing today catches drift if a future change touches
one but not the others.

A drift-detection test would `import` all three `file_to_label`
implementations and assert they produce identical output for a
representative input set — but `tests/run.sh` builds each test
with `--lib lib`, and `tools/` isn't on that path. Two options:

- Make `tests/run.sh` add `tools/` to `--lib` for tests that
  import from there. Touches the harness.
- Consolidate the three copies into one shared library function
  (probably under `lib/build/` or a new `lib/aeb_internal/`).
  Harder — `gen-orchestrator` and `aeb-link` are standalone tools
  built independently, so the shared helper has to be a pure-Aether
  source-import, not a runtime dependency. This is the right
  long-term fix.

Until either lands, the test_file_to_label.ae file pins the
canonical implementation's behaviour, and a future drift in
`gen-orchestrator.ae` or `aeb-link.ae` would only surface as a
runtime bug, not a test failure.

## Container SDK — Proxmox support

The container module currently supports OCI images (podman/docker) and
LXC containers. Proxmox adds two more backends, both using the same
DSL-setter pattern.

### `container.pct()` — Proxmox LXC containers

Local mode (on the Proxmox host, shells out to `pct`):

```aether
import container
import container (template, hostname, memory, cores, net, storage)

container.pct(b) {
    template("local:vztmpl/ubuntu-24.04-standard_24.04-2_amd64.tar.zst")
    hostname("web-1")
    memory("2048")
    cores("2")
    net("name=eth0,bridge=vmbr0,ip=dhcp")
    storage("local-lvm")
}
```

Generates: `pct create <vmid> <template> --hostname web-1 --memory 2048 --cores 2 --net0 name=eth0,bridge=vmbr0,ip=dhcp --storage local-lvm`

Remote mode (over the wire via Proxmox REST API):

```aether
container.pct(b) {
    host("pve.internal:8006")
    api_token("user@pam!aeb", "token-secret")
    node("pve1")
    template("local:vztmpl/ubuntu-24.04-standard_24.04-2_amd64.tar.zst")
    hostname("web-1")
    memory("2048")
}
```

Generates: `curl -k -X POST https://pve.internal:8006/api2/json/nodes/pve1/lxc -H "Authorization: PVEAPIToken=user@pam!aeb=token-secret" -d 'ostemplate=local:vztmpl/...'`

### Local vs remote convention

The `host()` setter is the boundary. Present → remote API call via curl.
Absent → local CLI. Same DSL setters for the container config either way.
This convention extends to any future backends that have both local and
remote modes.

### Additional Proxmox setters

- `host(addr)` — Proxmox API address (e.g. `pve.internal:8006`)
- `api_token(user, secret)` — PVE API token for auth
- `node(name)` — target Proxmox node
- `hostname(name)` — container hostname
- `memory(mb)` — RAM in MB
- `cores(n)` — CPU cores
- `swap(mb)` — swap in MB
- `disk(spec)` — root disk (e.g. `local-lvm:8`)
- `net(spec)` — network interface config
- `storage(name)` — storage target
- `vmid(id)` — explicit VM ID (auto-assign if omitted)
- `unprivileged()` — create as unprivileged container
- `start_on_create()` — start immediately after creation
- `ssh_key(path)` — inject SSH public key

### `container.qm()` — Proxmox VMs (future)

Same pattern for full VMs via `qm create`. Lower priority — containers
cover most deployment use cases.

### Test approach

Same as other SDKs: test the command string builders (`pct_create_cmd`,
`pct_api_cmd`) in isolation. No Proxmox host needed for tests.

## ~~Build environment validation~~ (not doing)

Decided against `aeb --check`. The build already fails fast with a clear
error when a tool is missing, scoped to exactly the module that needed it.
A pre-flight check would need to stay in sync with SDK internals, and
would report missing tools you don't even need. If a specific SDK's
failure message is ever cryptic, fix that message rather than adding a
separate validation system.

## Scala

1. Assembly jar / fat jar packaging — replace `scala-cli --power package`.
   Needs a `scala.shade(b)` or reuse of `java.shade(b)` since Scala
   compiles to .class files on the JVM.

2. Scala version DSL — `scala_version("3.8.2")` setter exists but
   untested with versions other than 3.8.2. Cross-compilation
   (2.13 + 3.x) not supported.

3. sbt project migration — the current itest uses scala-cli's
   `//> using` convention. An sbt multi-module project (with
   `build.sbt`, `project/`, sub-projects) would prove aeb can replace
   sbt end-to-end. The Scala SDK already does direct `scalac` invocation
   so it's just a matter of writing `.build.ae` files for a real sbt project.

4. Compiler plugins — some Scala projects use compiler plugins
   (e.g. wartremover, better-monadic-for). Would need a
   `compiler_plugin("org.wartremover:wartremover_3:version")` setter
   that adds `-Xplugin:path.jar` to the scalac invocation.

## .NET

1. Hierarchical config via `load_config()` — a shared `.config.ae` file
   that sets default DSL values (sdk, target_framework, nullable, etc.),
   loadable at the top of each `.build.ae`. Leaf projects override
   individual setters. aeb generates the complete `.csproj` every time —
   no `Directory.Build.props` inheritance, no MSBuild walking up the
   tree. The `.build.ae` is the single source of truth.

2. Eliminate `Directory.Packages.props` — move NuGet version management
   into aeb. Either a shared `.versions.ae` file or version suffixes on
   `nuget("PackageName:1.2.3")`. Central version pinning becomes an aeb
   concern, not an MSBuild concern.

3. `dotnet publish` / deployment packaging — the current SDK has
   `build_project` and `test` but no publish/container/self-contained
   deployment support.

4. F# support — the SDK auto-detects `.fsproj` but hasn't been tested
   with F# projects. Should work since MSBuild handles F# the same way.

5. Multi-targeting (`net8.0;net10.0`) — the `target_framework()` setter
   currently takes a single TFM. Multi-targeting would need
   `<TargetFrameworks>` (plural) in the generated `.csproj`.

6. Package asset control — `PrivateAssets`/`IncludeAssets` on NuGet refs.
   Prevents analyzers/tools from flowing to downstream projects. Needs
   a richer `nuget()` setter, e.g. `nuget("EF.Tools", private: "all")`
   or a trailing block on nuget.

7. Conditional package references — some packages only apply in Release
   (e.g. `BuildBundlerMinifier`). Needs `nuget_if("Release", "Pkg")` or
   a `condition()` setter.

8. Content/resource copy rules — `CopyToOutputDirectory` for
   `appsettings.json`, Razor views, etc. Needed for `dotnet publish`
   and deployment. DSL: `copy_to_output("appsettings.json")`.

9. User secrets — `<UserSecretsId>` for ASP.NET Core dev-time secret
   management. DSL: `user_secrets("guid")`.

## Cross-cutting — inspired by Cake (C# Make)

Cake is a mature .NET build orchestrator. These are capabilities it has
that aeb doesn't yet, worth considering across all languages:

### Versioning

Cake integrates GitVersion for semantic versioning — the build script
knows its own version derived from git tags/branches, and injects it
into assembly metadata, package versions, and release notes. aeb has
no versioning story at all.

Possible aeb approach: a `version()` builder or DSL that reads git
tags/describes and exposes `${version}` in the build context. Language
SDKs use it for jar manifests, NuGet package versions, npm versions, etc.

### Test result reporting

Cake collects test results in structured formats (TRX, JUnit XML) in
organized directories. aeb currently just prints PASSED/FAILED to stdout.

Needed: write test results to `target/{module}/test-results/` in a
standard format (JUnit XML works across languages). Enables CI systems
to parse results, and `aeb` could print a summary table at the end.

### Task lifecycle hooks (setup/teardown)

Cake has global and per-task setup/teardown. aeb has nothing — if a
test starts a server and crashes, the server leaks. A `teardown()`
block in `.tests.ae` would run regardless of test outcome.

### Conditional task execution

Cake tasks have `.WithCriteria()` — skip a task based on runtime
conditions (branch name, env var, flag). aeb currently runs everything
it finds. Useful for: skip integration tests in CI, skip signing
locally, skip publish on non-tag builds.

Possible: `skip_if(b, "CI != true")` or a `criteria()` DSL setter.

### NuGet/Maven package publishing

Cake has DotNetNuGetPush, NuGetPush for publishing packages to feeds.
The Java side has `mvn deploy`. aeb has `shade()` for fat jars but no
publish step — built artifacts stay local.

Needed for `.dist.ae`: a `publish()` builder or DSL that pushes to
NuGet feeds, Maven Central, npm registry, etc.

### Code signing

Cake integrates Azure Key Vault + SignTool for signing packages before
publish. Relevant for any project that ships binaries.

### CI system detection

Cake auto-detects 15+ CI systems (GitHub Actions, GitLab CI, Jenkins,
etc.) and adjusts behaviour — setting output variables, uploading
artifacts, detecting PR context. aeb is CI-agnostic (runs the same
everywhere), which is a strength, but knowing "am I in CI" and "is
this a PR" would enable conditional logic.

Possible: `build.is_ci()`, `build.branch()`, `build.is_pr()` functions
that read standard CI env vars (GITHUB_ACTIONS, CI, GITLAB_CI, etc.).

### Multi-SDK bootstrapping

Cake's build.sh/build.ps1 install specific .NET SDK versions before
building. aeb assumes tools are on PATH. A `require_tool("dotnet", "10.0")`
or similar could validate/install prerequisites.

## Java/Maven

What we have today: `java.javac()` covers the maven-compiler-plugin
surface (release / source / target / lint / encoding / parameters /
debug / `--enable-preview` / `--module-path` / annotation processors /
generated sources). `java.junit()` and `java.junit5()` cover the
surefire core case. `java.shade()` builds classic fat JARs.
`maven.resolve()` + `maven.classpath()` + `load_bom_file()` handle
dependency resolution including BOM-managed versions.

Below is what's still missing measured against mainstream Maven
plugin grammars — grouped by how often a typical Java shop actually
needs each.

1. tools/aeb-resolve.jar - maybe not check that it. Maybe  have is slimmer and source transitive deps from ~/.m2/repository using the manifest. If those transitive 
deps are missing go get them and place them in there

2. ~~maven should have its own aeb module~~ (done — `lib/maven/module.ae`)

3. Surefire equivalent in aeb grammar — `build.junit(b)` already handles
   the core case (find test classes, fork JVM, run with JUnit). Missing
   pieces vs Surefire: test filtering/includes/excludes, parallel forks
   (`forkCount` / `reuseForks` / `argLine`), `parallel=classes` worker
   pool, XML report output (`target/surefire-reports/TEST-*.xml`).
   Single-JVM `junit5_cmd` won't scale on a real test suite. Add
   incrementally.

### Tier 1 — every serious Java project needs these

4. **Resources + resource filtering** (maven-resources-plugin). Copy
   `src/main/resources` into the classpath, optionally substituting
   `${project.version}` etc. Today `java.javac()` only sees `*.java`,
   so properties / YAML / XML / SQL resources have no path onto the
   classpath. DSL sketch:

   ```aether
   java.javac(b) {
       resources("src/main/resources")
       filter("application.yml")            // do placeholder sub
       property("project.version", "1.2.3")
   }
   ```

5. **Manifest / Main-Class** (maven-jar-plugin). Today plain `jar`
   output has no manifest control. Needed for runnable JARs and
   `-Multi-Release` headers. DSL sketch:

   ```aether
   java.jar(b) {
       main_class("com.example.Main")
       manifest_attribute("Implementation-Version", "1.2.3")
       multi_release(true)
   }
   ```

6. **Sources JAR + Javadoc JAR** (maven-source-plugin,
   maven-javadoc-plugin). `*-sources.jar` and `*-javadoc.jar` next to
   the main artifact — required by Maven Central, used by every IDE.
   Builders: `java.sources_jar(b)`, `java.javadoc(b) { link(...); doctitle(...) }`.

7. **Spring Boot fat-JAR layout** (`spring-boot-maven-plugin
   repackage`). Different from `shade()` — uses `BOOT-INF/`,
   `PropertiesLauncher`, layered jars. High leverage given the
   `itests/spring-data-examples` scale. DSL: `java.spring_boot_repackage(b)`.

8. **Integration test phase** (maven-failsafe-plugin). `*IT.java`
   pattern, separate from unit tests, post-suite verify that fails
   the build only after teardown runs. Today `junit5()` runs
   everything in one phase. Add `java.junit5_it(b)` with an `*IT`
   default include pattern and post-test cleanup hook.

9. **Code coverage** (jacoco-maven-plugin). Inject
   `-javaagent:jacocoagent.jar=destfile=...` into junit invocations
   and emit exec/HTML/XML reports. One agent flag in `junit5_cmd` +
   a small report builder. DSL:

   ```aether
   java.junit5(b) {
       coverage("jacoco")               // or coverage_off()
   }
   java.coverage_report(b) { format("xml", "html") }
   ```

### Tier 2 — needed to publish or distribute artifacts

10. **POM emission**. Maven Central (and most artifact stores) need
    a `pom.xml` next to the jar. Even Gradle has to emit one. Cheap
    to implement — a `pom_xml_content(opts, deps)` content builder in
    the same shape as the new `dotnet.csproj_content()`. Without this
    nothing aeb produces is publishable.

11. **mvn install / deploy** (maven-install-plugin,
    maven-deploy-plugin). `mvn install` populates `~/.m2` for
    cross-project local consumption; `mvn deploy` pushes to
    Nexus/Artifactory/Central. aeb's vendored/registry pattern
    doesn't write to `~/.m2`, and there's no push step at all.
    Builders: `java.install(b)`, `java.deploy(b) { repo(...);
    credentials(...) }`.

12. **GPG signing** (maven-gpg-plugin) + checksums. Central requires
    `.asc` signatures and `.md5` / `.sha1` / `.sha256` / `.sha512`
    siblings on every artifact. DSL: `java.sign(b) { key_id(...) }`,
    `java.checksums(b) { algorithms("sha256", "sha512") }`.

### Tier 3 — quality / static analysis

13. **Checkstyle / PMD / SpotBugs / Spotless**. Run as part of build,
    fail on violations. Each is a small `.cmd_string` builder
    invoking the respective standalone runner. Group under
    `java.lint(b) { checkstyle(...); spotbugs(...); spotless_check() }`
    or one builder per tool.

14. **errorprone / NullAway** (javac `-Xplugin:` style). Distinct
    from annotation processors — these ride `javac` itself. Add an
    `xplugin()` setter to `java.javac()` that emits `-Xplugin:` flags.
    Existing `processor()` covers AP path; this is the missing
    sibling.

15. **maven-enforcer-plugin** equivalents. Banned-deps,
    dep-convergence, no-snapshot-deps, required Java version. DSL:

    ```aether
    java.enforce(b) {
        require_java(">= 21")
        ban_dep("commons-logging:commons-logging")
        require_convergence()
        no_snapshots()
    }
    ```

### Tier 4 — build-system meta

16. **maven-toolchains-plugin** equivalent. Pick one of several
    installed JDKs by vendor/version per module — multi-JDK builds
    in a monorepo. Today aeb uses whatever `javac` is on PATH. This
    is the same gap as the hermetic-toolchain item in
    `bazel-gaps.md`; if that gets resolved at the runner level
    (hermetic-LLVM-style fetch + pin), this falls out.

17. **Profiles** (`<profile id="ci">`). Maven's environment-specific
    config switches. aeb has no profile concept. Could ride
    `criteria()` (cross-cutting Cake item above) plus a profile
    selector flag — same DAG, same SDK calls, just different setter
    values per profile.

18. **Properties / interpolation**. `${project.version}` and
    friends used by resources, manifest, POM, jib. Tied to the
    versioning story in the Cake section above and to (4) and (10)
    here. One `properties` map on the build context, expanded by
    each builder that reads strings.

19. **versions-maven-plugin**. `display-dependency-updates`,
    `use-latest-versions`. Mostly tooling, not build. Could be a
    standalone `aeb deps update` subcommand rather than a builder.

20. **flatten-maven-plugin**. Resolves parent/property references
    in the published POM. Only matters once (10) lands.

### Tier 5 — domain-specific

21. **protobuf-maven-plugin + os-maven-plugin**. `protoc` codegen
    with platform-specific compiler binary selection. Big for
    gRPC/Spring-gRPC shops. DSL: `proto.compile(b) { ... }` —
    probably its own SDK module rather than crammed into Java.

22. **jib-maven-plugin**. Daemonless layered OCI images. We have
    `lib/container/` via podman/docker; the Jib approach (no daemon,
    layered jars) is materially different and worth a separate
    builder.

23. **graalvm native-maven-plugin**. AOT `native-image`. DSL:
    `java.native_image(b) { no_fallback(); reflect_config(...) }`.

24. **Liquibase / Flyway**. DB migration tasks. Probably its own
    `db.migrate(b) { ... }` SDK rather than under Java.

### Suggested order if attacking this

POM emission (10) → Resources (4) → Manifest (5) → Sources/Javadoc
(6) → Spring Boot repackage (7) → Surefire forking + XML reports (3)
→ Jacoco (9). That sequence unblocks "publishable library" and
"runnable Spring Boot app" — the two shapes most Java itests in this
repo actually exercise.

