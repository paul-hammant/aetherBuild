#!/bin/sh
# resolve-imports.sh — BFS through SDK module imports
#
# Usage: resolve-imports.sh <source.ae> <lib_dir>
# Output: one `import X` line per transitive dep that <source.ae> does
#         NOT already import directly (so the caller can prepend them
#         without duplicating).
#
# POSIX shell only — no `declare -A`, no `grep -oP`, no process
# substitution. Runs under macOS's bash 3.2, BusyBox, and dash.

set -eu

SRC="$1"
LIB="$2"

# Extract top-level `import X` identifiers from an .ae file.
# Matches `import foo`, `import foo.bar`, `import foo (sel)`, etc.
# Dotted forms (e.g. `import std.string`) are captured intact so that
# stdlib sub-modules survive the round-trip and are re-emitted to the
# consumer file. Portable sed — works on BSD (macOS) and GNU alike.
extract_imports() {
    sed -n 's/^import \([A-Za-z_][A-Za-z0-9_.]*\).*$/\1/p' "$1"
}

tmp_seen=$(mktemp)
tmp_prev=$(mktemp)
direct=$(mktemp)
trap 'rm -f "$tmp_seen" "$tmp_prev" "$direct"' EXIT INT TERM

# Seed with the source file's direct imports. Remember this set so we
# don't re-emit them at the end.
extract_imports "$SRC" | sort -u > "$tmp_seen"
cp "$tmp_seen" "$direct"

# Fixed-point iteration: expand each seen module's own imports, sort
# the combined set, stop when nothing new was added.
while ! cmp -s "$tmp_seen" "$tmp_prev" 2>/dev/null; do
    cp "$tmp_seen" "$tmp_prev"
    while IFS= read -r mod; do
        [ -z "$mod" ] && continue
        mf="$LIB/$mod/module.ae"
        [ -f "$mf" ] || continue
        extract_imports "$mf" >> "$tmp_seen"
    done < "$tmp_prev"
    sort -u "$tmp_seen" -o "$tmp_seen"
done

# Emit every seen module that wasn't in the direct set.
comm -23 "$tmp_seen" "$direct" | while IFS= read -r m; do
    [ -n "$m" ] && echo "import $m"
done
