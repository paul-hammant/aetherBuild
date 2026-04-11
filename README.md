# Aether Build

A build system for polyglot monorepos. Five-line build files replace
thirty-line bash scripts. Convention does the work; you declare the intent.

## What it looks like

A Java component with one dependency:

```aether
import build

main() {
    b = build.start()
    build.dep(b, "java/components/vowelbase")
    build.javac(b)
}
```

A Rust shared library:

```aether
import build

main() {
    b = build.start()
    build.cargo_dep(b, "libs:rust/jni")
    build.cargo_build(b, "libvowelbase.so")
}
```

A TypeScript app with cross-language FFI (TypeScript → Go):

```aether
import build

main() {
    b = build.start()
    build.dep(b, "typescript/components/explanation")
    build.dep(b, "go/components/nasal")
    build.npm_dep(b, "libs:javascript/ffi-napi")
    build.tsc(b)
}
```

A test module:

```aether
import build

main() {
    b = build.start()
    build.dep(b, "java/components/vowelbase")
    build.lib(b, "java/junit/junit.jar")
    build.lib(b, "java/hamcrest/hamcrest.jar")
    build.javac_test(b)
    build.junit(b)
}
```

A fat jar (dist):

```aether
import build

main() {
    b = build.start()
    build.shade(b, "applications.monorepos_rule.MonoreposRule", "monorepos-rule.jar")
}
```

## How it works

Each module directory contains a `.build.ae` file (compilation),
`.tests.ae` file (tests), and optionally a `.dist.ae` file (packaging).

The runner (`aeb`) does four things:

1. **Scan** — find all `.build.ae` / `.tests.ae` / `.dist.ae` files
2. **Graph** — grep `dep()` lines to build the dependency DAG
3. **Sort** — topological order
4. **Generate** — produce a single `.ae` file with one function per module, compile it to a native binary, run it

The generated binary is one process with one in-memory visited-module map.
Each module function calls its deps directly — no subprocesses, no file-based
coordination. The visited map prevents redundant builds:

```
build_rust_components_vowelbase(s)    // compiles Rust .so
build_java_components_vowelbase(s)    // calls Rust first (already done → skip), then javac
build_java_components_vowels(s)       // calls vowelbase (skip), then javac
```

Module paths encode to C-safe function names: `/` → `_`, literal `_` → `__`.

## Setup

Initialize a repo to use aetherBuild:

```bash
/path/to/aeb --init
```

This creates symlinks in `lib/` for each shipped SDK module (build, java,
kotlin, go, rust, ts, scala, clojure, dotnet, maven, pnpm, jest, webpack, angular) and adds them to `.gitignore`. Safe to run
repeatedly — existing correct symlinks are left alone.

## Running

```bash
# From the monorepo root:
AETHER=/path/to/ae aeb
```

Builds all compile targets, then runs all dist steps, then runs all tests.

### Target filtering

Build a single target and its transitive deps:

```bash
aeb java/components/vowels                          # just this + deps
aeb javatests/components/vowelbase                   # auto-detects test, builds deps + runs it
aeb --dist java/applications/monorepos_rule          # compile + package
```

The target type is auto-detected from which build file exists at the
path (`.build.ae`, `.tests.ae`, or `.dist.ae`). Use `--dist` when a
module has both `.build.ae` and `.dist.ae` and you want packaging.

### Full build output

```
aeb: 18 compile + 2 dist + 17 test
go/components/nasal: compiling Go prod & test code
rust/components/vowelbase: compiling prod code
java/components/vowelbase: compiling prod code
...
dist:java/applications/monorepos_rule: packaging monorepos-rule.jar
...
javatests/components/vowelbase: tests PASSED
typescripttests/applications/mmmm: tests PASSED
```

Second run skips unchanged modules (timestamp-based):

```
go/components/nasal: skipping compilation of Go prod & test code (not changed)
...
javatests/components/vowelbase: tests PASSED
```

## Dependencies

Dependencies are declared as function calls, one per line:

```aether
build.dep(b, "java/components/vowelbase")    // module in the monorepo
build.lib(b, "java/junit/junit.jar")         // vendored binary (jar, .a, .so)
build.npm_dep(b, "libs:javascript/ffi-napi") // vendored npm package
build.cargo_dep(b, "libs:rust/jni")          // vendored cargo crate
```

