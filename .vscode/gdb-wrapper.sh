#!/usr/bin/env bash

# Clear all snap-injected vars before running gdb so gdb and its children
# do not inherit snap's library paths or module directories.
unset DEBUGINFOD_URLS
unset LD_LIBRARY_PATH LD_PRELOAD
unset GIO_MODULE_DIR GDK_PIXBUF_MODULE_FILE GDK_PIXBUF_MODULEDIR
unset GTK_PATH GTK_EXE_PREFIX GTK_IM_MODULE_FILE
unset GSETTINGS_SCHEMA_DIR LOCPATH SNAP_LIBRARY_PATH

exec /usr/bin/gdb \
    --nx \
    -iex "set debuginfod enabled off" \
    -iex "set auto-solib-add off" \
    -iex "set print thread-events off" \
    "$@"
