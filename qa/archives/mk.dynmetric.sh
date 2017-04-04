#!/bin/sh
#
# Create dynmetric archive
#

rm -f dynmetric.*

tmp=/var/tmp/$$
trap "rm -f $tmp.*; exit 0" 0 1 2 3 15

cat <<End-of-File >$tmp.config
log mandatory on default {
    sampledso.drift
    sampledso.secret
    sampledso.bin
    sampledso.long
}
End-of-File

pmlogger -s 10 -t 1 -c $tmp.config dynmetric
