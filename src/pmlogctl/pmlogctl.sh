#! /bin/sh
#
# Control program for managing pmlogger and pmie instances.
#
# Copyright (c) 2020 Ken McDonell.  All Rights Reserved.
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
# TODO
# - man page
# - qa
# - glob expansion for <class> and <host> names
# - resolve the semantics of specifying <class> and <host> in the control
#   actions (all but create where the semantics are clear)
# - create => create + start ... is there a case for create only?  
# - destroy => stop + destroy ... is there a case for destroy iff already
#   stopped?
# - handling of multiple pmloggers for the same host (using different
#   directories) ... probably does not work at the moment .... there are
#   checks (untested) for this in the script, but the control files may
#   already be in this state through upgrade or manual editing
# - multiple pmloggers in the one control file ... create will not do
#   this and destroy (will) check for it before removing the control
#   file, but I'd like to avoid a "migration" path 'cause splitting a
#   control file is messy and potentially dangerous, and subsequent manual
#   editing could bring the situation back at any time
# - pmfind integration
# - other sections in the "policy" files, especially with pmfind to
#   (a) at create, pick a class by probing the host
#   (b) at create, decide not to by probing the host or by hostname
#       pattern
#   (c) at destroy, decide not to or wait some time before destroying
#       (the latter is really hard)
# - IAM=pmie is completely untested
#

. $PCP_DIR/etc/pcp.env

prog=`basename $0`
case "$prog"
in
    pmlogctl*)	IAM=pmlogger
		CONTROLFILE=$PCP_PMLOGGERCONTROL_PATH
    		;;
    pmiectl*)	IAM=pmie
		CONTROLFILE=$PCP_PMIECONTROL_PATH
    		;;
    *)		echo >&2 "$0: who the hell are you, bozo?"
    		exit 1
		;;
esac
CONTROLDIR=${CONTROLFILE}.d

tmp=`mktemp -d $PCP_TMPFILE_DIR/${IAM}.XXXXXXXXX` || exit 1
status=0

_cleanup()
{
    _unlock
    rm -rf $tmp
}
trap "_cleanup; exit \$status" 0 1 2 3 15

cat >$tmp/usage <<End-of-File
# Usage: [options] command [arg ...]

Options:
  -c=NAME,--class=NAME    	${IAM} instances belong to the NAME class \
[default: default]
  -n,--showme             	perform a dry run, showing what would be done
  -p=POLICY,--policy=POLICY	use POLICY as the policy file \
[default: $PCP_ETC_DIR/pcp/${IAM}/policy.d/<class>]
  -v,--verbose            	increase verbosity
  --help
End-of-File

_warning()
{
    echo >&2 "Warning: $1"
}

_error()
{
    echo >&2 "Error: $1"
    status=1
    exit
}

_lock()
{
    _dir="$PCP_ETC_DIR/pcp/${IAM}"
    if [ ! -w "$_dir" ]
    then
	_warning "no write access in \"$_dir\" skip lock file processing"
    else
	# demand mutual exclusion
	#
	rm -f $tmp/stamp $tmp/out
	_delay=200		# tenths of a second, so max wait is 20sec
	while [ $_delay -gt 0 ]
	do
	    if pmlock -v "$_dir/lock" >>$tmp/out 2>&1
	    then
		echo "$$" >"$_dir/lock"
		break
	    else
		[ -f $tmp/stamp ] || touch -t `pmdate -30M %Y%m%d%H%M` $tmp/stamp
		find $tmp/stamp -newer "$_dir/lock" -print 2>/dev/null >$tmp/tmp
		if [ -s $tmp/tmp ]
		then
		    if [ -f "$_dir/lock" ]
		    then
			_warning "removing lock file older than 30 minutes (PID `cat $_dir/lock`)"
			LC_TIME=POSIX ls -l "$_dir/lock"
			rm -f "$_dir/lock"
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
	    _delay=`expr $_delay - 1`
	done

	if [ $_delay -eq 0 ]
	then
	    # failed to gain mutex lock
	    #
	    if [ -f "$_dir/lock" ]
	    then
		_warning "is another ${IAM}ctl job running concurrently?"
		LC_TIME=POSIX ls -l "$_dir/lock"
	    else
		_error "`cat $tmp/out`"
	    fi
	    _error "failed to acquire exclusive lock ($_dir/lock) ..."
	    return 1
	else
	    if $VERY_VERBOSE
	    then
		echo "Lock acquired `cat $_dir/lock` `ls -l $_dir/lock`"
	    fi
	fi
    fi

    return 0
}

