#! /bin/sh
#
# Copyright (c) 2013-2016,2018,2020 Red Hat.
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
. $PCP_SHARE_DIR/lib/utilproc.sh

# error messages should go to stderr, not the GUI notifiers
#
unset PCP_STDERR

# constant setup
#
tmp=`mktemp -d "$PCP_TMPFILE_DIR/pmlogger_daily.XXXXXXXXX"` || exit 1
status=0
echo >$tmp/lock
prog=`basename $0`
PROGLOG=$PCP_LOG_DIR/pmlogger/$prog.log
MYPROGLOG=$PROGLOG.$$
USE_SYSLOG=true

# optional begin logging to $PCP_LOG_DIR/NOTICES
#
if $PCP_LOG_RC_SCRIPTS
then
    logmsg="begin pid:$$ $prog args:$*"
    if which pstree >/dev/null 2>&1
    then
	logmsg="$logmsg [`pstree -lps $$`]"
	logmsg="`echo "$logmsg" | sed -e 's/---pstree([^)]*)//'`"
    fi
    $PCP_BINADM_DIR/pmpost "$logmsg"
fi

_cleanup()
{
    if [ -s "$MYPROGLOG" ]
    then
	rm -f "$PROGLOG"
	mv "$MYPROGLOG" "$PROGLOG"
    else
	rm -f "$MYPROGLOG"
    fi
    $USE_SYSLOG && [ $status -ne 0 ] && \
    $PCP_SYSLOG_PROG -p daemon.error "$prog failed - see $PROGLOG"
    if $SHOWME
    then
	: no pid file created
    else
	rm -f "$PCP_RUN_DIR/pmlogger_daily.pid"
    fi
    lockfile=`cat $tmp/lock 2>/dev/null`
    [ -n "$lockfile" ] && rm -f "$lockfile"
    rm -rf $tmp
    $VERY_VERBOSE && echo >&2 "End: `date '+%F %T.%N'`"
}
trap "_cleanup; exit \$status" 0 1 2 3 15

# pretty format a find-filter timespec
#
_timespec()
{
    echo "$1" | sed -e 's/:/ /g' \
    | while read _d _h _m
    do
	case "$_d:$_h:$_m"
	in
	    0::)
		echo "0 days"
		;;
	    0::1)
		echo "1 minute"
		;;
	    0::*)
		echo "$_m minutes"
		;;
	    0:1:)
		echo "1 hour"
		;;
	    0:*:)
		echo "$_h hours"
		;;
	    0:1:1)
		echo "1 hour and 1 minute"
		;;
	    0:1:*)
		echo "1 hour and $_m minutes"
		;;
	    0:*:*)
		echo "$_h hours and $_m minutes"
		;;
	    1::)
		echo "1 day"
		;;
	    1::1)
		echo "1 day and 1 minute"
		;;
	    1::*)
		echo "1 day and $_m minutes"
		;;
	    1:1:)
		echo "1 day and 1 hour"
		;;
	    1:*:)
		echo "1 day and $_h hours"
		;;
	    1:1:1)
		echo "1 day, 1 hour and 1 minute"
		;;
	    1:1:*)
		echo "1 day, 1 hour and $_m minutes"
		;;
	    1:*:1)
		echo "1 day, $_h hours and 1 minute"
		;;
	    1:*:*)
		echo "1 day, $_h hours and $_m minutes"
		;;
	    *::)
		echo "$_d days"
		;;
	    *::1)
		echo "$_d days and 1 minute"
		;;
	    *::*)
		echo "$_d days and $_m minutes"
		;;
	    *:1:)
		echo "$_d days and 1 hour"
		;;
	    *:*:)
		echo "$_d days and $_h hours"
		;;
	    *:1:1)
		echo "$_d days, 1 hour and 1 minute"
		;;
	    *:1:*)
		echo "$_d days, 1 hour and $_m minutes"
		;;
	    *:*:1)
		echo "$_d days, $_h hours and 1 minute"
		;;
	    *:*:*)
		echo "$_d days, $_h hours and $_m minutes"
		;;
	esac
    done
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
END		{ if (len > 0) printf "\n" }'
}

# check callback
# shell command is $1 possibly surrounded by quotes, so unpick
# "executable program" (first word) and if it exists and is executable
# append shell command to $2
#
_add_callback()
{
    case "$1"
    in
	\"*\")
		_shell="`echo "$1" | sed -e 's/^"//' -e 's/"$//'`"
		;;
	\'*\')
		_shell="`echo "$1" | sed -e "s/^'//" -e "s/'$//"`"
		;;
	*)
		_shell="$1"
		;;
    esac
    _prog=`echo "$_shell" | sed -e 's/ .*//'`

    if which "$_prog" >/dev/null 2>&1
    then
	echo "$_shell" >>"$2"
	return 0
    else
	_warning "$_prog: not a executable on PATH=$PATH"
	return 1
    fi
}

if is_chkconfig_on pmlogger
then
    PMLOGGER_CTL=on
else
    PMLOGGER_CTL=off
fi

# control files for pmlogger administration ... edit the entries in this
# file (and optional directory) to reflect your local configuration; see
# also -c option below.
#
CONTROL=$PCP_PMLOGGERCONTROL_PATH
CONTROLDIR=$PCP_PMLOGGERCONTROL_PATH.d

# default number of days to keep archive logs
#
CULLAFTER=14

# default compression program and days until starting compression and
# filename suffix pattern for file files to NOT compress
# 
COMPRESS=""
COMPRESS_CMDLINE=""
if which xz >/dev/null 2>&1
then
    if xz -0 --block-size=10MiB </dev/null >/dev/null 2>&1
    then
	# want minimal overheads, -0 is the same as --fast
	COMPRESS_DEFAULT="xz -0 --block-size=10MiB"
    else
	COMPRESS_DEFAULT=xz
    fi
else
    # overridden by $PCP_COMPRESS or if not set, no compression
    COMPRESS_DEFAULT=""
fi
COMPRESSAFTER_CMDLINE=""
eval `pmconfig -L -s transparent_decompress`
if $transparent_decompress
then
    COMPRESSAFTER_DEFAULT=0
else
    COMPRESSAFTER_DEFAULT="never"
fi
if [ -n "$PCP_COMPRESSAFTER" ]
then
    $PCP_BINADM_DIR/find-filter </dev/null >/dev/null 2>&1 mtime "+$PCP_COMPRESSAFTER"
    if [ $? != 0 -a X"$PCP_COMPRESSAFTER" != Xforever -a X"$PCP_COMPRESSAFTER" != Xnever ]
    then
	echo "Error: \$PCP_COMPRESSAFTER value ($PCP_COMPRESSAFTER) must be number, time, \"forever\" or \"never\""
	status=1
	exit
    fi
fi
COMPRESSREGEX=""
COMPRESSREGEX_CMDLINE=""
COMPRESSREGEX_DEFAULT="\.(index|Z|gz|bz2|zip|xz|lzma|lzo|lz4)$"

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

# determine path for pwd command to override shell built-in
PWDCMND=`which pwd 2>/dev/null | $PCP_AWK_PROG '
BEGIN	    	{ i = 0 }
/ not in /  	{ i = 1 }
/ aliased to /  { i = 1 }
 	    	{ if ( i == 0 ) print }
'`
[ -z "$PWDCMND" ] && PWDCMND=/bin/pwd
eval $PWDCMND -P >/dev/null 2>&1
[ $? -eq 0 ] && PWDCMND="$PWDCMND -P"
here=`$PWDCMND`

