#! /bin/sh
#
# Copyright (c) 2013-2015 Red Hat.
# Copyright (c) 1995-2000,2003 Silicon Graphics, Inc.  All Rights Reserved.
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
# Daily administrative script for PCP archive logs
#

. $PCP_DIR/etc/pcp.env
. $PCP_SHARE_DIR/lib/rc-proc.sh

# error messages should go to stderr, not the GUI notifiers
#
unset PCP_STDERR

# constant setup
#
prog=`basename $0`
tmp=`mktemp -d /tmp/pcp.XXXXXXXXX` || exit 1
status=0

_cleanup()
{
    lockfile=`cat $tmp/lock 2>/dev/null`
    rm -f "$PCP_RUN_DIR/pmlogger_daily.pid" "$lockfile"
    rm -rf $tmp
}
trap "_cleanup; exit \$status" 0 1 2 3 15
echo >$tmp/lock

if is_chkconfig_on pmlogger
then
    PMLOGGER_CTL=on
else
    PMLOGGER_CTL=off
fi

# control file for pmlogger administration ... edit the entries in this
# file to reflect your local configuration (see also -c option below)
#
CONTROL=$PCP_PMLOGGERCONTROL_PATH

# default number of days to keep archive logs
#
CULLAFTER=14

# default compression program and days until starting compression
# 
COMPRESS=xz
COMPRESSAFTER=""
COMPRESSREGEX="\.(meta|index|Z|gz|bz2|zip|xz|lzma|lzo|lz4)$"

# threshold size to roll $PCP_LOG_DIR/NOTICES
#
NOTICES=$PCP_LOG_DIR/NOTICES
ROLLNOTICES=20480

# mail addresses to send daily NOTICES summary to
# 
MAILME=""
MAILFILE=$PCP_LOG_DIR/NOTICES.daily

# search for your mail agent of choice ...
#
MAIL=''
for try in Mail mail email
do
    if which $try >/dev/null 2>&1
    then
	MAIL=$try
	break
    fi
done

# NB: FQDN cleanup; don't guess a 'real name for localhost', and
# definitely don't truncate it a la `hostname -s`.  Instead now
# we use such a string only for the default log subdirectory, ie.
# for substituting LOCALHOSTNAME in the fourth column of $CONTROL.

# determine path for pwd command to override shell built-in
# (see BugWorks ID #595416).
PWDCMND=`which pwd 2>/dev/null | $PCP_AWK_PROG '
BEGIN	    	{ i = 0 }
/ not in /  	{ i = 1 }
/ aliased to /  { i = 1 }
 	    	{ if ( i == 0 ) print }
'`
if [ -z "$PWDCMND" ]
then
    #  Looks like we have no choice here...
    #  force it to a known IRIX location
    PWDCMND=/bin/pwd
fi
eval $PWDCMND -P >/dev/null 2>&1
[ $? -eq 0 ] && PWDCMND="$PWDCMND -P"
here=`$PWDCMND`

echo > $tmp/usage
cat >> $tmp/usage <<EOF
Options:
  -c=FILE,--control=FILE  pmlogger control file
  -k=N,--discard=N        remove archives after N days
  -m=ADDRs,--mail=ADDRs   send daily NOTICES entries to email addresses
  -M		          do not rewrite, merge or rename archives
  -N,--showme             perform a dry run, showing what would be done
  -o                      merge yesterdays logs only (old form, default is all) 
  -r,--norewrite          do not process archives with pmlogrewrite(1)
  -s=SIZE,--rotate=SIZE   rotate NOTICES file after reaching SIZE bytes
  -t=WANT                 implies -VV, keep verbose output trace for WANT days
  -V,--verbose            verbose output (multiple times for very verbose)
  -x=N,--compress-after=N  compress archive data files after N days
  -X=PROGRAM,--compressor=PROGRAM  use PROGRAM for archive data file compression
  -Y=REGEX,--regex=REGEX  egrep filter when compressing files ["$COMPRESSREGEX"]
  --help
EOF

_usage()
{
    pmgetopt --progname=$prog --config=$tmp/usage --usage
    status=1
    exit
}

# option parsing
#
SHOWME=false
VERBOSE=false
VERY_VERBOSE=false
MYARGS=""
OFLAG=false
TRACE=0
RFLAG=false
MFLAG=false

