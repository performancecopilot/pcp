#!/bin/sh
#
# Control program for managing pmlogger instances.
#
# Copyright (c) 2020 Ken McDonell.  All Rights Reserved.
# Copyright (c) 2021,2025 Red Hat.
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
. "$PCP_DIR/etc/pcp.env"

# we are not an "rc" script, so dodge any special handling in
# rc-proc.sh
PCPQA_NO_RC_STATUS=true; export PCPQA_NO_RC_STATUS
. "$PCP_SHARE_DIR/lib/rc-proc.sh"

. "$PCP_SHARE_DIR/lib/utilproc.sh"

prog=pmlogctl; export prog
IAM=pmlogger; export IAM
CONTROLFILE=$PCP_PMLOGGERCONTROL_PATH; export CONTROLFILE
. "$PCP_SHARE_DIR/lib/ctl-tools.sh"

exit
