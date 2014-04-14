#!/bin/sh
# 
# Copyright (c) 1997,2003 Silicon Graphics, Inc.  All Rights Reserved.
# Copyright (c) 2013 Red Hat.
# Copyright (c) 2014 Ken McDonell.  All Rights Reserved.
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
# Move (rename) a PCP archive.
#
# Operation is atomic across the multiple files of a PCP archive
#

_usage()
{
    echo >&2 "Usage: pmlogmv [-NV] oldname newname"
    exit 1
}

verbose=false
showme=false
LN=ln
RM=rm
RMF="rm -f"

while getopts "NV?" c
do
    case $c
    in
	N)	# show me
	    showme=true
	    LN='echo >&2 + ln'
	    RM='echo >&2 + rm'
	    RMF='echo >&2 + rm -f'
	    ;;
	V)	# verbose
	    verbose=true
	    ;;
	?)
	    _usage
	    # NOTREADCHED
	    ;;
    esac
done
shift `expr $OPTIND - 1`

if [ $# -ne 2 ]
then
    _usage
    # NOTREADCHED
fi

case "$1"
in
    *[0-9])
	old=`echo "$1" | sed -e 's/\.[0-9][0-9]*$//'`
	;;
    *.index|*.meta)
	old=`echo "$1" | sed -e 's/\.[a-z][a-z]*$//'`
	;;
    *)
	old="$1"
	;;
esac
new="$2"

tmp=`mktemp -d /tmp/pcp.XXXXXXXXX` || exit 1
sts=1
trap "_cleanup; rm -rf $tmp; exit \$sts" 1 2 3 15
trap "rm -rf $tmp.*; exit \$sts" 0

_cleanup()
{
    if [ -f $tmp.old ]
    then
	for f in `cat $tmp.old`
	do
	    if [ ! -f "$f" ]
	    then
		part=`echo "$f" | sed -e 's/.*\.\([^.][^.]*\)$/\1/'`
		if [ ! -f "$new.$part" ]
		then
		    echo >&2 "pmlogmv: Fatal: $f and $new.$part lost"
		    ls -l "$old"* "$new"*
		    rm -f $tmp.old
		    return
		fi
		$verbose && echo >&2 "cleanup: recover $f from $new.$part"
		if eval $LN "$new.$part" "$f"
		then
		    :
		else
		    echo >&2 "pmlogmv: Fatal: ln $new.$part $f failed!"
		    ls -l "$old"* "$new"*
		    rm -f $tmp.old
		    return
		fi
	    fi
	done
    fi
    if [ -f $tmp.new ]
    then
	for f in `cat $tmp.new`
	do
	    $verbose && echo >&2 "cleanup: remove $f"
	    eval $RMF "$f"
	done
	rm -f $tmp.new
    fi
    exit
}

# get oldnames inventory check required files are present
#
ls "$old"* 2>&1 | egrep '\.(index|meta|[0-9][0-9]*)$' >$tmp.old
if [ -s $tmp.old ]
then
    :
else
    echo >&2 "pmlogmv: Error: cannot find any files for the input archive ($old)"
    exit
fi
if grep -q '.[0-9][0-9]*$' $tmp.old
then
    :
else
    echo >&2 "pmlogmv: Error: cannot find any data files for the input archive ($old)"
    ls -l "$old"*
    exit
fi
if grep -q '.meta$' $tmp.old
then
    :
else
    echo >&2 "pmlogmv: Error: cannot find .meta file for the input archive ($old)"
    ls -l "$old"*
    exit
fi

# (hard) link oldnames and newnames
#
for f in `cat $tmp.old`
do
    if [ ! -f "$f" ]
    then
	echo >&2 "pmlogmv: Error: ln-pass: input file vanished: $f"
	ls -l "$old"* "$new"*
	_cleanup
	# NOTREACHED
    fi
    part=`echo "$f" | sed -e 's/.*\.\([^.][^.]*\)$/\1/'`
    if [ -f "$new.$part" ]
    then
	echo >&2 "pmlogmv: Error: ln-pass: output file already exists: $new.$part"
	ls -l "$old"* "$new"*
	_cleanup
	# NOTREACHED
    fi
    $verbose && echo >&2 "link $f -> $new.$part"
    echo "$new.$part" >>$tmp.new
    if eval $LN "$f" "$new.$part"
    then
	:
    else
	echo >&2 "pmlogmv: Error: ln $f $new.$part failed!"
	ls -l "$old"* "$new"*
	_cleanup
	# NOTREACHED
    fi
done

# unlink oldnames provided link count is 2
#
for f in `cat $tmp.old`
do
    if [ ! -f "$f" ]
    then
	echo >&2 "pmlogmv: Error: rm-pass: input file vanished: $f"
	ls -l "$old"* "$new"*
	_cleanup
	# NOTREACHED
    fi
    links=`stat $f | sed -n -e '/Links:/s/.*Links:[ 	]*\([0-9][0-9]*\).*/\1/p'`
    xpect=2
    $showme && xpect=1
    if [ -z "$links" -o "$links" != $xpect ]
    then
	echo >&2 "pmlogmv: Error: rm-pass: link count "$links" (not $xpect): $f"
	ls -l "$old"* "$new"*
	_cleanup
    fi
    $verbose && echo >&2 "remove $f"
    if eval $RM "$f"
    then
	:
    else
	echo >&2 "pmlogmv: Warning: rm $f failed!"
    fi
done

sts=0
