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
		_abandon
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
	find "$try" -type f >>$tmp/args
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
	    rm -f /tmp/ok
	    if [ -f "$base".meta ]
	    then
		touch /tmp/ok
	    else
		for suff in $compress_suffixes
		do
		    if [ -f "$base.meta"$suff ]
		    then
			touch /tmp/ok
			break
		    fi
		done
	    fi
	    if [ ! -f /tmp/ok ]
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
	echo "$base" >>$tmp/archives
    fi
done

if [ -s $tmp/archives ]
then
    for archive in `cat $tmp/archives`
    do
	if $showme
	then
	    echo "+ pmlogrewrite $rewrite_args $archive"
	else
	    pmlogcheck -w "$base" >$tmp/out 2>&1
	    sts=$?
	    if [ $sts = 0 ]
	    then
		pmlogrewrite $rewrite_args $archive
	    elif [ -s $tmp/out ]
	    then
		$verbose && cat $tmp/out
		echo "Warning: $base: bad archive, rewriting skipped"
	    else
		# empty output but non-zero exit status?
		#
		echo "Warning: $base: bad archive (pmlogcheck exit status=$sts), rewriting skipped"
	    fi
	fi
    done
else
    $verbose && echo "Warning: no PCP archives found"
fi

exit
