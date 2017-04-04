#!/bin/sh
#
# Recreate foo+ archive
# ... this is foo + dups in the PMNS + event records
#

tmp=/var/tmp/$$
trap "rm -f $tmp.*; exit 0" 0 1 2 3 15

cat <<End-of-File >$tmp.config
log mandatory on once {
    sample.long
    sample.longlong
}

log mandatory on 1 sec {
    sample.seconds
    sample.bin
    sample.colour
    sample.drift
    sample.lights
    sampledso.event.records
    sampledso.event.highres_records
}
End-of-File

pmstore sampledso.event.reset 1
pmstore sampledso.event.reset_highres 1

rm -f foo+.index foo+.meta foo+.0
pmlogger -c $tmp.config -s 15 foo+
