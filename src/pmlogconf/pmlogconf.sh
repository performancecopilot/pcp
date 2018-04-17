#!/bin/sh
#
# pmlogconf - generate/edit a pmlogger configuration file
#
# control lines have this format
# #+ tag:on-off:delta
# where
#	tag	is arbitrary (no embedded :'s) and unique
#	on-off	y or n to enable or disable this group, else
#		x for groups excluded by probing from pmlogconf-setup
#		when the group was added to the configuration file
#	delta	delta argument for pmlogger "logging ... on delta" clause
#
# Copyright (c) 2014,2016,2017 Red Hat.
# Copyright (c) 1998,2003 Silicon Graphics, Inc.  All Rights Reserved.
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

# Get standard environment
. $PCP_DIR/etc/pcp.env

# Clear this part to ensure many short-lived pmprobe children
# don't waste time analyzing derived metrics.
PCP_DERIVED_CONFIG=
export PCP_DERIVED_CONFIG

status=1
tmp=`mktemp -d /tmp/pcp.XXXXXXXXX` || exit 1
trap "rm -rf $tmp; exit \$status" 0 1 2 3 15
#debug# tmp=`pwd`/tmp
prog=`basename $0`

cat > $tmp/usage << EOF
# Usage: [options] configfile

Options:
    -c                    add message and timestamp (not for interactive use)
    -d=DIR,--groups=DIR   specify path to the pmlogconf groups directory
    --host
    -q,--quiet            quiet, suppress logging interval dialog
    -r,--reprobe          every group reconsidered for inclusion in configfile
    -v,--verbose          increase diagnostic verbosity
    --help
EOF

_usage()
{
    pmgetopt --progname=$prog --config=$tmp/usage --usage
    exit
}

# Setup the $tmp/pmprobe.out file for use
_setup()
{
    rm -f $tmp/pmprobe.out $tmp/pmprobe.in $tmp/pmprobe
    find $BASE -type f \
    | sed \
	-e '/\/v1.0\//d' \
    | LC_COLLATE=POSIX sort \
    | while read tag
    do
	if sed 1q <"$tag" | grep '^#pmlogconf-setup 2.0' >/dev/null
	then
	    :
	else
	    # not one of our group files, skip it ...
	    continue
	fi
	grep "^probe" $tag | $PCP_AWK_PROG '{ print $2 }' >>$tmp/pmprobe.in
    done
    sort -u $tmp/pmprobe.in > $tmp/pmprobe
    [ -z "$HOST" ] && HOST=local:
    pmprobe -F -h $HOST -v `cat $tmp/pmprobe` >> $tmp/pmprobe.out
}

quick=false
pat=''
prompt=true
reprobe=false
autocreate=false
BASE=''
HOST=''
verbose=false
setupflags=''

ARGS=`pmgetopt --progname=$prog --config=$tmp/usage -- "$@"`
[ $? != 0 ] && exit 1

eval set -- "$ARGS"
while [ $# -gt 0 ]
do
    case "$1"
    in
	-c)	# automated, non-interactive file creation
		autocreate=true
		prompt=false
		;;

	-d)	# base directory for the group files
		BASE="$2"
		shift
		;;

	-h)	# host to contact for "probe" tests
		HOST="$2"
		shift
		;;

	-q)	# "quick" mode, don't change logging intervals
		quick=true
		;;

	-r)	# reprobe
		reprobe=true
		;;

	-v)	# verbose
		verbose=true
		setupflags="$setupflags -v"
		;;

	--)	# end options
		shift
		break
		;;

	-\?)	_usage
		;;
    esac
    shift
done

