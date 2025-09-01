#! /bin/sh
#
# Copyright (c) 2023 Ken McDonell.  All Rights Reserved.
# Copyright (c) 2013-2016,2018,2020-2022 Red Hat.
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
# Administrative script to check for pmlogger badness caused by processes
# and/or files that were once managed from the control files but have
# become detached from those control files.
#

. $PCP_DIR/etc/pcp.env
. $PCP_SHARE_DIR/lib/rc-proc.sh
. $PCP_SHARE_DIR/lib/utilproc.sh

# error messages should go to stderr, not the GUI notifiers
#
unset PCP_STDERR

# ensure mere mortals cannot write any configuration files,
# but that the unprivileged PCP_USER account has read access
#
umask 022

# constant setup
#
tmp=`mktemp -d "$PCP_TMPFILE_DIR/pmlogger_janitor.XXXXXXXXX"` || exit 1
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
	logmsg="$logmsg [`_pstree_oneline $$`]"
    fi
    $PCP_BINADM_DIR/pmpost "$logmsg"
fi

_cleanup()
{
    if [ "$PROGLOG" != "/dev/tty" ]
    then
	if [ -s "$MYPROGLOG" ]
	then
	    rm -f "$PROGLOG"
	    if mv "$MYPROGLOG" "$PROGLOG"
	    then
		:
	    else
		echo >&3 "Save $MYPROGLOG to $PROGLOG failed ..."
		cat >&3 "$MYPROGLOG"
	    fi
	else
	    rm -f "$MYPROGLOG"
	fi
    fi
    $USE_SYSLOG && [ $status -ne 0 ] && \
    $PCP_SYSLOG_PROG -p daemon.error "$prog failed - see $PROGLOG"
    lockfile=`cat $tmp/lock 2>/dev/null`
    [ -n "$lockfile" ] && rm -f "$lockfile"
    rm -rf $tmp
    $VERBOSE && echo >&2 "End [janitor]: `date '+%F %T.%N'` status=$status"
}

trap "_cleanup; exit \$status" 0 1 2 3 15

# control files for pmlogger administration ... edit the entries in this
# file (and optional directory) to reflect your local configuration; see
# also -c option below.
#
CONTROL=$PCP_PMLOGGERCONTROL_PATH
CONTROLDIR=$PCP_PMLOGGERCONTROL_PATH.d

# (compression setup and logic borrowed from pmlogger_daily)
# default compression program and filename suffix pattern for files
# to NOT compress
# 
COMPRESS=""
COMPRESS_CMDLINE=""
COMPRESS_DEFAULT="pmlogcompress"
COMPRESSREGEX=""
COMPRESSREGEX_CMDLINE=""
COMPRESSREGEX_DEFAULT="\.(index|Z|gz|bz2|zip|xz|lzma|lzo|lz4|zst)$"

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

# default location
#
logfile=pmlogger.log


# option parsing
#
SHOWME=false
KILL=pmsignal
VERBOSE=false
VERY_VERBOSE=false
VERY_VERY_VERBOSE=false
QUICKSTART=false

echo > $tmp/usage
cat >> $tmp/usage << EOF
Options:
  -c=FILE,--control=FILE  configuration of pmlogger instances to manage
  -l=FILE,--logfile=FILE  send important diagnostic messages to FILE
  -N,--showme             perform a dry run, showing what would be done
  -V,--verbose            increase diagnostic verbosity
  -X=PROGRAM,--compressor=PROGRAM  use PROGRAM for file compression
  -Y=REGEX,--regex=REGEX  grep -E filter for files to NOT compress ["$COMPRESSREGEX_DEFAULT"]
  --help
EOF

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
	-l)	PROGLOG="$2"
		MYPROGLOG="$PROGLOG".$$
		USE_SYSLOG=false
		shift
		;;
	-N)	SHOWME=true
		USE_SYSLOG=false
		KILL="echo + pmsignal"
		;;
	-q)	QUICKSTART=true
		;;
	-V)	if $VERY_VERBOSE
		then
		    VERY_VERY_VERBOSE=true
		elif $VERBOSE
		then
		    VERY_VERBOSE=true
		else
		    VERBOSE=true
		fi
		;;
	-X)	COMPRESS_CMDLINE="$2"
		shift
		if [ -n "$PCP_COMPRESS" -a "$PCP_COMPRESS" != "$COMPRESS_CMDLINE" ]
		then
		    echo "Warning: -X value ($COMPRESS_CMDLINE) ignored because \$PCP_COMPRESS ($PCP_COMPRESS) set in environment"
		    COMPRESS_CMDLINE=""
		fi
		;;
	-Y)	COMPRESSREGEX_CMDLINE="$2"
		shift
		if [ -n "$PCP_COMPRESSREGEX" -a "$PCP_COMPRESSREGEX" != "$COMPRESSREGEX_CMDLINE" ]
		then
		    echo "Warning: -Y value ($COMPRESSREGEX_CMDLINE) ignored because \$PCP_COMPRESSREGEX ($PCP_COMPRESSREGEX) set in environment"
		    COMPRESSREGEX_CMDLINE=""
		fi
		;;
	--)	shift
		break
		;;
	-\?)	pmgetopt --usage --progname=$prog --config=$tmp/usage
		status=1
		exit
		;;
    esac
    shift
