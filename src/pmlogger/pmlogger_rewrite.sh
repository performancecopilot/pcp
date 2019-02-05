#! /bin/sh
#
# Copyright (c) 2014 Red Hat.
# Copyright (c) 1995,2003 Silicon Graphics, Inc.  All Rights Reserved.
# Copyright (c) 2018 Ken McDonell.  All Rights Reserved.
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
# wrapper for pmlogrewrite
#
# exit status
# 0	nothing changed
# 1	at least one PCP archive was rewritten
# 2	non-fatal warning
# 4	aborted
#

# Get standard environment
. $PCP_DIR/etc/pcp.env
# ... and _is_archive()
. $PCP_SHARE_DIR/lib/utilproc.sh

prog=`basename $0`
tmp=`mktemp -d /tmp/pcp.XXXXXXXXX` || exit 1
status=0
trap "rm -rf $tmp; exit \$status" 0 1 2 3 15

cat > $tmp/usage << EOF
# Usage: [options] archive ...

Options:
  -c=FILE,--config=FILE  [pmlogrewrite] config file/dir
  -d, --desperate        [pmlogrewrite] desperate, save output archive even after error
  -N, --showme           perform a dry run, showing what would be done
  -s, --scale            [pmlogrewrite] do scale conversion
  -V, --verbose          increase diagnostic verbosity
  -v                     [pmlogrewrite] increased diagnostic verbosity
  -w, --warnings         [pmlogrewrite] emit warnings
  --help
EOF

# option parsing
ARGS=`pmgetopt --progname=$prog --config=$tmp/usage -- "$@"`
if [ $? != 0 ]
then
    status=4
    exit
fi

verbose=false
very_verbose=false
showme=false
rewrite_args="-iq"
eval set -- "$ARGS"
while [ $# -gt 0 ]
do
    case "$1"
    in

	-c)	shift
		rewrite_args="$rewrite_args -c \"$1\""
		;;

	-d)	rewrite_args="$rewrite_args -d"
		;;

	-N)	showme=true
		;;

	-s)	rewrite_args="$rewrite_args -s"
		;;

	-V)	if $verbose
		then
		    very_verbose=true
		else
		    verbose=true
		fi
		;;

	-v)	rewrite_args="$rewrite_args -v"
		;;

	-w)	rewrite_args="$rewrite_args -w"
		;;

	--)	shift
		break
		;;

	-\?)	pmgetopt --usage --progname=$prog --config=$tmp/usage
		status=4
		exit
		;;
    esac
    shift
done

if [ $# -eq 0 ]
then
    pmgetopt --usage --progname=$prog --config=$tmp/usage
    status=4
    exit
fi

eval `pmconfig -L -s compress_suffixes`
if [ -z "$compress_suffixes" ]
then
    # should not happen, fallback to a fixed list
    #
    compress_suffixes='.xz .lzma .bz2 .bz .gz .Z .z'
fi

# For each command line argument, if it is a directory descent to find
# all files and insert these into the command line arguments list in
# place of the directory name.
# Then for each (file) name argument, reduce it to a PCP archive base
# name (strip compression suffix, strip .index or .meta or the
# volume number), check the corresponding metadata file exists and this
# is not a base name we've already seen ... if you get this far, add the
# base name to the list of archives to be processed
#
rm -f $tmp/ags
touch $tmp/args
for try
do
    if [ -d "$try" ]
    then
	# crude filter here ... more precise filtering later on
	#
	find "$try" -type f \
	| egrep '(\.meta|\.index|\.[0-9][0-9]*)($|(\.xz|\.lzma|\.bz2|\.bz|\.gz|\.Z|\.z)$)' >>$tmp/args
    else
	echo "$try" >>$tmp/args
    fi
done
rm -f $tmp/archives
touch $tmp/archives
for try in `cat $tmp/args`
do
    base="$try"
    for suff in $compress_suffixes
    do
	if echo "$try" | grep "$suff\$" >/dev/null
	then
	    base=`echo "$try" | sed -e "s/$suff\$//"`
	    break
	fi
    done
    # only one valid match is possible below ...
    #
    case "$base"
    in
	*.index)
	    base=`echo "$base" | sed -e 's/\.index$//'`
	    ;;
	*.meta)
	    base=`echo "$base" | sed -e 's/\.meta$//'`
	    ;;
	*.[0-9]|*.[0-9][0-9]|*.[0-9]*[0-9])
	    base=`echo "$base" | sed -e 's/\.[0-9]*[0-9]$//'`
	    ;;
	*)
	    # may already be the base name of an archive ... look for
	    # a .meta or .meta.* file
	    #
	    rm -f $tmp/ok
	    if [ -f "$base".meta ]
	    then
		touch $tmp/ok
	    else
		for suff in $compress_suffixes
		do
		    if [ -f "$base.meta"$suff ]
		    then
			touch $tmp/ok
			break
		    fi
		done
	    fi
	    if [ ! -f $tmp/ok ]
	    then
		base=''
		$verbose && echo "Warning: $try: not a PCP archive name"
	    fi
	    ;;
    esac
    [ -z "$base" ] && continue
    if [ "$try" != "$base" ]
    then
	$very_verbose && echo "suffix stripping: arg: $try -> $base"
    fi
    if grep "^$base" $tmp/archives >/dev/null
    then
	$very_verbose && echo "$base: duplicate, already in archives list"
    else
	check=`echo $base.meta*`
	if $showme || _is_archive "$check"
	then
	    echo "$base" >>$tmp/archives
	fi
    fi
