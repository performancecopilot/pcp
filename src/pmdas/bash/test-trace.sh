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

. $PCP_DIR/etc/pcp.sh
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