done

if [ $# -ne 0 ]
then
    pmgetopt --usage --progname=$prog --config=$tmp/usage
    status=1
    exit
fi

# after argument checking, everything must be logged to ensure no mail is
# accidentally sent from cron.  Close stdout and stderr, then open stdout
# as our logfile and redirect stderr there too.
#
if $SHOWME
then
    # Exception is for -N where we want to see the output.
    #
    :
elif [ "$PROGLOG" = "/dev/tty" ]
then
    # special case for debugging ... no salt away previous
    #
    :
else
    # Salt away previous log, if any ...
    #
    _save_prev_file "$PROGLOG"
    # After argument checking, everything must be logged to ensure no mail is
    # accidentally sent from cron.  Close stdout and stderr, then open stdout
    # as our logfile and redirect stderr there too.
    #
    exec 3>&2 1>"$MYPROGLOG" 2>&1
fi

$VERBOSE && echo >&2 "Start [janitor]: `date '+%F %T.%N'`"
$VERY_VERBOSE && _pstree_all $$

# if SaveLogs exists in the $PCP_LOG_DIR/pmlogger directory and is writeable
# then save $MYPROGLOG there as well with a unique name that contains the date
# and time when we're run ... skip if -N (showme)
#
if [ "$PROGLOG" != "/dev/tty" ]
then
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
		    echo "Start [janitor]: `date '+%F %T.%N'`" >>$link
		    echo "Args: $ARGS" >>$link
		    _pstree_all $$
		fi
	    fi
	fi
    fi
fi

QUIETLY=false

if [ ! -f "$CONTROL" ]
then
    echo "$prog: Error: cannot find control file ($CONTROL)"
    status=1
    exit
fi

_restarting()
{
    $PCP_ECHO_PROG $PCP_ECHO_N "Restarting$iam pmlogger for host \"$host\" ...""$PCP_ECHO_C"
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
    pid=`cat "$PCP_RUN_DIR/pmlogger.pid" 2>/dev/null`
    if [ -z "$pid" ]
    then
	# No PID file, try the pmcd.pmlogger.* info files where "primary"
	# is a symlink to a <pid> file
	#
	pid=`ls -l $PCP_TMP_DIR/pmlogger/primary 2>/dev/null | sed -e 's;.*/\([0-9][0-9]*\)$;\1;'`
    fi
    echo "$pid"
}

# note on control file format version
#  1.0 was shipped as part of PCPWEB beta, and did not include the
#	socks field [this is the default for backwards compatibility]
#  1.1 is the first production release, and the version is set in
#	the control file with a $version=1.1 line (see below)
#
version=''

echo >$tmp/dir

# if this file exists at the end, we encountered a serious error
#
rm -f $tmp/err

rm -f $tmp/pmloggers

# come here from _parse_log_control() once per valid line in a control
# file ... see utilproc.sh for interface definitions
#
_callback_log_control()
{
    if $VERY_VERBOSE
    then
	pflag=''
	[ $primary = y ] && pflag=' -P'
	echo "Checking for: pmlogger$pflag -h $host ... in $dir ..."
    fi

    pid=`_find_matching_pmlogger`

    if [ -n "$pid" ]
    then
	# found matching pmlogger ... cull this one from
	$VERY_VERBOSE && echo "[$filename:$line] match PID $pid, nothing to be done"
	sed <$tmp/loggers >$tmp/tmp -e "/^$pid	/d"
	mv $tmp/tmp $tmp/loggers
    fi
}

# Pass 1 - look in the pmlogger status files for those with -m "note"
# that suggests they were started by pmlogger_check and friends
#
touch $tmp/loggers
if [ -d "$PCP_TMP_DIR/pmlogger" ]
then
    find "$PCP_TMP_DIR/pmlogger" -type f \
    | while read file
    do
	pid=`echo "$file" | sed -e "s@$PCP_TMP_DIR/pmlogger/@@"`
	# timing window here, file may have gone away between
	# find(1) and awk(1), so just ignore any errors ...
	#
	$PCP_AWK_PROG <"$file" 2>&1 '
NR == 3			{ dir = $1; sub(/\/[^/]*$/, "", dir) }
NR == 4 && NF == 1	{ if ($1 == "pmlogger_check" ||
			      $1 == "reexec") {
			    print "'"$pid"'	" dir >>"'$tmp/loggers'"
			  }
			}'
    done
fi

# Pass 2 - look in the ps(1) output for processes with -m "note" args
# that suggests they were started by pmlogger_check and friends
$PCP_PS_PROG $PCP_PS_ALL_FLAGS 2>&1 \
| sed -n -e 's/$/ /' -e '/\/[p]mlogger /{
/-m *pmlogger_check /bok
/-m *reexec /bok
b
:ok
s/ $//
s/^[^ ]*  *//
s/ .*//
p
}' \
| while read pid
do
    grep "($pid) Info: Start" $PCP_ARCHIVE_DIR/*/pmlogger.log \
    | sed \
	-e 's@/pmlogger.log:.*@@' \
	-e "s/^/$pid	/" \
    | while read pid dir
    do
	if grep "^$pid	$dir\$" $tmp/loggers >/dev/null
	then
	    : already have this one from the status files
	else
	    echo "$pid	$dir" >>$tmp/loggers
	fi
    done
done

# Pass 3 - parse the control file(s) culling pmlogger instances that
# match, as these ones are still under the control of pmlogger_check
# and pmlogger_daily
#
append=`ls $CONTROLDIR 2>/dev/null | LC_COLLATE=POSIX sort | sed -e "s;^;$CONTROLDIR/;g"`
for c in $CONTROL $append
do
    _parse_log_control "$c"
done

# check for any archives from remote pmloggers via pmproxy or
# pmlogpush ... if found, synthesize a control file for them
#
here=`pwd`
if cd "$PCP_REMOTE_ARCHIVE_DIR"
then
    for _host in *
    do
	# TODO - does this need to be smarter?  e.g. check for some
	# minimal dir contents (.index file?)
	if [ -d "$_host" ]
	then
	    $VERBOSE && echo "Info: processing archives from remote pmlogger on host $_host"
	    echo '$version=1.1' >$tmp/control
	    # optional global controls first
	    [ -f "./control" ] && cat "./control" >>$tmp/control
	    # optional per-host controls next
	    [ -f "$_host/control" ] && cat "$_host/control" >>$tmp/control
	    echo "$_host	n n PCP_REMOTE_ARCHIVE_DIR/$_host +" >>$tmp/control
	    if $VERY_VERBOSE
	    then
		echo >&2 "Synthesized control file ..."
		cat >&2 $tmp/control
	    fi
	    _parse_log_control $tmp/control
	fi
    done
    cd $here
fi

if [ ! -s $tmp/loggers ]
then
    # Take early exit if nothing to be done
    #
    exit
fi

# errors/warnings from here on have nothing to do with any specific
# control file (or line in a control file)
#
filename=''

# Pass 4 - kill of orphaned pmloggers
#
rm -f $tmp/one-trip
cat $tmp/loggers \
| while read pid dir
do
    # Send pmlogger a SIGTERM, which is noted as a pending shutdown.
    # Add pid to list of loggers sent SIGTERM - may need SIGKILL later.
    #
    if [ ! -f $tmp/one-trip ]
    then
	$PCP_PS_PROG $PCP_PS_ALL_FLAGS | grep -E '[P]ID|/[p]mlogger( |$)'
	touch $tmp/one-trip
    fi
    echo "Killing (TERM) pmlogger with PID $pid"
    eval $KILL -s TERM $pid
done

# Pass 5 - compress archives in orphans' directories
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
    COMPRESSREGEX="$PCP_COMPRESSREGEX"
    [ -z "$COMPRESSREGEX" ] && COMPRESSREGEX="$COMPRESSREGEX_CMDLINE"
    [ -z "$COMPRESSREGEX" ] && COMPRESSREGEX="$COMPRESSREGEX_DEFAULT"
    cat $tmp/loggers \
    | while read pid dir
    do
	here=`pwd`
	if [ -d "$dir" ]
	then
	    if cd $dir
	    then
		$SHOWME && echo "+ cd $dir"
		find "." -type f 2>&1 \
		| _filter_filename \
		| grep -E -v "$COMPRESSREGEX" \
		| while read file
		do
		    echo "Compressing $dir/$file"
		    if $SHOWME
		    then
			echo "+ $COMPRESS $file"
		    else
			if $COMPRESS $file
			then
			    :
			else
			    _warning "$COMPRESS $file failed"
			fi
		    fi
		done
		cd $here
	    else
		_warning "cd $dir failed"
	    fi
	fi
    done
fi

# give pmloggers a chance to exit
#
sleep 3

# check all the SIGTERM'd loggers really died - if not, use a bigger hammer.
# 
if $SHOWME
then
    :
else
    cat $tmp/loggers \
    | while read pid dir
    do
	if $PCP_PS_PROG -p "$pid" >/dev/null 2>&1
	then
	    echo "Killing (KILL) pmlogger with PID $pid"
	    eval $KILL -s KILL $pid >/dev/null 2>&1
	    delay=30        # tenths of a second
	    while $PCP_PS_PROG -f -p "$pid" >$tmp/alive 2>&1
	    do
		if [ $delay -gt 0 ]
		then
		    pmsleep 0.1
		    delay=`expr $delay - 1`
		    continue
		fi
		echo "$prog: Error: pmlogger process(es) will not die"
		cat $tmp/alive
		status=1
		break
	    done
	fi
    done
fi

[ -f $tmp/err ] && status=1

# optional end logging to $PCP_LOG_DIR/NOTICES
#
if $PCP_LOG_RC_SCRIPTS
then
    $PCP_BINADM_DIR/pmpost "end pid:$$ $prog status=$status"
fi

exit

