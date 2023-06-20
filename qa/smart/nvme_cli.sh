#!/bin/sh
# SMART drive PMDA test helper - invoke as, e.g.:
# $ cd 001/smart
# $ ../../nvme_cli.sh

# With the nvme cli output we want the arg5 "get-feature -f 0x02 -H /dev/XXX"
device=`echo "$5" | sed -e 's,/dev/,,g'`

cat $device.nvme
