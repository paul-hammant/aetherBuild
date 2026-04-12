#!/bin/bash
# cargo-member-to-ae — converts a member crate's Cargo.toml to .build.ae format
#
# Usage: cargo-member-to-ae.sh crates/lib/Cargo.toml > crates/lib/.build.ae
#
# Extracts: crate name, workspace refs, path deps, features
set -eo pipefail

CARGO_TOML="${1:?Usage: cargo-member-to-ae.sh <member/Cargo.toml>}"

if [[ ! -f "$CARGO_TOML" ]]; then
    echo "error: $CARGO_TOML not found" >&2
    exit 1
fi

# Extract crate name
crate_name=$(grep -m1 '^name' "$CARGO_TOML" | sed 's/.*= *"//; s/".*//')

echo "import build"
echo "import rust"
echo "import rust (crate_name, edition, ws_dep, path_dep)"
echo ""
echo "main() {"
echo "    b = build.start()"
echo "    build.dep(b, \".build.ae\")"
echo "    rust.cargo_member(b) {"
echo "        crate_name(\"$crate_name\")"
echo "        edition(\"2024\")"

# Extract workspace deps
in_deps=0
in_section=""
while IFS= read -r line; do
    # Track sections
    if [[ "$line" =~ ^\[([^\]]+)\] ]]; then
        section="${BASH_REMATCH[1]}"
        if [[ "$section" == "dependencies" || "$section" == "dependencies."* ]]; then
            in_deps=1
            in_section="$section"
        else
            in_deps=0
            in_section=""
        fi
        continue
    fi

    [[ $in_deps -eq 0 ]] && continue
    [[ -z "$line" || "$line" == \#* ]] && continue

    # workspace = true deps
    if [[ "$line" =~ ^([a-zA-Z0-9_-]+)[[:space:]]*=.*workspace[[:space:]]*=[[:space:]]*true ]]; then
        dep_name="${BASH_REMATCH[1]}"
        echo "        ws_dep(\"$dep_name\")"
        continue
    fi

    # path deps
    if [[ "$line" =~ ^([a-zA-Z0-9_-]+)[[:space:]]*=.*path[[:space:]]*=[[:space:]]*\"([^\"]+)\" ]]; then
        dep_name="${BASH_REMATCH[1]}"
        dep_path="${BASH_REMATCH[2]}"
        echo "        path_dep(\"$dep_name\", \"$dep_path\")"
        continue
    fi

done < "$CARGO_TOML"

echo "    }"
echo "}"