echo > $tmp/usage
cat >> $tmp/usage <<EOF
Options:
  -c=FILE,--control=FILE  pmlogger control file
  -D,--noreport           do not run pmlogger_daily_report
  -E,--expunge            expunge metrics with metadata inconsistencies when merging archives
  -f,--force              force actions (intended for QA, not production)
  -k=TIME,--discard=TIME  remove archives after TIME (format DD[:HH[:MM]])
  -K                      compress, but no other changes
  -l=FILE,--logfile=FILE  send important diagnostic messages to FILE
  -m=ADDRs,--mail=ADDRs   send daily NOTICES entries to email addresses
  -M		          do not rewrite, merge or rename archives
  -N,--showme             perform a dry run, showing what would be done
  -o                      merge yesterdays logs only (old form, default is all) 
  -p                      poll and exit if processing already done for today
  -r,--norewrite          do not process archives with pmlogrewrite(1)
  -R,--rewriteall         check and rewrite all archives
  -s=SIZE,--rotate=SIZE   rotate NOTICES file after reaching SIZE bytes
  -t=WANT                 implies -VV, keep verbose output trace for WANT days
  -V,--verbose            verbose output (multiple times for very verbose)
  -x=TIME,--compress-after=TIME  compress archive data files after TIME (format DD[:HH[:MM]])
  -X=PROGRAM,--compressor=PROGRAM  use PROGRAM for archive data file compression
  -Y=REGEX,--regex=REGEX  egrep filter when compressing files ["$COMPRESSREGEX_DEFAULT"]
  -Z                      QA mode, force pmlogger re-exec
  -z                      QA mode, do not re-exec pmlogger
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
COMPRESSONLY=false
OFLAG=false
PFLAG=false
REEXEC_MODE=normal
TRACE=0
RFLAG=false
REWRITEALL=false
MFLAG=false
EXPUNGE=""
FORCE=false
KILL=pmsignal
DO_DAILY_REPORT=true

ARGS=`pmgetopt --progname=$prog --config=$tmp/usage -- "$@"`
[ $? != 0 ] && exit 1

eval set -- "$ARGS"
while [ $# -gt 0 ]
do
    case "$1"
    in
	-c)	CONTROL="$2"
		CONTROLDIR="$2.d"
		shift
		;;
	-D)	DO_DAILY_REPORT=false
		;;
	-E)	EXPUNGE="-E"
		;;
	-f)	FORCE=true
		;;
	-k)	CULLAFTER="$2"
		shift
		$PCP_BINADM_DIR/find-filter </dev/null >/dev/null 2>&1 mtime "+$CULLAFTER"
		if [ $? != 0 -a X"$CULLAFTER" != Xforever -a X"$CULLAFTER" != Xnever ]
		then
		    echo "Error: -k value ($CULLAFTER) must be number, time, \"forever\" or \"never\""
		    status=1
		    exit
		fi
		;;
	-K)	if $PFLAG
		then
		    echo "Error: -p and -K are mutually exclusive"
		    status=1
		    exit
		fi
		COMPRESSONLY=true
		PROGLOG=$PCP_LOG_DIR/pmlogger/$prog-K.log
		MYPROGLOG=$PROGLOG.$$
		;;
	-l)	PROGLOG="$2"
		MYPROGLOG=$PROGLOG.$$
		USE_SYSLOG=false
		shift
		;;
	-m)	MAILME="$2"
		shift
		;;
	-M)	if $REWRITEALL
		then
		    echo "Error: -R and -M are mutually exclusive"
		    status=1
		    exit
		fi
		MFLAG=true
		RFLAG=true
  		;;
	-N)	SHOWME=true
		USE_SYSLOG=false
		MYARGS="$MYARGS -N"
		;;
	-o)	OFLAG=true
		;;
	-p)     if $COMPRESSONLY
		then
		    echo "Error: -K and -p are mutually exclusive"
		    status=1
		    exit
		fi
		PFLAG=true
		;;
	-r)	if $REWRITEALL
		then
		    echo "Error: -R and -r are mutually exclusive"
		    status=1
		    exit
		fi
		RFLAG=true
		;;
	-R)	if $RFLAG
		then
		    echo "Error: -r and -R are mutually exclusive"
		    status=1
		    exit
		fi
		REWRITEALL=true
		;;
	-s)	ROLLNOTICES="$2"
		shift
		check=`echo "$ROLLNOTICES" | sed -e 's/[0-9]//g'`
		if [ ! -z "$check" ]
		then
		    echo "Error: -s value ($ROLLNOTICES) must be numeric"
		    status=1
		    exit
		fi
		;;
	-t)	TRACE="$2"
		shift
		# send all stdout and stderr output (after argument parsing) to
		# $PCP_LOG_DIR/pmlogger/daily.<date>.trace
		#
		PROGLOG=$PCP_LOG_DIR/pmlogger/daily.`date "+%Y%m%d.%H.%M"`.trace
		MYPROGLOG=$PROGLOG.$$
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
	-x)	COMPRESSAFTER_CMDLINE="$2"
		shift
		if [ -n "$PCP_COMPRESSAFTER" -a "$PCP_COMPRESSAFTER" != "$COMPRESSAFTER_CMDLINE" ]
		then
		    echo "Warning: -x value ($COMPRESSAFTER_CMDLINE) ignored because \$PCP_COMPRESSAFTER ($PCP_COMPRESSAFTER) set in environment"
		    COMPRESSAFTER_CMDLINE=""
		    continue
		fi
		$PCP_BINADM_DIR/find-filter </dev/null >/dev/null 2>&1 mtime "+$COMPRESSAFTER_CMDLINE"
		if [ $? != 0 -a X"$COMPRESSAFTER_CMDLINE" != Xforever -a X"$COMPRESSAFTER_CMDLINE" != Xnever ]
		then
		    echo "Error: -x value ($COMPRESSAFTER_CMDLINE) must be number, time, \"forever\" or \"never\""
		    status=1
		    exit
		fi
		;;
	-X)	COMPRESS_CMDLINE="$2"
		shift
		if [ -n "$PCP_COMPRESS" -a "$PCP_COMPRESS" != "$COMPRESS_CMDLINE" ]
		then
		    echo "Warning: -X value ($COMPRESS_CMDLINE) ignored because \$PCP_COMPRESS ($PCP_COMPRESS) set in environment"
		    COMPRESS_CMDLINE=""
		    continue
		fi
		;;
	-Y)	COMPRESSREGEX_CMDLINE="$2"
		shift
		if [ -n "$PCP_COMPRESSREGEX" -a "$PCP_COMPRESSREGEX" != "$COMPRESSREGEX_CMDLINE" ]
		then
		    echo "Warning: -Y value ($COMPRESSREGEX_CMDLINE) ignored because \$PCP_COMPRESSREGEX ($PCP_COMPRESSREGEX) set in environment"
		    COMPRESSREGEX_CMDLINE=""
		    continue
		fi
		;;
	-Z)	REEXEC_MODE=force
		;;
	-z)	REEXEC_MODE=skip
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

if $SHOWME
then
    :
else
    # Salt away previous log, if any ...
    #
    _save_prev_file "$PROGLOG"
    # After argument checking, everything must be logged to ensure no mail is
    # accidentally sent from cron.  Close stdout and stderr, then open stdout
    # as our logfile and redirect stderr there too.  Create the log file with
    # correct ownership first.
    #
    # Exception ($SHOWME, above) is for -N where we want to see the output.
    #
    touch "$MYPROGLOG"
    chown $PCP_USER:$PCP_GROUP "$MYPROGLOG" >/dev/null 2>&1
    exec 1>"$MYPROGLOG" 2>&1
fi

if $COMPRESSONLY
then
    # no merge callbacks or autosave with -K
    :
