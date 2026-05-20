#!/usr/bin/env bash

unset DEBUGINFOD_URLS
export DEBUGINFOD_URLS=

exec /usr/bin/gdb \
    --nx \
    -iex "set debuginfod enabled off" \
    -iex "set auto-solib-add off" \
    -iex "set print thread-events off" \
    "$@"
