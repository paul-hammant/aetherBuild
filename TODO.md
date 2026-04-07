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

### Target filtering

`ae-build <target>` builds only the named target and its transitive deps.
Currently builds everything.

```bash
ae-build java/applications/monorepos_rule          # just this + deps
ae-build --test javatests/components/vowelbase      # just this test + deps
ae-build --dist java/applications/monorepos_rule    # compile + dist
```

### Parallel execution

Independent modules in the DAG can build concurrently. The visited map
needs thread-safe access (mutex or atomic). Aether actors are a natural
fit — one actor per module, message-passing for completion.

### Affected-target detection

Given `git diff`, compute which modules' sources changed, trace the
reverse dependency graph, rebuild only affected targets. Massive CI
speedup for large monorepos.

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

## Import path for sibling projects

Currently the monorepo needs a symlink `lib/build/ → aetherBuild/lib/build/`.
Aether's import resolver should support a project-root marker or config
that adds search paths, so sibling projects can import each other without
symlinks.
