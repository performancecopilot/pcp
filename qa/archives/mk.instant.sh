#!/bin/sh
#
# Create instant-* archives
#

rm -f instant-*.meta instant-*.0 instant-*.index

tmp=/var/tmp/$$
trap "rm -f $tmp.*; exit 0" 0 1 2 3 15

cat <<End-of-File >$tmp.config
log mandatory on default {
    sample.seconds
    sample.milliseconds
    sample.double.bin_ctr
    disk.dev.total
    disk.dev.read
    disk.dev.write
}
End-of-File

pmlogger -s 10 -t 1 -c $tmp.config instant-base

cat <<End-of-File >$tmp.config
metric 29.*.* { sem -> INSTANT }
metric disk.dev.total { sem -> INSTANT }
End-of-File

pmlogrewrite -c $tmp.config instant-base instant-1