These lines are greppable — the build graph is extracted by scanning
files, not by compiling them. Same contract as Bazel's BUILD files.

## Language SDKs

### Java

```aether
build.javac(b)                      // compile **/*.java
build.javac_test(b)                  // compile test **/*.java
build.junit(b)                       // run *Tests.class with JUnitCore
build.shade(b, "com.Main", "a.jar")  // fat jar with all deps + native libs
```

### Kotlin

```aether
build.kotlinc(b)                                       // compile **/*.kt
build.kotlinc_test(b)                                   // compile test **/*.kt
build.kotlin_test(b, "pkg.TestClassKt")                 // run test class via java -ea
```

Shares JVM classpath format with Java. Cross-language deps just work.

### Go

```aether
build.go_build(b, "c-shared", "libname.so")  // shared library
build.go_test(b)                              // go test -v .
```

### Rust

```aether
build.cargo_dep(b, "libs:rust/jni")           // local registry crate
build.cargo_build(b, "libname.so")            // cargo build --release
```

### TypeScript

```aether
build.tsc(b)                                  // compile via tsconfig.json
build.mocha(b, "path/to/Tests.js")            // run with Mocha + NODE_PATH
```

## Cross-language dependencies

Modules in different languages can depend on each other:

```aether
build.dep(b, "rust/components/vowelbase")  // Java gets .so via ldlibdeps
build.dep(b, "kotlin/components/sonorants")  // Java gets JVM classpath
build.dep(b, "go/components/nasal")          // TypeScript gets .so via LD_LIBRARY_PATH
```

Each SDK writes artifact metadata to `target/{module}/`. Downstream
SDKs read these files to assemble classpaths, library paths, and
tsconfig mappings transitively.

Working cross-language chains:
- Java → Rust (JNI shared library)
- Java → Kotlin (JVM classpath interop)
- Java → Go (shared library)
- TypeScript → Go (FFI via ffi-napi)

## Incremental builds

SDK functions check file timestamps before doing work. If source files
haven't changed since the last build, compilation is skipped. Tests
always run (only compilation is skipped).

## Project structure

```
aetherBuild/
├── aeb                        # runner script (scan, topo-sort, generate, compile, run)
├── lib/
│   ├── build/module.ae        # core: session, deps, context, shared helpers
│   ├── java/module.ae         # language: javac, junit, junit5, shade
│   ├── kotlin/module.ae       # language: kotlinc
│   ├── go/module.ae           # language: go build, go test
│   ├── rust/module.ae         # language: cargo build
│   ├── ts/module.ae           # language: tsc, mocha
│   ├── scala/module.ae        # language: scala-cli compile, test, package
│   ├── clojure/module.ae      # language: clojure compile, clojure.test
│   ├── dotnet/module.ae       # language: .csproj generation, dotnet build/test
│   ├── maven/module.ae        # package mgr: BOM, dep resolution, classpath
│   ├── pnpm/module.ae         # package mgr: npm dep resolution via pnpm
│   ├── jest/module.ae         # test runner: Jest
│   ├── webpack/module.ae      # bundler: Webpack
│   ├── angular/module.ae      # framework: ngc, ng build
│   └── container/module.ae    # infra: OCI images, LXC containers
├── README.md
└── TODO.md
```

The consuming monorepo needs SDK modules in `lib/` to resolve imports.
Run `aeb --init` from the monorepo root to set up the symlinks.

## Example repo

