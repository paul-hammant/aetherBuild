# Pants Python monorepo to aeb Migration Status

Upstream: https://github.com/SystemCraftsman/pants-python-monorepo-demo.git

## Modules

| Module | Install | Tests | Notes |
|--------|---------|-------|-------|
| chatapp/profanitymasker | OK | 1/2 pass | Upstream test bug (partial word matching) |
| chatapp/contentbuilder | OK | 2/2 pass | Depends on profanitymasker |

2 modules, dep installation works, 3/4 tests pass (1 upstream bug).

## What aeb replaces

- Pants BUILD files → `.build.ae` / `.tests.ae`
- `pants test ::` → per-module `python.pytest(b)`
- `pants package` → not yet implemented

## What aeb uses

- `python.install(b)` — creates venv in `.aeb/venv/`, installs pip deps
- `python.pytest(b)` — runs `pytest` from the venv
- DSL: `pip("package")`, `requirements_file("path")`
- Venv is shared across all modules in the project

## Notes

- Python 3.13 removed `pkgutil.ImpImporter` — upstream code uses deprecated
  `pkg_resources` which requires `setuptools>=70,<81` for 3.13 compatibility.
- The upstream `requirements.txt` pins `setuptools>=56,<57` (too old for 3.13) —
  the `.build.ae` overrides this with a compatible version range.
