#! /bin/sh
#
# Copyright (c) 2013-2015 Red Hat.
# Copyright (c) 2007 Aconex.  All Rights Reserved.
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
# Daily administrative script for pmie logfiles
#

. $PCP_DIR/etc/pcp.env
. $PCP_SHARE_DIR/lib/rc-proc.sh

# added to handle problem when /var/log/pcp is a symlink, as first
# reported by Micah_Altman@harvard.edu in Nov 2001
#
_unsymlink_path()
{
    [ -z "$1" ] && return
    __d=`dirname $1`
    __real_d=`cd $__d 2>/dev/null && $PWDCMND`
    if [ -z "$__real_d" ]
    then
	echo $1
    else
	echo $__real_d/`basename $1`
    fi
}

# error messages should go to stderr, not the GUI notifiers
#
unset PCP_STDERR

# constant setup
#
tmp=`mktemp -d /tmp/pcp.XXXXXXXXX` || exit 1
status=0
echo >$tmp/lock
prog=`basename $0`
PROGLOG=$PCP_LOG_DIR/pmie/$prog.log
USE_SYSLOG=true

_cleanup()
{
    $USE_SYSLOG && [ $status -ne 0 ] && \
    $PCP_SYSLOG_PROG -p daemon.error "$prog failed - see $PROGLOG"
    [ -s "$PROGLOG" ] || rm -f "$PROGLOG"
    lockfile=`cat $tmp/lock 2>/dev/null`
    rm -f "$lockfile"
    rm -rf $tmp
}
trap "_cleanup; exit \$status" 0 1 2 3 15

if is_chkconfig_on pmie
then
    PMIE_CTL=on
else
    PMIE_CTL=off
fi

# control files for pmie administration ... edit the entries in this
# file (and optional directory) to reflect your local configuration;
# see also -c option below.
CONTROL=$PCP_PMIECONTROL_PATH
CONTROLDIR=$PCP_PMIECONTROL_PATH.d

# default number of days to keep pmie logfiles
#
CULLAFTER=14

# default compression program and days until starting compression
#
COMPRESS=xz
COMPRESSAFTER=""
COMPRESSREGEX="\.(meta|index|Z|gz|bz2|zip|xz|lzma|lzo|lz4)$"

# mail addresses to send daily logfile summary to
#
MAILME=""

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
# for substituting LOCALHOSTNAME in the third column of $CONTROL.

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
  -c=FILE,--control=FILE  pmie control file
  -k=N,--discard=N        remove pmie log files after N days
  -l=FILE,--logfile=FILE  send important diagnostic messages to FILE
  -m=ADDRs,--mail=ADDRs   send daily log files to email addresses
  -N,--showme             perform a dry run, showing what would be done
  -V,--verbose            verbose output (multiple times for very verbose)
  -x=N,--compress-after=N  compress pmie log files after N days
  -X=PROGRAM,--compressor=PROGRAM  use PROGRAM for pmie log file compression
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
RM=rm
KILL=pmsignal
VERBOSE=false
VERY_VERBOSE=false
MYARGS=""

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
	-l)	PROGLOG="$2"
		USE_SYSLOG=false
		shift
		;;
	-m)	MAILME="$2"
		shift
		;;
	-N)	SHOWME=true
		USE_SYSLOG=false
		RM="echo + rm"
		KILL="echo + kill"
		MYARGS="$MYARGS -N"
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

# after argument checking, everything must be logged to ensure no mail is
# accidentally sent from cron.  Close stdout and stderr, then open stdout
# as our logfile and redirect stderr there too.
#
[ -f "$PROGLOG" ] && mv "$PROGLOG" "$PROGLOG.prev"
exec 1>"$PROGLOG"
exec 2>&1

if [ ! -f "$CONTROL" ]
then
    echo "$prog: Error: cannot find control file ($CONTROL)"
    status=1
    exit
fi

SUMMARY_LOGNAME=`pmdate -1d %Y%m%d`

_error()
{
    _report Error "$@"
    touch $tmp/err
}

_warning()
{
    _report Warning "$@"
}

_report()
{
    echo "$prog: $1: $2"
    echo "[$controlfile:$line] ... inference engine for host \"$host\" unchanged"
}

_unlock()
{
    rm -f lock
    echo >$tmp/lock
}

