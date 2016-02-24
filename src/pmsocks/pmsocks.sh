#!/bin/sh
#
# Copyright (c) 2016 Red Hat.
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
# Dynamically sockify the argument program using the tsocks library
# from http://tsocks.sourceforge.net/
#

PROG=`basename $0`
if [ $# -eq 0 -o "X$1" = "X-?" ]
then
    echo "Usage: $PROG [path]program [args ...]" 1>&2
    exit 1
fi

TSOCKS=`which tsocks`
if [ -z "$TSOCKS" ]
then
   echo "No tsocks wrapper script found - install the 'tsocks' package" 1>&2
   exit 1
fi

exec "$TSOCKS" "$@"
