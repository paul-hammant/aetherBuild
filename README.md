# Aether Build

A build system for polyglot monorepos. Small declarative files replace
Makefiles, `pom.xml`, `package.json` scripts, `deps.edn`, `Cargo.toml`,
`.csproj`, and `pyproject.toml` — and replace them consistently across
languages. Convention does the work; you declare the intent.

## What it looks like

A Java component with one dependency:

```aether
import build
import java

main() {
    b = build.start()
    build.dep(b, "java/components/vowelbase/.build.ae")
    java.javac(b)
}
```

A Rust shared library that depends on a vendored `jni` crate:

```aether
import build
import rust
import rust (crate_name, edition, crate_type, lib_path, lib_name)

main() {
    b = build.start()
    build.dep(b, "libs/rust/registry/vendor/jni/.jni.crate.ae")
    rust.cargo_project(b) {
        crate_name("vowelbase")
        edition("2021")
        crate_type("cdylib")
        lib_path("lib.rs")
        lib_name("libvowelbase.so")
    }
}
```

A JUnit test module with vendored `junit.jar`:

```aether
import build
import java

main() {
    b = build.start()
    build.dep(b, "java/components/vowelbase/.build.ae")
    build.dep(b, "libs/java/junit/.junit.jar.ae")
    build.dep(b, "libs/java/hamcrest/.hamcrest.jar.ae")
    java.javac_test(b)
    java.junit(b)
}
```

A Spring Boot test module with Maven dependencies via a BOM:

```aether
import build
import java
import maven (load_bom_file)
import java (release, source_layout, enable_preview, parameters)

main() {
    b = build.start()
    build.dep(b, "jpa/example/.build.ae")
    load_bom_file(b, "../../spring-boot.bom.ae")
    build.dep(b, "org.springframework.boot:spring-boot-starter-test")
    build.dep(b, "org.springframework.boot:spring-boot-data-jpa-test")
    java.javac_test(b) {
        release("25")
        source_layout("maven idiomatic")
        enable_preview()
        parameters()
    }
    java.junit5(b) {
        enable_preview()
    }
}
```

## How it works

A module's build intent lives in one or more dot-prefixed `.ae` files:

| File                       | Role                                      |
|----------------------------|-------------------------------------------|
| `.build.ae`                | compile the module                        |
| `.build-<tag>.ae`          | additional binary in the same module dir  |
| `.tests.ae`                | compile + run tests                       |
| `.tests-<tag>.ae`          | additional test binary in the same dir    |
| `.dist.ae`                 | package (fat jar, Docker image, wheel…)   |
| `.dist-<tag>.ae`           | additional packaging variant              |
| `.{name}.jar.ae`           | vendored JVM jar                          |
| `.{name}.crate.ae`    | vendored / registry Rust crate            |
| `.{name}.npm.ae`      | vendored / registry npm package           |
| `.{name}.nupkg.ae`    | vendored / registry NuGet package         |
| `.{name}.whl.ae`      | vendored / registry Python wheel          |

The runner (`aeb`) does four things:

1. **Scan** — walk the tree and collect every `.*.ae` file
2. **Graph** — grep each `dep(b, "…")` line to derive a file-based DAG
3. **Sort** — topologically order the files
4. **Generate + link** — produce a single orchestrator `.ae` file with one
   function per module, compile them all to C, link into a single native
   binary, run it

The generated binary is one process with one in-memory visited-module map.
Each module function calls its deps directly — no subprocesses, no
file-based coordination. Per-module artifacts (classpaths, library paths,
crate paths, NuGet refs, source paths) are written to `target/<module>/`
so downstream modules can read and compose them transitively.

File paths encode to C-safe function names:

```
java/components/vowelbase/.build.ae
→ java_components_vowelbase__D_build_D_ae
```

(`/` → `_`, `.` → `_D_`, `-` → `_H_`, `_` → `__`.)

## Setup

Initialize a repo to use aetherBuild:

```bash
/path/to/aetherBuild/aeb --init
```

This creates symlinks in `.aeb/lib/` for every shipped SDK module and
adds `.aeb/` to `.gitignore`. Safe to re-run.

## Running

```bash
# Build and test everything under the current directory:
aeb

# Run one target (file-based):
aeb jpa/example/.build.ae
aeb jpa/example/.tests.ae

# Or from inside the module directory:
cd jpa/example && aeb .tests.ae
```

