#! /bin/sh
#Tag 0x00010D13
#
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
# You should have received a copy of the GNU General Public License along
# with this program; if not, write to the Free Software Foundation, Inc.,
# 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
# 
# Contact information: Silicon Graphics, Inc., 1500 Crittenden Lane,
# Mountain View, CA 94043, USA, or: http://www.sgi.com
#
# Example daily administrative script for PCP archive logs
#

# Get standard environment
. /etc/pcp.env

# Get the portable PCP rc script functions
if [ -f $PCP_SHARE_DIR/lib/rc-proc.sh ] ; then
    . $PCP_SHARE_DIR/lib/rc-proc.sh
fi

# error messages should go to stderr, not the GUI notifiers
#
unset PCP_STDERR

# constant setup
#
tmp=/tmp/$$
status=0
echo >$tmp.lock
trap "rm -f \`[ -f $tmp.lock ] && cat $tmp.lock\` $tmp.*; exit \$status" 0 1 2 3 15
prog=`basename $0`

# 24 hours and 10 minutes ago
#
touch -t `pmdate -24H -10M %Y%m%d%H%M` $tmp.merge

# control file for pmlogger administration ... edit the entries in this
# file to reflect your local configuration (see also -c option below)
#
CONTROL=$PCP_VAR_DIR/config/pmlogger/control

# default number of days to keep archive logs
#
CULLAFTER=14

# default compression program
# 
COMPRESS=compress
COMPRESSAFTER=""
COMPRESSREGEX=".meta$|.index$|.Z$|.gz$"

# threshold size to roll $PCP_LOG_DIR/NOTICES
#
NOTICES=$PCP_LOG_DIR/NOTICES
ROLLNOTICES=20480

# mail addresses to send daily NOTICES summary to
# 
MAILME=""
MAIL=Mail
MAILFILE=$PCP_LOG_DIR/NOTICES.daily

# determine real name for localhost
LOCALHOST=`hostname | sed -e 's/\..*//'`
[ -z "$LOCALHOST" ] && LOCALHOST=localhost

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

_usage()
{
    cat - <<EOF
Usage: $prog [options]

Options:
  -c control    pmlogger control file
  -s size       rotate NOTICES file after reaching size bytes
  -k discard    remove archives after "discard" days
  -m addresses  send daily NOTICES entries to email addresses
  -N            show-me mode, no operations performed
  -V            verbose output
  -x compress   compress archive data files after "compress" days
  -X program    use program for archive data file compression
  -Y regex      egrep filter for files to compress ["$COMPRESSREGEX"]
EOF
    status=1
    exit
}

# option parsing
#
SHOWME=false
VERBOSE=false
VERY_VERBOSE=false
MYARGS=""

while getopts c:k:m:Ns:Vx:X:Y:? c
do
    case $c
    in
	c)	CONTROL="$OPTARG"
		;;
	k)	CULLAFTER="$OPTARG"
		check=`echo "$CULLAFTER" | sed -e 's/[0-9]//g'`
		if [ ! -z "$check" -a X"$check" != Xforever ]
		then
		    echo "Error: -k option ($CULLAFTER) must be numeric"
		    status=1
		    exit
		fi
		;;
	m)	MAILME="$OPTARG"
		;;
	N)	SHOWME=true
		MYARGS="$MYARGS -N"
		;;
	s)	ROLLNOTICES="$OPTARG"
		check=`echo "$ROLLNOTICES" | sed -e 's/[0-9]//g'`
		if [ ! -z "$check" ]
		then
		    echo "Error: -s option ($ROLLNOTICES) must be numeric"
		    status=1
		    exit
		fi
		;;
	V)	if $VERBOSE
		then
		    VERY_VERBOSE=true
		else
		    VERBOSE=true
		fi
		MYARGS="$MYARGS -V"
		;;
	x)	COMPRESSAFTER="$OPTARG"
		check=`echo "$COMPRESSAFTER" | sed -e 's/[0-9]//g'`
		if [ ! -z "$check" ]
		then
		    echo "Error: -x option ($COMPRESSAFTER) must be numeric"
		    status=1
		    exit
		fi
		;;
	X)	COMPRESS="$OPTARG"
		;;
	Y)	COMPRESSREGEX="$OPTARG"
		;;
	?)	_usage
		;;
    esac
