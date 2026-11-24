#!/bin/sh
# 
# Copyright (c) 2014,2016 Red Hat.
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
# Cross-platform signal/event sender for Performance Co-Pilot utilities.
# Supports a minimal set of signals, used by PCP tools on all platforms.
#

. $PCP_DIR/etc/pcp.env

status=1
tmp=`mktemp -d "$PCP_TMPFILE_DIR/pmsignal.XXXXXXXXX"` || exit 1
trap "rm -rf $tmp; exit \$status" 0 1 2 3 15
prog=`basename $0`
sigs="HUP USR1 USR2 TERM KILL"

cat > $tmp/usage << EOF
# Usage: [options] PID ... | name ...

Options:
  -a,--all          send signal to all named processes (killall mode)
  -l,--list         list available signals
  -n,--dry-run      list processes that would be affected
  -p,--program      extract process identifiers from $PCP_RUN_DIR programs
  -s=N,--signal=N   signal to send ($sigs)"
  --help
EOF

usage()
{
    [ ! -z "$@" ] && echo $@ 1>&2
    pmgetopt --progname=$prog --config=$tmp/usage --usage
    exit 1
}

check()
{
    for sig in $sigs
    do
	[ $sig = "$1" ] && echo $sig && return
    done
    usage "$prog: invalid signal - $1"
}

# optional auditing if $auditfile already exists and is writeable
#
auditfile=/tmp/pmsignal.audit
if [ -w $auditfile ]
then
    echo "[$$] `date +%Y-%m-%d\ %H:%M:%S.%N` args: $*" >>$auditfile
    $PCP_PS_PROG --format pid,ppid,user,group,args -p $$ >$tmp/ps 2>&1
    sed -e "s/^/[$$] /" <$tmp/ps >>$auditfile
    # and roll your own pstree -s with extra process info
    #
    while true
    do
	ppid=`$PCP_AWK_PROG <$tmp/ps '$1 ~ /^[0-9][0-9]*$/ { print $2 }'`
	[ -z "$ppid" ] && break
	$PCP_PS_PROG --format pid,ppid,user,group,args -p $ppid >$tmp/ps 2>&1
	sed -e '/^  *PID/d' -e "s/^/[$$] /" <$tmp/ps >>$auditfile
	[ "$ppid" = 1 ] && break
    done
fi

signal=TERM
pmsignal=""
aflag=false
lflag=false
nflag=false
pflag=false

ARGS=`pmgetopt --progname=$prog --config=$tmp/usage -- "$@"`
[ $? != 0 ] && exit 1

eval set -- "$ARGS"
while [ $# -gt 0 ]
do
    case "$1"
    in
	-a)	aflag=true ;;
	-l)	lflag=true ;;
	-n)	nflag=true ;;
	-p)	pflag=true ;;
	-s)	signal=`check "$2"`
		shift
		;;
	--)	shift
		break
		;;
	-\?)	usage ""
		;;
    esac
    shift
done

$lflag && echo "$sigs" && exit 0

[ $aflag = true -a $pflag = true ] && \
    usage "$prog: Cannot specify both \"all\" and \"program\" modes"
[ $# -lt 1 ] && usage "$prog: Insufficient arguments"


if $aflag
then
    pids=''
    pfx=''
    for name in "$@"; do
	program=`basename "$name"`
	pidlist=`_get_pids_by_name -a "$program"`
	pids="$pids$pfx$pidlist"
	pfx=' '
    done
    if [ -w $auditfile ]
    then
	if [ -n "$pids" ]
	then
	    echo "[$$] -a $@ expands to target PIDs $pids ..." >>$auditfile
	    $PCP_PS_PROG --format pid,ppid,user,group,args -p "$pids" 2>&1 \
	    | sed -e "s/^/[$$] /" >>$auditfile
	else
	    echo "[$$] -a expands to nothing" >>$auditfile
	fi
    fi
elif $pflag
then
    pids=''
    pfx=''
    for name in "$@"; do
	program=`basename "$name"`
	pidlist=`cat "$PCP_RUN_DIR/${program}.pid" 2>/dev/null`
	pids="$pids$pfx$pidlist"
	pfx=' '
    done
    if [ -w $auditfile ]
    then
	if [ -n "$pids" ]
	then
	    echo "[$$] -p $@ expands to target PIDs $pids ..." >>$auditfile
	    $PCP_PS_PROG --format pid,ppid,user,group,args -p "$pids" 2>&1 \
	    | sed -e "s/^/[$$] /" >>$auditfile
	else
	    echo "[$$] -p expands to nothing" >>$auditfile
	fi
    fi
else
    pids="$@"
    if [ -w $auditfile ]
    then
	if [ -n "$pids" ]
	then
	    echo "[$$] target PIDs $pids ..." >>$auditfile
	    $PCP_PS_PROG --format pid,ppid,user,group,args -p "$pids" 2>&1 \
	    | sed -e "s/^/[$$] /" >>$auditfile
	else
	    echo "[$$] no targets" >>$auditfile
	fi
    fi
fi
$nflag && pmsignal=echo

sts=0
# some versions of kill include a gratitous \n after and error
# message, hence the sed at the end
if [ "$PCP_PLATFORM" = mingw ]
then
    for pid in $pids ; do
	$pmsignal pcp-setevent $signal $pid
	if [ $? -ne 0 ]
	then
	    echo "pmsignal: failing command: $pmsignal pcp-setevent $signal $pid"
	    sts=1
	fi
    done
else
    for pid in $pids ; do
	$pmsignal kill -$signal $pid
	if [ $? -ne 0 ]
	then
	    echo "pmsignal: failing command: $pmsignal kill  $signal $pid"
	    sts=1
	fi
    done
fi 2>&1 \
| sed -e '/^$/d'

status=$sts
exit
