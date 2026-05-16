# aether SDK — authoring notes

Surfaced by `ae help <script>.build.ae --lib .aeb/lib` when a name
below appears. See docs/cic-help.md for the mechanism.

## `extra_source` / `link_flag` / `regen` flip the build path

`aether.program(b)` shells out to `ae build` by default — the simple,
recommended path. Declaring ANY of `extra_source(...)`, `link_flag(...)`
or `regen(...)` opts the target into the manual `aetherc + gcc` path
instead, which compiles each file itself and links explicitly. That is
intentional, but surprising if you added one `extra_source` line not
realising it changes how the whole target builds.

Pattern: literal-name `extra_source`

## Bare setters need the second import line

Setters inside an `aether.program(b) { ... }` block (`source`,
`output`, `extra_source`, `link_flag`, `regen`) resolve as plain
top-level calls, not against the `aether` namespace. The block needs
BOTH `import aether` and a selective `import aether (source, output)`.
A missing second import surfaces as `undefined function 'source'`.

Pattern: literal-name `source`

## `regen` runs before the build, gated on mtime

`regen(...)` declares an `.ae` → generated-`.c` step that runs before
the program build, skipped when the output is newer than the input.
It does not run unconditionally and is not a post-build hook.

Pattern: literal-name `regen`
