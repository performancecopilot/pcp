#!/bin/sh
#
# Copyright (c) 2009 Aconex.  All Rights Reserved.
# 
# This program is free software; you can redistribute it and/or modify it
# under the terms of the GNU General Public License as published by the
# Free Software Foundation; either version 2 of the License, or (at your
# option) any later version.
# 
# This program is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
# or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
# for more details.
#

. $PCP_DIR/etc/pcp.conf

if echo "`id -un`" | grep '^root' >/dev/null
then
    :
else
    echo "$0: Error: must be run as user \"root\" not \"`id -un`\""
    exit 1
fi

# redirect stderr ...
#
if [ -f "$PCP_LOG_DIR/pmcd/zimbraprobe.log" ]
then
    mv "$PCP_LOG_DIR/pmcd/zimbraprobe.log" "$PCP_LOG_DIR/pmcd/zimbraprobe.log.prev"
    exec 2>"$PCP_LOG_DIR/pmcd/zimbraprobe.log"
fi

echo "zimbraprobe starting ... `date`" >&2

delay=${1:-10}

id >&2
echo "PPID=$PPID" >&2

# First one produces any errors to PMDA logfile
#
$PCP_PMDAS_DIR/zimbra/runaszimbra "zmcontrol status"

while true
do
    date +'%d/%m/%Y %H:%M:%S'
    sleep $delay
    $PCP_PMDAS_DIR/zimbra/runaszimbra "zmcontrol status"
    if kill -s 0 $PPID
    then
	# parent alive, keep going ...
	#
	:
    else
	# time for me to die also
	#
	exit
    fi
done
