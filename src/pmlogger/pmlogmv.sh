#!/bin/sh
# 
# Copyright (c) 1997,2003 Silicon Graphics, Inc.  All Rights Reserved.
# Copyright (c) 2013-2014 Red Hat.
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

. $PCP_DIR/etc/pcp.env

status=1
tmp=`mktemp -d /tmp/pcp.XXXXXXXXX` || exit 1
trap "rm -rf $tmp; exit \$status" 0
trap "_cleanup; rm -rf $tmp; exit \$status" 1 2 3 15
prog=`basename $0`

cat > $tmp/usage << EOF
# Usage: [options] oldname newname

Options:
  -N, --showme    perform a dry run, showing what would be done
  -V, --verbose   increase diagnostic verbosity
  --help
EOF

_usage()
{
    pmgetopt --progname=$prog --config=$tmp/usage --usage
    exit 1
}

verbose=false
showme=false
LN=ln
RM=rm
RMF="rm -f"

ARGS=`pmgetopt --progname=$prog --config=$tmp/usage -- "$@"`
[ $? != 0 ] && exit 1

eval set -- "$ARGS"
while [ $# -gt 0 ]
do
    case "$1"
    in
	-N)	# show me
	    showme=true
	    LN='echo >&2 + ln'
	    RM='echo >&2 + rm'
	    RMF='echo >&2 + rm -f'
	    ;;
	-V)	# verbose
	    verbose=true
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

if [ -f "$1" ]
then
    # oldname is an existing file, strip the expected PCP suffix
    #
    case "$1"
    in
	*[0-9])
	    old=`echo "$1" | sed -e 's/\.[0-9][0-9]*$//'`
	    ;;
	*.index|*.meta)
	    old=`echo "$1" | sed -e 's/\.[a-z][a-z]*$//'`
	    ;;
	*)
	    echo >&2 "$prog: Error: oldname argument ($1) is not a PCP archive"
	    exit
	    ;;
    esac
else
    old="$1"
fi
new="$2"

_cleanup()
{
    if [ -f $tmp/old ]
    then
	for f in `cat $tmp/old`
	do
	    if [ ! -f "$f" ]
	    then
		part=`echo "$f" | sed -e 's/.*\.\([^.][^.]*\)$/\1/'`
		if [ ! -f "$new.$part" ]
		then
		    echo >&2 "$prog: Fatal: $f and $new.$part lost"
		    ls -l "$old"* "$new"*
		    rm -f $tmp/old
		    return
		fi
		$verbose && echo >&2 "cleanup: recover $f from $new.$part"
		if eval $LN "$new.$part" "$f"
		then
		    :
		else
		    echo >&2 "$prog: Fatal: ln $new.$part $f failed!"
		    ls -l "$old"* "$new"*
		    rm -f $tmp/old
		    return
		fi
	    fi
	done
    fi
    if [ -f $tmp/new ]
    then
	for f in `cat $tmp/new`
	do
	    $verbose && echo >&2 "cleanup: remove $f"
	    eval $RMF "$f"
	done
	rm -f $tmp/new
    fi
    exit
}

# get oldnames inventory check required files are present
#
ls "$old".* 2>&1 | egrep '\.(index|meta|[0-9][0-9]*)$' >$tmp/old
if [ -s $tmp/old ]
then
    # $old may be an ambiguous suffix, e.g. 20140417.00 (with more than
    # one .HH archives) ... pick the suffixes and make sure there are
    # no duplicates
    #
    touch $tmp/ok
    sed <$tmp/old \
	-e 's/.*\.index$/index/' \
	-e 's/.*\.meta$/meta/' \
	-e 's/.*\.\([0-9][0-9]*\)$/\1/' \
    | sort \
    | uniq -c \
    | while read c x
    do
	case $c
	in
	    1)
	    	;;
	    *)
	    	echo >&2 "$prog: Error: oldname argument ($old) is a prefix for multiple PCP archive files:"
		grep "\\.$x\$" $tmp/old | sed -e 's/^/    /' >&2
		rm -f $tmp/ok
		;;
	esac
    done
    [ -f $tmp/ok ] || exit
else
    echo >&2 "$prog: Error: cannot find any files for the input archive ($old)"
    exit
fi
if grep -q '.[0-9][0-9]*$' $tmp/old
then
    :
else
    echo >&2 "$prog: Error: cannot find any data files for the input archive ($old)"
    ls -l "$old"*
    exit
fi
if grep -q '.meta$' $tmp/old
then
    :
else
    echo >&2 "$prog: Error: cannot find .metadata file for the input archive ($old)"
    ls -l "$old"*
    exit
fi

# (hard) link oldnames and newnames
#
for f in `cat $tmp/old`
do
    if [ ! -f "$f" ]
    then
	echo >&2 "$prog: Error: ln-pass: input file vanished: $f"
	ls -l "$old"* "$new"*
	_cleanup
	# NOTREACHED
    fi
    part=`echo "$f" | sed -e 's/.*\.\([^.][^.]*\)$/\1/'`
    if [ -f "$new.$part" ]
    then
	echo >&2 "$prog: Error: ln-pass: output file already exists: $new.$part"
	ls -l "$old"* "$new"*
	_cleanup
	# NOTREACHED
    fi
    $verbose && echo >&2 "link $f -> $new.$part"
    echo "$new.$part" >>$tmp/new
    if eval $LN "$f" "$new.$part"
    then
	:
    else
	echo >&2 "$prog: Error: ln $f $new.$part failed!"
	ls -l "$old"* "$new"*
	_cleanup
	# NOTREACHED
    fi
done

# unlink oldnames provided link count is 2
#
for f in `cat $tmp/old`
do
    if [ ! -f "$f" ]
    then
	echo >&2 "$prog: Error: rm-pass: input file vanished: $f"
	ls -l "$old"* "$new"*
	_cleanup
	# NOTREACHED
    fi
    links=`stat $f | sed -n -e '/Links:/s/.*Links:[ 	]*\([0-9][0-9]*\).*/\1/p'`
    xpect=2
    $showme && xpect=1
    if [ -z "$links" -o "$links" != $xpect ]
    then
	echo >&2 "$prog: Error: rm-pass: link count "$links" (not $xpect): $f"
	ls -l "$old"* "$new"*
	_cleanup
    fi
    $verbose && echo >&2 "remove $f"
    if eval $RM "$f"
    then
	:
    else
	echo >&2 "$prog: Warning: rm $f failed!"
    fi
done

status=0
