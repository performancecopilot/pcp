#! /bin/sh
#
# Copyright (c) 2018 Red Hat.
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
#
# Administrative script for daily sar-style report summaries.
# This is intened to be run from cron, an hour or two after the pmlogger_daily
# cron script has completed, e.g. around 2am would be a suitable time.
#
# Sample crontab entry:
#
# # daily generation of system activity reports
# 0     2  *  *  *  pcp  /usr/libexec/pcp/bin/pmlogger_daily_report
#
#
# By default, the daily report will be written to /var/log/pcp/sa/sarXX
# where XX is the day of the month, yesterday.
#

. $PCP_DIR/etc/pcp.env
. $PCP_SHARE_DIR/lib/utilproc.sh

status=0
prog=`basename $0`

# optional begin logging to $PCP_LOG_DIR/NOTICES
#
if $PCP_LOG_RC_SCRIPTS
then
    logmsg="begin pid:$$ $prog args:$*"
    if which pstree >/dev/null 2>&1
    then
	logmsg="$logmsg [`_pstree_oneline $$`]"
    fi
    $PCP_BINADM_DIR/pmpost "$logmsg"
fi

# error messages should go to stderr, not the GUI notifiers
unset PCP_STDERR

# want mode for mkdir below to reliably be rwxrwxr-x (775)
#
umask 022

# default message log file
PROGLOG=$PCP_LOG_DIR/pmlogger/$prog.log
USE_SYSLOG=true
tmp=`mktemp -d "$PCP_TMPFILE_DIR/pmlogger_daily_report.XXXXXXXXX"` || exit 1

_cleanup()
{
    $USE_SYSLOG && [ $status -ne 0 ] && \
    $PCP_SYSLOG_PROG -p daemon.error "$prog failed - see $PROGLOG"
    if [ "$PROGLOG" != "/dev/tty" ]
    then
	[ -s "$PROGLOG" ] || rm -f "$PROGLOG"
    fi
    rm -rf $tmp
}
trap "_cleanup; exit \$status" 0 1 2 3 15

echo > $tmp/usage
cat >> $tmp/usage <<EOF
Options:
  -a=FILE		  input archive file basename or directory path (default yesterdays $PCP_ARCHIVE_DIR/<hostname>/YYYYMMDD)
  -f=FILE		  output filename (default "sarXX" in directory specified with -o, for XX yesterdays day of the month)
  -h=HOSTNAME		  hostname, affects the default input archive file path (default local hostname)
  -l=FILE,--logfile=FILE  send important diagnostic messages to FILE
  -p                      poll and exit if processing already done for today
  -o=DIRECTORY		  output directory (default $PCP_SA_DIR, see also -f option)
  -t=TIME		  reporting interval, default 10m
  -A			  use start and end times of input archive for the report (default midnight yesterday for 24hours)
  -V,--verbose            verbose messages in log (multiple times for very verbose), see also -l option
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
VERBOSE=false
ARCHIVETIMES=false
VERY_VERBOSE=false
PFLAG=false
MYARGS=""
DEBUG=false		# desperate debugging

ARGS=`pmgetopt --progname=$prog --config=$tmp/usage -- "$@"`
[ $? != 0 ] && exit 1

eval set -- "$ARGS"
while [ $# -gt 0 ]
do
    case "$1"
    in
	-a)	ARCHIVEPATH="$2"
		$DEBUG && echo "debug: after -a:  ARCHIVEPATH=$ARCHIVEPATH"
		shift
		;;
	-f)	REPORTFILE="$2"
		shift
		;;
	-h)	HOSTNAME="$2"
		shift
		;;
	-l)	PROGLOG="$2"
		USE_SYSLOG=false
		shift
		;;
	-o)	REPORTDIR="$2"
		shift
		;;
	-p)     PFLAG=true
		;;
	-t)	INTERVAL="$2"
		shift
		;;
	-A)	ARCHIVETIMES=true
		;;
	-V)	if $VERBOSE
		then
		    VERY_VERBOSE=true
		else
		    VERBOSE=true
		fi
		MYARGS="$MYARGS -V"
		;;
	-\?)	_usage
		;;
    esac
    shift
done