[ $# -eq 1 ] || _usage

if [ -n "$BASE" -a ! -d "$BASE" ]
then
    echo "$prog: Error: base directory ($BASE) for group files does not exist"
    exit
fi

config="$1"

# split $tmp/ctl at the line containing the unprocessed tag to
# produce
# 	$tmp/head
# 	$tmp/tag	- one line
# 	$tmp/tail
#
_split()
{
    rm -f $tmp/head $tmp/tag $tmp/tail
    $PCP_AWK_PROG <$tmp/ctl '
BEGIN						{ out = "'"$tmp/head"'" }
/DO NOT UPDATE THE FILE ABOVE/			{ seen = 1 }
seen == 0 && /^\#\? [^:]*:[ynx]:/		{ print >"'"$tmp/tag"'"
						  out = "'"$tmp/tail"'"
						  seen = 1
						  next
						}
						{ print >out }'
}

# do all of the real iterative work
#
_update()
{
    # strip the existing pmlogger config and leave the comments
    # and the control lines
    #

    $PCP_AWK_PROG <$tmp/in >$tmp/ctl '
/DO NOT UPDATE THE FILE ABOVE/	{ tail = 1 }
tail == 1			{ print; next }
/^\#\+ [^:]*:[ynx]:/		{ sub(/\+/, "?", $1); print; skip = 1; next }
skip == 1 && /^\#----/		{ skip = 0; next }
skip == 1			{ next }
				{ print }'

    # now need to be a little smarter ... tags may have appeared or
    # disappeared from the shipped defaults, so need to munge the contents
    # of $tmp/ctl to reflect this
    #
    find $BASE -type f \
    | sed \
	-e "s;$BASE/;;" \
	-e '/^v1.0\//d' \
    | LC_COLLATE=POSIX sort \
    | while read tag
    do
	if sed 1q <$BASE/"$tag" | grep '^#pmlogconf-setup 2.0' >/dev/null
	then
	    :
	else
	    # not one of our group files, skip it ...
	    continue
	fi
	if grep "^#? $tag:" $tmp/ctl >/dev/null
	then
	    :
	else
	    $verbose && echo "need to add new group tag=$tag"
	    rm -f $tmp/pre $tmp/post
	    $PCP_AWK_PROG <$tmp/ctl '
BEGIN						{ out = "'"$tmp/pre"'" }
/DO NOT UPDATE THE FILE ABOVE/			{ out = "'"$tmp/post"'" }
						{ print >out }'
	    mv $tmp/pre $tmp/ctl
	    [ -z "$HOST" ] && HOST=local:
	    if $PCP_BINADM_DIR/pmlogconf-setup -h $HOST -t $tmp/pmprobe.out $setupflags $BASE/"$tag" 2>$tmp/err >$tmp/out
	    then
		:
	    else
		echo >&2 "$prog: Warning: $BASE/$tag: pmlogconf-setup failed"
		sts=1
	    fi
	    sed -e "s;$BASE/;;" <$tmp/out >$tmp/tmp
	    [ -s $tmp/err ] && cat $tmp/err
	    sed -e '/^#+/s/+/?/' <$tmp/tmp >>$tmp/ctl
	    [ -s $tmp/post ] && cat $tmp/post >>$tmp/ctl
	fi
    done

    while true
    do
	_split
	[ ! -s $tmp/tag ] && break
	eval `sed <$tmp/tag -e 's/^#? /tag="/' -e 's/:/" onoff="/' -e 's/:/" delta="/' -e 's/:.*/"/'`

	if [ ! -f $BASE/"$tag" ]
	then
	    # the tag file has gone away ...
	    #
	    if $autocreate
	    then
		echo >&2 "$prog: Warning: cannot find group file ($tag): deleting obsolete group"
		cat $tmp/head $tmp/tail >$tmp/ctl
		continue
	    fi
	fi

	[ -z "$delta" ] && delta=default

	if $reprobe
	then
	    [ -z "$HOST" ] && HOST=local:
	    if $PCP_BINADM_DIR/pmlogconf-setup -h $HOST -t $tmp/pmprobe.out $setupflags $BASE/"$tag" 2>$tmp/err >$tmp/out
	    then
		:
	    else
		echo >&2 "$prog: Warning: $BASE/$tag: pmlogconf-setup failed"
		sts=1
	    fi
	    sed -e "s;$BASE/;;" <$tmp/out >$tmp/tmp
	    [ -s $tmp/err ] && cat $tmp/err
	    if [ -s $tmp/tmp ]
	    then
		eval `sed <$tmp/tmp -e 's/^#+ /tag_r="/' -e 's/:/" onoff_r="/' -e 's/:/" delta_r="/' -e 's/:.*/"/'`
		[ -z "$delta_r" ] && delta_r=default
		if [ "$tag" != "$tag_r" ]
		then
		    echo >&2 "Botch: reprobe for $tag found new tag ${tag_r}, no change"
		    cat $tmp/tmp
		else
		    if [ "$onoff" = y ]
		    then
			# existing y takes precedence
			if [ "$onoff_r" = x ]
			then
			    echo >&2 "Warning: reprobe for $tag suggests exclude, keeping current include status"
			fi
		    else
			onoff=$onoff_r
			[ "$delta" != "default" ] && delta=$delta_r
		    fi
		fi
	    fi
	fi

	case $onoff
	in
	    y|n)    ;;
	    x)      # excluded group from setup
		    cat $tmp/head >$tmp/ctl
		    echo "#+ $tag:x::" >>$tmp/ctl
		    echo "#----" >>$tmp/ctl
		    cat $tmp/tail >>$tmp/ctl
		    continue
		    ;;
	    *)	echo >&2 "Warning: tag=$tag onoff is illegal ($onoff) ... setting to \"n\""
		    onoff=n
		    ;;
	esac

	if [ -f $BASE/$tag ]
	then
	    eval `$PCP_AWK_PROG <$BASE/$tag '
BEGIN		{ desc = ""; metrics = "" }
$1 == "ident"	{ if (desc != "") desc = desc "\n"
		  for (i = 2; i <= NF; i++) {
		      if (i == 2) desc = desc $2
		      else desc = desc " " $i
		  }
		  next
		}
END		{ printf "desc='"'"'%s'"'"'\n",desc }'`

	    sed -n <$BASE/$tag >$tmp/metrics \
		-e '/^[ 	]/s/[ 	]*//p'
	    #debug# echo $tag:
	    #debug# echo "desc: $desc"
	else
	    case "$tag"
	    in
		v1.0/*)
			# from migration, silently do nothing
			;;
		*)
			echo >&2 "Warning: cannot find group file ($tag): no change is possible"
			;;
	    esac
	    $PCP_AWK_PROG <"$config" >>$tmp/head '
BEGIN			{ tag="'"$tag"'" }
$1 == "#+" && $2 ~ tag	{ want = 1 }
want == 1		{ print }
want == 1 && /^#----/	{ exit }'
	    cat $tmp/head $tmp/tail >$tmp/ctl
	    continue
	fi

	if [ ! -z "$pat" ]
	then
	    if echo "$desc" | grep "$pat" >/dev/null
	    then
		pat=''
		prompt=true
	    fi
	    if grep "$pat" $tmp/metrics >/dev/null
	    then
		pat=''
		prompt=true
	    fi
	fi
	if $prompt
	then
	    # prompt for answers
	    #
	    echo
	    was_onoff=$onoff
	    echo "Group: $desc" \
	    | if [ "$PCP_PLATFORM" = netbsd ]
	    then
		fmt -g 74 -m 75
	    else
		fmt -w 74
	    fi \
	    | sed -e '1!s/^/       /'
	    while true
	    do
		$PCP_ECHO_PROG $PCP_ECHO_N "Log this group? [$onoff] ""$PCP_ECHO_C"
		read ans
		if [ "$ans" = "?" ]
		then
		    echo 'Valid responses are:
m         report the names of the metrics in this group
n         do not log this group
q         quit; no change for this or any of the following groups
y         log this group
/pattern  no change for this group and search for a group containing pattern
	  in the description or the metrics associated with the group'
		    continue
		fi
		if [ "$ans" = m ]
		then
		    echo "Metrics in this group ($tag):"
		    sed -e 's/^/    /' $tmp/metrics
		    continue
		fi
		if [ "$ans" = q ]
		then
		    # quit ...
		    ans="$onoff"
		    prompt=false
		fi
		pat=`echo "$ans" | sed -n 's/^\///p'`
		if [ ! -z "$pat" ]
		then
		    echo "Searching for \"$pat\""
		    ans="$onoff"
		    prompt=false
		fi
		[ -z "$ans" ] && ans="$onoff"
		[ "$ans" = y -o "$ans" = n ] && break
		echo "Error: you must answer \"m\" or \"n\" or \"q\" or \"y\" or \"/pattern\" ... try again"
	    done
	    onoff="$ans"
	    if [ $prompt = true -a "$onoff" = y ]
	    then
		if $quick
		then
		    if [ $was_onoff = y ]
		    then
			# no change, be quiet
			:
		    else
			echo "Logging interval: $delta"
		    fi
		else
		    while true
		    do
			$PCP_ECHO_PROG $PCP_ECHO_N "Logging interval? [$delta] ""$PCP_ECHO_C"
			read ans
			if [ -z "$ans" ]
			then
			    # use suggested value, assume this is good
			    #
			    ans="$delta"
			    break
			else
			    # do some sanity checking ...
			    #
			    ok=`echo "$ans" \
			        | sed -e 's/^every //' \
				| $PCP_AWK_PROG '
/^once$/			{ print "true"; exit }
/^default$/			{ print "true"; exit }
/^[0-9][0-9]* *msec$/		{ print "true"; exit }
/^[0-9][0-9]* *msecs$/		{ print "true"; exit }
/^[0-9][0-9]* *millisecond$/	{ print "true"; exit }
/^[0-9][0-9]* *milliseconds$/	{ print "true"; exit }
/^[0-9][0-9]* *sec$/		{ print "true"; exit }
/^[0-9][0-9]* *secs$/		{ print "true"; exit }
/^[0-9][0-9]* *second$/		{ print "true"; exit }
/^[0-9][0-9]* *seconds$/	{ print "true"; exit }
/^[0-9][0-9]* *min$/		{ print "true"; exit }
/^[0-9][0-9]* *mins$/		{ print "true"; exit }
/^[0-9][0-9]* *minute$/		{ print "true"; exit }
/^[0-9][0-9]* *minutes$/	{ print "true"; exit }
/^[0-9][0-9]* *hour$/		{ print "true"; exit }
/^[0-9][0-9]* *hours$/		{ print "true"; exit }
				{ print "false"; exit }'`
			    if $ok
			    then
				delta="$ans"
				break
			    else

				echo "Error: logging interval must be of the form \"once\" or \"default\" or"
				echo "\"<integer> <scale>\", where <scale> is one of \"sec\", \"secs\", \"min\","
				echo "\"mins\", etc ... try again"
			    fi
			fi
		    done
		fi
	    fi
	else
	    $PCP_ECHO_PROG $PCP_ECHO_N ".""$PCP_ECHO_C"
	fi

	echo "#+ $tag:$onoff:$delta:" >>$tmp/head
	echo "$desc" | fmt | sed -e 's/^/## /' >>$tmp/head
	if [ "$onoff" = y ]
	then
	    if [ -s $tmp/metrics ]
	    then
		echo "log advisory on $delta {" >>$tmp/head
		sed -e 's/^/	/' <$tmp/metrics >>$tmp/head
		echo "}" >>$tmp/head
	    fi
	fi
	echo "#----" >>$tmp/head
	cat $tmp/head $tmp/tail >$tmp/ctl

    done
}

if $autocreate || $reprobe
then
    # Once-off check for pmcd connectivity, to avoid subsequent repeated
    # failures in pmlogconf-setup (which may take awhile, especially when
    # the environment is setup with slow/faraway PMCD timeout values).
    #
    PMCD="$HOST"
    [ -z "$PMCD" ] && PMCD=local:
    WAIT="-t 10"
    [ -z "$PMCD_WAIT_TIMEOUT" ] || WAIT="-t $PMCD_WAIT_TIMEOUT"
    if $PCP_BINADM_DIR/pmcd_wait -h "$PMCD" $WAIT -v 2>$tmp/err
    then
	:
    else
	sed -e "s/pmcd_wait/$prog/g" < $tmp/err
	exit
    fi
fi

if [ ! -s "$config" ]
then
    # create a new config file
    #
    touch "$config"
    if [ ! -f "$config" ]
    then
	echo "$prog: Error: config file \"$config\" does not exist and cannot be created"
	exit
    fi

    $PCP_ECHO_PROG "Creating config file \"$config\" using default settings ..."
    prompt=false
    new=true
    [ -z "$HOST" ] && HOST=local:
    [ -z "$BASE" ] && BASE=$PCP_VAR_DIR/config/pmlogconf
    _setup

    cat <<End-of-File >$tmp/in
#pmlogconf 2.0
#
# pmlogger(1) config file created and updated by pmlogconf
End-of-File
    $autocreate && echo "# Auto-generated by pmlogconf on:  "`date` >>$tmp/in
    cat <<End-of-File >>$tmp/in
#
# DO NOT UPDATE THE INITIAL SECTION OF THIS FILE.
# Any changes may be lost the next time pmlogconf is used
# on this file.
#
#+ groupdir $BASE
#
End-of-File

    find $BASE -type f \
    | sed \
	-e '/\/v1.0\//d' \
    | LC_COLLATE=POSIX sort \
    | while read tag
    do
	if sed 1q <"$tag" | grep '^#pmlogconf-setup 2.0' >/dev/null
	then
	    :
	else
	    # not one of our group files, skip it ...
	    continue
	fi
	if $PCP_BINADM_DIR/pmlogconf-setup -h $HOST -t $tmp/pmprobe.out $setupflags "$tag" 2>$tmp/err >$tmp/out
	then
	    :
	else
	    echo >&2 "$prog: Warning: $BASE/$tag: pmlogconf-setup failed"
	    [ -s $tmp/err ] && cat $tmp/err
	    sts=1
	fi
	sed -e "s;$BASE/;;" <$tmp/out >>$tmp/in
	[ -s $tmp/err ] && cat $tmp/err
    done

    cat <<End-of-File >>$tmp/in

# DO NOT UPDATE THE FILE ABOVE THIS LINE
# Otherwise any changes may be lost the next time pmlogconf is
# used on this file.
#
# It is safe to make additions from here on ...
#

[access]
disallow .* : all;
disallow :* : all;
allow local:* : enquire;
End-of-File

else
    # updating an existing config file
    #
    new=false
    magic=`sed 1q "$config"`
    if echo "$magic" | grep "^#pmlogconf" >/dev/null
    then
	version=`echo $magic | sed -e "s/^#pmlogconf//" -e 's/^  *//'`
	if [ "$version" = "1.0" ]
	then
	    echo "$prog: migrating \"$config\" from version 1.0 to 2.0 ..."
	    [ -z "$BASE" ] && BASE=$PCP_VAR_DIR/config/pmlogconf
	    sed <"$config" >$tmp/in \
		-e '1s/1\.0/2.0/' \
		-e "/# on this file./a\\
