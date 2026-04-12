#!/bin/bash
# resolve-imports.sh — BFS through SDK module imports
# Usage: resolve-imports.sh <source.ae> <lib_dir>
# Output: import lines to prepend (one per line), or empty
set -eo pipefail
SRC="$1"
LIB="$2"

declare -A seen full
queue=()

while IFS= read -r m; do
    [[ -n "$m" ]] && seen[$m]=1 && queue+=("$m")
done < <(grep -oP '^import \K\w+' "$SRC" | sort -u)

while IFS= read -r m; do
    [[ -n "$m" ]] && full[$m]=1
done < <(grep -oP '^import \K\w+$' "$SRC")

i=0
while (( i < ${#queue[@]} )); do
    cur=${queue[$i]}; i=$((i+1))
    mf="$LIB/$cur/module.ae"
    [[ -f "$mf" ]] || continue
    while IFS= read -r d; do
        [[ -n "$d" ]] || continue
        if [[ -z "${seen[$d]+x}" ]]; then
            seen[$d]=1
            queue+=("$d")
        fi
    done < <(grep -oP '^import \K\w+' "$mf" | sort -u)
done

for m in "${!seen[@]}"; do
    [[ -z "${full[$m]+x}" ]] && echo "import $m"
done
