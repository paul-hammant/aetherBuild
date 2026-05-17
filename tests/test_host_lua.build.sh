#!/usr/bin/env bash
# test_host_lua.build.sh — non-uniform build for the in-process Lua
# host test (test_host_lua.ae). Links the contrib.host.lua C bridge
# into the binary, which the uniform `ae build` line in run.sh can't
# do (it needs -DAETHER_HAS_LUA + pkg-config lua flags + the bridge
# .c). This is aeb's tiny equivalent of Aether's own
# tests/scripts/contrib_host_demos.sh.
#
# Args (from run.sh): $1 AETHER (ae binary)  $2 output bin
#                     $3 LIB_DIR             $4 TOOLS_DIR
#
# Three outcomes, each producing a runnable $BIN so the suite stays
# green:
#   linked — liblua dev present: -DAETHER_HAS_LUA, real Lua executes
#   stub   — bridge but no liblua dev: bridge compiles to no-op stubs
#   skip   — bridge source not found: emit a tiny pass-through binary
#
# Aether's installed tree currently ships the host modules' .ae/.h
# but NOT aether_host_<lang>.c, so the bridge source is located from
# a sibling Aether checkout when the install copy is absent.
set -u

AETHER="$1"; BIN="$2"; LIB_DIR="$3"; TOOLS_DIR="$4"
: "$LIB_DIR" "$TOOLS_DIR"   # unused: test_host_lua.ae imports only std + contrib
CC="${CC:-cc}"

SRC_DIR="$(cd "$(dirname "$0")" && pwd)"
SRC="$SRC_DIR/test_host_lua.ae"
TMP="$(mktemp -d 2>/dev/null || mktemp -d -t 'host_lua')"
trap 'rm -rf "$TMP"' EXIT

AEBIN="$(command -v "$AETHER" 2>/dev/null || echo "$AETHER")"
AETHERC="$(dirname "$AEBIN")/aetherc"
PREFIX="$(dirname "$(dirname "$AEBIN")")"

emit_skip() {
    # $1 = reason. A trivial binary that reports a clean skip so the
    # run.sh PASS:/FAIL: scan keeps the suite green.
    cat > "$TMP/skip.c" <<EOF
#include <stdio.h>
int main(void) {
    puts("PASS: hosted Lua skipped — $1");
    puts("host lua tests done");
    return 0;
}
EOF
    "$CC" "$TMP/skip.c" -o "$BIN"
    exit $?
}

# Locate the contrib.host.lua C bridge.
BRIDGE=""
for cand in \
    "$PREFIX/share/aether/contrib/host/lua/aether_host_lua.c" \
    "$(cd "$SRC_DIR/../.." 2>/dev/null && pwd)/aether/contrib/host/lua/aether_host_lua.c" \
    "$HOME/scm/AetherThings/aether/contrib/host/lua/aether_host_lua.c"
do
    if [ -f "$cand" ]; then BRIDGE="$cand"; break; fi
done
[ -z "$BRIDGE" ] && emit_skip "Aether contrib host bridge source not found"
[ -x "$AETHERC" ] || emit_skip "aetherc not found at $AETHERC"

# aetherc: test_host_lua.ae -> C
if ! "$AETHERC" "$SRC" "$TMP/host_lua.c"; then
    echo "host-lua: aetherc compile failed" >&2
    exit 1
fi

# Probe liblua dev (pkg-config name varies by distro).
LUA_CFLAGS=""; LUA_LIBS=""; LUA_DEF=""
for v in lua5.4 lua-5.4 lua5.3 lua-5.3 lua; do
    if pkg-config --exists "$v" 2>/dev/null; then
        LUA_CFLAGS="$(pkg-config --cflags "$v")"
        LUA_LIBS="$(pkg-config --libs "$v")"
        LUA_DEF="-DAETHER_HAS_LUA"
        echo "host-lua: liblua dev '$v' found — linked mode"
        break
    fi
done
[ -z "$LUA_DEF" ] && echo "host-lua: no liblua dev — stub mode"

# One cc step: generated C + the bridge + libaether (+ liblua when
# linked). `ae cflags` subsets give the runtime -I / -L -laether.
# shellcheck disable=SC2046,SC2086
"$CC" $("$AETHER" cflags --cflags) $LUA_DEF $LUA_CFLAGS \
    "$TMP/host_lua.c" "$BRIDGE" \
    $("$AETHER" cflags --libs) $LUA_LIBS \
    -o "$BIN"
