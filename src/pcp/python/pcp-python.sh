#! /bin/sh
# 
# Copyright (c) 2015 Red Hat.
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
# Use the python interpreter identified by PCP_PYTHON_PROG, which so far
# could be set to any of: "python", "python2", "python26", "python3" ...
# depending on the local system installation.
#

. $PCP_DIR/etc/pcp.env
if [ -z "$PCP_PYTHON_PROG" ]
then
   echo "No python interpreter configured in $PCP_DIR/etc/pcp.env (PCP_PYTHON_PROG)" 1>&2
   exit 1
fi
exec $PCP_PYTHON_PROG $@
