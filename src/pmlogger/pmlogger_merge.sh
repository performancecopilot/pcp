#! /bin/sh
#
# Copyright (c) 2014 Red Hat.
# Copyright (c) 1995,2003 Silicon Graphics, Inc.  All Rights Reserved.
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
# merge a group of logfiles, e.g. all those for today
#
# default case, w/out arguments uses the default pmlogger filename
# conventions for today's logs, namely `date +%Y%m%d` for both the
# input-basename and the output-name
#

# Get standard environment
. $PCP_DIR/etc/pcp.env


prog=`basename $0`
tmp=`mktemp -d /tmp/pcp.XXXXXXXXX` || exit 1
status=0
trap "rm -rf $tmp; exit \$status" 0 1 2 3 15

force=false
VERBOSE=false
SHOWME=false
RM=rm

_abandon()
{
    echo "$prog: These error(s) are fatal, no output archive has been created."
    status=1
    exit
}

_warning()
{
    echo "$prog: Trying to continue, although output archive may be corrupted."
    force=false
}

cat > $tmp/usage << EOF
# Usage: [options] [input-basename ... output-name]

Options:
  -f, --force    remove input files after creating output files
  -N, --showme   perform a dry run, showing what would be done
  -V, --verbose  increase diagnostic verbosity
  --help
EOF

# option parsing
ARGS=`pmgetopt --progname=$prog --config=$tmp/usage -- "$@"`
[ $? != 0 ] && exit 1

eval set -- "$ARGS"
while [ $# -gt 0 ]
do
    case "$1"
    in

	-f)	force=true
		;;

	-N)	SHOWME=true
		RM="echo + rm"
		;;

	-V)	VERBOSE=true
		;;

	--)	shift
		break
		;;

	-\?)	pmgetopt --usage --progname=$prog --config=$tmp/usage
		_abandon
		;;
    esac
    shift
done

if [ $# -eq 0 ]
then
    trylist=`date +%Y%m%d`
    output=$input
elif [ $# -ge 2 ]
then
    trylist=""
    while [ $# -ge 2 ]
    do
	trylist="$trylist $1"
	shift
    done
    output="$1"
else
    pmgetopt --usage --progname=$prog --config=$tmp/usage
    status=1
    exit
fi

fail=false
mergelist=""

# handle glob expansion if command line arguments contain
# shell metacharacters and handle dupicate-breaking name
# form of the base name i.e. YYYYMMDD.HH.MM-seq# ...
# and ensure no duplicates
#
rm -f $tmp/input
echo >$tmp/input
for try in $trylist
do
    # regular YYYYMMDD.HH.MM.meta files (maybe with compression)
    #
    for xxx in $try.meta*
    do
	[ "$xxx" = "$try.meta*" ] && continue
	base=`echo "$xxx" | sed -e 's/\(.*\)\.meta.*/\1/'`
	grep "^$base\$" $tmp/input >/dev/null || echo "$base" >>$tmp/input
    done
    # duplicate-breaking YYYYMMDD.HH.MM-seq#.meta files (maybe with compression)
    #
    for xxx in $try-*.meta*
    do
	[ "$xxx" = "$try-*.meta*" ] && continue
	base=`echo "$xxx" | sed -e 's/\(.\)\.meta.*/\1/'`
	grep "^$base\$" $tmp/input >/dev/null || echo "$base" >>$tmp/input
    done
done

eval `pmconfig -L -s compress_suffixes`
if [ -z "$compress_suffixes" ]
then
    # should not happen, fallback to a fixed list
    #
    compress_suffixes='.xz .lzma .bz2 .bz .gz .Z .z'
fi

# For each input archive, need to have at least .0 and .meta and
# warn if .index is missing.
# Need to handle compressed versions of all of these.
#
for input in `cat $tmp/input`
do
    empty=0
    for part in index meta 0
    do
	rm -f $tmp/found
	for suff in "" $compress_suffixes
	do
	    if [ -f "$input.$part$suff" ]
	    then
		touch $tmp/found
		if [ ! -s "$input.$part$suff" ]
		then
		    empty=`expr $empty + 1`
		fi
		break
	    fi
	done
	if [ ! -f $tmp/found ]
	then
	    case $part
	    in
		index)
		    echo "$prog: Warning: \"index\" file missing for archive \"$input\""
		    empty=`expr $empty + 1`
		    ;;
		meta)
		    echo "$prog: Error: \"meta\" file missing for archive \"$input\""
		    fail=true
		    ;;
		0)
		    echo "$prog: Error: \"volume 0\" file missing for archive \"$input\""
		    ls ${file}*
		    fail=true
		    ;;
	    esac
	fi
    done
    if [ $empty -eq 3 ]
    then
	echo "$prog: Warning: archive \"$input\" is empty and will be skipped"
    else
	mergelist="$mergelist $input"
    fi
