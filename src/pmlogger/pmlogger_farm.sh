#! /bin/sh
#
# Copyright (c) 2021 Red Hat.
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
# Administrative script to start the pmlogger_farm service.
# All arguments to pmlogger_check are passed from pmlogger_farm.service.
#
. $PCP_DIR/etc/pcp.env

# Start the pmlogger farm and then wait for a signal.
# Any errors are reported by pmlogger_check in it's log.
$PCP_BINADM_DIR/pmlogger_check $* 2>/dev/null
sts=$?
[ $sts -eq 0 ] || exit $sts
exec $PCP_BINADM_DIR/pmpause
