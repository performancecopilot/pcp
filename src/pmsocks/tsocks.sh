#!/bin/sh
#
# Copyright (c) 1995-1999,2008 Silicon Graphics, Inc.  All Rights Reserved.
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
# You should have received a copy of the GNU General Public License along
# with this program; if not, write to the Free Software Foundation, Inc.,
# 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
#
# Dynamically sockify the argument program using tsocks
# from http://www.progsoc.uts.edu.au/~delius/
#

prog=`basename $0`

if [ $# -eq 0 -o "X$1" = "X-?" ]
then
    echo "Usage: $prog [path]program [args ...]"
    exit 1

fi

if [ ! -f /etc/tsocks.conf -o ! -f /usr/lib/libtsocks.so ]
then
    echo "$prog: Error \"tsocks\" doesn't seem to be installed."
    echo "*** Get it from http://www.progsoc.uts.edu.au/~delius/"
    exit 1
fi

target=`which "$1" 2>/dev/null | grep -v "^alias "`
if [ -z "$target" -o ! -x "$target" ]
then
    echo "$prog: Error: \"$1\": Command not found."
    exit 1
fi

shift
args=""
for arg
do
    args="$args \"$1\""
    shift
done

LD_PRELOAD=/usr/lib/libtsocks.so
export LD_PRELOAD
eval exec $target $args