aeb auto-detects Podman's socket and sets `DOCKER_HOST` so TestContainers
works without a daemon.

### Sparse checkout (`aeb gcheckout`)

`aeb gcheckout` walks the `.ae`-file dependency DAG starting from a target
and adds each visited module's directory to the VCS sparse-checkout, so a
monorepo consumer can fetch only the modules they actually need.

```bash
aeb gcheckout --init                      # enable sparse-checkout, add scaffolding
aeb gcheckout add jpa/example/.tests.ae   # walk DAG from target, sparse-add each dir
aeb gcheckout add java/components/vowels  # directory form (resolves to .build.ae etc.)
aeb gcheckout --reset                     # disable sparse-checkout
```

The walk goes through the same `extract-deps` tool the rest of `aeb` uses,
so any `.jar.ae` / `.crate.ae` / `.npm.ae` / `.nupkg.ae` / `.whl.ae`
third-party dep file is followed automatically.

> **Note** — the implementation currently shells out to `git
> sparse-checkout` and is therefore Git-only. Mercurial support would need
> a small VCS abstraction layer (`narrowhg` extension on the hg side, or
> a different command per VCS). The dep-walking logic itself is
> VCS-agnostic and would be reused unchanged.

### Typical output

```
aeb: 18 compile + 2 dist + 17 test
  compile: rust/components/vowelbase
  compile: java/components/vowelbase
  ...
  dist:    java/applications/monorepos_rule
  test:    javatests/components/vowelbase
  ...
javatests/components/vowelbase: tests PASSED
```

### Test output is preserved

Each test SDK that participates writes its full stdout+stderr to
`target/<module>/test_output.log` for failure diagnosis after the
build. Terminal scrollback isn't reliable (especially in CI), so
the persisted file is the authoritative record. Currently wired
in `java.junit5`, `java.junit`, `jest.test`, `python.pytest`, and
`dotnet.test`; other SDKs are tracked for follow-up.

### Build graph visualisation (`aeb --graph`)

`aeb --graph` emits the build-file DAG as a graphviz DOT description
on stdout. `aeb --graph mermaid` emits a Mermaid flowchart suitable
for Markdown.

```bash
# Render to SVG via graphviz
aeb --graph | dot -Tsvg > deps.svg

# Render to PNG
aeb --graph | dot -Tpng > deps.png

# Embed in Markdown via Mermaid (renders inline on GitHub)
aeb --graph mermaid > deps.md
```

The graph reflects exactly what the build pipeline sees: every
dot-prefixed `.ae` target under cwd, with edges drawn from each
`build.dep("path/.foo.ae")` line. No render-only data; this is the
authoritative DAG. Useful for debugging "why isn't X depending on
Y", reviewing dep changes in a PR, or onboarding someone to a
monorepo's structure.

### Affected-target detection (`aeb --since`, `aeb --print-affected`)

`aeb --since <git-ref>` builds only the targets transitively
impacted by changes since `<git-ref>`. `aeb --print-affected <ref>`
lists them without building.

```bash
# CI shape: in a PR, build/test only what the PR touched.
aeb --since main
aeb --since origin/main          # vs the merge base
aeb --since HEAD~10              # vs 10 commits back

# Inspect the impact set without building (one path per line).
aeb --print-affected main
```

The walk: `git diff --name-only <ref>` → list of changed paths →
each path's owning target (nearest enclosing dir with a build
file) → reverse-dep BFS → affected target set. Targets outside
the impact set don't run.

Source-to-target ownership uses **nearest enclosing dir with a
build file**. A change to `lib/foo/src/main.c` is owned by the
nearest `.build*.ae` walking up: `lib/foo/.build.ae` if present,
else `lib/.build.ae`, else nothing. Multiple build files in one
dir (the `.build-<tag>.ae` case) all share ownership of files in
that dir.

Combines well with cache integration: targets that are affected
but cache-hit on their inputs still skip work. Telemetry shows
what built and what hit the cache.

### Watch mode (`aeb --watch`)

`aeb --watch [target]` does an initial build, then watches every
source directory and re-runs aeb (against the affected-target
set) on every change. Save a file, the right things rebuild —
most cache-hit, telemetry shows what actually ran.

