#!/usr/bin/env bash
# Asks every live WarbandLib copy to unload (plugins first, then the runtime).
set -euo pipefail
BIN_DIR="$(realpath "${1:-build/build-release-clangcl-x86/bin}")"
for dll in "$BIN_DIR"/live/*/warbandlib_runtime.dll; do
    [[ -f "$dll" ]] && touch "$dll.unload"
done