[ $# -ne 0 ] && _usage

# optionally uncompress a compressed archive
#
_uncompress()
{
    if pmlogdecompress -t $tmp "$1"
    then
	echo $tmp/`basename "$1"`
    else
	$VERBOSE && echo >&2 "Uncompressing failed, reverting to compressed archive"
	echo "$1"
    fi
}

if $PFLAG
then
    rm -f $tmp/ok
    if [ -f $PCP_LOG_DIR/pmlogger/pmlogger_daily_report.stamp ]
    then
	last_stamp=`sed -e '/^#/d' <$PCP_LOG_DIR/pmlogger/pmlogger_daily_report.stamp`
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
		# nothing to be done, yet
		exit
	    fi
	    touch $tmp/ok
	fi
    fi
    if [ ! -f $tmp/ok ]
    then
	# special start up logic when pmlogger_daily_report.stamp does not
	# exist ... punt on archive files being below $PCP_LOG_DIR/sa
	#
	if [ -d $PCP_LOG_DIR/sa ]
	then
	    find $PCP_LOG_DIR/sa -name "sar`pmdate -1d %d`" >$tmp/tmp
	    if [ -s $tmp/tmp ]
	    then
		# heuristic match, assume nothing to be done
		exit
	    fi
	fi
    fi
fi

# if it looks like we can create/update the date-and-timestamp
# then do it ... this will be checked by -p polling
#
if [ -w $PCP_LOG_DIR/pmlogger ]
then
    if [ ! -f $PCP_LOG_DIR/pmlogger/pmlogger_daily_report.stamp \
           -o -w $PCP_LOG_DIR/pmlogger/pmlogger_daily_report.stamp ]
    then
	if _save_prev_file $PCP_LOG_DIR/pmlogger/pmlogger_daily_report.stamp
	then
	    :
	else
	    echo "Warning: cannot save previous date-and-timestamp" >&2
	fi
	# only update date-and-timestamp if we can write the file
	#
	pmdate '# %Y-%m-%d %H:%M:%S
%s' >$tmp/stamp
	if cp $tmp/stamp $PCP_LOG_DIR/pmlogger/pmlogger_daily_report.stamp
	then
	    :
	else
	    echo "Warning: cannot install new date-and-timestamp" >&2
	fi
    fi
fi

if [ "$PROGLOG" = "/dev/tty" ]
then
    # special case for debugging ... no salt away previous
    #
    :
else
    # After argument checking, everything must be logged to ensure no mail is
    # accidentally sent from cron.  Close stdout and stderr, then open stdout
    # as our logfile and redirect stderr there too.
    #
    [ -f "$PROGLOG" ] && mv "$PROGLOG" "$PROGLOG.prev"
    exec 1>"$PROGLOG" 2>&1
fi

# Default hostname is the name of the local host
#
[ -z "$HOSTNAME" ] && HOSTNAME=`hostname`

# Default input archive is the merged archive for yesterday.
# Unfortunately we can't just specify the entire directory
# and rely on multi-archive mode because this script might
# take a long time to run as a result.
#
[ -z "$ARCHIVEPATH" ] && ARCHIVEPATH=$PCP_ARCHIVE_DIR/$HOSTNAME/`pmdate -1d %Y%m%d`
$DEBUG && echo "debug: after default:  ARCHIVEPATH=$ARCHIVEPATH"

# If input archive has been compressed, then uncompress it
# into temporary files and use these, to avoid repeated
# uncompressing for pmrep below.
#
ORIG_ARCHIVEPATH="$ARCHIVEPATH"
ARCHIVEPATH=`_uncompress $ARCHIVEPATH`
$DEBUG && echo "debug: after _uncompress:  ARCHIVEPATH=$ARCHIVEPATH"
$VERBOSE && echo ARCHIVEPATH=$ARCHIVEPATH

# Default output directory
#
[ -z "$REPORTDIR" ] && REPORTDIR="$PCP_SA_DIR"
$VERBOSE && echo REPORTDIR=$REPORTDIR

# Create output directory - if this fails for any reason we exit later
#
# mode rwxrwxr-x is the default for pcp:pcp dirs
umask 002
[ -d "$REPORTDIR" ] \
    || mkdir -p -m 0775 "$REPORTDIR" 2>/dev/null
# reset the default mode to rw-rw-r- for files
umask 022

# Default output file is the day of month for yesterday in REPORTDIR
#
[ -z "$REPORTFILE" ] && REPORTFILE=$REPORTDIR/sar`pmdate -1d %d`
$VERBOSE && echo REPORTFILE=$REPORTFILE

# Default reporting interval is 10m (same as sysstat uses)
#
[ -z "$INTERVAL" ] && INTERVAL="10m"

# If the input archive doesn't exist, we exit.
#
if [ ! -f "`echo $ARCHIVEPATH.meta*`" ]; then
    # report this to the log
    $DEBUG && ls -l $ARCHIVEPATH.*
    echo "$prog: FATAL error: Failed to find input archive \"$ARCHIVEPATH\""
    exit 1
fi

# Common reporting options, including time window: midnight yesterday for 24h
#
REPORT_OPTIONS="-a $ARCHIVEPATH -z -E 0 -p -f%H:%M:%S -t$INTERVAL"
if ! $ARCHIVETIMES
then
    # specific start/finish times - midnight yesterday for 24h
    start=`pmdate -1d "@%d-%b-%Y"`
    REPORT_OPTIONS="$REPORT_OPTIONS -S$start -T 24h"
fi
$VERBOSE && echo REPORT_OPTIONS=$REPORT_OPTIONS

# Truncate or create the output file. Exit if this fails.
#
if ! cp /dev/null $REPORTFILE
then
    status=$?
    echo "$prog: FATAL error: cannot create \"$REPORTFILE\""
    exit
fi

#
# Common reporting function
#
_report()
{
    _conf=$1
    _comment="$2"

    $VERBOSE && echo Generating report for $_conf $_comment
    echo >>$REPORTFILE; echo >>$REPORTFILE
    pmdumplog -z -l $ARCHIVEPATH | awk '/commencing/ {print "# ",$2,$3,$4,$5,$6}' >>$REPORTFILE
    echo $_comment >>$REPORTFILE
    $VERBOSE && echo pmrep $REPORT_OPTIONS $_conf
    pmrep $REPORT_OPTIONS $_conf >$tmp/out 2>$tmp/err
    if [ -s $tmp/out ]
    then
    	cat $tmp/out >>$REPORTFILE
    else
	if grep 'PM_ERR_NAME' $tmp/err >/dev/null 2>&1
	then
	    metric=`$PCP_AWK_PROG <$tmp/err '/PM_ERR_NAME/ { print $3; exit }'`
	    echo "-- no report for config \"$_conf\" because the metric \"$metric\" is not in the archive" >>$REPORTFILE
	elif grep 'PM_ERR_INDOM_LOG' $tmp/err >/dev/null 2>&1
	then
	    metric=`$PCP_AWK_PROG <$tmp/err '/PM_ERR_INDOM_LOG/ { print $3; exit }'`
	    echo "-- no report for config \"$_conf\" because there are no values for any instance of the metric \"$metric\" in the archive" >>$REPORTFILE
	elif grep 'PM_ERR_BADDERIVE' $tmp/err >/dev/null 2>&1
	then
	    metric=`$PCP_AWK_PROG <$tmp/err '/PM_ERR_BADDERIVE/ { print $3; exit }'`
	    echo "-- no report for config \"$_conf\" because one or more metrics for the derived metric \"$metric\" is not in the archive" >>$REPORTFILE
	else
	    cat $tmp/err >>$REPORTFILE
	    echo "-- no report for config \"$_conf\"" >>$REPORTFILE
	fi
    fi
}

echo "System Activity Report" >>$REPORTFILE
echo >>$REPORTFILE
echo "Host:            $HOSTNAME" >>$REPORTFILE
echo "Archive:         $ORIG_ARCHIVEPATH" >>$REPORTFILE
echo "Report created:  `date`" >>$REPORTFILE

_report :sar-u-ALL '# CPU Utilization statistics, all CPUS'
_report :sar-u-ALL-P-ALL '# CPU Utilization statistics, per-CPU'
_report :vmstat '# virtual memory (vmstat) statistics'
_report :vmstat-a '# virtual memory active/inactive memory statistics'
_report :sar-B '# paging statistics'
_report :sar-b '# I/O and transfer rate statistics'
_report :sar-d-dev '# block device statistics'
_report :sar-d-dm '# device-mapper device statistics'
_report :sar-F '# mounted filesystem statistics'
_report :sar-H '# hugepages utilization statistics'
_report :sar-I-SUM '# interrupt statistics, summed'
_report :sar-n-DEV '# network statistics, per device'
_report :sar-n-EDEV '# network error statistics, per device'
_report :sar-n-NFSv4 '# NFSv4 client and RPC statistics'
_report :sar-n-NFSDv4 '# NFSv4 server and RPC statistics'
_report :sar-n-SOCK '# socket statistics'
_report :sar-n-TCP-ETCP '# TCP statistics'
_report :sar-q '# queue length and load averages'
_report :sar-r '# memory utilization statistics'
_report :sar-S '# swap usage statistics'
_report :sar-W '# swapping statistics'
_report :sar-w '# task creation and system switching statistics'
_report :sar-y '# TTY devices activity'
_report :numa-hint-faults '# NUMA hint fault statistics'
_report :numa-per-node-cpu '# NUMA per-node CPU statistics'
_report :numa-pgmigrate-per-node '# NUMA per-node page migration statistics'

# optional end logging to $PCP_LOG_DIR/NOTICES
#
if $PCP_LOG_RC_SCRIPTS
then
    $PCP_BINADM_DIR/pmpost "end pid:$$ $prog status=$status"
fi

exit