```bash
# Watch everything visible from cwd
aeb --watch

# Watch with an initial-build target
aeb --watch ae/svnserver
```

Platform support:
- **Linux**: requires `inotifywait` from `inotify-tools` (apt:
  `inotify-tools`, dnf: `inotify-tools`).
- **macOS**: requires `fswatch` (`brew install fswatch`).

`target/`, `.aeb/`, and `.git/` are excluded from the watch (the
build itself writes there; including them would feedback-loop).
Per-target test output (`test_output.log`), cache markers
(`.aeb_cache`), and timestamps are also excluded.

The watch list is computed from the edges file at start; only
directories that *exist on disk* are watched, so it composes
cleanly with `aeb gcheckout`'s sparse-checkout (dirs that
sparse-checkout has hidden don't error). Re-running aeb-watch
after `aeb gcheckout add ...` picks up the newly materialised
dirs.

### Coverage (`aeb --coverage`)

Cross-cutting build flag — every SDK that knows what coverage
means for its language honors it; SDKs that don't (e.g. `bash.test`)
ignore it.

```bash
aeb --coverage                        # build everything with coverage
aeb --coverage <target>               # one target, with coverage
aeb --coverage --since main           # affected-targets, with coverage
```

Currently wired in `lib/aether/`:

- `aether.program`/`aether.program_test` shell-out path: appends
  `--coverage` to `ae build` (the Aether 0.115 feature). The
  driver injects `gcc --coverage` and forces `-O0 -g` for
  accurate `.ae.gcov` line attribution.
- `aether.program`/`aether.program_test` manual path (when
  `extra_source`/`link_flag`/`regen` is declared): swaps gcc's
  `-O2` for `-O0 -g --coverage`.

After running an instrumented binary, `.gcda` files appear next
to the binary. aeb does not render reports — delegate to your
preferred tool:

```bash
# graphviz / per-file gcov reports
gcov -p -b -c target/<module>/bin/<binary>-*.gcda

# HTML report via gcovr
gcovr -r . --html --html-details -o coverage.html
```

The cache key includes the coverage flag, so `aeb` and `aeb --coverage`
back-to-back both run real builds (no stale instrumented hit when
you wanted a clean build, or vice versa).

Other SDKs (java, jest, pytest, dotnet, go, rust, etc.) have
their own native coverage flows (jacoco, jest --coverage,
coverage.py, dotnet test --collect, `-cover`, llvm-cov, etc.) —
not yet wired through `aeb --coverage`. Tracked in TODO.md as
follow-up.

### Distribution metadata (`meta` SDK) and exporters (`brew`, …)

Distribution is just another target type. Declare metadata via
the `meta` SDK and an exporter closure (e.g. `brew.formula(b)`)
inside a `.dist.ae` file; `aeb` walks it like any other target,
running the exporter to emit the formula.

```aether
// lib/hello/.dist.ae
import build
import meta
import brew
import brew (aeb_target, binary)

main() {
    b = build.start()
    meta.desc(b, "Tiny hello-world greeter")
    meta.homepage(b, "https://example.com/hello")
    meta.license(b, "MIT")
    meta.version(b, "0.1.0")
    meta.url(b, "https://example.com/dl/hello-0.1.0.tar.gz")
    meta.sha256(b, "0123...cdef")

    brew.formula(b) {
        aeb_target("lib/hello/.build.ae")  // what `def install` runs
        binary("hello-world")               // optional override
    }
}
```

```bash
aeb lib/hello/.dist.ae          # emits target/lib/hello/hello-world.rb
```

`meta.url` + `meta.sha256` are required (Homebrew won't accept a
formula without them); `aeb_target` is required (so `def install`
knows which target to build). Class name is auto-derived from the
binary name (hyphens become CamelCase: `hello-world` → `HelloWorld`,
overridable via `class_name(...)`). Custom test assertion via
`test_assertion("...")`.

Because distribution targets are real graph nodes, they compose
with everything else: `aeb --affected --since main` will surface
a `.dist.ae` whose upstream `meta.version` changed; `aeb --graph`
shows them; `aeb --watch` rebuilds them on file edits.

`meta.*` is the source-of-truth — future exporters
(`nix.derivation`, `deb.control`, `pkgsrc.makefile`) will read
the same fields, only adding their packaging-specific overrides
in their own closure.

## Dependencies

