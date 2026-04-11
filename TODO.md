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

### Parallel execution

Independent modules in the DAG can build concurrently. The visited map
needs thread-safe access (mutex or atomic). Aether actors are a natural
fit — one actor per module, message-passing for completion.

### Affected-target detection

Given `git diff`, compute which modules' sources changed, trace the
reverse dependency graph, rebuild only affected targets. Massive CI
speedup for large monorepos.

### Remote build cache

Content-addressable cache of build outputs keyed by hash of
(source files + deps + compiler flags). Currently aeb uses
timestamp-based caching in `target/` — local only, invalidated
by clean builds.

Bazel, Gradle, Nx, and Turborepo all share caches across machines.
A remote cache means CI doesn't rebuild what a developer already built
(and vice versa). Implementation: hash inputs → check remote store
(S3, GCS, local server) → download artifacts or build locally →
upload result. The artifact metadata files aeb already writes
(`jvm_classpath_deps_including_transitive`, `maven_classpath`, etc.)
are the natural cache units.

### Build graph visualization

`aeb --graph` to output the dependency DAG in DOT or Mermaid format.
Useful for understanding large monorepos and debugging dep cycles.
The graph is already computed during the scan phase — just needs a
render path instead of executing builds.

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

### Build telemetry

Timing per module (compile, test, resolve), cache hit/miss rates,
total wall time vs build time. Write to `target/_aeb/telemetry.json`.
Enables: "which module is the slowest?", "what's the cache hit rate?",
"how much time does parallelism save?".

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

1. tools/aeb-resolve.jar - maybe not check that it. Maybe  have is slimmer and source transitive deps from ~/.m2/repository using the manifest. If those transitive 
deps are missing go get them and place them in there

2. ~~maven should have its own aeb module~~ (done — `lib/maven/module.ae`)

3. Surefire equivalent in aeb grammar — `build.junit(b)` already handles
   the core case (find test classes, fork JVM, run with JUnit). Missing
   pieces vs Surefire: test filtering/includes/excludes, parallel forks,
   XML report output. Add incrementally to the Java SDK as needed.