#\\
#+ groupdir $BASE" \
		-e '/^#\+/{
s; C0:; cpu/summary:;
s; C1:; cpu/percpu:;
s; C2:; v1.0/C2:;
s; C3:; v1.0/C3:;
s; D0:; disk/summary:;
s; D1:; disk/percontroller:;
s; D2:; disk/perdisk:;
s; D3:; v1.0/D3:;
s; F0:; filesystem/all:;
s; F1:; filesystem/xfs-io-linux:;
s; F2:; filesystem/xfs-all:;
s; F3:; sgi/xlv-activity:;
s; F4:; sgi/xlv-stripe-io:;
s; F5:; sgi/efs:;
s; F6:; sgi/xvm-ops:;
s; F7:; sgi/xvm-stats:;
s; F8:; sgi/xvm-all:;
s; H0:; sgi/craylink:;
s; H1:; sgi/hub:;
s; H2:; sgi/cpu-evctr:;
s; H3:; sgi/xbow:;
s; I0:; platform/hinv:;
s; K0:; v1.0/K0:;
s; K1:; kernel/syscalls-irix:;
s; K2:; kernel/syscalls-percpu-irix:;
s; K3:; kernel/read-write-data:;
s; K4:; kernel/interrupts-irix:;
s; K5:; kernel/bufcache-activity:;
s; K6:; kernel/bufcache-all:;
s; K7:; kernel/vnodes:;
s; K8:; kernel/inode-cache:;
s; K9:; sgi/kaio:;
s; Ka:; kernel/queues-irix:;
s; M0:; memory/swap-activity:;
s; M1:; memory/tlb-irix:;
s; M2:; kernel/memory-irix:;
s; M3:; memory/swap-all:;
s; M4:; memory/swap-config:;
s; M5:; sgi/node-memory:;
s; M6:; sgi/numa:;
s; M7:; sgi/numa-summary:;
s; N0:; networking/interface-summary:;
s; N1:; networking/interface-all:;
s; N2:; networking/tcp-activity-irix:;
s; N3:; networking/tcp-all:;
s; N4:; networking/udp-packets-irix:;
s; N5:; networking/udp-all:;
s; N6:; networking/socket-irix:;
s; N7:; networking/other-protocols:;
s; N8:; networking/mbufs:;
s; N9:; networking/multicast:;
s; Na:; networking/streams:;
s; S0:; v1.0/S0:;
s; S1:; v1.0/S1:;
s; S2:; networking/rpc:;
}'
	    reprobe=true
	elif [ "$version" = "2.0" ]
	then
	    # start with existing config file
	    #
	    cp "$config" $tmp/in
	else
	    echo "$prog: Error: existing config file \"$config\" is wrong version ($version)"
	    exit
	fi
    else
	echo "$prog: Error: existing \"$config\" is not a $prog control file"
	exit
    fi
    if [ ! -w "$config" ]
    then
	echo "$prog: Error: existing config file \"$config\" is not writeable"
	exit
    fi

    [ -n "$HOST" -a ! $reprobe ] && echo >&2 "$prog: Warning: existing config file, -h $HOST will be ignored"

    CBASE=`sed -n -e '/^#+ groupdir /s///p' <$tmp/in`
    if [ -z "$BASE" ]
    then
	BASE="$CBASE"
    else
	if [ "$BASE" != "$CBASE" ]
	then
	    echo >&2 "$prog: Warning: using base directory for group files from command line ($BASE) which is different from that in $config ($CBASE)"
	fi
    fi