ARGS=`pmgetopt --progname=$prog --config=$tmp/usage -- "$@"`
[ $? != 0 ] && exit 1

eval set -- "$ARGS"
while [ $# -gt 0 ]
do
    case "$1"
    in
	-c)	CONTROL="$2"
		shift
		;;
	-k)	CULLAFTER="$2"
		shift
		check=`echo "$CULLAFTER" | sed -e 's/[0-9]//g'`
		if [ ! -z "$check" -a X"$check" != Xforever ]
		then
		    echo "Error: -k option ($CULLAFTER) must be numeric"
		    status=1
		    exit
		fi
		;;
	-m)	MAILME="$2"
		shift
		;;
	-N)	SHOWME=true
		MYARGS="$MYARGS -N"
		;;
	-M)	MFLAG=true
		RFLAG=true
  		;;
	-o)	OFLAG=true
		;;
	-r)	RFLAG=true
		;;
	-s)	ROLLNOTICES="$2"
		shift
		check=`echo "$ROLLNOTICES" | sed -e 's/[0-9]//g'`
		if [ ! -z "$check" ]
		then
		    echo "Error: -s option ($ROLLNOTICES) must be numeric"
		    status=1
		    exit
		fi
		;;
	-t)	TRACE="$2"
		shift
		# from here on, all stdout and stderr output goes to
		# $PCP_LOG_DIR/pmlogger/daily.<date>.trace
		#
		exec 1>$PCP_LOG_DIR/pmlogger/daily.`date "+%Y%m%d.%H.%M"`.trace 2>&1
		VERBOSE=true
		VERY_VERBOSE=true
		MYARGS="$MYARGS -V -V"
		;;
	-V)	if $VERBOSE
		then
		    VERY_VERBOSE=true
		else
		    VERBOSE=true
		fi
		MYARGS="$MYARGS -V"
		;;
	-x)	COMPRESSAFTER="$2"
		shift
		check=`echo "$COMPRESSAFTER" | sed -e 's/[0-9]//g'`
		if [ ! -z "$check" ]
		then
		    echo "Error: -x option ($COMPRESSAFTER) must be numeric"
		    status=1
		    exit
		fi
		;;
	-X)	COMPRESS="$2"
		shift
		;;
	-Y)	COMPRESSREGEX="$2"
		shift
		;;
	--)	shift
		break
		;;
	-\?)	_usage
		;;
    esac
    shift
done

[ $# -ne 0 ] && _usage

if [ ! -f "$CONTROL" ]
then
    echo "$prog: Error: cannot find control file ($CONTROL)"
    status=1
    exit
fi

# each new archive log started by pmnewlog or pmlogger_check is named
# yyyymmdd.hh.mm
#
LOGNAME=`date "+%Y%m%d.%H.%M"`

_error()
{
    _report Error "$1"
}

_warning()
{
    _report Warning "$1"
}

_report()
{
    echo "$prog: $1: $2"
    echo "[$CONTROL:$line] ... logging for host \"$host\" unchanged"
    touch $tmp/err
}

_unlock()
{
    rm -f lock
    echo >$tmp/lock
}

# filter file names to leave those that look like PCP archives
# managed by pmlogger_check and pmlogger_daily, namely they begin
# with a datestamp
#
# need to handle both the year 2000 and the old name formats, and
# possible ./ prefix (from find .)
# 
_filter_filename()
{
    sed -n \
	-e 's/^\.\///' \
	-e '/^[12][0-9][0-9][0-9][0-1][0-9][0-3][0-9][-.]/p' \
	-e '/^[0-9][0-9][0-1][0-9][0-3][0-9][-.]/p'
}

_get_ino()
{
    # get inode number for $1
    # throw away stderr (and return '') in case $1 has been removed by now
    #
    stat "$1" 2>/dev/null \
    | sed -n '/Device:[ 	].*[ 	]Inode:/{
s/Device:[ 	].*[ 	]Inode:[ 	]*//
s/[ 	].*//
p
}'
}

