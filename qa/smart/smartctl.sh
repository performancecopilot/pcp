#!/bin/sh
# SMART drive PMDA test helper - invoke as, e.g.:
# $ cd 001/smart
# $ ../../smartctl.sh -Hi /dev/sda
# $ ../../smartctl.sh -A /dev/sda
# $ ../../smartctl.sh -c /dev/sda

option="$1"
device=`echo "$2" | sed -e 's,/dev/,,g'`

if [ "$option" = "-Hi" ]
then
    cat $device.info
elif [ "$option" = "-A" ]
then
    cat $device.data
elif [ "$option" = "-c" ]
then
    test -f $device.power && cat $device.power
else
    echo "Unknown option $option - try -[Hi|A|c] /dev/xxx"
    exit 1
fi
