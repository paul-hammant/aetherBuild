# Maven to aetherBuild Migration — Dependency Status

90 leaf modules converted from Maven to aeb. All compilation and testing
is driven by `.build.ae` / `.tests.ae` files using the Java SDK.

## Still invoked by aeb daily

- **`javac`** — `java.javac(b)` in `.build.ae` compiles `src/main/java/**/*.java` with `release("25")`, `source_layout("maven")`, and `enable_preview()`. aeb resolves Maven dependencies via `aeb-resolve.jar` using the BOM for version management.

- **`javac` (tests)** — `java.javac_test(b)` in `.tests.ae` compiles `src/test/java/**/*.java` with the same settings.

- **JUnit 5** — `java.junit5(b)` in `.tests.ae` discovers and runs test classes. Replaces Maven Surefire for test execution.

## BOM and dependency management

A single `spring-boot.bom.ae` at the repo root declares:
```
bom(b, "org.springframework.boot:spring-boot-dependencies:4.0.4")
```

Every module loads it via `load_bom_file(b, "../../spring-boot.bom.ae")`.
Dependencies are declared without versions — the BOM provides them,
the same way Maven's `<dependencyManagement>` works. Resolution is handled
by `aeb-resolve.jar` which uses the Maven Resolver API to fetch transitive
deps into `~/.aeb/repo`.

## Maven tooling still present but not invoked

The original `pom.xml` files (107 of them) remain in the repo as reference
but are not used by aeb. Notable Maven plugins that were replaced:

- **maven-compiler-plugin** — replaced by `java.javac(b)`
- **maven-surefire-plugin** — replaced by `java.junit5(b)`
- **apt-maven-plugin** (QueryDSL) — used in mongodb/querydsl for annotation
  processing. Not yet migrated to aeb; would need a `java.annotation_processor()`
  SDK function or a pre-build step.

## Module counts

- 90 `.build.ae` files (compilation)
- 86 `.tests.ae` files (test execution)
- 0 `.dist.ae` files (no packaging — these are library examples, not deployable apps)
- 14 data store categories: JDBC, JPA, MongoDB, Redis, Cassandra, Couchbase,
  Elasticsearch, Neo4j, R2DBC, LDAP, Map, REST, Web, Multi-store

## Google-style DAG and sparse checkout

Same full DAG as [google-monorepo-sim](https://github.com/paul-hammant/google-monorepo-sim)
and the nx-examples migration. aeb scans all `.build.ae` / `.tests.ae` files,
topological-sorts the dependency graph, and generates a single orchestrator
binary. In-repo deps are explicit `dep()` calls (e.g. `dep(b, "jdbc/basics")`),
greppable for `gcheckout` sparse-checkout support.

Most modules in this repo are independent (no in-repo deps), but some do
depend on shared utilities (e.g. `mongodb/util`, `cassandra/util`, `redis/util`),
forming small DAG clusters.