Every dependency — local module, third-party library, Maven coordinate, npm
package, NuGet package, Cargo crate, or Python wheel — is declared with a
single `build.dep()` call. The form of the argument distinguishes them:

```aether
// Local module (reference the dep module's .build.ae file)
build.dep(b, "java/components/vowelbase/.build.ae")

// Vendored / registry third-party (reference its .{name}.jar.ae etc.)
build.dep(b, "libs/java/junit/.junit.jar.ae")
build.dep(b, "libs/rust/registry/vendor/jni/.jni.crate.ae")
build.dep(b, "libs/javascript/mocha/.mocha.npm.ae")
build.dep(b, "libs/dotnet/Shouldly/.Shouldly.nupkg.ae")
build.dep(b, "libs/python/pytest/.pytest.whl.ae")

// Maven coordinate (colon-separated; version from BOM if omitted)
build.dep(b, "org.springframework.boot:spring-boot-starter-data-jpa")
build.dep(b, "com.github.javafaker:javafaker:1.0.2")
```

All of these lines are greppable — the build graph is extracted by
scanning source files, not by compiling them. Same contract as Bazel's
`BUILD` files.

### Third-party dep files: `.ae-as-dep`

Rather than a dozen bespoke helpers (`build.npm_dep`, `build.cargo_dep`,
`build.lib`, …), each third-party dep is its own `.ae` file that registers
its contribution via a language-SDK builder. Examples:

```aether
// libs/java/junit/.junit.jar.ae — vendored jar
main() {
    b = build.start()
    java.jar_vendored(b, "libs/java/junit/junit.jar")
}
```

```aether
// libs/rust/registry/vendor/jni/.jni.crate.ae — registry Rust crate
main() {
    b = build.start()
    rust.crate_registry(b, "jni~0.21.1")
}
```

```aether
// libs/python/pytest/.pytest.whl.ae — pypi wheel
main() {
    b = build.start()
    python.wheel_registry(b, "pytest~")
}
```

Each SDK has symmetric `X_vendored` / `X_registry` builders:

| Language | Vendored              | Registry / fetched     |
|----------|-----------------------|------------------------|
| Java     | `java.jar_vendored`   | `java.jar_registry`    |
| Rust     | `rust.crate_vendored` | `rust.crate_registry`  |
| TS       | `ts.npm_vendored`     | `ts.npm_registry`      |
| .NET     | `dotnet.nuget_vendored` | `dotnet.nuget_registry` |
| Python   | `python.wheel_vendored` | `python.wheel_registry` |

Consumers don't care which — they just `build.dep(b, "libs/.../.foo.bar.ae")`.

## Language SDKs

```aether
import java
java.javac(b) { release("25") source_layout("maven idiomatic") }
java.javac_test(b) { enable_preview() parameters() }
java.junit(b)          // JUnit 4
java.junit5(b)         // JUnit 5 / Jupiter
java.shade(b, "com.Main", "app.jar")
```

```aether
import kotlin
kotlin.kotlinc(b)
kotlin.kotlinc_test(b)
kotlin.kotlin_test(b, "pkg.TestClassKt")
```

```aether
import go
go.go_build(b, "c-shared", "libname.so")
go.go_test(b)
```

```aether
import rust
rust.cargo_project(b) { crate_name("…") edition("2021") crate_type("cdylib") }
rust.cargo_workspace(b) { … }    // root-level workspace Cargo.toml
rust.cargo_crate(b) { … }        // workspace member Cargo.toml
rust.check_workspace(b)
rust.test_workspace(b)
```

```aether
import ts
ts.tsc(b)
```

```aether
import scala
scala.scalac(b)
scala.scalac_test(b)
scala.munit(b)
```

```aether
import clojure
clojure.compile(b)
clojure.test(b)
```

```aether
import dotnet
dotnet.csproj(b) { … }      // generates .{name}.generated.csproj
dotnet.build(b)
dotnet.test(b)
```

```aether
import python
python.install(b)           // pip install deps into venv
python.pytest(b)
python.package(b) { … }     // generate pyproject.toml and build wheel
```

