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
    $VERY_VERBOSE && echo "End: `date '+%F %T.%N'`"
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
COMPRESSREGEX=""
COMPRESSREGEX_CMDLINE=""
COMPRESSREGEX_DEFAULT="\.(index|Z|gz|bz2|zip|xz|lzma|lzo|lz4)$"

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
    :
elif [ "$PROGLOG" = "/dev/tty" ]
then
    # special case for debugging ... no salt away previous, no chown, no exec
    #
    :
else
    # Salt away previous log, if any ...
    #
    PROGLOGDIR=`dirname "$PROGLOG"`
    [ -d "$PROGLOGDIR" ] || mkdir_and_chown "$PROGLOGDIR" 755 $PCP_USER:$PCP_GROUP 2>/dev/null
    _save_prev_file "$PROGLOG"
    # After argument checking, everything must be logged to ensure no mail is
    # accidentally sent from cron.  Close stdout and stderr, then open stdout
    # as our logfile and redirect stderr there too.  Create the log file with
    # correct ownership first.
    #
    # Exception ($SHOWME, above) is for -N where we want to see the output.
    #
    if touch "$MYPROGLOG" 2>/dev/null
    then
	:
    else
	MYPROGLOG=/var/tmp/pmlogger_janitor.$$
    fi
    chown $PCP_USER:$PCP_GROUP "$MYPROGLOG" >/dev/null 2>&1
    exec 3>&2 1>"$MYPROGLOG" 2>&1
fi

if $VERY_VERBOSE
then
    echo "Start: `date '+%F %T.%N'`"
    _pstree_all $$
fi

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
		    echo "Start: `date '+%F %T.%N'`" >>$link
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

_error()
{
    [ -n "$controlfile" ] && echo "$prog: [$controlfile:$line]"
    echo "Error: $@"
    touch $tmp/err
}

_warning()
{
    [ -n "$controlfile" ] && echo "$prog [$controlfile:$line]"
    echo "Warning: $@"
}

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

