#!/bin/sh
#
# Copyright (c) 2024 Ken McDonell.  All Rights Reserved.
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
# Intended as "run after reboot" to make sure transient things that PCP
# needs have been setup ...
#

tmp=/var/tmp/pcp-reboot-init-$$
status=1	# fail is the default
trap "rm -f $tmp; exit \$status" 0 1 2 3 15

if [ ! -f "$PCP_DIR/etc/pcp.env" ]
then
    echo "$0: Error: \$PCP_DIR/etc/pcp.env missing ... arrgh"
    exit
fi
. "$PCP_DIR/etc/pcp.env"

if [ `id -u` -ne 0 ]
then
    echo "$0: Error: You must be root (uid 0) to run script"
    exit
fi

if [ -z "$PCP_RUN_DIR" ]
then
    echo "$0: Botch: no PCP_RUN_DIR= in $PCP_DIR/etc/pcp.conf"
    exit
fi

if [ -z "$PCP_USER" ]
then
    echo "$0: Botch: no PCP_USER= in $PCP_DIR/etc/pcp.conf"
    exit
fi

if [ -z "$PCP_GROUP" ]
then
    echo "$0: Botch: no PCP_GROUP= in $PCP_DIR/etc/pcp.conf"
    exit
fi

# Need $PCP_RUN_DIR to exist and be owned by $PCP_USER:$PCP_GROUP
#
if [ ! -d "$PCP_RUN_DIR" ]
then
    if mkdir "$PCP_RUN_DIR"
    then
	:
    else
	echo "$0: Error: mkdir for \$PCP_RUN_DIR ($PCP_RUN_DIR) failed"
	ls -ld "$PCP_RUN_DIR/.." "$PCP_RUN_DIR"
	exit
    fi
fi

# stat(1) line looks like
# /var/run/pcp 220 0 41fd 998 998 19 2373 2 0 0 1705345899 1705375503 1705375503 0 4096
# last 3 digits mode  ^^^ ^^^ ^^^ group
# (in hex)                user
#
if stat -t "$PCP_RUN_DIR" >$tmp
then
    :
else
    echo "$0: Error: stat \$PCP_RUN_DIR ($PCP_RUN_DIR) failed"
    ls -ld "$PCP_RUN_DIR/.." "$PCP_RUN_DIR"
fi
mode=`awk '{print $4}' <$tmp | sed -e 's/.*\(...\)$/\1/'`
user=$(id -n -u `awk '{print $5}' <$tmp`)
group=$(id -n -g `awk '{print $6}' <$tmp`)

if [ "$user" != $PCP_USER -o "$group" != $PCP_GROUP ]
then
    if chown $PCP_USER:$PCP_GROUP "$PCP_RUN_DIR"
    then
	:
    else
	echo "$0: Error: chown for \$PCP_RUN_DIR ($PCP_RUN_DIR) failed"
	ls -ld "$PCP_RUN_DIR"
	exit
    fi
fi

# mode 0x1fd == 0775 == rwxrwxr-x
#
if [ "$mode" != "1fd" ]
then
    if chmod 775 "$PCP_RUN_DIR"
    then
	:
    else
	echo "$0: Error: chmod for \$PCP_RUN_DIR ($PCP_RUN_DIR) failed"
	ls -ld "$PCP_RUN_DIR"
	exit
    fi
fi

status=0
exit