# filter for pmie log files in working directory -
# pass in the number of days to skip over (backwards) from today
#
# pv:821339 too many sed commands for IRIX ... split into groups
#           of at most 200 days
#
_date_filter()
{
    # start with all files whose names match the patterns used by
    # the PCP pmie log file management scripts ... this list may be
    # reduced by the sed filtering later on
    #
    ls | sed -n >$tmp/in -e '/[-.][12][0-9][0-9][0-9][0-1][0-9][0-3][0-9]/p'

    i=0
    while [ $i -le $1 ]
    do
	dmax=`expr $i + 200`
	[ $dmax -gt $1 ] && dmax=$1
	echo "/[-.][12][0-9][0-9][0-9][0-1][0-9][0-3][0-9]/{" >$tmp/sed1
	while [ $i -le $dmax ]
	do
	    x=`pmdate -${i}d %Y%m%d`
	    echo "/[-.]$x/d" >>$tmp/sed1
	    i=`expr $i + 1`
	done
	echo "p" >>$tmp/sed1
	echo "}" >>$tmp/sed1

	# cull file names with matching dates, keep other file names
	#
	sed -n -f $tmp/sed1 <$tmp/in >$tmp/tmp
	mv $tmp/tmp $tmp/in
    done

    cat $tmp/in
}

# note on control file format version
#  1.0 is the first release, and the version is set in the control file
#  with a $version=x.y line
#
version=1.0
eval `grep '^version=' "$CONTROL" | sort -rn`
if [ $version != "1.0" ]
then
    _error "unsupported version (got $version, expected 1.0)"
    status=1
    exit
fi

rm -f $tmp/err $tmp/mail

_parse_control()
{
    controlfile="$1"
    line=0

    sed -e "s;PCP_LOG_DIR;$PCP_LOG_DIR;g" $controlfile | \
    while read host socks logfile args
    do
	# start in one place for each iteration (beware relative paths)
	cd "$here"
	line=`expr $line + 1`

	# NB: FQDN cleanup: substitute the LOCALHOSTNAME marker in the config
	# line differently for the directory and the pcp -h HOST arguments.
	logfile_hostname=`hostname || echo localhost`
	logfile=`echo $logfile | sed -e "s;LOCALHOSTNAME;$logfile_hostname;"`
	logfile=`_unsymlink_path $logfile`
	[ "x$host" = "xLOCALHOSTNAME" ] && host=local:

	$VERY_VERBOSE && echo "[$controlfile:$line] host=\"$host\" socks=\"$socks\" log=\"$logfile\" args=\"$args\""

	case "$host"
	in
	    \#*|'')	# comment or empty
		continue
		;;

	    \$*)	# in-line variable assignment
		$SHOWME && echo "# $host $socks $logfile $args"
		cmd=`echo "$host $socks $logfile $args" \
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
			    echo eval $cmd >>$tmp/cmd
			    eval $cmd
			    ;;
		    esac
		fi
		continue
		;;
	esac

	[ -f $tmp/cmd ] && . $tmp/cmd
	if [ -z "$socks" -o -z "$logfile" -o -z "$args" ]
	then
	    _error "insufficient fields in control file record"
	    continue
	fi

	dir=`dirname $logfile`
	$VERY_VERBOSE && echo "Check pmie -h $host ... in $dir ..."

	if [ ! -d "$dir" ]
	then
	    [ "$PMIE_CTL" = "on" ] && \
		_error "logfile directory ($dir) does not exist"
	    continue
	fi

	cd "$dir"
	dir=`$PWDCMND`
	$SHOWME && echo "+ cd $dir"

	if $VERBOSE
	then
	    echo
	    echo "=== daily maintenance of pmie log files for host $host ==="
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

	# match $logfile from control file to running pmies
	pid=""
	$VERY_VERBOSE && echo "Looking for logfile=$logfile"
	for pidfile in `ls "$PCP_TMP_DIR/pmie"`
	do
	    p_id=$pidfile
	    pidfile="$PCP_TMP_DIR/pmie/$pidfile"
	    p_logfile=""
	    p_pmcd_host=""

	    $VERY_VERBOSE && $PCP_ECHO_PROG $PCP_ECHO_N "Check p_id=$p_id ... ""$PCP_ECHO_C"
	    if $PCP_PS_PROG -p "$p_id" >/dev/null 2>&1
	    then
		eval `$PCP_BINADM_DIR/pmiestatus $pidfile | $PCP_AWK_PROG '
