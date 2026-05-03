# Notes to self (LLM assisting on aetherBuild / aeb)

Not a CLAUDE.md — short, opinionated, written for a future LLM picking
up mid-task. Re-read at start of every session.

## What aetherBuild is, in one paragraph

Polyglot-monorepo build runner. Replaces `Makefile` / `pom.xml` /
`package.json scripts` / `Cargo.toml` / `.csproj` / `pyproject.toml`
with small declarative dot-prefixed `.ae` files co-located with each
module. Convention does the work; the file declares intent (sources,
deps, output). The runner (`aeb`) walks the tree, builds a file-based
DAG from `build.dep("path/to/.foo.ae")` lines (greppable, like Bazel
BUILD files — every dep edge is a literal string in source, no
runtime evaluation), topo-sorts, generates a single orchestrator
`.ae` file with one function per module, compiles the whole thing to
C, links to a native binary, runs it. Each module function is
`_static`, sharing one in-memory visited-module map; no subprocesses,
no file-based coordination at runtime.

### Filenames, not magic targets

aeb has *no special target source-file names*. It scans every
dot-prefixed `.ae` file under cwd (`.build.ae`, `.tests.ae`,
`.foo.ae`, `.whatever.ae`) and builds a DAG over them. The
suffix-naming convention (`.tests.ae` classifies as a test target
in summaries; `.dist.ae` as a packaging target; `.{name}.jar.ae`
etc. as third-party-dep declarations) is just a classification layer
on top of "any dot-prefixed `.ae` file under cwd is a node in the
graph." Default classification when no suffix matches is `build`.

A repo wanting `.foo.ae` and `.bar.ae` as siblings gets them — same
syntax, same DAG semantics. The `.build-<tag>.ae` /
`.tests-<tag>.ae` / `.dist-<tag>.ae` shapes extend the suffix family
without changing the "any dot-prefixed `.ae` file is a target" rule.
Multiple build files in the same directory get distinct DAG nodes
via a `:tag` suffix on their visited-set key (preserved in
human-display labels, stripped before deriving filesystem paths).

### How the DAG is actually drawn

`build.dep(b, "path/to/.foo.ae")` is the only edge-declaration
mechanism. Each call inside a `.build.ae`/`.tests.ae`/`.dist.ae`
adds one edge from the calling file to the named file.
`tools/extract-deps.ae` greps for these calls statically (a regex
pass, no Aether evaluation required) — the same shape as Bazel's
`BUILD` files where dep relationships are visible to text tools, not
hidden behind macro expansion. Three points worth knowing:

1. **Edges are file-to-file, not module-to-module.** Two
   `.build*.ae` in the same directory are distinct nodes via the
   tag-disambiguation rule above. `dep("foo/.build-seed.ae")` is a
   different edge from `dep("foo/.build.ae")`.
2. **No reverse edges, no fan-out queries.** The DAG is constructed
   top-down from the target inwards, like a recursive-descent
   walk. "Who depends on X" requires a separate scan of the entire
   tree (the `gcheckout` walker is an example of doing this for
   sparse-checkout purposes).
3. **`build.dep()` is a runtime no-op.** It does nothing at execution
   time; the DAG is built entirely from textual extraction *before*
   any `.ae` file runs. Like `BUILD` rules, deps are data, not
   procedure.

## How to anchor aeb against build systems you already know

Think of it as: **Bazel's DAG shape + a per-language SDK pattern
(more like Buck) + Aether closures as the configuration DSL.**

**Directed-graph style, like Bazel — more so than depth-first-recursive
like Maven.** Maven's reactor traverses `<modules>` depth-first and
walks each module through its lifecycle (validate → compile → test →
package → install) before moving on; one module's `package` happens
before the next module's `compile` even if there's no dep edge.
aeb scans the whole tree first, builds a directed graph from
`build.dep("path/to/.build.ae")` edges (greppable, statically
extractable, like Bazel BUILD files), topo-sorts, and produces every
artifact in dependency order. Modules with no edge between them
have no implied ordering — independent subgraphs can in principle
build concurrently (parallelism is a TODO; the graph supports it).

- **NOT Make/CMake** — no targets-as-rules, no shell-scriptlets in
  build files. Each `.build.ae` is an Aether program with a `main()`
  that calls `build.start()` then a language SDK builder.
