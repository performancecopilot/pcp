#!/usr/bin/env bash
#
# Copyright (c) 2012 Nathan Scott.  All Rights Reserved.
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

# --- start common preamble
if [ -z "$PCP_SH_DONE" ]
then
    if [ -n "$PCP_CONF" ]
    then
	__CONF="$PCP_CONF"
    elif [ -n "$PCP_DIR" ]
    then
	__CONF="$PCP_DIR/etc/pcp.conf"
    else
	__CONF=/etc/pcp.conf
    fi
    if [ ! -f "$__CONF" ]
    then
	echo "pcp.env: Fatal Error: \"$__CONF\" not found" >&2
	exit 1
    fi
    eval `sed -e 's/"//g' $__CONF \
    | awk -F= '
/^PCP_/ && NF == 2 {
	exports=exports" "$1
	printf "%s=${%s:-\"%s\"}\n", $1, $1, $2
} END {
	print "export", exports
}'`
    export PCP_ENV_DONE=y
fi
. $PCP_SHARE_DIR/lib/bashproc.sh
# --- end common preamble

pcp_trace on $0 $@

tired()
{
	sleep $1
}

count=0
while true
do
	(( count++ ))
	echo "awoke, $count"	# top level
	tired 2		# call a shell function
	branch=$(( count % 3 ))
	case $branch
	in
		0)	./test-child.sh $count &
			;;
		2)	wait
			;;
	esac
done

pcp_trace off
exit 0
