#!/bin/sh
#
# atop-daily - daily maintenance for pcp-atop PCP archives
#
# Usage:
#   atop-daily [--compress-only] [LOGPATH]
#
#   --compress-only  Compress prior-day archives only; skip culling.
#                    Used when called from pcp-atop itself at startup.
#   LOGPATH          Archive directory (overrides $LOGPATH env var).
#
# Without --compress-only (normal daily run via timer or ExecStartPre):
#   - Creates LOGPATH if absent
#   - Compresses prior-day archives (rename to .tmp, then xz/.zst in bg)
#   - Culls archives older than LOGGENERATIONS days
#
# Compression is non-blocking: files are atomically renamed then compressed
# by background children so the caller (and pcp-atop) can proceed immediately.
# The rename is safe whether pcp-atop is running or stopped:
#   - Running: open file descriptors keep writing to the original inode
#   - Stopped: no contention; rename is instantaneous
#
# Copyright (c) 2026 Red Hat.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#

. ${PCP_DIR}/etc/pcp.env

compress_only=false

_usage()
{
    echo >&2 "Usage: atop-daily [options] [LOGPATH]"
    echo >&2
    echo >&2 "Options:"
    echo >&2 "  --compress-only  compress prior-day archives only, skip culling"
    echo >&2 "  --help           show this usage message"
    echo >&2
    echo >&2 "LOGPATH overrides the default archive directory (/var/log/atop)."
    exit 1
}

for arg in "$@"
do
    case "$arg" in
    --compress-only)
        compress_only=true
        ;;
    --help|-\?)
        _usage
        ;;
    -*)
        echo "$0: unknown option: $arg" >&2
        _usage
        ;;
    *)
        LOGPATH="$arg"
        ;;
    esac
done

: ${LOGPATH:=/var/log/atop}
: ${LOGGENERATIONS:=28}

$compress_only || mkdir -p "$LOGPATH" || exit 1

today=$(date +%Y%m%d)

#
# Atomically rename prior-day archive files to .tmp staging names, then
# fork background compression for each.  The script exits immediately
# after forking so recording (or service startup) is not delayed.
#
for metafile in "$LOGPATH"/*-[0-9][0-9][0-9][0-9][0-9][0-9][0-9][0-9].meta
do
    [ -f "$metafile" ] || continue
    base="${metafile%.meta}"
    day="${base##*-}"
    [ "$day" = "$today" ] && continue

    mv -f "$metafile" "$metafile.tmp" 2>/dev/null || continue

    for vol in "$base".[0-9]*
    do
        [ -f "$vol" ] && mv -f "$vol" "$vol.tmp" 2>/dev/null
    done
done

for f in "$LOGPATH"/*.meta.tmp
do
    [ -f "$f" ] || continue
    out="${f%.tmp}.xz"
    (xz -q -c "$f" > "$out" && rm -f "$f") &
done

for f in "$LOGPATH"/*.[0-9]*.tmp
do
    [ -f "$f" ] || continue
    out="${f%.tmp}.zst"
    (zstd -q -o "$out" "$f" && rm -f "$f") &
done

# Wait for background compression children before exiting so systemd does
# not kill them when ExecStartPre completes.  pcp-atop also compresses via
# its own internal atop-daily --compress-only fork, so this is belt-and-
# suspenders but ensures completeness in both call paths.
wait

$compress_only && exit 0

#
# Cull archives (all suffixes) by the date embedded in the filename.
# The YYYYMMDD stamp is reliable; mtime is not (compression updates it).
# LOGGENERATIONS=0 means keep forever; skip culling.
#
if [ "${LOGGENERATIONS:-0}" -gt 0 ]
then
    cutdate=$(date -d "-${LOGGENERATIONS} days" +%Y%m%d 2>/dev/null || \
              date -v"-${LOGGENERATIONS}d" +%Y%m%d 2>/dev/null)
    if [ -n "$cutdate" ]
    then
        for f in "$LOGPATH"/*-[0-9][0-9][0-9][0-9][0-9][0-9][0-9][0-9].*
        do
            [ -f "$f" ] || continue
            base="${f##*/}"
            day="${base#*-}"; day="${day%%.*}"
            [ "$day" -lt "$cutdate" ] 2>/dev/null && rm -f "$f"
        done
    fi
fi

exit 0