done

if [ ! -s $tmp/archives ]
then
    $verbose && echo "Warning: no PCP archives found"
    exit
fi

# now for every selected archive ... check it is not active and the archive
# is OK, then rewrite and if a file is changed, recompress if the original
# file was compressed
#
for archive in `cat $tmp/archives`
do
    if $showme
    then
	echo "+ check $archive not active and OK"
	echo "+ pmlogrewrite $rewrite_args $archive"
	continue
    fi
    pid=`pmdumplog -L $archive | sed -n -e '/^PID for pmlogger: /s///p' 2>/dev/null`
    if [ -z "$pid" ]
    then
	pmdumplog -L $archive
	echo "Botch: cannot get pmlogger PID from label record for $archive"
	continue
    fi
    if pmprobe -I pmcd.pmlogger.port | grep "\"$pid\"" >/dev/null
    then
	$verbose && echo "Warning: skip archive $archive, pmlogger PID $pid is still running"
	continue
    fi
    if pmlogcheck -w "$archive" >$tmp/out 2>&1
    then
	# OK
	:
    elif [ -s $tmp/out ]
    then
	$verbose && cat $tmp/out
	echo "Warning: $base: bad archive, rewriting skipped"
	continue
    else
	# empty output but non-zero exit status?
	#
	echo "Warning: $base: bad archive (pmlogcheck exit status=$?), rewriting skipped"
	continue
    fi
    if `which sum >/dev/null 2>&1`
    then
	SUM=sum
    elif `which cksum >/dev/null 2>&1`
    then
	SUM=cksum
    else
	# Using wc(1) is lame, but no real choice!
	#
	echo "Warning: can't find sum(1) or chksum(1)"
	SUM="wc -c"
    fi
    # use sum(1) or cksum(1) to detect changes at the level of individual files
    #
    rm -f $tmp/sum.before
    for file in $archive.*
    do
	echo "$file `$SUM "$file" | sed -e 's/[ 	][ 	]*/ /g'`" >>$tmp/sum.before
    done

    eval pmlogrewrite $rewrite_args "$archive"

    rm -f $tmp/sum.after
    for file in $archive.*
    do
	echo "$file `$SUM "$file" | sed -e 's/[ 	][ 	]*/ /g'`" >>$tmp/sum.after
    done
    # now use comm to find the lines that are in $tmp/sum.after and not
    # in $tmp/sum.before ... these are the files that have been changed
    # by pmlogrewrite
    #
    comm -13 $tmp/sum.before $tmp/sum.after \
    | while read file stuff
    do
	old_file=`grep "^$file" $tmp/sum.before | sed -e 's/[ 	].*//'`
	if [ "$file" != "$old_file" ]
	then
	    # original file name and current file name do not match ...
	    # only cause should be compressed input file, uncompressed
	    # output file
	    #
	    case "$old_file"
	    in
		*.xz|*.lzma)
		    if which xz >/dev/null 2>&1
		    then
			# same logic as in pmlogger_daily for optimal
			# compression
			#
			if xz -0 --block-size=10MiB </dev/null >/dev/null 2>&1
			then
			    # want minimal overheads, -0 is the same as --fast
			    xz -0 --block-size=10MiB "$file"
			else
			    xz "$file"
			fi
			$very_verbose && echo "changed and recompressed: $old_file"
		    else
			echo "Warning: no xz(1), cannot recompress $file"
		    fi
		    ;;
		*.bz2|*.bz)
		    if which bzip2 >/dev/null 2>&1
		    then
			bzip2 "$file"
			$very_verbose && echo "changed and recompressed: $old_file"
		    else
			echo "Warning: no bzip2(1), cannot recompress $file"
		    fi
		    ;;
		*.gz|*.Z|*.z)
		    if which gzip >/dev/null 2>&1
		    then
			gzip "$file"
			$very_verbose && echo "changed and recompressed: $old_file"
		    else
			echo "Warning: no gzip(1), cannot recompress $file"
		    fi
		    ;;
		*)
		    echo "Botch: cannot handle rewriting file name change: $old_file -> $file"
		    ;;
	    esac
	else
	    $very_verbose && echo "changed: $old_file"
	fi
    done
done

exit
