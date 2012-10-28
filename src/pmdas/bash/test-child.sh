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

wired()
{
	# burn a little CPU, then sleep
	for i in 0 1 2 3 4 5 6 7 8 9 0
	do
		/bin/true && /bin/true
	done
	sleep $1
}

count=0
while true
do
	(( count++ ))
	echo "get busy, $count"	# top level
	wired 2		# call a shell function
	[ $count -ge 10 ] && break
done

pcp_trace off
exit 0