_unlock()
{
    _dir="$PCP_ETC_DIR/pcp/${IAM}"
    if [ -f "$_dir/lock" ]
    then
	rm -f "$_dir/lock"
	$VERY_VERBOSE && echo "Lock released"
    fi
}

_usage()
{
    pmgetopt >&2 --progname=$prog --config=$tmp/usage --usage
    cat >&2 <<End-of-File

Avaliable commands:
   create [-c class] host ...
   status [host ...]
   {start|stop|restart|destroy} [-c class] [host ...]
End-of-File
    status=1
    exit
}

# $1 is policy file (known to already exist)
# $2 is section name (expect alphabetic(s): at start of line)
# strip backslashes before output
#
_get_policy_section()
{
    $PCP_AWK_PROG <"$1" '
$1 == "'"$2"':"		{ want = 1; next }
$1 ~ /^[a-z]*:$/	{ want = 0; next }
want == 1		{ print }' \
    | sed -e 's;\\;;g'
}

# do what ${IAM}_check does to a control line in terms of variable
# expansion
#
_expand_control()
{
    sed \
	-e 's/[ 	][ 	]*/ /g' \
	-e "s; PCP_ARCHIVE_DIR; $PCP_ARCHIVE_DIR;g" \
	-e "s/LOCALHOSTNAME /`hostname` /g" \
    # end
}

# status command
#
_do_status()
{
    # TODO - deal with args (limit report to named hosts)

    # see if system-level controls have stopped (all) ${IAM} processes
    #
    systemctl_state=''
    if which systemctl >/dev/null 2>&1
    then
	if [ -n "$PCP_SYSTEMDUNIT_DIR" -a -f "$PCP_SYSTEMDUNIT_DIR/${IAM}.service" ]
	then
	    # systemctl is handling this
	    #
	    if [ "`systemctl is-enabled ${IAM}.service`" = enabled ]
	    then
		if [ "`systemctl is-active ${IAM}.service`" = active ]
		then
		    # all healthy
		    :
		else
		    systemctl_state='stopped by systemctl'
		fi
	    else
		systemctl_state='disabled by systemctl'
	    fi
	fi
    fi
    fmt="%-20s %-17s %-8s %7s %-8s\n"
    printf "$fmt" "pmcd Host" Archive Class PID State
    find $PCP_TMP_DIR/${IAM} -type f -a ! -name primary \
    | while read f
    do
	sed -n -e 3p $f
    done >>$tmp/archive
    class=''
    egrep -r "^([^#]|#!#)" $CONTROLFILE $CONTROLDIR \
    | sed \
	-e 's/^[^:]*://' \
    | _expand_control \
    | while read host primary socks dir args
    do
	state=running
	case "$host"
	in
	    \$class=*)	class=`echo "$host" | sed -e 's/.*=//'`
			continue
			;;
	    \$*)	continue
			;;
	    \#!\#*)	host=`echo "$host" | sed -e 's/^#!#//'`
			state='stopped by pmlogctl'
			;;
	esac
	[ "$primary" = y ] && class=primary
	archive=`grep $dir $tmp/archive \
	         | sed -e 's;.*/;;'`
	[ -z "$archive" ] && archive='?'
	if [ "$archive" = '?' ]
	then
	    if [ "$state" = running ]
	    then
		if [ -n "$systemctl_state" ]
		then
		    state="$systemctl_state"
		else
		    state="dead"
		fi
	    fi
	fi
	pid=`grep -rl "$dir" $PCP_TMP_DIR/${IAM} \
	     | sed -e 's;.*/;;'`
	[ -z "$pid" ] && pid='?'
	printf "$fmt" "$host" "$archive" "$class" "$pid" "$state"
	class=''
    done \
    | sort
}