else
    # merge callback initialization ...
    # pmlogger_daily_report goes first, then
    # values (script names) set in the environment, then
    # (later) any values (script names) set in the control files
    #
    touch $tmp/merge_callback
    if $DO_DAILY_REPORT && [ -x "$PCP_BINADM_DIR/pmlogger_daily_report" ]
    then
	# pmlogger_daily_report script is present, need to see if it
	# is enabled ... in systemd-land that's easy, otherwise we
	# defer to the existence of the $PCP_SA_DIR directory
	#
	rm -f $tmp/want_daily_report
	if which systemctl >/dev/null 2>&1
	then
	    # if pmlogger_daily_report.timer is enabled, assume we're good
	    # to go ...
	    #
	    if [ "`systemctl is-enabled pmlogger_daily_report.timer 2>/dev/null`" = enabled ]
	    then
		touch $tmp/want_daily_report
	    fi
	else
	    if [ -d "$PCP_SA_DIR" ]
	    then
		touch $tmp/want_daily_report
	    fi
	fi
	if [ -f $tmp/want_daily_report ]
	then
	    # Note: actual achive name will follow -a when callback
	    # happens
	    #
	    if _add_callback "$PCP_BINADM_DIR/pmlogger_daily_report -o $PCP_SA_DIR -a" $tmp/merge_callback
	    then
		$VERBOSE && echo "Add merge callback for: pmlogger_daily_report"
	    fi
	fi
    fi
    if [ -n "$PCP_MERGE_CALLBACK" ]
    then
	if _add_callback "$PCP_MERGE_CALLBACK" $tmp/merge_callback
	then
	    $VERBOSE && echo "Add merge callback from environment: $PCP_MERGE_CALLBACK"
	fi
    fi

    # autosave initialization ...
    #
    touch $tmp/autosave
    touch $tmp/savefiles
    if [ -n "$PCP_AUTOSAVE_DIR" ]
    then
	echo "$PCP_AUTOSAVE_DIR" >$tmp/autosave
	$VERBOSE && echo "Using \$PCP_AUTOSAVE_DIR from environment: $PCP_AUTOSAVE_DIR"
    fi
fi

# compress callback initialization ...
# values (script names) set in the environment first, then
# (later) any values (script names) set in the control files
#
touch $tmp/compress_callback
if [ -n "$PCP_COMPRESS_CALLBACK" ]
then
    if _add_callback "$PCP_COMPRESS_CALLBACK" $tmp/compress_callback
    then
	$VERBOSE && echo "Add compress callback from environment: $PCP_COMPRESS_CALLBACK"
    fi
fi

if $PFLAG
then
    rm -f $tmp/ok
    if [ -f $PCP_LOG_DIR/pmlogger/pmlogger_daily.stamp ]
    then
	last_stamp=`sed -e '/^#/d' <$PCP_LOG_DIR/pmlogger/pmlogger_daily.stamp`
	if [ -n "$last_stamp" ]
	then
	    # Polling happens every 60 mins, so if pmlogger_daily was last
	    # run more than 23.5 hours ago, we need to do it again, otherwise
	    # exit quietly
	    #
	    now_stamp=`pmdate %s`
	    check=`expr $now_stamp - \( 23 \* 3600 \) - 1800`
	    if [ "$last_stamp" -ge "$check" ]
	    then
		$SHOWME && echo "-p stamp $last_stamp now $now_stamp check $check do nothing"
		exit
	    fi
	    $SHOWME && echo "-p stamp $last_stamp now $now_stamp check $check do work"
	    touch $tmp/ok
	fi
    fi
    if [ ! -f $tmp/ok ]
    then
	# special start up logic when pmlogger_daily.stamp does not exist
	# ... by convention, archive files are stored in per-host directories
	# below $PCP_ARCHIVE_DIR
	#
	find "$PCP_ARCHIVE_DIR" -name "`pmdate -1d %Y%m%d`.index" >$tmp/tmp
	if [ -s $tmp/tmp ]
	then
	    $SHOWME && echo "-p start up already run heuristic match, do nothing"
	    exit
	fi
	$SHOWME && echo "-p start up do work"
    fi
fi

# write date-and-timestamp to be checked by -p polling
#
if $SHOWME
then
    echo "+ date-and-timestamp `pmdate '%Y-%m-%d %H:%M:%S %s'`"
elif $COMPRESSONLY
then
    # no date-and-timestamp update with -K
    :
else
    # doing the whole shootin' match ...
    #
    if _save_prev_file $PCP_LOG_DIR/pmlogger/pmlogger_daily.stamp
    then
	:
    else
	echo "Warning: cannot save previous date-and-timestamp"
    fi
    # only update date-and-timestamp if we can write the file
    #
    pmdate '# %Y-%m-%d %H:%M:%S
%s' >$tmp/stamp
    if cp $tmp/stamp $PCP_LOG_DIR/pmlogger/pmlogger_daily.stamp
    then
	:
    else
	echo "Warning: cannot install new date-and-timestamp"
    fi
fi

if $VERY_VERBOSE
then
    echo >&2 "Start: `date '+%F %T.%N'`"
    if which pstree >/dev/null 2>&1
    then
	if pstree -spa $$ >$tmp/tmp 2>&1
	then
	    echo >&2 "Called from:"
	    cat >&2 $tmp/tmp
	else
	    # pstree not functional for us ... -s not supported in older
	    # versions
	    :
	fi
    fi
fi

# if SaveLogs exists in the $PCP_LOG_DIR/pmlogger directory and is writeable
# then save $MYPROGLOG there as well with a unique name that contains the date
# and time when we're run ... skip if -N (showme)
#
if [ -d $PCP_LOG_DIR/pmlogger/SaveLogs -a -w $PCP_LOG_DIR/pmlogger/SaveLogs ]
then
    now="`date '+%Y%m%d.%H:%M:%S.%N'`"
    link=`echo $MYPROGLOG | sed -e "s@.*$prog@$PCP_LOG_DIR/pmlogger/SaveLogs/$now-$prog@"`
    if [ ! -f "$link" ]
    then
	if $SHOWME
	then
	    echo "+ ln $MYPROGLOG $link"
	else
	    ln $MYPROGLOG $link
	    if [ -w $link ]
	    then
		echo "--- Added by $prog when SaveLogs dir found ---" >>$link
		echo "Start: `date '+%F %T.%N'`" >>$link
		echo "Args: $ARGS" >>$link
		if which pstree >/dev/null 2>&1
		then
		    if pstree -spa $$ >$tmp/tmp 2>&1
		    then
			echo "Called from:" >>$link
			cat $tmp/tmp >>$link
		    else
			# pstree not functional for us ... -s not supported
			# in older versions
			:
		    fi
		fi
	    fi
	fi
    fi
fi

if [ ! -f "$CONTROL" ]
then
    echo "$prog: Error: cannot find control file ($CONTROL)"
    status=1
    exit
fi

_error()
{
    echo "$prog: [$filename:$line]"
    echo "Error: $@"
    echo "... logging for host \"$host\" unchanged"
    touch $tmp/err
}

_warning()
{
    echo "$prog: [$filename:$line]"
    echo "Warning: $@"
}

_skipping()
{
    echo "$prog: Warning: $@"
    echo "[$filename:$line] ... skip log merging and compressing for host \"$host\""
    touch $tmp/skip
}

_lock()
{
    if [ ! -w $1 ]
    then
	_warning "no write access in $1 skip lock file processing"
    else
	# demand mutual exclusion
	#
	rm -f $tmp/stamp $tmp/out
	delay=200	# tenths of a second
	while [ $delay -gt 0 ]
	do
	    if pmlock -v "$1/lock" >>$tmp/out 2>&1
	    then
		echo "$1/lock" >$tmp/lock
		break
	    else
		[ -f $tmp/stamp ] || touch -t `pmdate -30M %Y%m%d%H%M` $tmp/stamp
		find $tmp/stamp -newer "$1/lock" -print 2>/dev/null >$tmp/tmp
		if [ -s $tmp/tmp ]
		then
		    if [ -f "$1/lock" ]
		    then
			_warning "removing lock file older than 30 minutes"
			LC_TIME=POSIX ls -l "$1/lock"
			rm -f "$1/lock"
		    else
			# there is a small timing window here where pmlock
			# might fail, but the lock file has been removed by
			# the time we get here, so just keep trying
			#
			:
		    fi
		fi
	    fi
	    pmsleep 0.1
	    delay=`expr $delay - 1`
	done

	if [ $delay -eq 0 ]
	then
	    # failed to gain mutex lock
	    #
	    if [ -f "$1/lock" ]
	    then
		_warning "is another PCP cron job running concurrently?"
		LC_TIME=POSIX ls -l "$1/lock"
	    else
		echo "$prog: `cat $tmp/out`"
	    fi
	    _error "failed to acquire exclusive lock ($1/lock) ..."
	    return 1
	fi
    fi

    return 0
}

