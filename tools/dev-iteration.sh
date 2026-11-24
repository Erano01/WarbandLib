#!/usr/bin/env bash
# Linux dev loop: configure/build with the clang-cl preset, run unit tests, inject.
set -euo pipefail
cd "$(dirname "${BASH_SOURCE[0]}")/.."
PRESET="${WARBANDLIB_PRESET:-build-release-clangcl-x86}"
case "${1:-}" in
    configure) cmake --preset "$PRESET" ;;
    build) cmake --build --preset "$PRESET" ;;
    test) cmake --preset linux-host-tests >/dev/null && cmake --build --preset linux-host-tests && ctest --preset linux-host-tests ;;
    inject) tools/hot-reload.sh "build/$PRESET/bin" ;;
    unload) tools/unload.sh "build/$PRESET/bin" ;;
    loop) cmake --preset "$PRESET" >/dev/null && cmake --build --preset "$PRESET" && tools/hot-reload.sh "build/$PRESET/bin" ;;
    docs) (cd docs && npm install && npm run dev) ;;
    *) echo "usage: $0 configure|build|test|inject|unload|loop|docs" >&2; exit 1 ;;
esac