# create command
#
_do_create()
{
    host="$1"
    egrep -lr "^($host|#!#$host)[ 	]" $CONTROLFILE $CONTROLDIR >$tmp/out
    [ -s $tmp/out ] && _error "host \"$host\" already defined in `cat $tmp/out`"
    [ -f $CONTROLDIR/$host ] && _error "control file $CONTROLDIR/$host already exists"
    cat <<End-of-File >$tmp/control
# created by pmlogctl on `date`
\$class=$CLASS

End-of-File
    _get_policy_section "$POLICY" control >$tmp/tmp
    [ ! -s $tmp/tmp ] && _error "\"control:\" section is missing from $POLICY policy file"
    sed -e "s/%h/$host/g" <$tmp/tmp >>$tmp/control
    if $SHOWME
    then
	echo "--- start control file ---"
	cat $tmp/control
	echo "--- end control file ---"
    fi
    $CP $tmp/control $CONTROLDIR/$host
    $CHECK -c $CONTROLDIR/$host
    # TODO ... check really started
    return 0
}

# destroy command
#
_do_destroy()
{
    echo TODO
    return 0
}

# start command
#
_do_start()
{
    host="$1"
    egrep -r "^($host|#!#$host)[ 	]" $CONTROLFILE $CONTROLDIR >$tmp/out
    nhost=`wc -l <$tmp/out | sed -e 's/ //g'`
    if [ "$nhost" -eq 0 ]
    then
	_warning "host \"$host\" not defined in any ${IAM} control file"
	return 1
    elif [ "$nhost" -gt 1 ]
    then
	_error "host \"$host\" is defined in multiple times, don't know which one to start"
	sed <$tmp/out >&2 \
	    -e 's/:/ /'
	return 1
    fi
    dir=`cat $tmp/out | _expand_control | $PCP_AWK_PROG '{print $4}'`
    $VERBOSE && echo "Looking for ${IAM} using dir \"$dir\" ..."
    pid=`grep -rl "^$dir/" $PCP_TMP_DIR/${IAM} \
	 | sed -e 's;.*/;;'`
    if [ -n "$pid" ]
    then
	$VERBOSE && echo "${IAM} PID $pid already running for host \"$host\", nothing to do"
	return 0
    else
	$VERBOSE && echo "Not found, launching new ${IAM}"
    fi
    control=`cat $tmp/out | sed -e 's/:.*//'`
    if [ ! -f "$control" ]
    then
	_warning "control file \"$control\" for host \"$host\" ${IAM} has vanished"
	return 1
    fi
    if grep "^[^:]*:#!#$host[ 	]" $tmp/out >/dev/null
    then
	# stopped by pmlogctl, remove the stopped prefix (#!#)
	#
	if $SHOWME
	then
	    echo "+ unmark host as stopped in $control"
	else
	    sed -e "/^#!#$host[ 	]/s/^#!#//" <"$control" >$tmp/control
	    $VERBOSE && diff "$control" $tmp/control
	    $CP $tmp/control "$control"
	fi
    fi
    $CHECK -c $control
    # TODO ... check really started
    return 0
}

# stop command
#
_do_stop()
{
    host="$1"
    egrep -r "^($host|#!#$host)[ 	]" $CONTROLFILE $CONTROLDIR >$tmp/out
    nhost=`wc -l <$tmp/out | sed -e 's/ //g'`
    if [ "$nhost" -eq 0 ]
    then
	_warning "host \"$host\" not defined in any ${IAM} control file"
	return 1
    elif [ "$nhost" -gt 1 ]
    then
	_error "host \"$host\" is defined in multiple times, don't know which one to stop"
	sed <$tmp/out >&2 \
	    -e 's/:/ /'
	return 1
    elif grep "^[^:]*:#!#$host[ 	]" $tmp/out >/dev/null
    then
	_warning "${IAM} for host \"$host\" already stopped, nothing to do"
	return 0
    fi
    dir=`cat $tmp/out | _expand_control | $PCP_AWK_PROG '{print $4}'`
    $VERBOSE && echo "Looking for ${IAM} using dir \"$dir\" ..."
    pid=`grep -rl "^$dir/" $PCP_TMP_DIR/${IAM} \
	 | sed -e 's;.*/;;'`
    if [ -z "$pid" ]
    then
	_warning "cannot find PID for host \"$host\" ${IAM}, already exited?"
	return 1
    fi
    $VERBOSE && echo "Found PID \"$pid\" to stop"
    control=`cat $tmp/out | sed -e 's/:.*//'`
    if [ ! -f "$control" ]
    then
	_warning "control file \"$control\" for host \"$host\" ${IAM} has vanished"
	return 1
    fi
    $KILL TERM $pid
    # TODO ... check really stopped
    if $SHOWME
    then
	echo "+ mark host as stopped in $control"
    else
	sed -e "/^$host[ 	]/s/^/#!#/" <"$control" >$tmp/control
	$VERBOSE && diff "$control" $tmp/control
	$CP $tmp/control "$control"
    fi
    return 0
}