_unlock()
{
    rm -f "$1/lock"
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

_get_primary_logger_pid()
{
    pidfile="$PCP_TMP_DIR/pmlogger/primary"
    if [ ! -L "$pidfile" ]
    then
	pid=''
    elif which realpath >/dev/null 2>&1
    then
	pri=`readlink $pidfile`
	pid=`basename "$pri"`
    else
	pri=`ls -l "$pidfile" | sed -e 's/.*-> //'`
	pid=`basename "$pri"`
    fi
    echo "$pid"
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
	    # when run from cron, this will still likely end up as an email
	    echo "$prog: Warning: cannot find a mail agent to send mail ..."
	    echo "PCP NOTICES summary for `hostname`"
	    cat $tmp/pcp
	fi
        [ -w `dirname "$NOTICES"` ] && mv -f $tmp/pcp "$MAILFILE"
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
	    chown $PCP_USER:$PCP_GROUP "$NOTICES" >/dev/null 2>&1
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
# Skip all of this if -N (showme)
#
if $SHOWME
then
    :
else
    if [ ! -d "$PCP_RUN_DIR" ]
    then
	mkdir -p -m 775 "$PCP_RUN_DIR" 2>/dev/null
	# might be running from cron as unprivileged user
	[ $? -ne 0 -a "$PMLOGGER_CTL" = "off" ] && exit 0
	chown $PCP_USER:$PCP_GROUP "$PCP_RUN_DIR" >/dev/null 2>&1
	if which restorecon >/dev/null 2>&1
	then
	    restorecon -r "$PCP_RUN_DIR"
	fi
    fi
    echo $$ >"$PCP_RUN_DIR/pmlogger_daily.pid"
fi

# note on control file format version
#  1.0 was shipped as part of PCPWEB beta, and did not include the
#	socks field [this is the default for backwards compatibility]
#  1.1 is the first production release, and the version is set in
#	the control file with a $version=1.1 line (see below)
#
version=''

# if this file exists at the end, we encountered a serious error
#
rm -f $tmp/err

# root around in PCP_TMP_DIR/pmlogger looking for files like:
# 8193:
#	4330
#	bozo
#	/var/log/pcp/pmlogger/bozo/20180313.08.53
#	pmlogger_daily
# if the directory containing the archive matches, then the name
# of the file is the pid.
#
# The pid(s) (if any) appear on stdout, so be careful to send any
# diagnostics to stderr.
#
_get_non_primary_logger_pid()
{
    pid=''
    for log in $PCP_TMP_DIR/pmlogger/[0-9]*
    do
	[ "$log" = "$PCP_TMP_DIR/pmlogger/[0-9]*" ] && continue
	if $VERY_VERBOSE
	then
	    _host=`sed -n 2p <$log`
	    _arch=`sed -n 3p <$log`
	    $PCP_ECHO_PROG >&2 $PCP_ECHO_N "... try $log host=$_host arch=$_arch: ""$PCP_ECHO_C"
	fi
	# throw away stderr in case $log has been removed by now
	match=`sed -e '3s@/[^/]*$@@' $log 2>/dev/null | \
	$PCP_AWK_PROG '
BEGIN				{ m = 0 }
NR == 3 && $0 == "'$dir'"	{ m = 2; next }
END				{ print m }'`
	$VERY_VERBOSE && $PCP_ECHO_PROG >&2 $PCP_ECHO_N "match=$match ""$PCP_ECHO_C"
	if [ "$match" = 2 ]
	then
	    pid=`echo $log | sed -e 's,.*/,,'`
	    if _get_pids_by_name pmlogger | grep "^$pid\$" >/dev/null
	    then
		$VERY_VERBOSE && echo >&2 "pmlogger process $pid identified, OK"
		break
	    fi
	    $VERY_VERBOSE && echo >&2 "pmlogger process $pid not running, skip"
	    pid=''
	else
	    $VERY_VERBOSE && echo >&2 "different directory, skip"
	fi
    done
    echo "$pid"
}

_parse_control()
{
    controlfile="$1"
    line=0

    # strip leading directories from pathname to get useful filename
    #
    dirname=`dirname $PCP_PMLOGGERCONTROL_PATH`
    filename=`echo "$controlfile" | sed -e "s@$dirname/@@"`

    if echo "$controlfile" | grep -q -e '\.rpmsave$' -e '\.rpmnew$' -e '\.rpmorig$' -e '\.dpkg-dist$' -e '\.dpkg-old$' -e '\.dpkg-new$'
    then
	echo "Warning: ignoring backup control file \"$controlfile\""
	return
    fi

    sed \
	-e "s;PCP_ARCHIVE_DIR;$PCP_ARCHIVE_DIR;g" \
	-e "s;PCP_LOG_DIR;$PCP_LOG_DIR;g" \
	$controlfile | \
    while read host primary socks dir args
    do
	# start in one place for each iteration (beware of relative paths)
	cd "$here"
	line=`expr $line + 1`

	if $VERY_VERBOSE
	then
	    case "$host"
	    in
		\#!#*)	# stopped by pmlogctl ... we'll be checking this one
			echo >&2 "[$filename:$line] host=\"$host\" primary=\"$primary\" socks=\"$socks\" dir=\"$dir\" args=\"$args\""
			;;
		\#*|'')	# comment or empty
			;;
		*)
			echo >&2 "[$filename:$line] host=\"$host\" primary=\"$primary\" socks=\"$socks\" dir=\"$dir\" args=\"$args\""
			;;
	    esac
	fi

	case "$host"
	in
	    \#!#*)	# stopped by pmlogctl ... need to check this one
		host=`echo "$host" | sed -e 's/^#!#//'`
		;;
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
		    rm -f $tmp/cmd
		    case "$cmd"
		    in
			'export PATH;'*)
			    _warning "cannot change \$PATH, line ignored"
			    ;;

			'export IFS;'*)
			    _warning "cannot change \$IFS, line ignored"
			    ;;

			'export PCP_COMPRESS;'*)
			    old_value="$PCP_COMPRESS"
			    $SHOWME && echo "+ $cmd"
			    echo eval $cmd >>$tmp/cmd
			    eval $cmd
			    if [ -n "$old_value" ]
			    then
				_warning "\$PCP_COMPRESS ($PCP_COMPRESS) reset from control file, previous value ($old_value) ignored"
			    fi
			    if [ -n "$PCP_COMPRESS" -a -n "$COMPRESS_CMDLINE" -a "$PCP_COMPRESS" != "$COMPRESS_CMDLINE" ]
			    then
				_warning "\$PCP_COMPRESS ($PCP_COMPRESS) reset from control file, -X value ($COMPRESS_CMDLINE) ignored"
				COMPRESS_CMDLINE=""
			    fi
			    ;;

			'export PCP_COMPRESSAFTER;'*)
			    old_value="$PCP_COMPRESSAFTER"
			    check=`echo "$cmd" | sed -e 's/.*=//' -e 's/  *$//'`
			    $PCP_BINADM_DIR/find-filter </dev/null >/dev/null 2>&1 mtime "+$check"
			    if [ $? != 0 -a -n "$check" -a X"$check" != Xforever -a X"$check" != Xnever ]
			    then
				_error "\$PCP_COMPRESSAFTER value ($check) must be number, time, \"forever\" or \"never\""
			    else
				$SHOWME && echo "+ $cmd"
				echo eval $cmd >>$tmp/cmd
				eval $cmd
				if [ -n "$old_value" ]
				then
				    _warning "\$PCP_COMPRESSAFTER ($PCP_COMPRESSAFTER) reset from control file, previous value ($old_value) ignored"
				fi
				if [ -n "$PCP_COMPRESSAFTER" -a -n "$COMPRESSAFTER_CMDLINE" -a "$PCP_COMPRESSAFTER" != "$COMPRESSAFTER_CMDLINE" ]
				then
				    _warning "\$PCP_COMPRESSAFTER ($PCP_COMPRESSAFTER) reset from control file, -x value ($COMPRESSAFTER_CMDLINE) ignored"
				    COMPRESSAFTER_CMDLINE=""
				fi
			    fi
			    ;;

			'export PCP_COMPRESSREGEX;'*)
			    old_value="$PCP_COMPRESSREGEX"
			    $SHOWME && echo "+ $cmd"
			    echo eval $cmd >>$tmp/cmd
			    eval $cmd
			    if [ -n "$old_value" ]
			    then
				_warning "\$PCP_COMPRESSREGEX ($PCP_COMPRESSREGEX) reset from control file, previous value ($old_value) ignored"
			    fi
			    if [ -n "$PCP_COMPRESSREGEX" -a -n "$COMPRESSREGEX_CMDLINE" -a "$PCP_COMPRESSREGEX" != "$COMPRESSREGEX_CMDLINE" ]
			    then
				_warning "\$PCP_COMPRESSREGEX ($PCP_COMPRESSREGEX) reset from control file, -Y value ($COMPRESSREGEX_CMDLINE) ignored"
				COMPRESSREGEX_CMDLINE=""
			    fi
			    ;;

			'export PCP_MERGE_CALLBACK;'*)
			    if ! $COMPRESSONLY
			    then
				$SHOWME && echo "+ $cmd"
				script="`echo "$cmd" | sed -e 's/.*BACK; PCP_MERGE_CALLBACK=//'`"
				if _add_callback "$script" $tmp/merge_callback
				then
				    $VERBOSE && echo "Add merge callback: $script"
				fi
			    fi
			    ;;

			'export PCP_COMPRESS_CALLBACK;'*)
			    $SHOWME && echo "+ $cmd"
			    script="`echo "$cmd" | sed -e 's/.*BACK; PCP_COMPRESS_CALLBACK=//'`"
			    if _add_callback "$script" $tmp/compress_callback
			    then
				$VERBOSE && echo "Add compress callback: $script"
			    fi
			    ;;

			'export PCP_AUTOSAVE_DIR;'*)
			    if ! $COMPRESSONLY
			    then
				$SHOWME && echo "+ $cmd"
				eval $cmd
				if [ -s $tmp/autosave ]
				then
				    _warning "\$PCP_AUTOSAVE_DIR ($PCP_AUTOSAVE_DIR) reset from control file, previous value (`cat $tmp/autosave`) ignored"
				fi
				echo "$PCP_AUTOSAVE_DIR" >$tmp/autosave
				$VERBOSE && echo "Using \$PCP_AUTOSAVE_DIR: $PCP_AUTOSAVE_DIR"
			    fi
			    ;;

			*)
			    $SHOWME && echo "+ $cmd"
			    echo eval $cmd >>$tmp/cmd
			    eval $cmd
			    ;;
		    esac
		fi
		continue
		;;
	esac

	# set the version and other global variables
	#
	[ -f $tmp/cmd ] && . $tmp/cmd

	if [ -z "$version" -o "$version" = "1.0" ]
	then
	    if [ -z "$version" ]
	    then
		_warning "processing default version 1.0 control format"
		version=1.0
	    fi
	    args="$dir $args"
	    dir="$socks"
	    socks=n
	fi

	# do shell expansion of $dir if needed
	#
	_do_dir_and_args

	if [ -z "$primary" -o -z "$socks" -o -z "$dir" -o -z "$args" ]
	then
	    _error "insufficient fields in control file record"
	    continue
	fi

	# substitute LOCALHOSTNAME marker in this config line
	# (differently for directory and pcp -h HOST arguments)
	#
	dirhostname=`hostname || echo localhost`
	dir=`echo $dir | sed -e "s;LOCALHOSTNAME;$dirhostname;"`
	[ $primary = y -o "x$host" = xLOCALHOSTNAME ] && host=local:

	if $VERBOSE
	then
	    echo
	    if $COMPRESSONLY
	    then
		echo "=== compressing PCP archives for host $host ==="
	    else
		echo "=== daily maintenance of PCP archives for host $host ==="
	    fi
	    echo
	fi

	if $VERY_VERBOSE
	then
	    pflag=''
	    [ $primary = y ] && pflag=' -P'
	    echo >&2 "Check pmlogger$pflag -h $host ... in $dir ..."
	fi

	# make sure output directory hierarchy exists and $PCP_USER
	# user can write there
	#
	if [ ! -d "$dir" ]
	then
	    mkdir_and_chown "$dir" 755 $PCP_USER:$PCP_GROUP >$tmp/tmp 2>&1
	    if [ ! -d "$dir" ]
	    then
		cat $tmp/tmp
		_error "cannot create directory ($dir) for PCP archive files"
		continue
	    else
		_warning "creating directory ($dir) for PCP archive files"
	    fi
	fi

	cd $dir
	dir=`$PWDCMND`
	$SHOWME && echo "+ cd $dir"

	if $SHOWME
	then
	    echo "+ get mutex lock"
	else
	    if _lock "$dir"
	    then
		:
	    else
		# fatal error, reported in _lock()
		#
		continue
	    fi
	fi

	# For archive rewriting (to make metadata consistent across
	# archives) find the rules as follows:
	# - if pmlogrewrite exists (as a file, directory or symlink)
	#   in the current archive directory use that
	# - else use $PCP_VAR_DIR/config/pmlogrewrite
	#
	rewrite=''
	for type in -f -d -L
	do
	    if [ $type "./pmlogrewrite" ]
	    then
		rewrite="$rewrite -c `pwd`/pmlogrewrite"
		break
	    fi
	done
	[ -z "$rewrite" ] && rewrite='-c $PCP_VAR_DIR/config/pmlogrewrite'

	if $REWRITEALL
	then
	    # Do the pmlogrewrite -qi thing (using pmlogger_rewrite) for
	    # all archives in this directory
	    #
	    rewrite_args="$rewrite"
	    if $VERBOSE
	    then
		echo "Info: pmlogrewrite all archives in $dir"
		rewrite_args="$rewrite_args -V"
	    fi
	    $VERY_VERBOSE && rewrite_args="$rewrite_args -V"
	    if $SHOWME
	    then
		echo "+ $PCP_BINADM_DIR/pmlogger_rewrite $rewrite_args $dir"
	    else
		if eval $PCP_BINADM_DIR/pmlogger_rewrite $rewrite_args $dir
		then
		    :
		else
		    _error "pmlogger_rewrite failed in $dir"
		fi
	    fi
	fi

	pid=''
	if [ X"$primary" = Xy ]
	then
	    if test -e "$PCP_TMP_DIR/pmlogger/primary"
	    then
		_host=`sed -n 2p <"$PCP_TMP_DIR/pmlogger/primary"`
		_arch=`sed -n 3p <"$PCP_TMP_DIR/pmlogger/primary"`
		$VERY_VERBOSE && echo >&2 "... try $PCP_TMP_DIR/pmlogger/primary: host=$_host arch=$_arch"
		pid=`_get_primary_logger_pid`
	    fi
	    if [ -z "$pid" ]
	    then
		if $VERY_VERBOSE
		then
		    echo >&2 "primary pmlogger process PID not found"
		    ls >&2 -l "$PCP_TMP_DIR/pmlogger"
		    $PCP_PS_PROG $PCP_PS_ALL_FLAGS | egrep >&2 '[P]ID|[p]mlogger'
		fi
	    elif _get_pids_by_name pmlogger | grep "^$pid\$" >/dev/null
	    then
		$VERY_VERBOSE && echo >&2 "primary pmlogger process $pid identified, OK"
	    else
		$VERY_VERBOSE && echo >&2 "primary pmlogger process $pid not running"
		pid=''
	    fi
	else
	    # pid(s) on stdout, diagnostics on stderr
	    #
	    pid=`_get_non_primary_logger_pid`
	    if $VERY_VERBOSE
	    then
		if [ -z "$pid" ]
		then
		    $VERY_VERBOSE && echo >&2 "No non-primary pmlogger process(es) found"
		else
		    $VERY_VERBOSE && echo >&2 "non-primary pmlogger process(es) $pid identified, OK"
		fi
	    fi
	fi

	if [ -z "$pid" ]
	then
	    if [ "$PMLOGGER_CTL" = "on" ]
	    then
		_error "no pmlogger instance running for host \"$host\""
	    else
		_warning "no pmlogger instance running for host \"$host\""
	    fi
	    _warning "skipping log rotation because we don't know which pmlogger to signal"
	elif ! $COMPRESSONLY
	then
	    touch $tmp/reexec
	    if [ "$REEXEC_MODE" = force ]
	    then
		# -Z, force reexec
		:
	    elif [ "$REEXEC_MODE" = skip ]
	    then
		# -z, skip reexec
		rm -f $tmp/reexec
	    elif [ -f "$PCP_TMP_DIR/pmlogger/$pid" ]
	    then
		# if pmlogger is already creating an archive with today's
		# date then "reexec" has already been done, so we do not need
		# to do it again ... this can happen when systemd decides
		# to run pmlogger_daily at pmlogger service start, rather
		# than just at 00:10 (by default) as originally intended
		# and implemented in the cron-driven approach
		#
		current_archive=`sed -n -e '3s;.*/;;p' <"$PCP_TMP_DIR/pmlogger/$pid"`
		case "$current_archive"
		in
		    `pmdate %Y%m%d`*)
				$VERBOSE && echo "Skip sending SIGUSR2 to $pid (reexec already done)"
				rm -f $tmp/reexec
				;;
		esac
	    fi
	    if [ -f $tmp/reexec ]
	    then
		#
		# send pmlogger a SIGUSR2 to "roll the archive logs"
		#
		if $SHOWME
		then
		    echo "+ $KILL -s USR2 $pid"
		else
		    $VERBOSE && echo "Sending SIGUSR2 to reexec $pid"
		    $KILL -s USR2 "$pid"
		    if $VERY_VERBOSE
		    then
			# We have seen in qa/793, but never in a "real"
			# deployment, cases where the pmlogger process
			# vanishes a short time after the SIGUSR2 has been
			# sent and caught, the process has called exec() and
			# main() has restarted.  This check is intended
			# detect and report when this happens if -VV is
			# in play.
			#
			sleep 1
			if $PCP_PS_PROG -p "$pid" 2>&1 | grep "^$pid[ 	]" >/dev/null
			then
			    : still alive
			else
			    echo >&2 "Error: pmlogger process $pid has vanished"
			fi
		    fi
		fi
		rm -f $tmp/reexec
	    fi
	fi

	if ! $COMPRESSONLY
	then
	    # Cull any old archives.  
	    #
	    # We now do this first, so that if the archives are bad for
	    # any reason we don't want failures to merge or rewrite to
	    # prevent removing old files as this can lead to full
	    # filesystems if left unattended.
	    #
	    if [ X"$CULLAFTER" != Xforever -a X"$CULLAFTER" != Xnever ]
	    then
		# *BSD semantics for find(1) -mtime +N are "rounded up to
		# the next full 24-hour period", compared to GNU/Linux
		# semantics "any fractional part is ignored".  So, these are
		# almost always off by one day in terms of the files selected.
		# 
		# For GNU/Linux reduce the number of days by one (opens up
		# the window).
		#
		# The real filtering is done by find-filter, we just want to
		# be sure that we include all of the candidate files in the
		# find(1) part.
		#
		CULLDAYS=`echo $CULLAFTER | sed -e 's/:.*//'`
		if [ "$PCP_PLATFORM" = freebsd -o "$PCP_PLATFORM" = netbsd -o "$PCP_PLATFORM" = openbsd ]
		then
		    :
		else
		    # decrement, unless already zero
		    #
		    [ "$CULLDAYS" -gt 0 ] && CULLDAYS=`expr "$CULLDAYS" - 1`
		fi
		if [ "$CULLDAYS" -gt 0 ]
		then
		    find . -type f -mtime +$CULLDAYS
		else
		    find . -type f
		fi \
		| $PCP_BINADM_DIR/find-filter mtime +$CULLAFTER \
		| _filter_filename \
		| sort >$tmp/list
		if [ -n "$pid" ]
		then
		    # pmlogger is running, make sure we don't cull the current
		    # archive
		    #
		    if [ -f "$PCP_TMP_DIR/pmlogger/$pid" ]
		    then
			current_base=`sed -n -e '3s;.*/;;p' <"$PCP_TMP_DIR/pmlogger/$pid"`
			if [ -n "$current_base" ]
			then
			    $VERY_VERBOSE && echo >&2 "[$filename:$line] skip cull for current $current_base archive"
			    sed -e "/^$current_base/d" <$tmp/list >$tmp/tmp
			    mv $tmp/tmp $tmp/list
			fi
		    fi
		fi
		if [ -s $tmp/list ]
		then
		    if $VERBOSE
		    then
			echo "Archive files older than `_timespec $CULLAFTER` being removed ..."
			_fmt <$tmp/list | sed -e 's/^/    /'
		    fi
		    if $SHOWME
		    then
			cat $tmp/list | xargs echo + rm -f 
		    else
			cat $tmp/list | xargs rm -f
		    fi
		else
		    $VERY_VERBOSE && echo >&2 "Warning: no archive files found to cull"
		fi
	    fi

	    # Merge archive logs.
	    #
	    # Will work for new style YYYYMMDD.HH.MM[-NN] archives and old style
	    # YYMMDD.HH.MM[-NN] archives.
	    # Note: we need to handle duplicate-breaking forms like
	    # YYYYMMDD.HH.MM-seq# (even though pmlogger_merge already picks most
	    # of these up) in case the base YYYYMMDD.HH.MM archive is for some
	    # reason missing here
	    #
	    # Assume if the .meta or .meta.* file is present then other
	    # archive components are also present (if not the case it
	    # is a serious process botch, and pmlogger_merge will fail below)
	    #
	    # Find all candidate input archives, remove any that contain today's
	    # date and group the remainder by date.
	    #
	    TODAY=`date +%Y%m%d`

	    find *.meta* \
		 \( -name "*.[0-2][0-9].[0-5][0-9].meta*" \
		    -o -name "*.[0-2][0-9].[0-5][0-9]-[0-9][0-9].meta*" \
		 \) \
		 -print 2>/dev/null \
	    | sed \
		-e "/^$TODAY\./d" \
		-e 's/\.meta\..*//' \
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

	    rm -f $tmp/skip
	    if $MFLAG
	    then
		# -M don't rewrite, merge or rename
		#
		:
	    else
		if [ ! -s $tmp/list ]
		then
		    if $SHOWME
		    then
			:
		    else
			if $VERBOSE
			then
			    echo "Warning: no archives found to merge"
			    $VERY_VERBOSE && ls >&2 -l
			fi
		    fi
		else
		    cat $tmp/list \
		    | while read outfile inlist
		    do
			if [ -f $outfile.0 -o -f $outfile.index -o -f $outfile.meta ]
			then
			    _skipping "output archive ($outfile) already exists"
			    continue
			else
			    $VERY_VERBOSE && echo >&2 "Rewriting input archives using $rewrite"
			    if $RFLAG
			    then
				:
			    else
				for arch in $inlist
				do
				    if $SHOWME
				    then
					echo "+ pmlogrewrite -iq $rewrite $arch"
				    else
					if eval pmlogrewrite -iq $rewrite $arch
					then
					    :
					else
					    _skipping "rewrite for $arch failed using $rewrite failed"
					    continue
					fi
				    fi
				done
			    fi
			    [ -f $tmp/skip ] && continue
			    if $VERY_VERBOSE
			    then
				for arch in $inlist
				do
				    echo >&2 "Input archive $arch ..."
				    if $SHOWME
				    then
					echo "+ pmdumplog -L $arch"
				    else
					pmdumplog >&2 -L $arch
				    fi
				done
			    fi
			    narch=`echo $inlist | wc -w | sed -e 's/ //g'`
			    rm -f $tmp/mergefile
			    if [ "$narch" = 1 ]
			    then
				# optimization - rename, don't merge, for one input archive
				#
				if $SHOWME
				then
				    echo "+ pmlogmv$MYARGS $inlist $outfile"
				    echo "$outfile" >$tmp/mergefile
				elif pmlogmv$MYARGS $inlist $outfile
				then
				    if $VERY_VERBOSE
				    then
					echo >&2 "Renamed output archive $outfile ..."
					pmdumplog >&2 -L $outfile
				    fi
				    echo "$outfile" >$tmp/mergefile
				else
				    _error "problems executing pmlogmv for host \"$host\""
				fi
			    else
				# more than one input archive, merge away
				#
				if $SHOWME
				then
				    echo "+ pmlogger_merge$MYARGS $EXPUNGE -f $inlist $outfile"
				    echo "$outfile" >$tmp/mergefile
				elif pmlogger_merge$MYARGS $EXPUNGE -f $inlist $outfile
				then
				    if $VERY_VERBOSE
				    then
					echo >&2 "Merged output archive $outfile ..."
					pmdumplog >&2 -L $outfile
				    fi
				    echo "$outfile" >$tmp/mergefile
				else
				    _error "problems executing pmlogger_merge for host \"$host\""
				fi
			    fi
			    if [ -s $tmp/mergefile -a -s $tmp/merge_callback ]
			    then
				# Do merge callbacks
				#
				$VERBOSE && echo "Merge callbacks ..."
				mergefile="$dir/`cat $tmp/mergefile`"
				cat $tmp/merge_callback \
				| while read exec
				do
				    case "$exec"
				    in
					$PCP_BINADM_DIR/pmlogger_daily_report)
					    # only do this one for the primary
					    # pmlogger instance
					    #
					    [ "$primary" != y ] && continue
					    ;;
				    esac
				    if $SHOWME
				    then
					echo "+ $exec $mergefile"
				    else
					$VERBOSE && echo "callback: $exec $mergefile"
					eval $exec $mergefile
					sts=$?
					if [ "$sts" = 0 ]
					then
					    :
					else
					    _warning "$exec $mergefile returned status $sts"
					fi
				    fi
				done
			    fi
			    if [ -s $tmp/mergefile -a -s $tmp/autosave ]
			    then
				# save archive basename for possible AUTOSAVE use later
				#
				cat $tmp/mergefile >>$tmp/savefiles
			    fi
			fi
		    done
		fi
	    fi
	fi

	# and compress old archive data files
	# (after cull - don't compress unnecessarily)
	#
	COMPRESSAFTER="$PCP_COMPRESSAFTER"
	[ -z "$COMPRESSAFTER" ] && COMPRESSAFTER="$COMPRESSAFTER_CMDLINE"
	[ -z "$COMPRESSAFTER" ] && COMPRESSAFTER="$COMPRESSAFTER_DEFAULT"
	$VERY_VERBOSE && echo >&2 "COMPRESSAFTER=$COMPRESSAFTER"
	if [ -n "$COMPRESSAFTER" -a X"$COMPRESSAFTER" != Xforever -a X"$COMPRESSAFTER" != Xnever ]
	then
	    # may have some compression to do ...
	    #
	    COMPRESS="$PCP_COMPRESS"
	    [ -z "$COMPRESS" ] && COMPRESS="$COMPRESS_CMDLINE"
	    [ -z "$COMPRESS" ] && COMPRESS="$COMPRESS_DEFAULT"
	    # $COMPRESS may have args, e.g. -0 --block-size=10MiB so
	    # extract executable command name
	    #
	    COMPRESS_PROG=`echo "$COMPRESS" | sed -e 's/[ 	].*//'`
	    if [ -n "$COMPRESS_PROG" ] && which "$COMPRESS_PROG" >/dev/null 2>&1
	    then
		current_vol=''
		if [ -n "$pid" ]
		then
		    # pmlogger running, need to avoid the current volume
		    #
		    # may need to wait for pmlogger to get going ... logic here
		    # is based on _wait_for_pmlogger() in qa/common.check
		    #
		    i=1
		    while true
		    do
			# pmlc may race with pmlogger starting up here - a timeout is required
			# to avoid pmlc blocking forever and hanging pmlogger_daily. RHBZ#1892326
			[ -z "$PMLOGGER_REQUEST_TIMEOUT" ] && export PMLOGGER_REQUEST_TIMEOUT=2
			if pmlc "$pid" </dev/null 2>&1 | tee $tmp/out \
				| grep "^Connected to .*pmlogger" >/dev/null
			then
			    # pmlogger socket has been set up ...
			    break
			else
			    [ $i -eq 20 ] && break
			    i=`expr $i + 1`
			    sleep 1
			fi
		    done
		    echo status | pmlc "$pid" >$tmp/out 2>&1
		    current_vol=`sed -n <$tmp/out -e '/^log volume/s/.*[^0-9]\([0-9][0-9]*\)$/\1/p'`
		    if [ -z "$current_vol" ]
		    then
			_warning "cannot get current volume for pmlogger PID=$pid (after $i attempts)"
			cat $tmp/out
		    else
			pminfo -f pmcd.pmlogger.archive >$tmp/out
			current_base=`sed -n <$tmp/out -e '/ or "'$pid'"]/{
s/.*\///
s/"//
p
}'`
			if [ -z "$current_base" ]
			then
			    _warning "cannot get archive basename pmlogger PID=$pid"
			    cat $tmp/out
			fi
		    fi
		fi

		if $FORCE || [ -n "$current_vol" -a -n "$current_base" ]
		then
		    COMPRESSREGEX="$PCP_COMPRESSREGEX"
		    [ -z "$COMPRESSREGEX" ] && COMPRESSREGEX="$COMPRESSREGEX_CMDLINE"
		    [ -z "$COMPRESSREGEX" ] && COMPRESSREGEX="$COMPRESSREGEX_DEFAULT"
		    if [ "$COMPRESSAFTER" -eq 0 ]
		    then
			# compress all possible files immediately
			#
			find . -type f
		    else
			# compress files last modified more than $COMPRESSSAFTER
			# days ago
			#
			if [ "$PCP_PLATFORM" = freebsd -o "$PCP_PLATFORM" = netbsd -o "$PCP_PLATFORM" = openbsd ]
			then
			    # See note above re. find(1) on FreeBSD/NetBSD/OpenBSD
			    #
			    mtime=`expr $COMPRESSAFTER - 1`
			else
			    mtime=$COMPRESSAFTER
			fi
			find . -type f -mtime +$mtime
		    fi \
		    | _filter_filename \
		    | egrep -v "$COMPRESSREGEX" \
		    | sort >$tmp/list
		    if [ -s $tmp/list -a -n "$current_base" -a -n "$current_vol" ]
		    then
			# don't compress current volume (or later ones, if
			# pmlogger has moved onto a new volume since
			# $current_vol was determined), and don't compress
			# either the current index or the current metadata
			# files
			#
			$VERY_VERBOSE && echo >&2 "[$filename:$line] skip compress for current $current_base.$current_vol volume"
			rm -f $tmp/out
			touch $tmp/out
			# need to handle both the year 2000 and the old name
			# formats, the ...DDMM and ...DDMM.HH.MM, and the
			# ...DDMM.HH.MM-seq# variants to get the base name
			# separated from the other part of the file name, but
			# on the upside compressed file names were stripped out
			# above by the egrep -v "$COMPRESSREGEX"
			#
			sed -n <$tmp/list \
			    -e '/\./s/\.\([^.][^.]*\)$/ \1/p' \
			| while read base other
			do
			    if [ "$base" != "$current_base" ]
			    then
				echo "$base.$other" >>$tmp/out
			    else
				case "$other"
				in
				    .index*|.meta*)
					# don't do these ones
					;;
				    [0-9]*)
				    	# data volume
					if [ "$other" -lt "$current_vol" ]
					then
					    echo "$base.$other" >>$tmp/out
					fi
					;;
				esac
			    fi
			done
			mv $tmp/out $tmp/list
		    fi
		    if [ -s $tmp/list ]
		    then
			if $VERBOSE
			then
			    if [ "$COMPRESSAFTER" -eq 0 ]
			    then
				echo "Archive files being compressed ..."
			    else
				echo "Archive files older than $COMPRESSAFTER days being compressed ..."
			    fi
			    _fmt <$tmp/list | sed -e 's/^/    /'
			fi
			if $SHOWME
			then
			    cat $tmp/list | xargs echo + $COMPRESS
			else
			    cat $tmp/list | xargs $COMPRESS
			fi
			if [ -s $tmp/compress_callback ]
			then
			    # Do compress callbacks, but only for full
			    # day's archives
			    #
			    $VERBOSE && echo "Compress callbacks ..."
			    cat $tmp/list \
			    | sed -n \
				-e 's/\.[0-9][0-9]*$//' \
				-e 's/\.meta$//' \
				-e 's/\.index$//' \
				-e '/^[12][0-9][0-9][0-9][0-1][0-9][0-3][0-9]$/p' \
				-e '/^[0-9][0-9][0-1][0-9][0-3][0-9]$/p' \
			    | sort -n \
			    | uniq \
			    | while read compressfile
			    do
				compressfile="$dir/$compressfile"
				cat $tmp/compress_callback \
				| while read exec
				do
				    if $SHOWME
				    then
					echo "+ $exec $compressfile"
				    else
					$VERBOSE && echo "callback: $exec $compressfile"
					eval $exec $compressfile
					sts=$?
					if [ "$sts" = 0 ]
					then
					    :
					else
					    _warning "$exec $compressfile returned status $sts"
					fi
				    fi
				done
			    done
			fi
		    else
			$VERY_VERBOSE && echo >&2 "Warning: no archive files found to compress"
		    fi
		elif [ -z "$COMPRESSAFTER" -o X"$COMPRESSAFTER" = Xforever -o X"$COMPRESSAFTER" = Xnever ]
		then
		    # never going to do compression, so don't warn ...
		    :
		else
		    _warning "current volume of current pmlogger not known, compression skipped"
		fi
	    else
		_error "$COMPRESS_PROG: compression program not found"
	    fi
	fi

	# autosave any newly merged (and possibly compressed) archives
	# if PCP_AUTOSAVE_DIR is in play
	#
	if [ -s $tmp/autosave -a -s $tmp/savefiles ]
	then
	    if $VERBOSE
	    then
		( echo "Autosave ... "; _fmt <$tmp/savefiles ) \
		| sed -e 's/^/    /'
	    fi
	    last_mkdir=''
	    cat $tmp/savefiles \
	    | while read savefile
	    do
		# make sure destination directory hierarchy exists and $PCP_USER
		# user can write there
		#
		DATEYYYY=`echo "$savefile" | sed -e 's/^\(....\).*/\1/'`
		DATEMM=`echo "$savefile" | sed -e 's/^....\(..\).*/\1/'`
		DATEDD=`echo "$savefile" | sed -e 's/^......\(..\)/\1/'`
		auto_dir="`cat $tmp/autosave \
			   | sed \
			       -e s/DATEYYYY/$DATEYYYY/g \
			       -e s/DATEMM/$DATEMM/g \
			       -e s/DATEDD/$DATEDD/g \
			       -e s/LOCALHOSTNAME/$dirhostname/`"
		if [ ! -d "$auto_dir" -a "$auto_dir" != "$last_mkdir" ]
		then
		    mkdir_and_chown "$auto_dir" 755 $PCP_USER:$PCP_GROUP >$tmp/tmp 2>&1
		    if [ ! -d "$auto_dir" ]
		    then
			cat $tmp/tmp
			_error "cannot create directory ($auto_dir) for autosave"
		    else
			_warning "creating directory ($auto_dir) for autosave"
			# fall through and another warning will come from
			# _autosave()
		    fi
		    last_mkdir="$auto_dir"
		fi
		_autosave "$dir" "$savefile" "$auto_dir"
	    done
	fi

	# and cull old trace files (from -t option)
	#
	if [ "$TRACE" -gt 0 ] && ! $COMPRESSONLY
	then
	    if [ "$PCP_PLATFORM" = freebsd -o "$PCP_PLATFORM" = netbsd -o "$PCP_PLATFORM" = openbsd ]
	    then
		# See note above re. find(1) on FreeBSD/NetBSD/OpenBSD
		#
		mtime=`expr $TRACE - 1`
	    else
		mtime=$TRACE
	    fi
	    find "$PCP_ARCHIVE_DIR" -type f -mtime +$mtime \
	    | sed -n -e '/pmlogger\/daily\..*\.trace/p' \
	    | sort >$tmp/list
	    if [ -s $tmp/list ]
	    then
		if $VERBOSE
		then
		    echo "Trace files older than $TRACE days being removed ..."
		    _fmt <$tmp/list | sed -e 's/^/    /'
		fi
		if $SHOWME
		then
		    cat $tmp/list | xargs echo + rm -f
		else
		    cat $tmp/list | xargs rm -f
		fi
	    else
		$VERY_VERBOSE && echo >&2 "Warning: no trace files found to cull"
	    fi
	fi

	_unlock "$dir"
    done
}

