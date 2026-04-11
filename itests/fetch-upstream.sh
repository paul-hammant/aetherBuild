#!/usr/bin/env bash
#
# Fetch upstream sources needed by the integration tests.
# Run once from the itests/ directory before running aeb.
#
set -euo pipefail
cd "$(dirname "$0")"

TMPDIR=$(mktemp -d)
trap 'rm -rf "$TMPDIR"' EXIT

fetch_repo() {
    local repo_url="$1"
    local dest="$2"

    echo "Fetching $repo_url ..."
    git clone --depth 1 "$repo_url" "$TMPDIR/clone"
    # Copy everything except .git into the destination
    rsync -a --exclude='.git' "$TMPDIR/clone/" "$dest/"
    rm -rf "$TMPDIR/clone"
    echo "  -> $dest updated"
}

fetch_repo "https://github.com/nrwl/nx-examples.git" "nx-examples"
fetch_repo "https://github.com/spring-projects/spring-data-examples.git" "spring-data-examples"

echo "Done. Upstream sources restored."