```aether
import bash
import bash (script, jobs, pre_command, post_command, fixture_seed, fixture_server)  // see note below

bash.test(b) {              // exit 0 = PASS, non-zero = FAIL
    script("test_a.sh")
    script("test_b.sh")
}
bash.test(b)                // no script(...) → auto-discover test_*.sh
bash.test(b) {              // run up to 4 scripts concurrently
    jobs(4)
}
bash.test(b) { jobs(0) }    // 0 = auto = nproc/2

bash.test(b) {              // pre/post commands run around every script.
    pre_command("source setup.sh")    // pres always run before each script
    post_command("source teardown.sh") // posts always run after, pass or fail
    script("test_acl.sh")
}                           // (forces sequential mode if jobs(N>1) also set)

bash.test(b) {                                 // structured server fixtures —
    fixture_seed("primary",                    //   spawned per-script, env vars
                 "/tmp/svnae_repo",            //   exposed to the script,
                 "target/svnae-seed/bin/seed") //   cleaned up after.
    fixture_server("primary",                  // server bin + args + port +
                   "target/svnserver/bin/srv", //   ready_after_ms (ms to sleep
                   "demo $PRIMARY_REPO 9540 --token X", //   between spawn and script).
                   9540, 1500)
    script("test_acl.sh")                      // sees $PRIMARY_REPO, $PRIMARY_PORT,
}                                              // $PRIMARY_BIN, $PRIMARY_PID.

bash.test(b) {                                 // multi-fixture: declare each with
    fixture_seed("source",      "/tmp/r1", "")            //   a different name. The names
    fixture_seed("destination", "/tmp/r2", "")            //   become env-var prefixes
    fixture_server("source",      "$BIN", "", 9430, 500)  //   ($SOURCE_PORT,
    fixture_server("destination", "$BIN", "", 9431, 500)  //   $DESTINATION_PORT, …).
    script("test_svnadmin.sh")                            // Ports are caller-chosen;
}                                                         //   ready-check is sleep-then-go.

bash.script(b) {            // non-test runner: codegen, asset prep, etc.
    script("gen.sh")
}
```

```aether
import aether
import aether (source, output, extra_source, extra_source_glob, link_flag, regen, regen_with)

aether.program(b) {                   // shells out to `ae build` by default —
    source("main.ae")                 //   honours aether.toml [[bin]].
    output("svn")
}
aether.program(b) {                   // declaring any of extra_source /
    source("main.ae")                 //   link_flag / regen opts into the
    output("svn")                     //   manual aetherc + gcc path (.build.ae
    regen("ae/client/accessors.ae")   //   becomes the single source of truth,
    regen("ae/client/handlers.ae")    //   aether.toml ignored for this target).
    link_flag("-pthread")
}
                                      // regen(X.ae) runs `aetherc --emit=lib`
                                      // when the paired X_generated.c is missing
                                      // or older than X.ae, then links it in.
                                      // Capabilities (--with=net|fs|os) are
                                      // auto-detected from the .ae's
                                      // `import std.X` lines: std.http /
                                      // std.tcp / std.net → net, std.fs → fs,
                                      // std.os → os.

aether.program(b) {                   // regen_with overrides auto-detection —
    source("main.ae")                 //   use when caps come in transitively
    output("svn")                     //   and the import scan misses them.
    regen_with("ae/client/auth.ae", "net,fs")
}

aether.program(b) {                   // hand-written extras still work via
    source("main.ae")                 //   extra_source(...) — combine freely
    output("svn")                     //   with regen(...) entries.
    extra_source("legacy_helper.c")
    regen("ae/client/accessors.ae")
}
aether.program(b) {                   // extra_source_glob expands a glob at
    source("main.ae")                 //   build-time. Pattern is module-relative;
    output("svn")                     //   the matched files are content-hashed
    extra_source_glob("contrib/*.c")  //   into the cache key, so adding/removing
    extra_source_glob("gen/*.c")      //   matched files invalidates the cache.
}
aether.program_test(b) { ... }        // same as program, plus runs the binary

aether.driver_test(b) {               // Aether driver program that exercises a
    driver("test_svn_driver.ae")      //   *separate* compiled binary built
    output("svn_driver")              //   elsewhere in the graph (e.g. a server,
    binary_under_test("svn",          //   a CLI). The driver imports
                       "target/svn/bin/svn") //   contrib.aeocha (or whatever),
                                      //   spawns $SVN_BIN via os.run_capture,
    fixture_seed("primary",           //   asserts about its output. Same fixture
                  "/tmp/svnae_repo",  //   grammar as bash.test (fixture_seed /
                  "")                 //   fixture_server) — env vars exposed to
    fixture_server("primary",         //   the driver are $PRIMARY_REPO,
                    "$PRIMARY_BIN",   //   $PRIMARY_PORT, $PRIMARY_BIN, $PRIMARY_PID,
                    "demo $PRIMARY_REPO 9540", // plus $<NAME>_BIN for each
                    9540, 1500)       //   binary_under_test. Driver's exit code
}                                     //   is the PASS/FAIL signal.
```