- **NOT Bazel** — no rule definitions, no Skylark, no remote
  execution. Closer to Buck's "languages have opinionated SDKs" but
  the SDK is hand-written Aether under `lib/<lang>/module.ae`, not
  starlark.
- **NOT Maven/Gradle** — but it can read `aether.toml [[bin]]` for
  Aether targets (the shell-out path), and `lib/maven/` resolves
  Maven coordinates against `~/.m2` for Java targets. Maven config
  still travels in `pom.xml` for projects that have it; aeb's
  Java SDK doesn't try to replace Maven's resolver.
- **NOT Nix** — and the overlap is shallower than it looks. Nix is
  whole-system: it packages a derivation closure (compiler, libc,
  every transitive dep) and is hermetic by construction (sandbox,
  content-addressed store, reproducible bit-for-bit). aeb operates
  inside an already-cloned repo, uses whatever's on `PATH`, and has
  per-module mtime caching. Nix's value comes from the closure;
  aeb's comes from the per-module DSL ergonomics. Could aeb emit
  Nix derivations from `.build.ae` files in the future? Mechanically
  yes — `.build.ae` is already the declarative description Nix
  would need. Practically: nobody's done it, and the value-add
  over "use Nix directly with Nix's own language" is unclear.
  The natural shape if it ever happens is an `aeb-to-nix`
  exporter, not re-architecting the runtime around the Nix store.
- **DSL shape is closure-with-setters** — same idiom as Aether's
  `actor { state ... receive { ... } }` blocks:
  `aether.program(b) { source("main.ae") output("svn") regen("...") }`.
  Setters are plain functions that take an invisible `_ctx` first
  arg; aeb reads the populated map after the block runs.

### Scope coverage — honest scoring

What modern build systems claim, and how aeb measures up today.
"Scope" here means the dimension that system most prizes; "aeb
status" is the unembellished current state, not the roadmap.

| Dimension                          | What "good" looks like                                          | aeb status                                                                                              |
|------------------------------------|-----------------------------------------------------------------|---------------------------------------------------------------------------------------------------------|
| Build-graph topology               | Real DAG, statically extractable, greppable                     | ✓ Done. File-based DAG via `build.dep("path/.build.ae")` lines, scanned without compilation.            |
| Multi-language polyglot            | First-class for >=5 languages, real cross-language deps         | ✓ Done. 12+ SDKs (Java, Kotlin, Go, Rust, TS, Scala, Clojure, .NET, Python, Aether, Bash, Container).   |
| Cross-language FFI artifacts       | JNI / cdylib / shared-library handoff between SDKs              | ✓ Done. Java→Rust (JNI), Java→Kotlin, Java→Go (.so), TS→Go, C#→Rust, Python→Rust (ctypes) all wired.    |
| Local incremental cache            | Skip work when inputs unchanged                                 | ◐ Partial. Content-addressed cache (`lib/cache`, sha256+zlib) wired into `lib/maven` (classpath), `lib/aether` (manual-path link), `lib/java` (javac + javac_test classes-tree, tar+zlib). Other SDKs still use mtime-only. Wiring proceeds SDK by SDK. |
| Remote build cache                 | Share artifacts across machines (Bazel, Gradle, Turborepo)      | ✗ TODO. Roadmap entry; `target/<module>/` artifact metadata files are the natural cache units.         |
| Affected-target detection          | `git diff` → reverse-dep walk → only-build-changed              | ✓ Done. `aeb --since <ref>` builds only targets impacted by changes; `aeb --print-affected <ref>` lists them. Source-to-target ownership: nearest enclosing dir with a build file. |
| Hermetic toolchains                | Pinned compiler/runtime per build, downloaded if missing        | ✗ Uses whatever's on `PATH`. Same gap as `bazel-gaps.md` § hermetic-LLVM.                                |
| Dependency resolution (transitive) | Maven / npm / Cargo / NuGet closures resolved + classpath built | ✓ Done for Maven (via `tools/aeb-resolve.jar`), npm (pnpm), NuGet, Cargo, Python wheels.                 |
| Lockfiles for reproducibility      | Pin every transitive dep to exact version + hash                | ✗ TODO. Resolver computes the closure but doesn't write/check a lockfile.                                |
| Test orchestration                 | Discover, run, report, parallelise, isolate                     | ◐ Partial. `bash.test` (jobs/pre/post hooks), `*.junit5` etc. exist; per-target pass/fail counts feed `[telemetry]` block (`14/14 PASS` / `28/30 FAIL`). Structured XML reports TODO. |
| Artifact publishing                | Push to npm / Maven Central / NuGet feed / OCI registry         | ✗ TODO. `shade()` builds fat jars; no `.dist`-side publish step yet.                                    |
| Build graph visualisation          | DOT / Mermaid / interactive graph                               | ✓ Done. `aeb --graph` (DOT) / `aeb --graph mermaid`. Pipe to `dot -Tsvg` or paste into a Markdown fence. |
| IDE / LSP integration              | Editor knows about build targets, jump-to-source                | ✗ TODO. No `aeb-lsp` yet; `.build.ae` files use the Aether LSP for syntax only.                          |
| Watch mode                         | Rebuild on file change                                          | ✓ Done. `aeb --watch [target]` watches source dirs (Linux: inotifywait, macOS: fswatch); change events flow through the affected-target walk and a narrowed rebuild fires. Composes with cache + telemetry. |
| Sandboxing / isolation             | Build steps see only declared inputs                            | ✗ TODO. Aether's runtime sandbox is per-process, not per-build-step. Roadmap.                            |
| Sparse checkout for monorepos      | Fetch only the modules a target needs                           | ✓ Done via `aeb gcheckout` (DAG walk → `git sparse-checkout`).                                          |
| Configuration DSL ceiling          | Expressive without escape hatches into bash/python/Skylark      | ✓ Closure-with-setters, fully Aether-native, no eval'd config.                                          |
| Migration story                    | Add to existing repo without big-bang rewrite                   | ✓ Per-module `.build.ae`, coexists with whatever's there. itests prove this against real repos.        |
| Cross-compilation                  | Build for non-host OS/arch                                      | ✗ TODO. Roadmap.                                                                                         |
| Build telemetry                    | Per-module timing, cache hit rates, bottleneck analysis         | ◐ Partial. Per-module wall-time + cache outcome as `[telemetry]` block at end of every build (in-memory records, stdout renderer). Future renderers (file dump, web view) plug in via `build.render_telemetry` and the records list. |
| CI system integration              | Auto-detects GHA/GitLab/Jenkins, sets outputs, tags artifacts   | ✗ Deliberately CI-agnostic today; "is this CI" detection is a roadmap line item.                        |

