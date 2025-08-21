#! /bin/sh
#
# Copyright (c) 2014,2020 Red Hat.
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
tmp=`mktemp -d "$PCP_TMPFILE_DIR/pmlogger_merge.XXXXXXXXX"` || exit 1
tmpmerge=`mktemp -d $PCP_TMPFILE_DIR/pmlogger_merge.XXXXXXXXX` || exit 1
status=0
trap "rm -rf $tmp $tmpmerge; exit \$status" 0 1 2 3 15

force=false
VERBOSE=false
SHOWME=false
EXPUNGE=""
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

# replacement for fmt(1) that is the same on every platform ... mimics
# the FSF version with a maximum line length of 75 columns
#
_fmt()
{
    $PCP_AWK_PROG '
BEGIN		{ len = 0 }
		{ for (i = 1; i <= NF; i++) {
		    wlen = length($i)
		    if (wlen >= 75) {
			# filename longer than notional max line length
			# print partial line (if any), then this one
			# on a line by itself
			if (len > 0) {
			    printf "\n"
			    len = 0
			}
			print $i
		    }
		    else {
			if (len + 1 + wlen > 75) {
			    printf "\n"
			    len = 0
			}
			if (len + 1 + wlen <= 75) {
			    if (len == 0) {
				printf "%s",$i
				len = wlen;
			    }
			    else {
				printf " %s",$i
				len += 1 + wlen
			    }
			}
		    }
		  }
		}
END		{ if (len > 0) printf "\n" }'
}

_usage()
{
    pmgetopt --usage --progname=$prog --config=$tmp/usage
    status=1
    exit
}

cat > $tmp/usage << EOF
# Usage: [options] [input-basename ... output-name]

Options:
  -f, --force    remove input files after creating output files
  -N, --showme   perform a dry run, showing what would be done
  -V, --verbose  increase diagnostic verbosity
  -E, --expunge  expunge metrics with metadata mismatches between archives
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

	-E)	EXPUNGE="-x" # for pmlogextract -x
		;;

	--)	shift
		break
		;;

	-\?)	_usage
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
    _usage
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

# get $compress_suffixes variable
eval `pmconfig -L -s compress_suffixes`

# For each input archive, need to have at least .0 and .meta and
# warn if .index is missing.
# Need to handle compressed versions of all of these.
#
rm -f $tmp/was_compressed
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
		[ -n "$suff" ] && touch $tmp/was_compressed
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
		    echo "Files present for this archive ..."
		    ls `pmlogbasename ${input}`* | _fmt | sed -e 's/^/    /'
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
	    $VERBOSE && echo "		-> partial merge to $tmpmerge/$part"
	    cmd="pmlogextract $EXPUNGE $list $tmpmerge/$part"
	    if $SHOWME
	    then
		echo "+ $cmd"
	    else
		if $cmd
		then
		    :
		else
		    $VERBOSE || echo "		-> partial merge to $tmpmerge/$part"
		    echo "$prog: Directory: `pwd`"
		    echo "$prog: Failed: pmlogextract $list $tmpmerge/$part"
		    _warning
		fi
	    fi
	    list=$tmpmerge/$part
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

    cmd="pmlogextract $EXPUNGE $list $output"
    if $SHOWME
    then
	echo "+ $cmd"
    else
	if $cmd
	then
	    if [ -f $tmp/was_compressed ]
	    then
		# some component of the input archive(s) was
		# compressed, so compress the output archive
		#
		$VERBOSE && echo "Compressing $output"
		if pmlogcompress $output
		then
		    :
		else
		    echo "$prog: Failed: pmlogcompress $output"
		    _warning
		fi
	    fi
	else
	    echo "$prog: Directory: `pwd`"
	    echo "$prog: Failed: pmlogextract $list $output"
	    _warning
	fi
	$VERBOSE && echo "Output archive files:"
	for file in $output.meta $output.index $output.0
	do
	    for suff in "" $compress_suffixes
	    do
		if [ -f "$file$suff" ]
		then
		    file="$file$suff"
		    break
		fi
	    done
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
