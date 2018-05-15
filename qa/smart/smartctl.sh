#!/bin/sh
# SMART drive PMDA test helper - invoke as, e.g.:
# $ cd 001/smart
# $ ../../smartctl.sh -Hi /dev/sda
# $ ../../smartctl.sh -A /dev/sda

option="$1"
device=`echo "$2" | sed -e 's,/dev/,,g'`

if [ "$option" = "-Hi" ]
then
    cat $device.info
elif [ "$option" = "-A" ]
then
    cat $device.data
else
    echo "Unknown option $option - try -[Hi|A] /dev/xxx"
    exit 1
fi
