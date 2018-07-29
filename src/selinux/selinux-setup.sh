#!/bin/sh
#
# selinux-setup - install or remove PCP selinux policy files
# Usage: selinux-setup <path> [install|remove] <policy>
#
# Copyright (c) 2018 Red Hat.
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

trap "exit 0" 0 1 2 3 15
prog=`basename $0`

if [ $# -lt 3 ]
then
    echo "$prog: invalid arguments -- $@" 1>&2
    exit
fi

# check all prerequisites and always exit cleanly for packagers
test -x /usr/sbin/selinuxenabled || exit
test -x /usr/sbin/semodule || exit
/usr/sbin/selinuxenabled || exit

selinuxdir="$1"
command="$2"
policy="$3"

case "$command"
in
    install)
	test -f "$selinuxdir/$policy.pp" || exit
	if semodule -h | grep -q -- "-X" >/dev/null 2>&1
	then
	    semodule -X 400 -i "$selinuxdir/$policy.pp"
	else
	    semodule -i "$selinuxdir/$policy.pp"
	fi #semodule -X flag check
	;;

    remove)
	semodule -l | grep "$policy" >/dev/null 2>&1 || exit
	if semodule -h | grep -q -- "-X" >/dev/null 2>&1
	then
	    semodule -X 400 -r "$policy" >/dev/null
	else
	    semodule -r "$policy" >/dev/null
	fi #semodule -X flag check
	;;
esac

exit
