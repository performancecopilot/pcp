#!/bin/sh
# 
# Copyright (c) 2023 Ken McDonell.  All Rights Reserved.
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
# Redact sensitive information from a PCP archive.
#

. $PCP_DIR/etc/pcp.env

status=1
tmp=`mktemp -d "$PCP_TMPFILE_DIR/pmlogredact.XXXXXXXXX"` || exit 1
trap "rm -rf $tmp; exit \$status" 0
prog=`basename $0`

cat > $tmp/usage << EOF
# getopts: c:D:vx?
# Usage: [options] inarch outarch

Options:
  -c, --config=FILE_or_DIR additional configuration file or directory
  -v, --verbose            increase diagnostic verbosity
  -x, --exclude-std        do not use the standard configuration files
  --help
EOF

_usage()
{
    pmgetopt --progname=$prog --config=$tmp/usage --usage
    exit 1
}

vflag=''
debug=''
std_config='-c ${PCP_SYSCONF_DIR}/pmlogredact'

ARGS=`pmgetopt --progname=$prog --config=$tmp/usage -- "$@"`
[ $? != 0 ] && exit 1

eval set -- "$ARGS"
while [ $# -gt 0 ]
do
    case "$1"
    in
	-c)	# additional config file(s)
	    config="$config -c '$2'"
	    shift
	    ;;
	-D)	# debug
	    debug="$debug -D '$2'"
	    shift
	    ;;
	-x)	# exclude std configs
	    std_config=''
	    ;;
	-v)	# verbose
	    vflag=-v
	    ;;
	--)
	    shift
	    break
	    ;;
	-\?)
	    _usage
	    # NOTREACHED
	    ;;
    esac
    shift
done

if [ $# -ne 2 ]
then
    _usage
    # NOTREACHED
fi

eval pmlogrewrite $debug $vflag $std_config $config "$1" "$2"