# mails out any entries for the previous 24hrs from the PCP notices file
# 
if [ ! -z "$MAILME" ]
then
    # get start time of NOTICES entries we want - all earlier are discarded
    # 
    args=`pmdate -1d '-v yy=%Y -v my=%b -v dy=%d'`
    args=`pmdate -1d '-v Hy=%H -v My=%M'`" $args"
    args=`pmdate '-v yt=%Y -v mt=%b -v dt=%d'`" $args"

    # 
    # Basic algorithm:
    #   from NOTICES head, look for a DATE: entry for yesterday or today;
    #   if its yesterday, find all HH:MM timestamps which are in the window,
    #       until the end of yesterday is reached;
    #   copy out the remainder of the file (todays entries).
    # 
    # initially, entries have one of three forms:
    #   DATE: weekday mon day HH:MM:SS year
    #   Started by pmlogger_daily: weekday mon day HH:MM:SS TZ year
    #   HH:MM message
    # 

    # preprocess to provide a common date separator - if new date stamps are
    # ever introduced into the NOTICES file, massage them first...
    # 
    rm -f $tmp/pcp
    $PCP_AWK_PROG '
/^Started/	{ print "DATE:",$4,$5,$6,$7,$9; next }
		{ print }
	' $NOTICES | \
    $PCP_AWK_PROG -F ':[ \t]*|[ \t]+' $args '
$1 == "DATE" && $3 == mt && $4 == dt && $8 == yt { tday = 1; print; next }
$1 == "DATE" && $3 == my && $4 == dy && $8 == yy { yday = 1; print; next }
	{ if ( tday || (yday && $1 > Hy) || (yday && $1 == Hy && $2 >= My) )
	    print
	}' >$tmp/pcp

    if [ -s $tmp/pcp ]
    then
	if [ ! -z "$MAIL" ]
	then
	    $MAIL -s "PCP NOTICES summary for `hostname`" $MAILME <$tmp/pcp
	else
	    echo "$prog: Warning: cannot find a mail agent to send mail ..."
	    echo "PCP NOTICES summary for `hostname`"
	    cat $tmp/pcp
	fi
        [ -w `dirname "$NOTICES"` ] && mv $tmp/pcp "$MAILFILE"
    fi
fi


# Roll $PCP_LOG_DIR/NOTICES -> $PCP_LOG_DIR/NOTICES.old if larger
# that 10 Kbytes, and you can write in $PCP_LOG_DIR
#
if [ -s "$NOTICES" -a -w `dirname "$NOTICES"` ]
then
    if [ "`wc -c <"$NOTICES"`" -ge $ROLLNOTICES ]
    then
	if $VERBOSE
	then
	    echo "Roll $NOTICES -> $NOTICES.old"
	    echo "Start new $NOTICES"
	fi
	if $SHOWME
	then
	    echo "+ mv -f $NOTICES $NOTICES.old"
	    echo "+ touch $NOTICES"
	else
	    echo >>"$NOTICES"
	    echo "*** rotated by $prog: `date`" >>"$NOTICES"
	    mv -f "$NOTICES" "$NOTICES.old"
	    echo "Started by $prog: `date`" >"$NOTICES"
	    (id "$PCP_USER" && chown $PCP_USER:$PCP_GROUP "$NOTICES") >/dev/null 2>&1
	fi
    fi
fi

# Keep our pid in $PCP_RUN_DIR/pmlogger_daily.pid ... this is checked
# by pmlogger_check when it fails to obtain the lock should it be run
# while pmlogger_daily is running
#
# For most packages, $PCP_RUN_DIR is included in the package,
# but for Debian and cases where /var/run is a mounted filesystem
# it may not exist, so create it here before it is used to create
# any pid/lock files
#
# $PCP_RUN_DIR creation is also done in other daemons startup, but we
# have no guarantee any other daemons are running on this system.
#
if [ ! -d "$PCP_RUN_DIR" ]
then
    mkdir -p -m 775 "$PCP_RUN_DIR" 2>/dev/null
    # might be running from cron as unprivileged user
    [ $? -ne 0 -a "$PMLOGGER_CTL" = "off" ] && exit 0
    chown $PCP_USER:$PCP_GROUP "$PCP_RUN_DIR"
fi
echo $$ >"$PCP_RUN_DIR"/pmlogger_daily.pid

# note on control file format version
#  1.0 was shipped as part of PCPWEB beta, and did not include the
#	socks field [this is the default for backwards compatibility]
#  1.1 is the first production release, and the version is set in
#	the control file with a $version=1.1 line (see below)
#