fi

while true
do
    _update

    [ -z "$pat" ] && break

    echo " not found."
    while true
    do
	$PCP_ECHO_PROG $PCP_ECHO_N "Continue searching from start of the file? [y] ""$PCP_ECHO_C"
	read ans
	[ -z "$ans" ] && ans=y
	[ "$ans" = y -o "$ans" = n ] && break
	echo "Error: you must answer \"y\" or \"n\" ... try again"
    done
    mv $tmp/ctl $tmp/in
    if [ "$ans" = n ]
    then
	pat=''
	prompt=true
    else
	echo "Searching for \"$pat\""
    fi
done

if $new
then
    echo
    cp $tmp/ctl "$config"
else
    echo
    if diff "$config" $tmp/ctl >/dev/null
    then
	echo "No changes"
    else
	echo "Differences ..."
	${DIFF-diff} -c "$config" $tmp/ctl
	while true
	do
	    $PCP_ECHO_PROG $PCP_ECHO_N "Keep changes? [y] ""$PCP_ECHO_C"
	    read ans
	    [ -z "$ans" ] && ans=y
	    [ "$ans" = y -o "$ans" = n ] && break
	    echo "Error: you must answer \"y\" or \"n\" ... try again"
	done
	[ "$ans" = y ] && cp $tmp/ctl "$config"
    fi
fi

status=0
exit