[google-monorepo-sim](https://github.com/paul-hammant/google-monorepo-sim) —
a simulated Google-style monorepo with Java, Kotlin, Go, Rust, and TypeScript
modules, cross-language FFI, and JNI. Originally built with bash scripts,
now ported to Aether Build: 18 compile targets, 2 fat jars, 17 test suites
all passing from a single `aeb` invocation.

## Integration tests (`itests/`)

Real-world open-source projects converted from their native build systems
to aetherBuild. Upstream sources are fetched via `itests/fetch-upstream.sh`
and not committed — only the `.build.ae`, `.tests.ae`, `.bom.ae`, and
migration status docs are tracked.

### spring-data-examples (Maven → aeb)

Source: [spring-projects/spring-data-examples](https://github.com/spring-projects/spring-data-examples)

90 leaf modules across JPA, MongoDB, Redis, Cassandra, JDBC, R2DBC, etc.
Replaces 107 `pom.xml` files. Uses `java.javac(b)` / `java.junit5(b)` with
Spring Boot 4.0.4 BOM for version management. Maven deps resolved via
`aeb-resolve.jar`. TestContainers works with Podman.

Insight: the BOM mechanism (`load_bom_file` + resolver) handles Maven's
`<dependencyManagement>` without reimplementing Maven's POM model.

### nx-examples (Nx → aeb)

Source: [nrwl/nx-examples](https://github.com/nrwl/nx-examples)

13 TypeScript modules — Angular 21 (ngc), React 18 (tsc), shared libraries,
Web Components. Uses `ts.tsc(b)` / `angular.ngc(b)` / `jest.test(b)`. npm
deps resolved via pnpm. 7/7 test suites pass.

Insight: separating `ts`, `angular`, `jest`, `pnpm`, and `webpack` into
independent SDK modules keeps each build file focused on what it compiles,
not how package management works.

### clojure-multiproject-example (deps.edn → aeb)

Source: [adityaathalye/clojure-multiproject-example](https://github.com/adityaathalye/clojure-multiproject-example)

6 Clojure modules — shared library (grugstack), 4 web apps (Ring/Jetty,
SQLite), 1 stub. Replaces `deps.edn` aliases and `build/build.clj`
orchestration. Uses `clojure.compile(b)` / `clojure.test(b)`. Clojure
libs come from Clojars via `clojars.bom.ae`.

Insight: Clojure's source-path-on-classpath model (vs Java's compiled-classes
model) required a `clojure_src_path` artifact to chain source directories
transitively through inter-module deps. Also exposed a resolver gap —
Jetty's POM uses parent-BOM property interpolation for version numbers,
requiring a `clojure-dep-patches.bom.ae` to list the missing transitives
explicitly. This led to adding `dep()` support in `.bom.ae` files.

### scala-cli-multi-module-demo (scala-cli → aeb)

Source: [VirtusLab/scala-cli-multi-module-demo](https://github.com/VirtusLab/scala-cli-multi-module-demo)

3 Scala 3 modules — shared library + 2 apps. Uses `scala.scalac(b)`,
`scala.scalac_test(b)`, `scala.munit(b)`. No scala-cli, Bloop, or
Coursier — the Scala 3 compiler is invoked directly as
`java -cp dotty.tools.dotc.Main`, same as javac.

Insight: Scala doesn't need its own build tool. The compiler is a JVM
program, deps are Maven coordinates, tests run via JUnit. The entire
scala-cli/sbt/Coursier stack is replaced by `aeb-resolve.jar` + `java`.

### dotnet-architecture-eShopOnWeb (.NET solution → aeb)

Source: [dotnet-architecture/eShopOnWeb](https://github.com/dotnet-architecture/eShopOnWeb)

10 .NET projects — ASP.NET Core MVC, Blazor WASM, REST API, EF Core,
xUnit tests. Upgraded from .NET 8 to .NET 10. aeb **generates**
`.{name}.generated.csproj` files from `.build.ae` declarations — NuGet
packages via `nuget("PackageName")`, project references via `build.dep()`,
properties via DSL setters. The original `.csproj` files become unnecessary.

Insight: unlike JVM languages where we replace the build tool entirely,
.NET keeps `dotnet build` and `Directory.Packages.props` for NuGet version
management. But the `.csproj` files themselves are generated — aeb owns the
dependency graph and project configuration. When .NET 11 drops `.csproj`
requirements, aeb is already prepared.

## Requirements

- [Aether](https://github.com/AetherLang/aether) compiler (`ae`) — multi-pattern-glob branch or later
- Language toolchains: `javac`, `kotlinc`, `go`, `rustc`/`cargo`, `tsc`, `node`

### Maven dependency resolver (required for Maven-based projects)

Projects that use `maven_dep()` or `bom()` need the resolver jar built once:

```bash
mvn -f tools/resolver/pom.xml package
mv tools/resolver/target/aeb-resolve-1.0.0.jar tools/aeb-resolve.jar
rm -rf tools/resolver/target
```

This produces `tools/aeb-resolve.jar` (not checked in — listed in `.gitignore`).
