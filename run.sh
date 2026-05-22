#!/usr/bin/env bash
# Use this script instead of running the binary directly from VS Code's terminal.
# VS Code runs as a snap and injects library paths that conflict with the system
# libpthread. This wrapper strips those paths before launching.
DIR="$(cd "$(dirname "$0")" && pwd)"
BUILD="${1:-Release}"
exec "$DIR/.vscode/run-wrapper.sh" "$DIR/build/$BUILD/CameraApp"
