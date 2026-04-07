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

The runner (`ae-build`) does four things:

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

## Running

```bash
# From the monorepo root:
AETHER=/path/to/ae ae-build
```

Builds all compile targets, then runs all dist steps, then runs all tests.

Output:

```
ae-build: 18 compile + 2 dist + 17 test
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
├── ae-build              # runner script (scan, topo-sort, generate, compile, run)
├── lib/build/module.ae   # SDK: all language compilers, test runners, packaging
├── README.md
└── TODO.md               # roadmap: config DSL, parallelism, sandboxing
```

The consuming monorepo needs `lib/build/` to resolve the `import build`
statement. Symlink or copy from aetherBuild:

```bash
ln -s /path/to/aetherBuild/lib/build monorepo/lib/build
```

## Example repo

[google-monorepo-sim](https://github.com/paul-hammant/google-monorepo-sim) —
a simulated Google-style monorepo with Java, Kotlin, Go, Rust, and TypeScript
modules, cross-language FFI, and JNI. Originally built with bash scripts,
now ported to Aether Build: 18 compile targets, 2 fat jars, 17 test suites
all passing from a single `ae-build` invocation.

## Requirements

- [Aether](https://github.com/AetherLang/aether) compiler (`ae`) — multi-pattern-glob branch or later
- Language toolchains: `javac`, `kotlinc`, `go`, `rustc`/`cargo`, `tsc`, `node`