# Paranoid archive saving
#
# Usage: _autosave src_dir archive_base dest_dir
#
_autosave()
{
    _src="$1"
    _base="$2"
    _dest="$3"
    if [ ! -d "$_dest" ]
    then
	_warning "$_base: AUTOSAVE skipped: directory $_dest does not exist"
	return
    fi
    if pmlogmv -c "$_src/$_base" "$_dest/$_base"
    then
	:
    else
	_warning "$_base: AUTOSAVE failed"
    fi
}


# .NeedRewrite in $PCP_LOG_DIR/pmlogger is just like -R, but needs
# only be done once, e.g. after software upgrade with new pmlogrewrite(1)
# configuration files
#
if ! $COMPRESSONLY && [ -f $PCP_LOG_DIR/pmlogger/.NeedRewrite ]
then
    REWRITEALL=true
    $VERBOSE && echo "Info: found .NeedRewrite => rewrite all archives"
fi

_parse_control $CONTROL
append=`ls $CONTROLDIR 2>/dev/null | LC_COLLATE=POSIX sort`
for extra in $append
do
    _parse_control $CONTROLDIR/$extra
done

if [ -f $tmp/err ]
then
    # serious errors
    #
    status=1
fi

# if need be, remove .NeedRewrite so we don't trigger -R processing
# next time
#
if ! $COMPRESSONLY && [ -f $PCP_LOG_DIR/pmlogger/.NeedRewrite ]
then
    if $SHOWME
    then
	echo + rm -f $PCP_LOG_DIR/pmlogger/.NeedRewrite
    else
	rm -f $PCP_LOG_DIR/pmlogger/.NeedRewrite
    fi
fi

# optional end logging to $PCP_LOG_DIR/NOTICES
#
if $PCP_LOG_RC_SCRIPTS
then
    $PCP_BINADM_DIR/pmpost "end pid:$$ $prog status=$status"
fi

exit
