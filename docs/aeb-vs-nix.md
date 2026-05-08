# aeb vs. Nix

aeb and Nix both describe build inputs and outputs, but they optimize
for different boundaries.

Nix is a whole-system build and package model. A Nix derivation can
describe the compiler, libc, runtime, transitive dependencies, build
environment, and output path in a content-addressed store. Its main
value is hermeticity and reproducibility across machines.

aeb is a repo-local build graph for polyglot source trees. A target is
a dot-prefixed `.ae` file next to the code it builds. The value is that
the source tree declares the module graph directly: compile targets,
test targets, dist targets, cross-language deps, affected-target
detection, and per-module telemetry all share the same graph.

## Where they overlap

An aeb `.build.ae` file already contains much of the declarative
information a Nix derivation would need:

- source roots
- dependency edges
- compiler/test/package intent
- named outputs
- target metadata

So an `aeb-to-nix` exporter is mechanically plausible. It could walk
the aeb graph and emit derivations or flakes for downstream consumers.

## Where they differ

aeb does not currently try to be Nix:

- It uses tools from `PATH` rather than pinning whole toolchains.
- It caches per-target artifacts, not complete system closures.
- It runs inside an already-cloned repo rather than materializing all
  inputs from a store.
- It is organized around source-tree modules, not package derivations.

Nix's value comes from owning the whole closure. aeb's value comes from
making the repo's build graph small, explicit, and language-aware.

## Rule of thumb

Do not rebuild aeb around the Nix store.

If Nix integration is useful, add an exporter:

```text
aeb graph -> aeb-to-nix -> derivations / flake outputs
```

That keeps aeb's native model intact while letting Nix users consume
the graph in their own ecosystem.