> **Note on the two `import` lines.** Aether resolves identifiers
> inside a `receiver.method(args) { block }` body as plain top-level
> calls, not against the receiver's namespace. So `bash.test(b) {
> script("…") }` won't find `script` unless it's also in scope at the
> top level — hence the second `import bash (script, jobs)` line.
> The alternative is to fully qualify every setter
> (`bash.test(b) { bash.script("…") }`), which works but reads
> noisily.

## Maven / BOM support

A `.bom.ae` file at the repo root declares Maven BOMs and extra repos:

```aether
// spring-boot.bom.ae
maven_bom("org.springframework.boot:spring-boot-dependencies:4.0.4")
```

Modules load it via `load_bom_file(b, "../../spring-boot.bom.ae")`, then
use `build.dep(b, "group:artifact")` with version omitted — the BOM
supplies it. Resolution is performed by `tools/aeb-resolve.jar`, which
wraps the Maven Resolver API and caches to `~/.aeb/repo`.

## Cross-language dependencies

Modules in different languages can depend on each other:

```aether
build.dep(b, "rust/components/vowelbase/.build.ae")    // Java → Rust (JNI .so)
build.dep(b, "kotlin/components/sonorants/.build.ae")  // Java → Kotlin (JVM classpath)
build.dep(b, "go/components/nasal/.build.ae")          // Java → Go (shared library)
```

Each SDK writes artifact metadata to `target/<module>/`:

- `jvm_classpath_deps_including_transitive`
- `rust_path_deps_including_transitive` / `rust_registry_deps_including_transitive`
- `npm_deps_including_transitive` / `npm_registry_deps_including_transitive`
- `dotnet_nuget_deps_including_transitive`
- `python_pypi_deps_including_transitive` / `python_vendored_wheels_including_transitive`
- `ldlibdeps` / `shared_library_deps` (for JNI-style linking)

Downstream SDKs read these files to assemble classpaths, library paths,
Cargo.toml `[dependencies]`, `<PackageReference>`s, `pyproject.toml`
`[project.dependencies]`, and `tsconfig` path mappings.

Working cross-language chains include Java→Rust (JNI), Java→Kotlin,
Java→Go, TypeScript→Go, C#→Rust, Python→Rust (ctypes).

## Incremental builds

SDK builders check source file timestamps against a per-module timestamp
file before doing work. Unchanged modules are skipped at the compile
step; tests always re-run.

## Project layout

`aeb` itself is a ~36-line bash trampoline. It picks up `AETHER`,
`AEB_HOME`, and the working directory, optionally exports a Podman
socket, and dispatches `--init` / `gcheckout` / normal builds to the
matching Aether-language tool under `tools/`. Everything else —
argument parsing, scan/target discovery, dep extraction, topo sort,
per-file compile, orchestrator generation, gcc link, exec — runs in
Aether.

```
aetherBuild/
├── aeb                        # thin bash trampoline → tools/aeb-main
├── lib/                       # shipped SDK modules (symlinked into consumer repos)
│   ├── build/module.ae        # core: session, deps, context, artifact helpers
│   ├── java/module.ae         # language: javac, junit, junit5, shade, jar_vendored, jar_registry
│   ├── kotlin/module.ae
│   ├── go/module.ae
│   ├── rust/module.ae         # cargo_project / cargo_workspace / cargo_crate / crate_vendored / crate_registry
│   ├── ts/module.ae           # tsc, npm_vendored, npm_registry
│   ├── scala/module.ae
│   ├── clojure/module.ae
│   ├── dotnet/module.ae       # .csproj generation, nuget_vendored, nuget_registry
│   ├── python/module.ae       # pyproject.toml generation, wheel_vendored, wheel_registry
│   ├── aether/module.ae       # native Aether programs (program / program_test)
│   ├── bash/module.ae         # bash test runner (exit 0 = PASS) and non-test script runner
│   ├── maven/module.ae        # BOM parsing, Maven resolution wrapper
│   ├── pnpm/module.ae         # pnpm-based npm resolution
│   ├── jest/module.ae
│   ├── webpack/module.ae
│   ├── angular/module.ae
│   └── container/module.ae    # OCI image builds, LXC
├── tools/                     # Aether-language tools that aeb dispatches to
│   ├── aeb-main.ae            # arg parsing, scan/target discovery, sort, exec aeb-link
│   ├── aeb-init.ae            # `aeb --init` symlink + .gitignore setup
│   ├── aeb-link.ae            # per-file compile + orchestrator + gcc link + exec
│   ├── gcheckout.ae           # `aeb gcheckout` sparse-checkout DAG walker
│   ├── encode-name.ae         # path → C-safe identifier
│   ├── infer-type.ae          # filename suffix → build/test/dist
│   ├── file-to-label.ae       # file path → build.begin() module label
│   ├── resolve-dep.ae         # dep reference → file path
│   ├── extract-deps.ae        # parse a .ae file's dep(b, "...") lines
│   ├── scan-ae-files.ae       # walk cwd for every .*.ae build file
│   ├── topo-sort.ae           # DFS post-order over the file dep graph
│   ├── transform-ae.ae        # rewrites user .ae files for linking
│   ├── gen-orchestrator.ae    # emits the single-binary orchestrator
│   ├── resolve-imports.sh     # transitive import resolution for transform-ae
│   └── resolver/              # Maven Resolver wrapper (→ aeb-resolve.jar)
├── itests/                    # integration tests (real open-source projects)
├── README.md
└── TODO.md
```

## Example repo

[google-monorepo-sim](https://github.com/paul-hammant/google-monorepo-sim)
— a simulated Google-style monorepo with Java, Kotlin, Go, Rust, C#,
TypeScript, and Python modules, cross-language FFI, JNI, and per-language
third-party deps. Everything builds and tests from a single `aeb`
invocation.

## Unit tests (`tests/`)

Command-string builder tests for every SDK — `javac_cmd()`, `cargo_build_cmd()`,
`go_build_cmd()`, `kotlinc_cmd()`, `junit_cmd()`, `lxc_cmd()`,
`image_build_cmd()`, `dockerfile_content()`. Each test constructs a config
map and asserts the generated command string matches expectations. No
external tools needed — pure string-builder testing.

Run locally on macOS or Linux:

```bash
./tests/run.sh              # all tests
./tests/run.sh cargo        # pattern filter — runs test_cargo_cmd only
AETHER=/path/to/ae ./tests/run.sh   # override ae binary
```

Output classifies each test as `BUILD OK / RUN PASS`, `BUILD FAIL`, or
`RUN FAIL`, so it's obvious whether a failure is a regression in aeb
or an upstream Aether compiler issue. Exits non-zero on any failure.

## Integration tests (`itests/`)

Real-world open-source projects converted from their native build systems
to aetherBuild. Upstream sources are fetched via `itests/fetch-upstream.sh`
and not committed — only the `.build.ae`, `.tests.ae`, `.dist.ae`,
`.bom.ae`, and migration-status docs are tracked. Some repositories are
pinned to a specific commit to avoid source drift against unreleased
upstream dependencies.

### spring-data-examples (Maven → aeb)

Source: [spring-projects/spring-data-examples](https://github.com/spring-projects/spring-data-examples)
(pinned to commit `cd0d2b36`)

~90 leaf modules across JPA, MongoDB, Redis, Cassandra, JDBC, R2DBC, REST,
Web, and multi-store scenarios. Replaces 107 `pom.xml` files. Uses
`java.javac(b)` + `java.junit5(b)` with a Spring Boot BOM for version
management, TestContainers for integration tests (Podman-compatible).

### nx-examples (Nx → aeb)

Source: [nrwl/nx-examples](https://github.com/nrwl/nx-examples)

13 TypeScript modules — Angular 21 (ngc), React 18 (tsc), shared
libraries, Web Components. Uses `ts.tsc` / `angular.ngc` / `jest.test`.
npm deps resolved via pnpm.

### clojure-multiproject-example (deps.edn → aeb)

Source: [adityaathalye/clojure-multiproject-example](https://github.com/adityaathalye/clojure-multiproject-example)

6 Clojure modules — shared library (grugstack), 4 Ring/Jetty/SQLite web
apps, 1 stub. Replaces `deps.edn` aliases and `build/build.clj`
orchestration. Clojure's source-path classpath model is chained
transitively via a `clojure_src_path` artifact.

### scala-cli-multi-module-demo (scala-cli → aeb)

Source: [VirtusLab/scala-cli-multi-module-demo](https://github.com/VirtusLab/scala-cli-multi-module-demo)

3 Scala 3 modules — shared library + 2 apps. The Scala 3 compiler is
invoked directly (`java -cp dotty.tools.dotc.Main`), same as javac. No
scala-cli, Bloop, Coursier, or sbt — `aeb-resolve.jar` + `java` replaces
the entire stack.

### dotnet-architecture-eShopOnWeb (.NET solution → aeb)

Source: [dotnet-architecture/eShopOnWeb](https://github.com/dotnet-architecture/eShopOnWeb)

10 .NET projects — ASP.NET Core MVC, Blazor WASM, REST API, EF Core,
xUnit. Upgraded from .NET 8 to .NET 10. aeb **generates**
`.{name}.generated.csproj` files from `.build.ae` declarations — NuGet
via `dotnet.nuget_registry`, project refs via `build.dep()`.

### fyne-io/fyne (Go workspace → aeb)

Source: [fyne-io/fyne](https://github.com/fyne-io/fyne)

Multi-module Go workspace. Uses `go.go_build` / `go.go_test`.

### Oxen-AI/Oxen (Rust workspace → aeb)

Source: [Oxen-AI/Oxen](https://github.com/Oxen-AI/Oxen)

Large Rust workspace; exercises `rust.cargo_workspace` and
`rust.cargo_crate`, with `Cargo.toml` generation for both the workspace
root and every member crate. `crate_registry` handles dozens of
dependencies with feature flags.

### python-monorepo-demo (Pants → aeb)

Source: [SystemCraftsman/pants-python-monorepo-demo](https://github.com/SystemCraftsman/pants-python-monorepo-demo)

A small Python monorepo. Uses `python.install` / `python.pytest` with
`.whl.ae` dep files (`python.wheel_registry` / `wheel_vendored`).
`python.package(b)` generates `pyproject.toml` at build time — no
hand-written packaging metadata.

### mrhdias_rust_store (Tokio → aeb)

Source: [mrhdias/store](https://github.com/mrhdias/store)

A small Tokio/axum Rust project. Single-crate `Cargo.toml` generation.

## Requirements

- [Aether](https://github.com/AetherLang/aether) compiler (`ae`) —
  recent enough to include the `hoist_loop_vars` and parameter-redeclare
  fixes in `codegen/`.
- Language toolchains for whatever you build: `javac`, `kotlinc`, `go`,
  `cargo`, `tsc`/`node`, `scala` (Scala 3 compiler jar), `clojure`,
  `dotnet` SDK, `python3`, `pnpm`.
- For Java projects with Maven deps: build the resolver jar once —

### macOS notes

`aeb` itself works on macOS — the bash trampoline uses only POSIX-level
features and the orchestration has been ported to Aether, so nothing in
the runner path requires GNU bash or GNU coreutils. You can run
`./tests/run.sh` (the unit-test runner) straight from a vanilla macOS
install.

There is one known limitation for full multi-module builds: the link
step in `tools/aeb-link.ae` passes `-Wl,--allow-multiple-definition`,
which is GNU ld only. Apple's ld64 rejects it, and a full `./aeb` run
that links more than a handful of modules will fail with duplicate
symbol errors until that flag is platform-gated or the underlying
duplicate-symbol emission is fixed upstream in the Aether compiler.
See `TODO.md` for tracking.

Container-related SDKs also have reduced functionality on macOS:
Docker Desktop or Podman is required for image builds, and LXC is
Linux-only. All other SDKs (Java, Kotlin, Go, Rust, TypeScript, Scala,
Clojure, .NET) work identically to Linux.

  ```bash
  mvn -f tools/resolver/pom.xml package
  mv tools/resolver/target/aeb-resolve-1.0.0.jar tools/aeb-resolve.jar
  rm -rf tools/resolver/target
  ```
- For TestContainers-based tests: Docker or Podman. aeb auto-detects the
  user-level Podman socket.
