#! /bin/sh
# 
# Copyright (c) 2016 Red Hat.
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
# Displays stats from the Performance Co-Pilot kernel domain agent.
#

. $PCP_DIR/etc/pcp.env

sts=2
progname=`basename $0`
tmp=`mktemp -d /tmp/pcp.XXXXXXXXX` || exit 1
trap "rm -rf $tmp; exit \$sts" 0 1 2 3 15

_usage()
{
    [ ! -z "$@" ] && echo $@ 1>&2
    pmgetopt --progname=$progname --usage --config=$tmp/usage
    exit 1
}

cat > $tmp/usage << EOF
# getopts: ?
# Usage: [delta [samples]]
# end
EOF
ARGS=`pmgetopt --progname=$progname --config=$tmp/usage -- "$@"`
[ $? != 0 ] && exit 1

eval set -- "$ARGS"
while [ $# -gt 0 ]
do
    case "$1" in
      -\?)
        sts=1
        _usage ""
        ;;
      --)       # end of options, start of arguments
        shift
        [ $# -gt 2 ] && _usage "Too many arguments"
        [ $# -gt 1 ] && export PCP_SAMPLES="$2"
        [ $# -gt 0 ] && export PCP_INTERVAL="$1"
        break
        ;;
    esac
    shift       # finished with this option now, move onto next
done

rm -rf $tmp     # cleanup now, no trap handler post-exec
exec $PCP_BIN_DIR/pmstat -x