Overall pattern: **graph, multi-language, and dependency resolution
are solid** (the load-bearing axes for "is this a real build system").
**Caching, publishing, IDE, and observability are the cluster of
weaknesses** that distinguish a young tool from a mature one. aeb is
~6 months in and maintained by one human and a sequence of session-
scoped LLMs; the gaps are honest, not hidden.

## How `aeb` actually works

Runtime flow (one `aeb` invocation):

1. **Bash trampoline** (`./aeb`, ~50 lines) sets `AETHER`, `AEB_HOME`,
   `ROOT`, optional `DOCKER_HOST` for podman. Routes `--init` /
   `gcheckout` / `--version` to subcommand-specific binaries; falls
   through to `tools/aeb-main` for normal builds.
2. **`tools/aeb-main`** parses args, walks the tree via
   `tools/scan-ae-files`, picks targets (specific path or every
   `.*.ae` under cwd), then exec's `tools/aeb-link`.
3. **`tools/aeb-link`** is the heavy lifting per-build: per-file
   `transform-ae` (rewrites user `.ae` to embed the orchestrator's
   visited-set guard), `aetherc src.ae src.c` per file,
   `gen-orchestrator` to emit one driver `.ae` calling each
   module fn, then **one `gcc`** linking everything into
   `target/_ae_build_all`, then `exec` that binary.
4. The exec'd binary calls each module's function once, in topo
   order. Functions are `static` per translation unit; the visited
   map dedups multi-imported modules.

The trampoline lazy-builds tools at first use (cached in `tools/*`
binaries, gitignored). `make install` pre-builds them and copies the
runtime tree to `$PREFIX/share/aeb/`, with a wrapper at
`$PREFIX/bin/aeb` that pins `AEB_HOME`.

## Files/dirs worth knowing

- `aeb` — bash trampoline. ~50 lines. Read it first if anything about
  trampoline behaviour confuses you.
- `tools/aeb-link.ae` — the per-build orchestration. THE largest
  Aether file. If a build fails between scan and link, the bug is
  here.