rm -f $tmp/err
line=0
version=''
cat $CONTROL \
| sed -e "s;PCP_LOG_DIR;$PCP_LOG_DIR;g" \
| while read host primary socks dir args
do
    # start in one place for each iteration (beware relative paths)
    cd "$here"
    line=`expr $line + 1`

    # NB: FQDN cleanup: substitute the LOCALHOSTNAME marker in the config line
    # differently for the directory and the pcp -h HOST arguments.
    dir_hostname=`hostname || echo localhost`
    dir=`echo $dir | sed -e "s;LOCALHOSTNAME;$dir_hostname;"`
    [ "x$host" = "xLOCALHOSTNAME" ] && host=local:

    $VERY_VERBOSE && echo "[control:$line] host=\"$host\" primary=\"$primary\" socks=\"$socks\" dir=\"$dir\" args=\"$args\""

    case "$host"
    in
	\#*|'')	# comment or empty
		continue
		;;

	\$*)	# in-line variable assignment
		$SHOWME && echo "# $host $primary $socks $dir $args"
		cmd=`echo "$host $primary $socks $dir $args" \
		     | sed -n \
			 -e "/='/s/\(='[^']*'\).*/\1/" \
			 -e '/="/s/\(="[^"]*"\).*/\1/' \
			 -e '/=[^"'"'"']/s/[;&<>|].*$//' \
			 -e '/^\\$[A-Za-z][A-Za-z0-9_]*=/{
s/^\\$//
s/^\([A-Za-z][A-Za-z0-9_]*\)=/export \1; \1=/p
}'`
		if [ -z "$cmd" ]
		then
		    # in-line command, not a variable assignment
		    _warning "in-line command is not a variable assignment, line ignored"
		else
		    case "$cmd"
		    in
			'export PATH;'*)
			    _warning "cannot change \$PATH, line ignored"
			    ;;
			'export IFS;'*)
			    _warning "cannot change \$IFS, line ignored"
			    ;;
			*)
			    $SHOWME && echo "+ $cmd"
			    eval $cmd
			    ;;
		    esac
		fi
		continue
		;;
    esac

    if [ -z "$version" -o "$version" = "1.0" ]
    then
	if [ -z "$version" ]
	then
	    echo "$prog: Warning: processing default version 1.0 control format"
	    version=1.0
	fi
	args="$dir $args"
	dir="$socks"
	socks=n
    fi

    if [ -z "$primary" -o -z "$socks" -o -z "$dir" -o -z "$args" ]
    then
	_error "insufficient fields in control file record"
	continue
    fi

    if $VERY_VERBOSE
    then
	pflag=''
	[ $primary = y ] && pflag=' -P'
	echo "Check pmlogger$pflag -h $host ... in $dir ..."
    fi

    if [ ! -d $dir ]
    then
	_error "archive directory ($dir) does not exist"
	continue
    fi

    cd $dir
    dir=`$PWDCMND`
    $SHOWME && echo "+ cd $dir"

    if $VERBOSE
    then
	echo
	echo "=== daily maintenance of PCP archives for host $host ==="
	echo
    fi

    if [ ! -w $dir ]
    then
	echo "$prog: Warning: no write access in $dir, skip lock file processing"
    else
	# demand mutual exclusion
	#
	fail=true
	rm -f $tmp/stamp
	for try in 1 2 3 4
	do
	    if pmlock -v lock >$tmp/out
	    then
		echo $dir/lock >$tmp/lock
		fail=false
		break
	    else
		if [ ! -f $tmp/stamp ]
		then
		    touch -t `pmdate -30M %Y%m%d%H%M` $tmp/stamp
		fi
		if [ ! -z "`find lock -newer $tmp/stamp -print 2>/dev/null`" ]
		then
		    :
		else
		    echo "$prog: Warning: removing lock file older than 30 minutes"
		    LC_TIME=POSIX ls -l $dir/lock
		    rm -f lock
		fi
	    fi
	    sleep 5
	done

	if $fail
	then
	    # failed to gain mutex lock
	    #
	    if [ -f lock ]
	    then
		echo "$prog: Warning: is another PCP cron job running concurrently?"
		LC_TIME=POSIX ls -l $dir/lock
	    else
		echo "$prog: `cat $tmp/out`"
	    fi
	    _warning "failed to acquire exclusive lock ($dir/lock) ..."
	    continue
	fi
    fi

    pid=''
    if [ X"$primary" = Xy ]
    then
        # NB: FQDN cleanup: previously, we used to quietly accept several
        # putative-aliases in the first (hostname) slot for a primary logger,
        # which were all supposed to refer to the local host.  So now we
        # squash them all to the officially pcp-preferred way to access it.
        host=local:

	if test -f "$PCP_TMP_DIR/pmlogger/primary"
	then
	    $VERY_VERBOSE && $PCP_ECHO_PROG $PCP_ECHO_N "... try $PCP_TMP_DIR/pmlogger/primary: ""$PCP_ECHO_C"
	    primary_inode=`_get_ino $PCP_TMP_DIR/pmlogger/primary`
	    $VERY_VERBOSE && echo primary_inode=$primary_inode
	    for file in $PCP_TMP_DIR/pmlogger/*
	    do
		case "$file"
		in
		    */primary|*\*)
			;;
		    */[0-9]*)
			inode=`_get_ino "$file"`
			$VERY_VERBOSE && echo $file inode=$inode
			if [ "$primary_inode" = "$inode" ]
			then
			    pid="`echo $file | sed -e 's/.*\/\([^/]*\)$/\1/'`"
			    break
			fi
			;;
		esac
	    done
	    if [ -z "$pid" ]
	    then
		if $VERY_VERBOSE
		then
		    echo "primary pmlogger process pid not found"
		    ls -l "$PCP_TMP_DIR/pmlogger"
		fi
	    else
		if _get_pids_by_name pmlogger | grep "^$pid\$" >/dev/null
		then
		    $VERY_VERBOSE && echo "primary pmlogger process $pid identified, OK"
		else
		    $VERY_VERBOSE && echo "primary pmlogger process $pid not running"
		    pid=``
		fi
	    fi
	fi
    else
	for log in $PCP_TMP_DIR/pmlogger/[0-9]*
	do
	    [ "$log" = "$PCP_TMP_DIR/pmlogger/[0-9]*" ] && continue
	    $VERY_VERBOSE && $PCP_ECHO_PROG $PCP_ECHO_N "... try $log: ""$PCP_ECHO_C"
	    match=`sed -e '3s/\/[0-9][0-9][0-9][0-9][0-9.]*$//' $log \
		   | $PCP_AWK_PROG '
BEGIN				{ m = 0 }
NR == 3 && $0 == "'$dir'"	{ m = 2; next }
END				{ print m }'`
	    $VERY_VERBOSE && $PCP_ECHO_PROG $PCP_ECHO_N "match=$match ""$PCP_ECHO_C"
	    if [ "$match" = 2 ]
	    then
		pid=`echo $log | sed -e 's,.*/,,'`
		if _get_pids_by_name pmlogger | grep "^$pid\$" >/dev/null
		then
		    $VERY_VERBOSE && echo "pmlogger process $pid identified, OK"
		    break
		fi
		$VERY_VERBOSE && echo "pmlogger process $pid not running, skip"
		pid=''
	    else
		$VERY_VERBOSE && echo "different directory, skip"
	    fi
	done
    fi

    if [ -z "$pid" ]
    then
	if [ "$PMLOGGER_CTL" = "on" ]
	then
	    _error "no pmlogger instance running for host \"$host\""
	fi
    else
	# now execute pmnewlog to "roll the archive logs"
	#
	[ X"$primary" != Xy ] && args="-p $pid $args"
        # else: -P is already the default
	[ X"$socks" = Xy ] && args="-s $args"
	args="$args -m pmlogger_daily"
	$SHOWME && echo "+ pmnewlog$MYARGS $args $LOGNAME"
	if pmnewlog$MYARGS $args $LOGNAME
	then
	    :
	else
	    _error "problems executing pmnewlog for host \"$host\""
	    touch $tmp/err
	fi
    fi
    $VERBOSE && echo

    # Merge archive logs.
    #
    # Will work for new style YYYYMMDD.HH.MM[-NN] archives and old style
    # YYMMDD.HH.MM[-NN] archives.
    # Note: we need to handle duplicate-breaking forms like
    # YYYYMMDD.HH.MM-seq# (even though pmlogger_merge already picks most
    # of these up) in case the base YYYYMMDD.HH.MM archive is for some
    # reason missing here
    #
    # Assume if .meta file is present then other archive components are
    # also present (if not the case it is a serious process botch, and
    # pmlogger_merge will fail below)
    #
    # Find all candidate input archives, remove any that contain today's
    # date and group the remainder by date.
    #
    TODAY=`date +%Y%m%d`

    find *.meta \
	     \( -name "*.[0-2][0-9].[0-5][0-9].meta" \
		-o -name "*.[0-2][0-9].[0-5][0-9]-[0-9][0-9].meta" \
	     \) \
	     -print 2>/dev/null \
    | sed \
	-e "/^$TODAY\./d" \
	-e 's/\.meta//' \
    | sort -n \
    | $PCP_AWK_PROG '
	{ if (lastdate != "" && match($1, "^" lastdate "\\.") == 1) {
	    # same date as previous one
	    inlist = inlist " " $1
	    next
	  }
	  else {
	    # different date as previous one
	    if (inlist != "") print lastdate,inlist
	    inlist = $1
	    lastdate = $1
	    sub(/\..*/, "", lastdate)
	  }
	}
