#!/usr/bin/env bash
# Unloads every live WarbandLib copy in the running game and injects a fresh one.
# Each copy gets its own folder (runtime + plugins), because re-injecting a path
# that wine still has mapped returns the stale module without running DllMain.
# Usage: tools/hot-reload.sh [bin dir]   (default: build/build-release-clangcl-x86/bin)
set -euo pipefail
BIN_DIR="$(realpath "${1:-build/build-release-clangcl-x86/bin}")"
LIVE="$BIN_DIR/live"
APPID="${WARBAND_STEAM_APPID:-48700}"

command -v protonhax >/dev/null || { echo "protonhax not found (AUR: protonhax); start Warband with 'protonhax init %COMMAND%'" >&2; exit 1; }
PHD="${XDG_RUNTIME_DIR:-/run/user/$UID}/protonhax/$APPID"
[[ -f "$PHD/exe" ]] || { echo "Warband (appid $APPID) is not running under protonhax" >&2; exit 1; }
PROTON_DIR="$(dirname "$(cat "$PHD/exe")")"
PFX="$(cat "$PHD/pfx")"
WINE=(protonhax exec "$APPID" env WINEPREFIX="$PFX" "$PROTON_DIR/files/bin/wine")

PID="$(grep -l 'mb_warband.exe' /proc/[0-9]*/maps 2>/dev/null | head -1 | cut -d/ -f3 || true)"
mkdir -p "$LIVE"
for dir in $(ls -1rd "$LIVE"/*/ 2>/dev/null); do
    dll="${dir%/}/warbandlib_runtime.dll"
    if [[ -n "$PID" ]] && grep -qF "${dir%/}/warbandlib_runtime.dll" "/proc/$PID/maps" 2>/dev/null; then
        echo "[reload] unloading $(basename "$dir")"
        touch "$dll.unload"
        for _ in $(seq 1 80); do grep -qF "${dir%/}/warbandlib_runtime.dll" "/proc/$PID/maps" || break; sleep 0.25; done
    fi
done

COPY="$LIVE/$(date +%H%M%S)"
mkdir -p "$COPY/plugins"
cp "$BIN_DIR/warbandlib_runtime.dll" "$COPY/"
[[ -f "$BIN_DIR/mb_warband.ini" ]] && cp "$BIN_DIR/mb_warband.ini" "$COPY/"
cp "$BIN_DIR"/plugins/*.dll "$COPY/plugins/" 2>/dev/null || true
WIN_PATH="Z:${COPY//\//\\}\\warbandlib_runtime.dll"
"${WINE[@]}" "$BIN_DIR/injector.exe" mb_warband.exe "$WIN_PATH" 2>&1 | grep -v fixme || true
echo "[reload] log: $COPY/warbandlib_runtime.dll.log"
