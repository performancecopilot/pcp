#!/bin/sh
# FARM log drive PMDA test helper - invoke as, e.g.:
# $ cd 001/farm
# $ ../../smartctl.sh -l farm /dev/sda


option="$1"
device=`echo "$3" | sed -e 's,/dev/,,g'`

if [ "$option" = "-l" ]
then
    cat $device.farm
else
    echo "Unknown option $option - try -l farm /dev/xxx"
    exit 1
fi