_parse_control()
{
    controlfile="$1"
    line=0

    if echo "$controlfile" | grep -q -e '\.rpmsave$' -e '\.rpmnew$' -e '\.rpmorig$' \
	-e '\.dpkg-dist$' -e '\.dpkg-old$' -e '\.dpkg-new$' >/dev/null 2>&1
    then
	_warning "ignored backup control file \"$controlfile\""
	return
    fi

    sed \
	-e "s;PCP_ARCHIVE_DIR;$PCP_ARCHIVE_DIR;g" \
	-e "s;PCP_LOG_DIR;$PCP_LOG_DIR;g" \
	$controlfile | \
    while read host primary socks dir args
    do
	# start in one place for each iteration (beware relative paths)
	cd "$here"
	line=`expr $line + 1`


	if $VERY_VERBOSE 
	then
	    case "$host"
	    in
	    \#*|'')	# comment or empty
			;;
	    *)		echo "[$controlfile:$line] host=\"$host\" primary=\"$primary\" socks=\"$socks\" dir=\"$dir\" args=\"$args\""
	    		;;
	    esac
	fi

	case "$host"
	in
	    \#*|'')	# comment or empty
		continue
		;;
	    \$*)	# in-line variable assignment
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
			    echo eval $cmd >>$tmp/cmd
			    eval $cmd
			    ;;
		    esac
		fi
		continue
		;;
	esac

	# set the version and other variables
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

	if $VERY_VERBOSE
	then
	    pflag=''
	    [ $primary = y ] && pflag=' -P'
	    echo "Checking for: pmlogger$pflag -h $host ... in $dir ..."
	fi

	# check for directory duplicate entries
	#
	if [ "`grep $dir $tmp/dir`" = "$dir" ]
	then
	    _error "Cannot start more than one pmlogger instance for archive directory \"$dir\""
	    continue
	else
	    echo "$dir" >>$tmp/dir
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

	# and the logfile is writeable, if it exists
	#
	[ -f "$logfile" ] && chown $PCP_USER:$PCP_GROUP "$logfile" >/dev/null 2>&1

	if cd "$dir"
	then
	    :
	else
	    _error "cannot chdir to directory ($dir) for PCP archive files"
	    continue
	fi
	dir=`$PWDCMND`

	if [ ! -w "$dir" ]
	then
	    _warning "no write access in $dir, skip lock file processing"
	    ls -ld "$dir"
	else
	    # demand mutual exclusion
	    #
	    rm -f $tmp/stamp $tmp/out
	    delay=200	# tenths of a second
	    while [ $delay -gt 0 ]
	    do
		if pmlock -v "$dir/lock" >$tmp/out 2>&1
		then
		    echo "$dir/lock" >$tmp/lock
		    if $VERY_VERBOSE
		    then
			echo "Acquired lock:"
			ls -l $dir/lock
		    fi
		    break
		else
		    [ -f $tmp/stamp ] || touch -t `pmdate -30M %Y%m%d%H%M` $tmp/stamp
		    find $tmp/stamp -newer "$dir/lock" -print 2>/dev/null >$tmp/tmp
		    if [ -s $tmp/tmp ]
		    then
			if [ -f "$dir/lock" ]
			then
			    echo "$prog: Warning: removing lock file older than 30 minutes"
			    LC_TIME=POSIX ls -l $dir/lock
			    rm -f "$dir/lock"
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
		# maybe pmlogger_daily is running ... check it, and silently
		# move on if this is the case
		#
		# Note: $PCP_RUN_DIR may not exist (see pmlogger_daily note),
		#       but only if pmlogger_daily has not run, so no chance
		#       of a collision
		#
		if [ -f "$PCP_RUN_DIR"/pmlogger_daily.pid ]
		then
		    # maybe, check pid matches a running /bin/sh
		    #
		    pid=`cat "$PCP_RUN_DIR"/pmlogger_daily.pid`
		    if _get_pids_by_name sh | grep "^$pid\$" >/dev/null
		    then
			# seems to be still running ... nothing for us to see
			# or do here
			#
			continue
		    fi
		fi
		if [ -f "$dir/lock" ]
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
	if [ "$primary" = y ]
	then
	    if test -e "$PCP_TMP_DIR/pmlogger/primary"
	    then
		if $VERY_VERBOSE
		then 
		    _host=`sed -n 2p <"$PCP_TMP_DIR/pmlogger/primary"`
		    _arch=`sed -n 3p <"$PCP_TMP_DIR/pmlogger/primary"`
		    echo "... try $PCP_TMP_DIR/pmlogger/primary: host=$_host arch=$_arch"
		fi
		pid=`_get_primary_logger_pid`
		if [ -z "$pid" ]
		then
		    if $VERY_VERBOSE
		    then
			echo "primary pmlogger process pid not found"
			ls -l "$PCP_RUN_DIR/pmlogger.pid"
			ls -l "$PCP_TMP_DIR/pmlogger"
		    fi
		elif _get_pids_by_name pmlogger | grep "^$pid\$" >/dev/null
		then
		    $VERY_VERBOSE && echo "primary pmlogger process $pid identified, OK"
		    $VERY_VERY_VERBOSE && $PCP_PS_PROG $PCP_PS_ALL_FLAGS | grep -E '[P]ID|[p]mlogger '
		else
		    $VERY_VERBOSE && echo "primary pmlogger process $pid not running"
		    $VERY_VERY_VERBOSE && $PCP_PS_PROG $PCP_PS_ALL_FLAGS | grep -E '[P]ID|[p]mlogger '
		    pid=''
		fi
	    else
		if $VERY_VERBOSE
		then
		    echo "$PCP_TMP_DIR/pmlogger/primary: missing?"
		    echo "Contents of $PCP_TMP_DIR/pmlogger"
		    ls -l $PCP_TMP_DIR/pmlogger
		    echo "--- end of ls output ---"
		fi
	    fi
	else
	    for log in $PCP_TMP_DIR/pmlogger/[0-9]*
	    do
		[ "$log" = "$PCP_TMP_DIR/pmlogger/[0-9]*" ] && continue
		if $VERY_VERBOSE
		then
		    _host=`sed -n 2p <$log`
		    _arch=`sed -n 3p <$log`
		    $PCP_ECHO_PROG $PCP_ECHO_N "... try $log host=$_host arch=$_arch: ""$PCP_ECHO_C"
		fi
		# throw away stderr in case $log has been removed by now
		match=`sed -e '3s@/[^/]*$@@' $log 2>/dev/null | \
		$PCP_AWK_PROG '
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
			$VERY_VERY_VERBOSE && $PCP_PS_PROG $PCP_PS_ALL_FLAGS | grep -E '[P]ID|[p]mlogger '
			break
		    fi
		    $VERY_VERBOSE && echo "pmlogger process $pid not running, skip"
		    $VERY_VERY_VERBOSE && $PCP_PS_PROG $PCP_PS_ALL_FLAGS | grep -E '[P]ID|[p]mlogger '
		    pid=''
		else
		    $VERY_VERBOSE && echo "different directory, skip"
		fi
	    done
	fi

	if [ -n "$pid" ]
	then
	    # found matching pmlogger ... cull this one from
	    $VERY_VERBOSE && echo "[$controlfile:$line] match PID $pid, nothing to be done"
	    sed <$tmp/loggers >$tmp/tmp -e "/^$pid	/d"
	    mv $tmp/tmp $tmp/loggers
	fi

	_unlock "$dir"
    done
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
| sed -n '/\/[p]mlogger /{
/-m *pmlogger_check /bok
/-m *reexec /bok
b
:ok
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
    _parse_control "$c"
done

if [ ! -s $tmp/loggers ]
then
    # Take early exit if nothing to be done
    #
    exit
fi

# errors/warnings from here on have nothing to do with any specific
# control file (or line in a control file)
#
controlfile=''

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
	$PCP_PS_PROG $PCP_PS_ALL_FLAGS | grep -E '[P]ID|[p]mlogger '
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