END	{ if (inlist != "") print lastdate,inlist }' >$tmp/list

    if $OFLAG
    then
	# -o option, preserve the old semantics, and only process the
	# previous day's archives ... aim for a time close to midday
	# yesterday and report that date
	#
	now_hr=`pmdate %H`
	hr=`expr 12 + $now_hr`
	grep "^[0-9]*`pmdate -${hr}H %y%m%d` " $tmp/list >$tmp/tmp
	mv $tmp/tmp $tmp/list
    fi

    # pmlogrewrite if no -r on command line and
    # (a) pmlogrewrite exists in the same directory that the input
    #     archives are found, or
    # (b) if $PCP_VAR_LIB/config/pmlogrewrite exists
    # "exists" => file, directory or symbolic link
    #
    rewrite=''
    if $RFLAG
    then
	:
    else
	for type in -f -d -L
	do
	    if [ $type "$dir/pmlogrewrite" ]
	    then
		rewrite="$dir/pmlogrewrite"
		break
	    fi
	done
	if [ -z "$rewrite" ]
	then
	    for type in -f -d -L
	    do
		if [ $type "$PCP_VAR_DIR/config/pmlogrewrite" ]
		then
		    rewrite="$PCP_VAR_DIR/config/pmlogrewrite"
		    break
		fi
	    done
	fi
    fi

    rm -f $tmp/skip
    if $MFLAG
    then
	# -M don't rewrite, merge or rename
	#
	:
    else
	if [ ! -s $tmp/list ]
	then
	    if $VERBOSE
	    then
		echo "$prog: Warning: no archives found to merge"
		$VERY_VERBOSE && ls -l
	    fi
	else
	    cat $tmp/list \
	    | while read outfile inlist
	    do
		if [ -f $outfile.0 -o -f $outfile.index -o -f $outfile.meta ]
		then
		    echo "$prog: Warning: output archive ($outfile) already exists"
		    echo "[$CONTROL:$line] ... skip log merging, culling and compressing for host \"$host\""
		    touch $tmp/skip
		    break
		else
		    if [ -n "$rewrite" ]
		    then
			$VERY_VERBOSE && echo "Rewriting input archives using $rewrite"
			for arch in $inlist
			do
			    if pmlogrewrite -iq -c "$rewrite" $arch
			    then
				:
			    else
				echo "$prog: Warning: rewrite for $arch using -c $rewrite failed"
				echo "[$CONTROL:$line] ... skip log merging, culling and compressing for host \"$host\""
				touch $tmp/skip
				break
			    fi
			done

		    fi
		    if $VERY_VERBOSE
		    then
			for arch in $inlist
			do
			    echo "Input archive $arch ..."
			    pmdumplog -L $arch
			done
		    fi
		    narch=`echo $inlist | wc -w | sed -e 's/ //g'`
		    if [ "$narch" = 1 ]
		    then
			# optimization ... rename don't merge for one input
			# archive case
			#
			if $SHOWME
			then
			    echo "+ pmlogmv$MYARGS $inlist $outfile"
			else
			    if pmlogmv$MYARGS $inlist $outfile
			    then
				if $VERY_VERBOSE
				then
				    echo "Renamed output archive $outfile ..."
				    pmdumplog -L $outfile
				fi
			    else
				_error "problems executing pmlogmv for host \"$host\""
			    fi
			fi
		    else
			# more than one input archive, merge away
			#
			if $SHOWME
			then
			    echo "+ pmlogger_merge$MYARGS -f $inlist $outfile"
			else
			    if pmlogger_merge$MYARGS -f $inlist $outfile
			    then
				if $VERY_VERBOSE
				then
				    echo "Merged output archive $outfile ..."
				    pmdumplog -L $outfile
				fi
			    else
				_error "problems executing pmlogger_merge for host \"$host\""
			    fi
			fi
		    fi
		fi
	    done
	fi
    fi

    if [ -f $tmp/skip ]
    then
	# this is sufficiently serious that we don't want to remove
	# the lock file, so problems are not compounded the next time
	# the script is run
	$VERY_VERBOSE && echo "Skip culling and compression ..."
	continue
    fi

    # and cull old archives
    #
    if [ X"$CULLAFTER" != X"forever" ]
    then
	if [ "$PCP_PLATFORM" = freebsd ]
	then
	    # FreeBSD semantics for find(1) -mtime +N are "rounded up to
	    # the next full 24-hour period", compared to GNU/Linux semantics
	    # "any fractional part is ignored".  So, these are almost always
	    # off by one day in terms of the files selected.
	    # For consistency, try to match the GNU/Linux semantics by using
	    # one MORE day.
	    #
	    mtime=`expr $CULLAFTER + 1`
	else
	    mtime=$CULLAFTER
	fi
	find . -type f -mtime +$mtime \
	| _filter_filename \
	| sort >$tmp/list
	if [ -s $tmp/list ]
	then
	    if $VERBOSE
	    then
		echo "Archive files older than $CULLAFTER days being removed ..."
		fmt <$tmp/list | sed -e 's/^/    /'
	    fi
	    if $SHOWME
	    then
		cat $tmp/list | xargs echo + rm -f 
	    else
		cat $tmp/list | xargs rm -f
	    fi
	else
	    $VERY_VERBOSE && echo "$prog: Warning: no archive files found to cull"
	fi
    fi

    # and compress old archive data files
    # (after cull - don't compress unnecessarily)
    #
    if [ ! -z "$COMPRESSAFTER" ]
    then
	if [ "$PCP_PLATFORM" = freebsd ]
	then
	    # See note above re. find(1) on FreeBSD
	    #
	    mtime=`expr $COMPRESSAFTER - 1`
	else
	    mtime=$COMPRESSAFTER
	fi
	find . -type f -mtime +$mtime \
	| _filter_filename \
	| egrep -v "$COMPRESSREGEX" \
	| sort >$tmp/list
	if [ -s $tmp/list ]
	then
	    if $VERBOSE
	    then
		echo "Archive files older than $COMPRESSAFTER days being compressed ..."
		fmt <$tmp/list | sed -e 's/^/    /'
	    fi
	    if $SHOWME
	    then
		cat $tmp/list | xargs echo + $COMPRESS
	    else
		cat $tmp/list | xargs $COMPRESS
	    fi
	else
	    $VERY_VERBOSE && echo "$prog: Warning: no archive files found to compress"
	fi
    fi

    # and cull old trace files (from -t option)
    #
    if [ "$TRACE" -gt 0 ]
    then
	if [ "$PCP_PLATFORM" = freebsd ]
	then
	    # See note above re. find(1) on FreeBSD
	    #
	    mtime=`expr $TRACE - 1`
	else
	    mtime=$TRACE
	fi
	find $PCP_LOG_DIR/pmlogger -type f -mtime +$mtime \
	| sed -n -e '/pmlogger\/daily\..*\.trace/p' \
	| sort >$tmp/list
	if [ -s $tmp/list ]
	then
	    if $VERBOSE
	    then
		echo "Trace files older than $TRACE days being removed ..."
		fmt <$tmp/list | sed -e 's/^/    /'
	    fi
	    if $SHOWME
	    then
		cat $tmp/list | xargs echo + rm -f
	    else
		cat $tmp/list | xargs rm -f
	    fi
	else
	    $VERY_VERBOSE && echo "$prog: Warning: no trace files found to cull"
	fi
    fi

    _unlock

done

[ -f $tmp/err ] && status=1
exit
