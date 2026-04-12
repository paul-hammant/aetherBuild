#!/bin/bash
# cargo-to-deps-ae — converts [workspace.dependencies] from Cargo.toml to .deps.ae format
#
# Usage: cargo-to-deps-ae.sh Cargo.toml > workspace.deps.ae
#
# Output: Aether DSL lines:
#   workspace_dep("name", "version")
#   workspace_dep_features("name", "version", "feat1,feat2")
set -eo pipefail

CARGO_TOML="${1:?Usage: cargo-to-deps-ae.sh Cargo.toml}"

if [[ ! -f "$CARGO_TOML" ]]; then
    echo "error: $CARGO_TOML not found" >&2
    exit 1
fi

echo "// Generated from $CARGO_TOML"
echo "// workspace dependencies for load_workspace_deps()"
echo ""

# Extract the [workspace.dependencies] section
in_ws_deps=0
while IFS= read -r line; do
    # Detect section boundaries
    if [[ "$line" =~ ^\[workspace\.dependencies\] ]]; then
        in_ws_deps=1
        continue
    fi
    if [[ "$line" =~ ^\[ ]] && [[ $in_ws_deps -eq 1 ]]; then
        break
    fi
    [[ $in_ws_deps -eq 0 ]] && continue

    # Skip comments and blank lines
    [[ -z "$line" || "$line" == \#* ]] && continue

    # Parse: name = "version"
    if [[ "$line" =~ ^([a-zA-Z0-9_-]+)[[:space:]]*=[[:space:]]*\"([^\"]+)\" ]]; then
        name="${BASH_REMATCH[1]}"
        ver="${BASH_REMATCH[2]}"
        echo "workspace_dep(\"$name\", \"$ver\")"
        continue
    fi

    # Parse: name = { ... } (table syntax with various fields)
    if [[ "$line" =~ ^([a-zA-Z0-9_-]+)[[:space:]]*=[[:space:]]*\{ ]]; then
        name="${BASH_REMATCH[1]}"

        ver=""
        if [[ "$line" =~ version[[:space:]]*=[[:space:]]*\"([^\"]+)\" ]]; then
            ver="${BASH_REMATCH[1]}"
        fi

        feats=""
        if [[ "$line" =~ features[[:space:]]*=[[:space:]]*\[([^\]]+)\] ]]; then
            raw_feats="${BASH_REMATCH[1]}"
            feats=$(echo "$raw_feats" | sed 's/"//g; s/ //g')
        fi

        pkg=""
        if [[ "$line" =~ package[[:space:]]*=[[:space:]]*\"([^\"]+)\" ]]; then
            pkg="${BASH_REMATCH[1]}"
            # If the package name matches the dep name, it's redundant
            if [[ "$pkg" == "$name" ]]; then
                pkg=""
            fi
        fi

        nodef="false"
        if [[ "$line" =~ default-features[[:space:]]*=[[:space:]]*false ]]; then
            nodef="true"
        fi

        # Decide which setter to use
        if [[ -n "$pkg" || "$nodef" == "true" ]]; then
            # Full form needed
            echo "workspace_dep_full(\"$name\", \"$ver\", \"$feats\", \"$pkg\", \"$nodef\")"
        elif [[ -n "$ver" && -n "$feats" ]]; then
            echo "workspace_dep_features(\"$name\", \"$ver\", \"$feats\")"
        elif [[ -n "$ver" ]]; then
            echo "workspace_dep(\"$name\", \"$ver\")"
        fi
        continue
    fi

done < "$CARGO_TOML"
