#!/usr/bin/env bash
# tests/run.sh — local test runner for aeb unit tests
#
# Discovers every test_*.ae in this directory, builds it with `ae`, runs
# the resulting binary, and reports build/run status per test. Designed
# to work identically on macOS (BSD userland, bash 3.2+) and Linux (GNU).
#
# Usage:
#   ./tests/run.sh                    # run all tests
#   ./tests/run.sh test_cargo         # run tests matching pattern
#   AETHER=/path/to/ae ./tests/run.sh # override ae binary
#
# Exit codes:
#   0  all tests passed
#   1  one or more tests failed to build or run

set -u

# Locate the script directory portably (no readlink -f which BSD lacks)
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
LIB_DIR="$REPO_ROOT/lib"

AETHER="${AETHER:-ae}"
PATTERN="${1:-}"

if ! command -v "$AETHER" >/dev/null 2>&1; then
    echo "error: '$AETHER' not found in PATH. Set AETHER=/path/to/ae or install it." >&2
    exit 1
fi

if [ ! -d "$LIB_DIR" ]; then
    echo "error: expected lib/ at $LIB_DIR" >&2
    exit 1
fi

TMPDIR_RUN="$(mktemp -d 2>/dev/null || mktemp -d -t 'aeb_tests')"
trap 'rm -rf "$TMPDIR_RUN"' EXIT INT TERM

# Counters
total=0
passed=0
build_failed=0
run_failed=0

# Track failures for final summary
failed_list=""

# Fixed-width name column for readable output
NAME_WIDTH=24

# ANSI colors (only if stdout is a tty)
if [ -t 1 ]; then
    C_GREEN="$(printf '\033[32m')"
    C_RED="$(printf '\033[31m')"
    C_YELLOW="$(printf '\033[33m')"
    C_DIM="$(printf '\033[2m')"
    C_RESET="$(printf '\033[0m')"
else
    C_GREEN=""
    C_RED=""
    C_YELLOW=""
    C_DIM=""
    C_RESET=""
fi

pad_name() {
    name="$1"
    printf '%-*s' "$NAME_WIDTH" "$name"
}

# Discover tests — portable find, no -printf, no -maxdepth quirks
test_files="$(find "$SCRIPT_DIR" -maxdepth 1 -type f -name 'test_*.ae' | sort)"

if [ -z "$test_files" ]; then
    echo "no test_*.ae files found in $SCRIPT_DIR"
    exit 0
fi

echo
echo "aeb test suite"
echo "lib: $LIB_DIR"
echo "ae:  $(command -v "$AETHER")"
echo

# Iterate over tests (read line-by-line instead of `while read` pipeline
# so variable updates persist — works on bash 3.2)
set -f
IFS='
'
for test_file in $test_files; do
    base="$(basename "$test_file" .ae)"

    # Pattern filter
    if [ -n "$PATTERN" ]; then
        case "$base" in
            *"$PATTERN"*) ;;
            *) continue ;;
        esac
    fi

    total=$((total + 1))
    bin="$TMPDIR_RUN/$base"
    build_log="$TMPDIR_RUN/$base.build.log"
    run_log="$TMPDIR_RUN/$base.run.log"

    printf '  %s ' "$(pad_name "$base")"

    # Build
    if ! AETHER_HOME="" "$AETHER" build "$test_file" -o "$bin" --lib "$LIB_DIR" \
            >"$build_log" 2>&1; then
        build_failed=$((build_failed + 1))
        # Extract first meaningful error line for the summary
        first_err="$(grep -E '^error\[|^error:' "$build_log" 2>/dev/null | head -1 | sed -E 's/^error[^:]*: *//')"
        if [ -z "$first_err" ]; then
            first_err="(see $build_log)"
        fi
        printf '%sBUILD FAIL%s  %s%s%s\n' \
            "$C_RED" "$C_RESET" "$C_DIM" "$first_err" "$C_RESET"
        failed_list="${failed_list}${base} (build)
"
        continue
    fi

    # Run
    if ! "$bin" >"$run_log" 2>&1; then
        run_failed=$((run_failed + 1))
        printf '%sRUN FAIL%s    %s(non-zero exit)%s\n' \
            "$C_RED" "$C_RESET" "$C_DIM" "$C_RESET"
        failed_list="${failed_list}${base} (run)
"
        continue
    fi

    # Check for "FAIL:" markers in test output (assert_eq prints these)
    fail_count="$(grep -c '^FAIL:' "$run_log" 2>/dev/null || true)"
    fail_count="${fail_count:-0}"
    pass_count="$(grep -c '^PASS:' "$run_log" 2>/dev/null || true)"
    pass_count="${pass_count:-0}"

    if [ "$fail_count" -gt 0 ]; then
        run_failed=$((run_failed + 1))
        printf '%sRUN FAIL%s    %s(%d assertion(s) failed)%s\n' \
            "$C_RED" "$C_RESET" "$C_DIM" "$fail_count" "$C_RESET"
        failed_list="${failed_list}${base} (assertions)
"
        continue
    fi

    passed=$((passed + 1))
    printf '%sRUN PASS%s    %s(%d assertion(s))%s\n' \
        "$C_GREEN" "$C_RESET" "$C_DIM" "$pass_count" "$C_RESET"
done
unset IFS
set +f

echo
if [ "$total" -eq 0 ]; then
    echo "no tests matched pattern: $PATTERN"
    exit 0
fi

if [ "$build_failed" -eq 0 ] && [ "$run_failed" -eq 0 ]; then
    printf '%s%d passed%s of %d tests\n' "$C_GREEN" "$passed" "$C_RESET" "$total"
    exit 0
fi

printf '%d passed, %s%d build failed%s, %s%d run failed%s of %d tests\n' \
    "$passed" \
    "$C_YELLOW" "$build_failed" "$C_RESET" \
    "$C_RED" "$run_failed" "$C_RESET" \
    "$total"

if [ -n "$failed_list" ]; then
    echo
    echo "failures:"
    printf '%s' "$failed_list" | while IFS= read -r line; do
        [ -n "$line" ] && printf '  - %s\n' "$line"
    done
fi

exit 1
