# Go single-module to aetherBuild Migration Status

Upstream: https://github.com/fyne-io/fyne.git

## Modules

| Module | Compile | Tests | Notes |
|--------|---------|-------|-------|
| app | OK | PASS | Compile-checks entire repo via `go.check(b)` |
| canvas | — | PASS | 12 test files |
| container | — | PASS | 22 test files |
| data | — | PASS | 15 test files |
| dialog | — | PASS | 19 test files |
| lang | — | PASS | 2 test files |
| layout | — | PASS | 10 test files |
| storage | — | PASS | 6 test files |
| theme | — | PASS | 10 test files |
| tools | — | PASS | 1 test file |
| widget | — | PASS | 65 test files |

1 compile target (full repo), 11/11 test suites pass.

## What aeb replaces

- `go test ./...` (monolithic test run) → per-package `go.go_test(b) { pkg("./widget/...") }`
- No build tool to replace — Go's toolchain is self-contained

## What aeb adds

- **Per-package test isolation**: each top-level package gets its own `.tests.ae`
  with targeted `pkg()` pattern. Failures in widget tests don't abort canvas tests.
- **Selective testing**: `aeb widget/` runs only widget tests + its compile dep.
- **Compile-check target**: `go.check(b)` runs `go build ./...` without producing
  a binary — validates compilation across all 65 packages.

## New Go SDK features

- `go.check(b)` builder — compile-check without binary output
- `pkg("./pattern/...")` DSL setter — target specific package subtrees
  for both `go.check` and `go.go_test`

## Notes

Fyne is a single-module Go repo (one `go.mod`, 65 packages). Unlike the
multi-module Java/Clojure/dotnet itests, there are no inter-module deps to
declare — Go handles intra-module imports automatically. aeb's value here
is build orchestration: per-package test isolation, selective targeting,
and integration into the same `aeb` command that builds Java, TypeScript,
Scala, Clojure, and .NET projects.
