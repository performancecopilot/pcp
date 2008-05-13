#!/bin/sh
#
# Copyright (C) 1995-1999 Silicon Graphics, Inc.
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
