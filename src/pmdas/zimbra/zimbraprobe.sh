#!/bin/sh
#
# Copyright (c) 2009 Aconex.  All Rights Reserved.
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

delay=${1:-10}

# First one produces any errors to PMDA logfile
su -c 'zmcontrol status' - zimbra

while true
do
    date +'%d/%m/%Y %H:%M:%S'
    sleep $delay
    su -c 'zmcontrol status' - zimbra 2>/dev/null
done
