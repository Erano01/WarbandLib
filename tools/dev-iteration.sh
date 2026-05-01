#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${WARBANDLIB_BUILD_DIR:-$ROOT_DIR/build}"
BUILD_TYPE="${WARBANDLIB_BUILD_TYPE:-RelWithDebInfo}"
MOD_SOURCE_DIR="${WARBANDLIB_MOD_SOURCE_DIR:-$ROOT_DIR/examples}"
MOD_TARGET_DIR="${WARBANDLIB_MOD_TARGET_DIR:-}"
LAUNCH_CMD="${WARBANDLIB_GAME_LAUNCH_CMD:-}"

usage() {
  cat <<'EOF'
Usage:
  tools/dev-iteration.sh configure
  tools/dev-iteration.sh build
  tools/dev-iteration.sh sync [--dry-run]
  tools/dev-iteration.sh launch
  tools/dev-iteration.sh loop [--dry-run]

Environment variables:
  WARBANDLIB_BUILD_DIR         Build directory (default: ./build)
  WARBANDLIB_BUILD_TYPE        CMake build type (default: RelWithDebInfo)
  WARBANDLIB_MOD_SOURCE_DIR    Folder to sync into your module folder (default: ./examples)
  WARBANDLIB_MOD_TARGET_DIR    Target module folder inside your game installation
  WARBANDLIB_GAME_LAUNCH_CMD   Full command to launch the game for test runs
EOF
}

configure_project() {
  cmake -S "$ROOT_DIR" -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE="$BUILD_TYPE"
}

build_project() {
  cmake --build "$BUILD_DIR" -j
}

sync_mod() {
  local dry_run="${1:-}"

  if [[ -z "$MOD_TARGET_DIR" ]]; then
    echo "WARBANDLIB_MOD_TARGET_DIR is not set."
    echo "Set it to your local Modules/<YourModule> path, then run sync again."
    exit 1
  fi

  if [[ ! -d "$MOD_SOURCE_DIR" ]]; then
    echo "Source folder not found: $MOD_SOURCE_DIR"
    exit 1
  fi

  mkdir -p "$MOD_TARGET_DIR"

  local rsync_args=(-av --delete)
  if [[ "$dry_run" == "--dry-run" ]]; then
    rsync_args+=(--dry-run)
  fi

  rsync "${rsync_args[@]}" "$MOD_SOURCE_DIR/" "$MOD_TARGET_DIR/"
}

launch_game() {
  if [[ -z "$LAUNCH_CMD" ]]; then
    echo "WARBANDLIB_GAME_LAUNCH_CMD is not set."
    echo "Example: export WARBANDLIB_GAME_LAUNCH_CMD='steam -applaunch 48700'"
    exit 1
  fi

  bash -lc "$LAUNCH_CMD"
}

main() {
  local cmd="${1:-}"
  shift || true

  case "$cmd" in
    configure)
      configure_project
      ;;
    build)
      build_project
      ;;
    sync)
      sync_mod "${1:-}"
      ;;
    launch)
      launch_game
      ;;
    loop)
      configure_project
      build_project
      sync_mod "${1:-}"
      ;;
    *)
      usage
      exit 1
      ;;
  esac
}

main "$@"