# restart command
#
_do_restart()
{
    host="$1"
    if _do_stop "$host"
    then
	if _do_start "$host"
	then
	    :
	else
	    _error "failed to stop host \"$host\""
	fi
    else
	_error "failed to start host \"$host\""
    fi

    return 0
}

ARGS=`pmgetopt --progname=$prog --config=$tmp/usage -- "$@"`
[ $? != 0 ] && exit 1

eval set -- "$ARGS"

SHOWME=false
CP=cp
CHECK="sudo -u $PCP_USER -g $PCP_GROUP $PCP_BINADM_DIR/${IAM}_check"
KILL="$PCP_BINADM_DIR/pmsignal -s"
VERBOSE=false
VERY_VERBOSE=false
CLASS=default
POLICY=''
EXPLICIT_CLASS=false
while [ $# -gt 0 ]
do
    case "$1"
    in
	-c)	CLASS="$2"
		EXPLICIT_CLASS=true
		shift
		;;
	-n)	SHOWME=true
		CP="echo + $CP"
		CHECK="echo + $CHECK"
		KILL="echo + $KILL"
		;;
	-p)	POLICY="$2"
		;;
	-v)	if $VERBOSE
		then
		    VERY_VERBOSE=true
		else
		    VERBOSE=true
		fi
		;;
	--)	shift
		break
		;;
	-\?)	_usage
		# NOTREACHED
		;;
    esac
    shift
done

if [ $# -lt 1 ]
then
    _usage
    # NOTREACHED
fi

if [ `id -u` != 0 -a "$1" != "status" ]
then
    _error "You must be root (uid 0) to change the Performance Co-Pilot logger setup"
    status=1
    exit
fi

[ "$1" != "status" ] && _lock

if $VERBOSE
then
    if $EXPLICIT_CLASS
    then
	echo "Using class: $CLASS"
    else
	echo "Using default class"
    fi
fi
[ -z "$POLICY" ] && POLICY="$PCP_ETC_DIR/pcp/${IAM}/policy.d/$CLASS"
if [ "$CLASS" = default ]
then
    if [ ! -f "$POLICY" ]
    then
	# This is the _real_ default policy, when there is no
	# $PCP_ETC_DIR/pcp/${IAM}/policy.d/default
	#
	cat <<'End-of-File' >$tmp/policy
control:
\#DO NOT REMOVE OR EDIT THE FOLLOWING LINE
$version=1.1
$class=default
%h n n PCP_ARCHIVE_DIR/%h -c config.default

destroy:
manual
End-of-File
	POLICY=$tmp/policy
	$VERBOSE && echo "Using default policy"
    fi
else
    if [ ! -f "$POLICY" ]
    then
	_error "policy file \"$POLICY\" does not exist"
	status=1
	exit
    fi
    $VERBOSE && echo "Using policy: $POLICY"
fi

action=$1
case "$action"
in
    create)
	    shift
	    if [ $# -lt 1 ]
	    then
		_error "\"create\" command requires at least one hostname argument"
		status=1
		exit
	    fi
	    _do_create "$1"
	    ;;

    start|stop|restart|destroy)
	    shift
	    if [ $# -eq 0 ]
	    then
		if ! $CLASSS_EXPLICIT
		then
		    _error "\"$action\" command requres hostname(s) and/or a --class"
		    status=1
		fi
	    fi
	    for host
	    do
		# TODO ... $host ... could be a host pattern and/or --class
		# could be a class pattern or maybe even "primary"
		#
		eval "_do_$action" "$host"
		cmd_sts=$?
		if [ $cmd_sts -ne 0 ]
		then
		    _warning "action not completed"
		fi
	    done
	    ;;

    status)
	    shift
	    _do_status $*
	    ;;

    *)	    _error "command \"$action\" not known"
	    status=1
	    exit
	    ;;
esac

exit
