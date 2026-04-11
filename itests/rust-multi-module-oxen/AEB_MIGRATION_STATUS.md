# Cargo workspace to aetherBuild Migration Status

Upstream: https://github.com/Oxen-AI/Oxen.git

## Modules

| Module | Compile | Tests | Notes |
|--------|---------|-------|-------|
| crates/lib (liboxen) | FAIL | — | RocksDB native build fails (C++ tooling) |
| crates/cli (oxen-cli) | FAIL | — | Depends on liboxen |
| crates/server (oxen-server) | FAIL | — | Depends on liboxen |
| crates/oxen-py | — | — | Skipped (needs PyO3 + Python) |

0/3 compile. Failure is RocksDB C++ native compilation, not aeb.

## What aeb replaces

- `Cargo.toml` `[workspace]` members → aeb topo-sort via `build.dep()`
- `cargo build` / `cargo test` (whole workspace) → per-crate `rust.check(b)` / `rust.test(b)`

## What aeb uses (middle ground)

- `rust.check(b) { crate_name("liboxen") }` — calls `cargo check -p liboxen`
- `rust.test(b) { crate_name("liboxen") }` — calls `cargo test -p liboxen`
- `rust.build_crate(b) { crate_name("oxen-cli") }` — calls `cargo build -p oxen-cli --release`
- Cargo still handles dep resolution, crate compilation, proc macros, build scripts
- aeb adds: workspace member ordering, selective targeting, per-crate test isolation

## Nirvana (future — TODO)

Replace Cargo entirely:
1. Resolve deps via `Cargo.lock` parsing + download from crates.io
2. Invoke `rustc` directly with `--extern` flags
3. Handle proc macros, build scripts, feature resolution in aeb
4. Generate `Cargo.toml` from `.build.ae` (like dotnet `.csproj` generation)

## New Rust SDK features

- `rust.check(b)` builder — `cargo check -p <name>` without binary output
- `rust.test(b)` builder — `cargo test -p <name>`
- `rust.build_crate(b)` builder — `cargo build -p <name> --release`
- `crate_name()` DSL setter — specify workspace member name

## Known issues

1. RocksDB (`librocksdb-sys`) fails to compile — needs specific C++ toolchain
   setup. This is a Cargo/environment issue, not aeb-specific.
2. `oxen-py` skipped — PyO3 build needs Python headers and maturin.