NR == 2	{ printf "p_logfile=\"%s\"\n", $0; next }
NR == 3	{ printf "p_pmcd_host=\"%s\"\n", $0; next }
	{ next }'`
		$VERY_VERBOSE && $PCP_ECHO_PROG $PCP_ECHO_N "p_pmcd_host=$p_pmcd_host p_logfile=$p_logfile""$PCP_ECHO_C"
		p_logfile=`_unsymlink_path $p_logfile`
		$VERY_VERBOSE && $PCP_ECHO_PROG $PCP_ECHO_N "->$p_logfile ... ""$PCP_ECHO_C"
		if [ "$p_logfile" = $logfile ]
		then
		    pid=$p_id
		    $VERY_VERBOSE && $PCP_ECHO_PROG match
		    break
		fi
		$VERY_VERBOSE && $PCP_ECHO_PROG "no match"
	    else
		# ignore, its not a running process
		eval $RM -f $pidfile
		$VERY_VERBOSE && $PCP_ECHO_PROG "process has vanished"
	    fi
	done

	if [ -z "$pid" ]
	then
	    if [ "$PMIE_CTL" = "on" ]
	    then
		_error "no pmie instance running for host \"$host\""
	    fi
	else
	    # now move current logfile name aside and SIGHUP to "roll the logs"
	    # creating a new logfile with the old name in the process.
	    #
	    $SHOWME && echo "+ mv $logfile ${logfile}.{SUMMARY_LOGNAME}"
	    if mv $logfile ${logfile}.${SUMMARY_LOGNAME}
	    then
		$VERY_VERBOSE && echo "+ $KILL -s HUP $pid"
		eval $KILL -s HUP $pid
		echo ${logfile}.${SUMMARY_LOGNAME} >> $tmp/mail
	    else
		_error "problems moving logfile \"$logfile\" for host \"$host\""
		touch $tmp/err
	    fi
	fi

	# and cull old logfiles
	#
	if [ X"$CULLAFTER" != X"forever" ]
	then
	    _date_filter $CULLAFTER >$tmp/list
	    if [ -s $tmp/list ]
	    then
		if $VERBOSE
		then
		    echo "Log files older than $CULLAFTER days being removed ..."
		    fmt <$tmp/list | sed -e 's/^/    /'
		fi
		if $SHOWME
		then
		    cat $tmp/list | xargs echo + rm -f
		else
		    cat $tmp/list | xargs rm -f
		fi
	    fi
	fi

	# finally, compress old log files
	# (after cull - don't compress unnecessarily)
	#
	if [ ! -z "$COMPRESSAFTER" ]
	then
	    _date_filter $COMPRESSAFTER | egrep -v "$COMPRESSREGEX" >$tmp/list
	    if [ -s $tmp/list ]
	    then
		if $VERBOSE
		then
		    echo "Log files older than $COMPRESSAFTER days being compressed ..."
		    fmt <$tmp/list | sed -e 's/^/    /'
		fi
		if $SHOWME
		then
		    cat $tmp/list | xargs echo + $COMPRESS
		else
		    cat $tmp/list | xargs $COMPRESS
		fi
	    fi
	fi

	_unlock
    done
}

_parse_control $CONTROL
append=`ls $CONTROLDIR 2>/dev/null | LC_COLLATE=POSIX sort`
for controlfile in $append
do
    _parse_control $CONTROLDIR/$controlfile
done

if [ -n "$MAILME" -a -s $tmp/mail ]
then
    logs=""
    for logfile in `cat $tmp/mail`
    do
	[ -f $logfile ] && logs="$logs $logfile"
    done
    egrep -v '( OK | OK$|^$|^Log |^pmie: PID)' $logs > $tmp/logmail
    if [ ! -s "$tmp/logmail" ]
    then
	:
    elif [ ! -z "$MAIL" ]
    then
	egrep -v '( OK | OK$|^$)' $logs | \
	    $MAIL -s "PMIE summary for $LOCALHOSTNAME" $MAILME
    else
	echo "$prog: PMIE summary for $LOCALHOSTNAME ..."
	egrep -v '( OK | OK$|^$)' $logs
    fi
    rm -f $tmp/mail $tmp/logmail
fi

[ -f $tmp/err ] && status=1
exit
