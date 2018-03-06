#!/bin/sh
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
. $PCP_DIR/etc/pcp.env

PCP_KUBECTL_PROG=${PCP_KUBECTL_PROG-'kubectl'}
which $PCP_KUBECTL_PROG >/dev/null 2>/dev/null || exit 0
[ -e "$PCP_SYSCONF_DIR/discover/pcp-kube-pods.disabled" ] && exit 0

args="--show-all -o jsonpath={.items[*].status.podIP}"
file="$PCP_SYSCONF_DIR/discover/pcp-kube-pods.conf" 
[ -f "$file" ] && args=`cat "$file"`
file="$PCP_SYSCONF_DIR/pmmgr/target-kubectl-pod" # back-compat
[ -f "$file" ] && args="$args `cat "$file"`"

exec $PCP_KUBECTL_PROG get pod $args
