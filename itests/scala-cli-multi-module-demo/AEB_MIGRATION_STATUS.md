# scala-cli to aetherBuild Migration Status

Upstream: https://github.com/VirtusLab/scala-cli-multi-module-demo.git

## Modules

| Module | Compile | Tests | Notes |
|--------|---------|-------|-------|
| common | OK | PASS | Shared library, munit tests via JUnit |
| module-1 | OK | — | App, depends on common |
| module-2 | OK | — | App, depends on common |

3 modules compile, 1/1 test suite passes.

## What aeb replaces

- scala-cli (entire tool — no longer needed)
- Bloop compilation server
- Coursier dependency resolution
- `//> using file` directives (inter-module deps) → `build.dep()`
- `//> using dep` directives (Maven deps) → `dep()` in `.build.ae`/`.tests.ae`
- `package.sh` scripts (not yet replaced — needs assembly jar or GraalVM)

## What aeb uses

- `scala.scalac(b)` — invokes Scala 3 compiler directly via `java -cp dotty.tools.dotc.Main`
- `scala.scalac_test(b)` — compiles test sources against prod classes
- `scala.munit(b)` — runs munit tests via JUnit 4 runner
- Scala compiler + library jars resolved via `aeb-resolve.jar` from Maven Central
- munit + hamcrest resolved the same way

## How it works

No Scala toolchain installation required — just Java. The SDK module:

1. Resolves `org.scala-lang:scala3-compiler_3:3.8.2` via `aeb-resolve.jar`
2. Resolves `org.scala-lang:scala3-library_3:3.8.2` for the compile classpath
3. Invokes `java -cp <compiler-jars> dotty.tools.dotc.Main -d classes/ -cp <library+deps> *.scala`
4. For tests, resolves munit, compiles test sources, runs via `org.junit.runner.JUnitCore`

This is the same pattern as Java (`javac`) and Kotlin (`kotlinc`) — the language
compiler is just a JVM program invoked with the right classpath.