done
shift `expr $OPTIND - 1`

[ $# -ne 0 ] && _usage

if [ ! -f $CONTROL ]
then
    echo "$prog: Error: cannot find control file ($CONTROL)"
    status=1
    exit
fi

# each new archive log started by pmnewlog or pmlogger_check is named
# yyyymmdd.hh.mm
#
LOGNAME=`date "+%Y%m%d.%H.%M"`

# each summarized log is named yyyymmdd using yesterday's date
# previous day's logs are named yymmdd (old format) or
# yyyymmdd (new year 2000 format)
#
SUMMARY_LOGNAME=`pmdate -1d %Y%m%d`

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
    touch $tmp.err
}

_unlock()
{
    rm -f lock
    echo >$tmp.lock
}

# filter for pmlogger archive files in working directory -
# pass in the number of days to skip over (backwards) from today
#
# pv:821339 too many sed commands for IRIX ... split into groups
#           of at most 200 days
# 
_date_filter()
{
    # start with all files whose names match the patterns used by
    # the PCP archive log management scripts ... this list may be
    # reduced by the sed filtering later on
    #
    # need to handle both the year 2000 and the old name formats
    #
    ls | sed -n >$tmp.in \
	-e '/^[12][0-9][0-9][0-9][0-1][0-9][0-3][0-9][-.]/p' \
	-e '/^[0-9][0-9][0-1][0-9][0-3][0-9][-.]/p'

    i=0
    while [ $i -le $1 ]
    do
	dmax=`expr $i + 200`
	[ $dmax -gt $1 ] && dmax=$1
	echo "/^[12][0-9][0-9][0-9][0-1][0-9][0-3][0-9][-.]/{" >$tmp.sed1
	echo "/^[0-9][0-9][0-1][0-9][0-3][0-9][-.]/{" >$tmp.sed2
	while [ $i -le $dmax ]
	do
	    x=`pmdate -${i}d %Y%m%d`
	    echo "/^$x\./d" >>$tmp.sed1
	    echo "/^$x-[0-9][0-9]\./d" >>$tmp.sed1
	    x=`pmdate -${i}d %y%m%d`
	    echo "/^$x\./d" >>$tmp.sed2
	    echo "/^$x-[0-9][0-9]\./d" >>$tmp.sed2
	    i=`expr $i + 1`
	done
	echo "p" >>$tmp.sed1
	echo "}" >>$tmp.sed1
	echo "p" >>$tmp.sed2
	echo "}" >>$tmp.sed2
	cat $tmp.sed2 >>$tmp.sed1

	# cull file names with matching dates, keep other file names
	#
	sed -n -f $tmp.sed1 <$tmp.in >$tmp.tmp
	mv $tmp.tmp $tmp.in
    done

    cat $tmp.in
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
    rm -f $tmp.pcp
    $PCP_AWK_PROG '
/^Started/	{ print "DATE:",$4,$5,$6,$7,$9; next }
		{ print }
	' $NOTICES | \
    $PCP_AWK_PROG -F ':[ \t]*|[ \t]+' $args '
$1 == "DATE" && $3 == mt && $4 == dt && $8 == yt { tday = 1; print; next }
$1 == "DATE" && $3 == my && $4 == dy && $8 == yy { yday = 1; print; next }
	{ if ( tday || (yday && $1 > Hy) || (yday && $1 == Hy && $2 >= My) )
	    print
	}' >$tmp.pcp

    [ -s $tmp.pcp ] && \
	$MAIL -s "PCP NOTICES summary for $LOCALHOST" $MAILME <$tmp.pcp
    [ -s $tmp.pcp -a -w `dirname $NOTICES` ] && mv $tmp.pcp $MAILFILE
fi


# Roll $PCP_LOG_DIR/NOTICES -> $PCP_LOG_DIR/NOTICES.old if larger
# that 10 Kbytes, and you can write in $PCP_LOG_DIR
#
if [ -s $NOTICES -a -w `dirname $NOTICES` ]
then
    if [ "`wc -c <$NOTICES`" -ge $ROLLNOTICES ]
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
	    echo >>$NOTICES
	    echo "*** rotated by $prog: `date`" >>$NOTICES
	    mv -f $NOTICES $NOTICES.old
	    echo "Started by $prog: `date`" >$NOTICES
	fi
    fi
fi

# note on control file format version
#  1.0 was shipped as part of PCPWEB beta, and did not include the
#	socks field [this is the default for backwards compatibility]
#  1.1 is the first production release, and the version is set in
#	the control file with a $version=1.1 line (see below)
#

rm -f $tmp.err
line=0
version=''
cat $CONTROL \
| sed -e "s/LOCALHOSTNAME/$LOCALHOST/g" \
      -e "s;PCP_LOG_DIR;$PCP_LOG_DIR;g" \
| while read host primary socks dir args
do
    line=`expr $line + 1`
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
	echo "Check pmlogger$flag -h $host ... in $dir ..."
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
	rm -f $tmp.stamp
	for try in 1 2 3 4
	do
	    if pmlock -v lock >$tmp.out
	    then
		echo $dir/lock >$tmp.lock
		fail=false
		break
	    else
		if [ ! -f $tmp.stamp ]
		then
		    touch -t `pmdate -30M %Y%m%d%H%M` $tmp.stamp
		fi
		if [ ! -z "`find lock -newer $tmp.stamp -print 2>/dev/null`" ]
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
		echo "$prog: `cat $tmp.out`"
	    fi
	    _warning "failed to acquire exclusive lock ($dir/lock) ..."
	    continue
	fi
    fi

    pid=''

    if [ X"$primary" = Xy ]
    then
	if [ X"$host" != X"$LOCALHOST" ]
	then
	    _error "\"primary\" only allowed for $LOCALHOST (localhost, not $host)"
	    _unlock
	    continue
	fi

        case "$PCP_PLATFORM" in
        irix)
	    if /sbin/chkconfig pmlogger
	    then
		:
	    else
		_warning "primary logging disabled via chkconfig for $host"
		_unlock
		continue
	    fi

	    test -l /var/tmp/pmlogger/primary
	    ;;

        linux)
	    #
	    # On linux, pmcd and (the primary) pmlogger are both controlled
	    # by the "pcp" chkconfig flag.
	    #
	    PMLOGGER_CTL=off
	    if is_chkconfig_on pcp
	    then
		PMLOGGER_CTL=on
	    fi

	    if [ "$PMLOGGER_CTL" = "off" ]
	    then
		_error "primary logging disabled for $host"
		_unlock
		continue
	    fi

	    test -L $PCP_TMP_DIR/pmlogger/primary

	    ;;

        solaris)
	    PMLOGGER_CTL=off
	    if is_chkconfig_on pcp
	    then
		PMLOGGER_CTL=on
	    fi

	    if [ "$PMLOGGER_CTL" = "off" ]
	    then
		_error "primary logging disabled for $host"
		_unlock
		continue
	    fi

	    test -h $PCP_TMP_DIR/pmlogger/primary

	    ;;
       
	esac

        if [ $? -eq 0 ]
	then
	    $VERY_VERBOSE && $PCP_ECHO_PROG $PCP_ECHO_N "... try $PCP_TMP_DIR/pmlogger/primary: ""$PCP_ECHO_C"
	    pid=`LC_TIME=POSIX ls -l $PCP_TMP_DIR/pmlogger/primary | sed -e 's,.*/,,'`
	    if _get_pids_by_name pmlogger | grep "^$pid\$" >/dev/null
	    then
		$VERY_VERBOSE && echo "pmlogger process $pid identified, OK"
	    else
		$VERY_VERBOSE && echo "pmlogger process $pid not running"
		pid=``
	    fi
	fi
    else
	fqdn=`pmhostname $host`
	for log in $PCP_TMP_DIR/pmlogger/[0-9]*
	do
	    [ "$log" = "[0-9]*" ] && continue
	    $VERY_VERBOSE && $PCP_ECHO_PROG $PCP_ECHO_N "... try $log: ""$PCP_ECHO_C"
	    match=`sed -e '3s/\/[0-9][0-9][0-9][0-9][0-9.]*$//' $log \
                   | $PCP_AWK_PROG '
BEGIN							{ m = 0 }
NR == 2	&& $1 == "'$fqdn'"				{ m = 1; next }
NR == 2	&& "'$fqdn'" == "'$host'" &&
	( $1 ~ /^'$host'\./ || $1 ~ /^'$host'$/ )	{ m = 1; next }
NR == 3 && m == 1 && $0 == "'$dir'"			{ m = 2; next }
END							{ print m }'`
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
	    elif [ "$match" = 0 ]
	    then
		$VERY_VERBOSE && echo "different host, skip"
	    elif [ "$match" = 1 ]
	    then
		$VERY_VERBOSE && echo "different directory, skip"
	    fi
	done
    fi

    if [ -z "$pid" ]
    then
	_error "no pmlogger instance running for host \"$host\""
    else
	if [ "`echo $pid | wc -w`" -gt 1 ]
	then
	    _error "multiple pmlogger instances running for host \"$host\", processes: $pid"
	    _unlock
	    continue
	fi

	# now execute pmnewlog to "roll the archive logs"
	#
	[ X"$primary" != Xy ] && args="-p $pid $args"
	[ X"$socks" = Xy ] && args="-s $args"
	$SHOWME && echo "+ pmnewlog$MYARGS $args $LOGNAME"
	if pmnewlog$MYARGS $args $LOGNAME
	then
	    :
	else
	    _error "problems executing pmnewlog for host \"$host\""
	    touch $tmp.err
	fi
    fi

    # concatenate yesterday's archive logs
    #
    # note: we need to handle duplicate-breaking forms like
    # YYYYMMDD.HH.MM-seq# (even though pmlogger_merge already picks most
    # of these up) in case the base YYYYMMDD.HH.MM archive is for some
    # reason missing here
    #
    $VERBOSE && echo

    WANT_LOG=`find *.meta \
		     \( -name "*.[0-2][0-9].[0-5][0-9].meta" \
		        -o -name "*.[0-2][0-9].[0-5][0-9]-[0-9][0-9].meta" \
		     \) \
		     -newer $tmp.merge \
		     -print \
	      | sed \
		  -e "/$LOGNAME/d" \
		  -e 's/\.meta//' \
		  -e 's/^\.\///'`

    if [ ! -z "$WANT_LOG" ]
    then
	if [ -f $SUMMARY_LOGNAME.0 -o -f $SUMMARY_LOGNAME.index -o -f $SUMMARY_LOGNAME.meta ]
	then
	    echo "$prog: Warning: output archive ($SUMMARY_LOGNAME) already exists"
	    echo "[$CONTROL:$line] ... skip log merging for host \"$host\""
	else
	    if $SHOWME
	    then
		echo "+ pmlogger_merge$MYARGS -f $WANT_LOG $SUMMARY_LOGNAME"
	    else
		if pmlogger_merge$MYARGS -f $WANT_LOG $SUMMARY_LOGNAME
		then
		    :
		else
		    _error "problems executing pmlogger_merge for host \"$host\""
		fi
	    fi
	fi
    fi

    # and cull old archives
    #
    if [ X"$CULLAFTER" != X"forever" ]
    then
	_date_filter $CULLAFTER >$tmp.list
	if [ -s $tmp.list ]
	then
	    if $VERBOSE
	    then
		echo "Archive files older than $CULLAFTER days being removed ..."
		fmt <$tmp.list | sed -e 's/^/    /'
	    fi
	    if $SHOWME
	    then
		cat $tmp.list | xargs echo + rm -f 
	    else
		cat $tmp.list | xargs rm -f
	    fi
	fi
    fi

    # finally, compress old archive data files
    # (after cull - don't compress unnecessarily)
    #
    if [ ! -z "$COMPRESSAFTER" ]
    then

	_date_filter $COMPRESSAFTER | egrep -v "$COMPRESSREGEX" >$tmp.list
	if [ -s $tmp.list ]
	then
	    if $VERBOSE
	    then
		echo "Archive files older than $COMPRESSAFTER days being compressed ..."
		fmt <$tmp.list | sed -e 's/^/    /'
	    fi
	    if $SHOWME
	    then
		cat $tmp.list | xargs echo + $COMPRESS
	    else
		cat $tmp.list | xargs $COMPRESS
	    fi
	fi
    fi

    _unlock

done

[ -f $tmp.err ] && status=1
exit
