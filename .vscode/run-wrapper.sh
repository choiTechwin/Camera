#!/usr/bin/env bash
# Runs the app in a clean environment, stripping VS Code snap library paths
# that conflict with the system libpthread.
REAL_HOME="${SNAP_REAL_HOME:-$HOME}"
exec env -i \
    HOME="$REAL_HOME" \
    PATH=/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin \
    DISPLAY="${DISPLAY:-}" \
    XAUTHORITY="${XAUTHORITY:-$REAL_HOME/.Xauthority}" \
    WAYLAND_DISPLAY="${WAYLAND_DISPLAY:-}" \
    XDG_RUNTIME_DIR="${XDG_RUNTIME_DIR:-}" \
    "$@"