- `tools/aeb-graph.ae` — `aeb --graph` renderer. Reads
  `target/_aeb/_edges.txt` and emits DOT (default) or Mermaid.
  Pure-render: no I/O beyond reading the edges file. Pattern model
  for future render-from-edges tools (e.g. telemetry).
- `lib/meta/module.ae` — distribution metadata SDK. Setters
  (`desc`, `homepage`, `license`, `version`, `url`, `sha256`,
  `maintainer`) record into the build map on `b`; orthogonal to
  building. Source-of-truth for downstream exporters.
- `lib/brew/module.ae` — Homebrew exporter SDK. `brew.formula(b)`
  closure in a `.dist.ae` reads `meta.*` plus its own setters
  (`aeb_target`, `binary`, `class_name`, `test_assertion`) and
  writes `target/<module>/<binary>.rb`. Pattern model for future
  exporter SDKs (`nix.derivation`, `deb.control`): each is just
  another `builder` that consumes the shared `meta` map. **No
  CLI flag** — distribution is a target type, not a switch.
- `tools/file-to-label.ae` / `tools/aeb-link.ae` / `tools/gen-orchestrator.ae`
  — three copies of the `file_to_label` logic that disambiguates
  multiple `.build*.ae` per directory. **They must stay in sync.**
  Tracked in `TODO.md` § Test coverage gaps. No drift detection
  today.
- `lib/build/module.ae` — the core API: `build.start()`,
  `build.begin()`, `build.dep()`, `build._get()`, artifact helpers.
  Every language SDK depends on this. Also hosts shared **fixture
  synthesis** (`_synth_fixture_pre`, `_synth_fixture_post`,
  `_has_fixtures`) — test SDKs that need spawn/seed/cleanup
  lifecycle (today: `bash.test`, future: `aether.driver_test`)
  populate `fixture_seeds` / `fixture_servers` records on their
  builder map and call these helpers to lower them into shell
  statements. The shell-quoting traps are nontrivial — read the
  docstrings before adding a new caller.
- `lib/<lang>/module.ae` — language SDKs. Java, Kotlin, Go, Rust,
  TypeScript, Scala, Clojure, .NET, Python, Aether (native programs),
  Bash (test runner), Maven (resolver), pnpm/jest/webpack/angular,
  Container (OCI/LXC). Each SDK exposes `<lang>.<verb>(b) { ... }`
  builders.
- `lib/aether/module.ae` — the Aether-program SDK.
  `aether.program(b)` shells out to `ae build` by default; declaring
  `extra_source(...)` / `link_flag(...)` / `regen(...)` opts into the
  manual `aetherc + gcc` path. Also hosts `aether.program_test` (a
  compiled-binary unit test) and `aether.driver_test` (a compiled
  driver binary that exercises a *separate* binary-under-test, with
  the same fixture grammar as `bash.test`). Driver tests work with
  contrib.aeocha or anything that uses exit code as PASS/FAIL.
- `lib/bash/module.ae` — bash test runner. `bash.test(b)` with
  `script(...)`, `jobs(N)`, `pre_command(...)`, `post_command(...)`,
  and structured server fixtures (`fixture_seed`, `fixture_server`).
  Parallel mode via `xargs -P` (jobs(0) = nproc/2). Hooks AND
  fixtures force sequential.
- `tests/` — string-builder unit tests, one `test_*.ae` per
  command-string-builder. Each asserts the exact string passed to
  exec. Run with `./tests/run.sh` (pattern arg filters).
- `itests/` — integration tests against real-world projects (Spring
  Boot, Angular, .NET eShop, Rust workspaces, etc.). Upstream
  sources fetched via `itests/fetch-upstream.sh`, not committed.
  **Partially passing — see `itests/README.md` for the status
  table.** Headline numbers: spring-data-examples is 68+/90 modules
  compiling, the Rust projects fail (RocksDB C++ build issue,
  upstream crate incompatibility with current rustc). Don't treat
  itests as "all green = ship it"; treat them as "real-world
  scaffolding for ongoing SDK development." Several have
  `AEB_MIGRATION_STATUS.md` files with per-project gap notes.
- `asks/` — feature-request docs, one per ask. Format isn't
  prescribed; recent ones happen to be from a downstream port but
  the directory is general-purpose. Each ask doc captures
  motivation, sketch, what's-not-being-asked, acceptance criteria.
  Ship-or-decline decisions live in commit messages alongside the
  implementation (or absence thereof).