done

if [ -f $output.index ]
then
    echo "$prog: Error: \"index\" file already exists for output archive \"$output\""
    fail=true
fi
if [ -f $output.meta ]
then
    echo "$prog: Error: \"meta\" file already exists for output archive \"$output\""
    fail=true
fi
if [ -f $output.0 ]
then
    echo "$prog: Error: \"volume 0\" file already exists for output archive \"$output\""
    fail=true
fi

$fail && _abandon

i=0
list=""
part=0
if [ -z "$mergelist" ]
then
    $VERBOSE && echo "No archives to be merged."
else
    $VERBOSE && echo "Input archives to be merged:"
    for input in $mergelist
    do
	if [ $i -ge 35 ]
	then
	    # this limit requires of the order of 3 x 35 input + 3 x 1
	    # output = 108 file descriptors which should be well below any
	    # shell-imposed or system-imposed limits
	    #
	    $VERBOSE && echo "		-> partial merge to $tmp/$part"
	    cmd="pmlogextract $list $tmp/$part"
	    if $SHOWME
	    then
		echo "+ $cmd"
	    else
		if $cmd
		then
		    :
		else
		    $VERBOSE || echo "		-> partial merge to $tmp/$part"
		    echo "$prog: Directory: `pwd`"
		    echo "$prog: Failed: pmlogextract $list $tmp/$part"
		    _warning
		fi
	    fi
	    list=$tmp/$part
	    part=`expr $part + 1`
	    i=0
	fi
	list="$list $input"
	$VERBOSE && $PCP_ECHO_PROG $PCP_ECHO_N "	$input""$PCP_ECHO_C"
	numarch=`echo $input.[0-9]* | wc -w | sed -e 's/  *//g'`
	if [ $numarch -gt 1 ]
	then
	    $VERBOSE && echo " ($numarch volumes)"
	else
	    $VERBOSE && echo
	fi
	i=`expr $i + 1`
    done

    cmd="pmlogextract $list $output"
    if $SHOWME
    then
	echo "+ $cmd"
    else
	if $cmd
	then
	    :
	else
	    echo "$prog: Directory: `pwd`"
	    echo "$prog: Failed: pmlogextract $list $output"
	    _warning
	fi
	$VERBOSE && echo "Output archive files:"
	for file in $output.meta $output.index $output.0
	do
	    if [ -f $file ]
	    then
		$VERBOSE && LC_TIME=POSIX ls -l $file
	    else
		echo "$prog: Error: file \"$file\" not created"
		force=false
	    fi
	done
    fi
fi

if $force
then
    $VERBOSE && $PCP_ECHO_PROG $PCP_ECHO_N "Removing input archive files ""$PCP_ECHO_C"
    for input in `cat $tmp/input`
    do
	if $VERBOSE
	then
	    for file in $input.index* $input.meta* $input.[0-9]*
	    do
		$PCP_ECHO_PROG $PCP_ECHO_N ".""$PCP_ECHO_C"
	    done
	fi
	eval $RM -f $input.index* $input.meta* $input.[0-9]*
    done
    $VERBOSE && echo " done"
fi

exit
