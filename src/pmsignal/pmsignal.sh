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
# Cross-platform signal/event sender for Performance Co-Pilot utilities.
# Supports a minimal set of signals, used by PCP tools on all platforms.
#

. $PCP_DIR/etc/pcp.env

prog=`basename $0`
sigs="HUP USR1 TERM KILL"

usage()
{
    [ ! -z "$@" ] && echo $@ 1>&2
    echo 1>&2 "Usage: $prog [options] PID ... | name ...

Options:
  -a            send signal to all named processes (killall mode)
  -l            list available signals
  -n            dry-run, list processes that would be affected
  -s signal     signal to send ($sigs)"
    exit 0
}

check()
{
    for sig in $sigs
    do
	[ $sig = "$1" ] && echo $sig && return
    done
    usage "$prog: invalid signal - $1"
}

signal=TERM
aflag=false
lflag=false
nflag=false
while getopts "?alns:" c
do
    case $c in
      a) aflag=true ;;
      l) lflag=true ;;
      n) nflag=true ;;
      s) signal=`check "$OPTARG"` ;;
      ?) usage "" ;;
    esac
done

[ $lflag = true ] && echo "$sigs" && exit 0

shift `expr $OPTIND - 1`
[ $# -lt 1 ] && usage "$prog: too few arguments"

if [ $aflag = true ]
then
    pids=""
    for name in "$@"; do
	program=`basename "$name"`
	pidlist=`_get_pids_by_name "$program"`
	pids="$pids $pidlist"
    done
else
    pids="$@"
fi
[ $nflag = true ] && echo "$pids" && exit 0

sts=0
if [ "$PCP_PLATFORM" = mingw ]
then
    for pid in $pids ; do
	pcp-setevent $signal $pid
	[ $? -eq 0 ] || sts=$?
    done
else
    for pid in $pids ; do
	kill -$signal $pid
	[ $? -eq 0 ] || sts=$?
    done
fi

exit $sts