- `TODO.md` — roadmap + known gaps. Section "Test coverage gaps"
  documents three deferred items (regen pass integration, parallel
  + hooks, three-copy `file_to_label`).
- `Makefile` — `make build` (pre-build tools), `make install`
  (proper copy install to `$PREFIX/share/aeb` + wrapper),
  `make uninstall`, `make clean`. `gcheckout` excluded from
  `INSTALL_TOOLS` because of an upstream Aether stdlib link issue.

## Idioms that keep biting

- **Aether is fixed-arity.** Setters that "want" variadic args
  (`extra_sources("a", "b", "c")` — won't compile) are repeated
  single-arg calls (`extra_source("a"); extra_source("b"); ...`).
  Same for `script(...)`, `regen(...)`, `pre_command(...)`,
  `link_flag(...)`. Each repeated call appends to a list in the
  builder map.
- **`_builder` is magic, only in scope inside `builder { ... }`
  bodies.** Plain helpers can't see it. Pass through as
  `builder_map: ptr` parameter from the call site. See
  `_compile_and_link` in `lib/aether/module.ae` for the pattern.
- **The two-`import` requirement for bare setters.** Inside a
  `receiver.method(b) { block }` body, identifiers in `block` are
  resolved as plain top-level calls, not against the receiver's
  namespace. So `bash.test(b) { script("...") }` won't find
  `script` unless `import bash (script)` is also at the top of the
  file. Same for every SDK. Documented in README's "note on the
  two import lines" callout. Real bug filed and pushed back as
  not-a-bug — `asks/aether-program-bare-setters-bug.md`.
- **`${...}` interpolation eats bash parameter expansions.**
  Aether's string interpolation grabs `${...}` at parse time, so
  embedding a literal `${1%%|*}` in a string literal silently
  fails to make it to the file. Workaround: build the string via
  `string.concat` with `$` and `{` separated. See
  `bash_runner_body()` in `lib/bash/module.ae` for the pattern.
- **`path.join` is naive.** `path.join("/foo", "/abs/x.c")` returns
  `/foo//abs/x.c`, not `/abs/x.c`. Aether's `path.join` doesn't
  honour the python-shaped "absolute-right-wins" rule. To
  conditionally resolve relative-vs-absolute, gate on a leading-`/`
  check first. See `extra_source`'s consumption loop in
  `lib/aether/module.ae`.
- **`file.exists` vs `dir.exists` vs `fs.exists`.** `file.exists`
  returns 0 for directories. `dir.exists` returns 0 for files.
  `fs.exists` is path-agnostic. Most aeb code uses `dir.exists`
  for directory probes, `fs.exists` for file/dir agnostic checks.
  Got bitten once during this session; documented in README.
- **`os.system` returns the POSIX exit code directly** (not the
  wait-status word). Don't shift right by 8.
- **`continue` is not a keyword in Aether.** Use a `needs_X = 1`
  flag and gate the trailing block. See `_run_regen_pass` in
  `lib/aether/module.ae`.
- **Multi-return is one-call-side destructure only.** `_, b =
  list.get(l, i)` works; declaring a function `-> (string, int)`
  doesn't ergonomically chain. Use a map or split-accessor pattern
  for >1 return value.

## SDK extension shape

Adding a language SDK or a new builder follows a fixed pattern.
Look at `lib/bash/module.ae` for the simplest complete example.

1. **Setters** at the top — pure data accumulation into the builder
   map. One arg per call. `_ctx: ptr` is the magic first parameter.
2. **Pure command-string builders** in the middle — `*_cmd(opts) ->
   string`. These are what `tests/test_*_cmd.ae` exercise. Keep
   them I/O-free.
3. **Builder functions** at the bottom — `builder verb(ctx: ptr)`.
   Read the builder map, call the pure builders, `os.system` the
   result, write artifact metadata to `target/<module>/`.
4. **Register the module name** in `tools/aeb-init.ae`'s
   `shipped_modules()` list. `aeb --init` reads that list to
   create the `.aeb/lib/<name>` symlinks in consumer repos.

The string-builder pattern (the `*_cmd(...)` helpers) is load-bearing
for testability. Every command that gets `os.system`'d should go
through one. Inline assembly in builder bodies isn't testable in
isolation.

## Out-of-repo SDKs (consumer-local libs)

Not every SDK belongs in `lib/<name>/` here. A consumer repo that
wants domain-specific build/test grammar can ship its own SDK
in-tree at `.aeb/lib/<name>/module.ae`, tracked in that repo:

```
.gitignore:
  /.aeb/lib/*
  !/.aeb/lib/<name>/
```

`aeb --init` writes the shipped-SDK symlinks but explicitly skips
non-symlink paths it finds at `.aeb/lib/<x>/`, so a tracked
consumer SDK coexists fine.

Typical shape: the consumer SDK's setters wrap aeb's generic
primitives (especially `pre_command(...)` / `post_command(...)`
from `lib/bash`) with domain-specific lifecycle, then call into
the consumer's own bash helpers. The bash glue lives in the
consumer repo (e.g. `tests/lib.sh`), invoked via
`pre_command("source tests/lib.sh && my_fixture_helper ...")`.

Example in the wild: **svn-aether** (~/scm/subversion/subversion)
ships `.aeb/lib/svnae/module.ae` with setters like
`svn_server(NAME, PORT)` and `empty_repo_with_algos(NAME, ALGOS)`
that wrap its Subversion-server fixture lifecycle. The .tests.ae
files there read as canonical aeb DSL; the bash plumbing
(server spawn, repo seed, kill/wait) lives in `tests/lib.sh`
under the surface.

This is the pressure-relief valve for the "don't accept
domain-specific into core" rule. If a downstream's domain isn't
generic, the SDK lives in its tree, not ours. aeb's core stays
small; consumer ergonomics still ratchet up.

## Design principles when extending aeb

A handful of recurring decisions shape what gets accepted into the
core SDKs versus pushed back to user-side helper scripts. None of
these are absolute, but skipping them tends to produce regrets.

1. **Generic vs domain-specific.** aeb takes generic build/test
   orchestration. Domain-specific lifecycle (e.g. an application's
   daemon spawning, a particular database's seed format) belongs
   in the consumer's own helper scripts, sourced via `pre_command`
   or invoked from a builder. The shape "spawn a binary, set env,
   run script, kill" is generic; "spawn *aether-svnserver* with
   `--superuser-token` and export `${PRIMARY_PORT}`" is not.
2. **Closure-DSL grammar over external config.** Where the first
   instinct is "let aeb read someones-config.toml", the better
   answer is usually native setters in `.build.ae`. aeb stays out
   of TOML/YAML/JSON parsing; external formats are honoured only
   via shell-outs to tools that already parse them (e.g. `ae build`
   for `aether.toml`). See `regen(...)` / `regen_with(...)`.
3. **Single-arg-per-call setters.** Aether is fixed-arity. Sketches
   like `fn("name", port: 9540, timeout: 1500)` don't compile.
   Land them as `fn("name"); fn_port("name", 9540); fn_timeout(...)`
   or similar split-setter shapes.
4. **Pure command-string builders.** Every command that gets
   `os.system`'d should be assembled by a pure helper that takes
   inputs and returns a string, with the actual exec done by the
   builder body. Tested in isolation in `tests/test_*_cmd.ae`.
   Inline assembly in builder bodies is not testable.
5. **Two-layer verification.** String-builder tests in `tests/`
   cover the exec-string regression surface; `/tmp/aeb-*-smoke`
   end-to-end runs verify the integration. Both should pass
   before a change ships. Smokes are not committed; they're
   session-local scaffolding.
6. **Capture the trade-off in commit messages.** "Going with X,
   not Y, because Z" reads better six months later than a diff
   alone. Past commit bodies are the design archive; skim them
   when a similar question comes up.

## What to verify before saying "done"

- `make install` ran, runtime tree at `~/.local/share/aeb/` matches
  `lib/` and `tools/` in the dev tree (`diff -q`).
- `./tests/run.sh` is green. Currently 33/33; new SDK additions
  should grow this.
- One `/tmp/aeb-*-smoke` end-to-end run succeeded.
- README updated if the user-visible surface changed.
- Commit message captures the trade-off, not just the diff.
- `asks/<name>.md` (if any) committed alongside the implementation
  so the trail is in-tree.

## What NOT to do

- Don't add aeb-side parsers for external config formats (TOML,
  YAML, JSON manifest) when a setter would do. The `aether.toml`
  shell-out is the one exception and it delegates to `ae build`,
  which already parses it.
- Don't add domain-specific builders to generic SDKs. If three
  downstreams want the same shape, factor; until then, push back.
- Don't refactor the three-copy `file_to_label` duplication
  speculatively. The right consolidation needs `tools/` reachable
  via `--lib`, which is a harness change. Tracked in TODO.md §
  Test coverage gaps; revisit when there's a concrete reason.
- Don't break the visited-set dedup contract. Two `.build*.ae` in
  the same dir get distinct labels via the `:tag` suffix; strip
  the tag before deriving filesystem paths but keep it in the
  human-display label.
- Don't `--no-verify` or skip hooks. There aren't pre-commit hooks
  in this repo today, but the principle stands.

## Known upstream Aether issues affecting aeb

- **macOS link step fails with duplicate symbols.** Compiler emits
  imported module functions into every TU without `static`. GNU ld
  hides this via `-Wl,--allow-multiple-definition` (currently
  hard-coded in `tools/aeb-link.ae`); macOS ld64 rejects the flag.
  Tracked in TODO.md § Aether compiler issues.
- **`tools/gcheckout`** doesn't currently link — externs against
  stdlib symbols don't resolve. Excluded from `INSTALL_TOOLS`.
  Same upstream class of issue. Lazy-builds at first use of
  `aeb gcheckout ...`; same failure surfaces there.
- Most other upstream gaps are documented inline in TODO.md.

## Recent upstream Aether features aeb could lean on

Tracked here so future sessions know what's available without
re-reading the upstream changelog. Not "must consume," just "this
exists if a need arises."

- **0.115 `#line` directives in generated C** — gcc errors now
  point at `.ae` source lines rather than post-merge C. The
  manual-path gcc invocation in `lib/aether/_compile_and_link`
  benefits automatically; nothing for aeb to do beyond knowing
  user-facing error quality has improved.
- **0.116 `@aether` per-param extern annotation** — for externs
  whose receiver is Aether-emitted (e.g. another `--emit=lib`
  module), the annotation preserves the AetherString header so
  `string.length` doesn't strlen-truncate at embedded NULs. aeb
  has zero such externs today (all our externs cross into naive
  C runtime, where the v0.98.0 auto-unwrap is correct); flagged
  in case future SDK additions cross Aether-to-Aether boundaries.
- **0.111 `make install-contrib`** — installs prebuilt static
  libs for sqlite + host bridges at `$(PREFIX)/lib/aether/`.
  aeb users can `link_flag("-laether_sqlite")` instead of pointing
  `extra_source` at the C file. aeb's existing `-L` includes that
  dir; no change needed.
- **0.111 `string.glob_match`** — POSIX fnmatch surface in
  stdlib. Useful if aeb grows pattern-based source discovery
  (e.g. a glob form of `extra_source(...)`); not consumed today.
- **0.111 `std.config` + `std.actors`** — process-global KV +
  actor registry. Relevant when aeb grows multi-thread or
  multi-process telemetry (Tier 1+ in TODO § telemetry vision).
- **0.115 `ae build --coverage`** — gcc instrumentation injected
  into user-program build, `.gcda` at runtime, `.ae.gcov`
  reports. **Now consumed**: `aeb --coverage` is a CLI flag (set
  by trampoline as env var `AEB_COVERAGE=1`); `lib/aether` honors
  it on both shell-out and manual paths. Cache key segregates
  coverage from non-coverage builds. Other test SDKs (jacoco,
  jest --coverage, etc.) not yet wired — tracked in TODO.

## The load-bearing principle

**The dot-prefixed `.ae` file is the single source of truth for
what aeb does for its target.** External config files
(`aether.toml`, `pom.xml`, `Cargo.toml`, etc.) are honoured via
shell-outs to tools that already parse them, but aeb itself doesn't
parse them. When in doubt, prefer adding a setter that the user
calls inside the `.ae` file over adding a parser to aeb. This is
what keeps the build-graph extraction text-only and the
configuration typeable / IDE-friendly / introspectable from `grep`.

Setters can come from aeb's `lib/` (the generic SDKs we ship) or
from a consumer's own `.aeb/lib/<name>/module.ae` (domain-specific
SDKs that wrap our primitives). Both feed the same `.ae`-as-truth
contract; the boundary between them is generic-vs-domain, not
core-vs-extension.
