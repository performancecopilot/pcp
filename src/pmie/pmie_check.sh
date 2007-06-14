#! /bin/sh
#Tag 0x00010D13
#
# Copyright (c) 1998-2000,2003 Silicon Graphics, Inc.  All Rights Reserved.
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
# $Id: pmie_check.sh,v 1.9 2005/01/19 00:04:20 kenmcd Exp $
#
# Administrative script to check pmie processes are alive, and restart
# them as required.
#

# Get standard environment
. /etc/pcp.env

PMIE=pmie

# added to handle problem when /var/log/pcp is a symlink, as first
# reported by Micah_Altman@harvard.edu in Nov 2001
#
_unsymlink_path()
{
    [ -z "$1" ] && return
    __d=`dirname $1`
    __real_d=`cd $__d 2>/dev/null && /bin/pwd`
    if [ -z "$__real_d" ]
    then
	echo $1
    else
	echo $__real_d/`basename $1`
    fi
}

have_pmieconf=false
if which pmieconf >/dev/null 2>&1
then
    have_pmieconf=true
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

# control file for pmie administration ... edit the entries in this
# file to reflect your local configuration
#
CONTROL=$PCP_VAR_DIR/config/pmie/control

# determine real name for localhost
LOCALHOSTNAME=`hostname | sed -e 's/\..*//'`
if [ -z "$LOCALHOSTNAME" ]
then
    echo "$prog: Error: cannot determine hostname, giving up"
    exit 1
fi

# determine whether SGI Embedded Support Partner events need to be used
CONFARGS="-F"
if which esplogger >/dev/null 2>&1
then
    CONFARGS='m global syslog_prefix $esp_prefix$'
fi

# option parsing
#
SHOWME=false
MV=mv
RM=rm
CP=cp
KILL=kill
VERBOSE=false
VERY_VERBOSE=false
START_PMIE=true
usage="Usage: $prog [-NsV] [-c control]"
while getopts c:NsV? c
do
    case $c
    in
	c)	CONTROL="$OPTARG"
		;;
	N)	SHOWME=true
		MV="echo + mv"
		RM="echo + rm"
		CP="echo + cp"
		KILL="echo + kill"
		;;
	s)	START_PMIE=false
		;;
	V)	if $VERBOSE
		then
		    VERY_VERBOSE=true
		else
		    VERBOSE=true
		fi
		;;
	?)	echo "$usage"
		status=1
		exit
		;;
    esac
done
shift `expr $OPTIND - 1`

if [ $# -ne 0 ]
then
    echo "$usage"
    status=1
    exit
fi

_error()
{
    echo "$prog: [$CONTROL:$line]"
    echo "Error: $1"
    echo "... automated performance reasoning for host \"$host\" unchanged"
    touch $tmp.err
}

_warning()
{
    echo "$prog [$CONTROL:$line]"
    echo "Warning: $1"
}

_message()
{
    case $1
    in
	'restart')
	    $PCP_ECHO_PROG $PCP_ECHO_N "Restarting pmie for host \"$host\" ..."
	    ;;
    esac
}

_lock()
{
    # demand mutual exclusion
    #
    fail=true
    rm -f $tmp.stamp
    for try in 1 2 3 4
    do
	if pmlock -v $logfile.lock >$tmp.out
	then
	    echo $logfile.lock >$tmp.lock
	    fail=false
	    break
	else
	    if [ ! -f $tmp.stamp ]
	    then
		touch -t `pmdate -30M %Y%m%d%H%M` $tmp.stamp
	    fi
	    if [ -n "`find $logfile.lock ! -newer $tmp.stamp -print 2>/dev/null`" ]
	    then
		_warning "removing lock file older than 30 minutes"
		ls -l $logfile.lock
		rm -f $logfile.lock
	    fi
	fi
	sleep 5
    done

    if $fail
    then
	# failed to gain mutex lock
	#
	if [ -f $logfile.lock ]
	then
	    _warning "is another PCP cron job running concurrently?"
	    ls -l $logfile.lock
	else
	    echo "$prog: `cat $tmp.out`"
	fi
	_warning "failed to acquire exclusive lock ($logfile.lock) ..."
	continue
    fi
}

_unlock()
{
    rm -f $logfile.lock
    echo >$tmp.lock
}

_check_logfile()
{
    if [ ! -f $logfile ]
    then
	echo "$prog: Error: cannot find pmie output file at \"$logfile\""
	logdir=`dirname $logfile`
	echo "Directory (`cd $logdir; pwd`) contents:"
	ls -la $logdir
    else
	echo "Contents of pmie output file \"$logfile\" ..."
	cat $logfile
    fi
}

_check_pmie_version()
{
    # the -C option was introduced at the same time as the $PCP_TMP_DIR/pmie
    # stats file support (required for pmie_check), so if this produces a
    # non-zero exit status, bail out
    # 
    if $PMIE -C /dev/null >/dev/null 2>&1
    then
	:
    else
	binary=`which $PMIE`
	echo "$prog: Error: wrong version of $binary installed"
	cat - <<EOF
You seem to have mixed versions in your Performance Co-Pilot (PCP)
installation, such that pmie(1) is incompatible with pmie_check(1).
EOF
	if [ "$PCP_PLATFORM" = "irix" ]
	then
	    #
	    # The following only makes sense on IRIX
	    #
	    echo
	    showfiles pcp\* \
	    | $PCP_AWK_PROG '/bin\/pmie$/ {print $4}' >$tmp.subsys

	    if [ -s $tmp.subsys ]
	    then
		echo "Currently $binary is installed from these subsystem(s):"
		echo
		versions `cat $tmp.subsys` </dev/null
		echo
	    else
		echo "The current installation history does not identify the subsystem(s)"
		echo "where $binary was installed from."
		echo
	    fi

	    case `uname -r`
	    in
		6.5*)
		    echo "Please upgrade pcp_eoe from the IRIX 6.5.5 (or later) distribution."
		    ;;
		*)
		    echo "Please upgrade pcp_eoe from the PCP 2.1 (or later) distribution."
		    ;;
	    esac
	fi

	status=1
	exit
    fi
}

_check_pmie()
{
    $VERBOSE && $PCP_ECHO_PROG $PCP_ECHO_N " [process $1] ""$PCP_ECHO_C"

    # wait until pmie process starts, or exits
    #
    delay=5
    [ ! -z "$PMCD_CONNECT_TIMEOUT" ] && delay=$PMCD_CONNECT_TIMEOUT
    x=5
    [ ! -z "$PMCD_REQUEST_TIMEOUT" ] && x=$PMCD_REQUEST_TIMEOUT

    # wait for maximum time of a connection and 20 requests
    #
    delay=`expr $delay + 20 \* $x`
    i=0
    while [ $i -lt $delay ]
    do
	$VERBOSE && $PCP_ECHO_PROG $PCP_ECHO_N ".""$PCP_ECHO_C"
	if [ -f $logfile ]
	then
	    # $logfile was previously removed, if it has appeared again then
	    # we know pmie has started ... if not just sleep and try again
	    #
	    if ls $PCP_TMP_DIR/pmie/$1 >$tmp.out 2>&1
	    then
		if grep "No such file or directory" $tmp.out >/dev/null
		then
		    :
		else
		    sleep 5
		    $VERBOSE && echo " done"
		    return 0
		fi
	    fi
	    case "$PCP_PLATFORM"
	    in
		irix)
		    ps -e | grep "^ *$1 " >/dev/null
		    ;;
		linux)
		    test -e /proc/$1 
		    ;;
	    esac

	    if [ $? -ne 0 ] 
	    then
		$VERBOSE || _message restart
		echo " process exited!"
		echo "$prog: Error: failed to restart pmie"
		echo "Current pmie processes:"
		ps $PCP_PS_ALL_FLAGS | sed -n -e 1p -e "/$PMIE/p"
		echo
		_check_logfile
		return 1
	    fi
	fi
	sleep 5
	i=`expr $i + 5`
    done
    $VERBOSE || _message restart
    echo " timed out waiting!"
    sed -e 's/^/	/' $tmp.out
    _check_logfile
    return 1
}

if $START_PMIE
then
    # ensure we have a pmie binary which supports the features we need
    # 
    _check_pmie_version
else
    # if pmie has never been started, there's no work to do to stop it
    # 
    [ ! -d $PCP_TMP_DIR/pmie ] && exit
    pmpost "stop pmie from $prog"
fi

if [ ! -f $CONTROL ]
then
    echo "$prog: Error: cannot find control file ($CONTROL)"
    status=1
    exit
fi

#  1.0 is the first release, and the version is set in the control file
#  with a $version=x.y line
#
version=1.0
eval `grep '^version=' $CONTROL | sort -rn`
if [ $version != "1.0" ]
then
    _error "unsupported version (got $version, expected 1.0)"
    status=1
    exit
fi

echo >$tmp.dir
rm -f $tmp.err $tmp.pmies

line=0
cat $CONTROL \
    | sed -e "s/LOCALHOSTNAME/$LOCALHOSTNAME/g" \
	  -e "s;PCP_LOG_DIR;$PCP_LOG_DIR;g" \
    | while read host socks logfile args
do
    logfile=`_unsymlink_path $logfile`
    line=`expr $line + 1`
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
			    eval $cmd
			    ;;
		    esac
		fi
		continue
		;;
    esac

    if [ -z "$socks" -o -z "$logfile" -o -z "$args" ]
    then
	_error "insufficient fields in control file record"
	continue
    fi

    [ $VERY_VERBOSE = "true" ] && echo "Check pmie -h $host -l $logfile ..."

    # make sure output directory exists
    #
    dir=`dirname $logfile`
    if [ ! -d $dir ]
    then
	mkdir -p $dir >$tmp.err 2>&1
	if [ ! -d $dir ]
	then
	    cat $tmp.err
	    _error "cannot create directory ($dir) for pmie log file"
	fi
    fi
    [ ! -d $dir ] && continue

    cd $dir
    dir=`pwd`
    $SHOWME && echo "+ cd $dir"

    if [ ! -w $dir ]
    then
	_warning "no write access in $dir, skip lock file processing"
	ls -ld $dir
    else
	_lock
    fi

    # match $logfile and $fqdn from control file to running pmies
    pid=""
    fqdn=`pmhostname $host`
    for file in `ls $PCP_TMP_DIR/pmie`
    do
	p_id=$file
	file="$PCP_TMP_DIR/pmie/$file"
	p_logfile=""
	p_pmcd_host=""

	case "$PCP_PLATFORM"
	in 
	    irix)
		test -f /proc/pinfo/$p_id 
		;;
	    linux)
		test -e /proc/$p_id
		;;
	esac
	if [ $? -eq 0 ]
	then
	    eval `tr '\0' '\012' < $file | sed -e '/^$/d' | sed -e 3q | $PCP_AWK_PROG '
NR == 2	{ printf "p_logfile=\"%s\"\n", $0; next }
NR == 3	{ printf "p_pmcd_host=\"%s\"\n", $0; next }
	{ next }'`
	    p_logfile=`_unsymlink_path $p_logfile`
	    if [ "$p_logfile" = $logfile -a "$p_pmcd_host" = "$fqdn" ]
	    then
		pid=$p_id
		break
	    fi
	else
	    # ignore, its not a running process
	    eval $RM -f $file
	fi
    done

    if $VERY_VERBOSE
    then
	if [ -z "$pid" ]
	then
	    echo "No current pmie process exists for:"
	else
	    echo "Found pmie process $pid monitoring:"
	fi
	echo "    host = $fqdn"
echo "    log file = $logfile"
    fi

    if [ -z "$pid" -a $START_PMIE = true ]
    then
	configfile=`echo $args | sed -n -e 's/^/ /' -e 's/[ 	][ 	]*/ /g' -e 's/-c /-c/' -e 's/.* -c\([^ ]*\).*/\1/p'`
	if [ ! -z "$configfile" ]
	then
	    # if this is a relative path and not relative to cwd,
	    # substitute in the default pmie search location.
	    #
	    if [ ! -f "$configfile" -a "`basename $configfile`" = "$configfile" ]
	    then
		configfile="$PCP_VAR_DIR/config/pmie/$configfile"
	    fi

	    if [ -f $configfile ]
	    then
		# look for "magic" string at start of file
		#
		if sed 1q $configfile | grep '^#pmieconf-rules [0-9]' >/dev/null
		then
		    # pmieconf file, see if re-generation is needed
		    # Note that pmieconf is in the pcp-pro product (not open source)
		    #
		    cp $configfile $tmp.pmie
		    if $have_pmieconf
		    then
			if pmieconf -f $tmp.pmie $CONFARGS >$tmp.diag 2>&1
			then
			    grep -v "generated by pmieconf" $configfile >$tmp.old
			    grep -v "generated by pmieconf" $tmp.pmie >$tmp.new
			    if diff $tmp.old $tmp.new >/dev/null
			    then
				:
			    else
				if [ -w $configfile ]
				then
				    $VERBOSE && echo "Reconfigured: \"$configfile\" (pmieconf)"
				    eval $CP $tmp.pmie $configfile
				else
				    _warning "no write access to pmieconf file \"$configfile\", skip reconfiguration"
				    ls -l $configfile
				fi
			    fi
			else
			    _warning "pmieconf failed to reconfigure \"$configfile\""
			    cat "s;$tmp.pmie;$configfile;g" $tmp.diag
			    echo "=== start pmieconf file ==="
			    cat $tmp.pmie
			    echo "=== end pmieconf file ==="
			fi
		    fi
		fi
	    else
		# file does not exist, generate it, if possible
		# Note that pmieconf is in the pcp-pro product (not open source)
		# 
		if $have_pmieconf
		then
		    if pmieconf -f $configfile $CONFARGS >$tmp.diag 2>&1
		    then
			:
		    else
			_warning "pmieconf failed to generate \"$configfile\""
			cat $tmp.diag
			echo "=== start pmieconf file ==="
			cat $configfile
			echo "=== end pmieconf file ==="
		    fi
		fi
	    fi
	fi

	args="-h $host -l $logfile $args"

	$VERBOSE && _message restart
	sock_me=''
	if [ "$socks" = y ] 
	then
	    # only check for pmsocks if it's specified in the control file
	    have_pmsocks=false
	    if which pmsocks >/dev/null 2>&1
	    then
		# check if pmsocks has been set up correctly
		if pmsocks ls >/dev/null 2>&1
		then
		    have_pmsocks=true
		fi
	    fi

	    if $have_pmsocks
	    then
		sock_me="pmsocks "
	    else
		echo "$prog: Warning: no pmsocks available, would run without"
		sock_me=""
	    fi
	fi

	[ -f $logfile ] && eval $MV -f $logfile $logfile.prior

	if $SHOWME
	then
	    $VERBOSE && echo
	    echo "+ ${sock_me}$PMIE -b $args"
	    _unlock
	    continue
	else
	    # since this is launched as a sort of daemon, any output should
	    # go on pmie's stderr, i.e. $logfile ... use -b for this
	    #
	    $VERY_VERBOSE && ( echo; $PCP_ECHO_PROG $PCP_ECHO_N "+ ${sock_me}$PMIE -b $args""$PCP_ECHO_C"; echo "..." )
	    pmpost "start pmie from $prog for host $host"
	    ${sock_me}$PMIE -b $args &
	    pid=$!
	fi

	# wait for pmie to get started, and check on its health
	_check_pmie $pid

    elif [ ! -z "$pid" -a $START_PMIE = false ]
    then
	# Send pmie a SIGTERM, which is noted as a pending shutdown.
	# Add pid to list of pmies sent SIGTERM - may need SIGKILL later.
	#
	$VERY_VERBOSE && echo "+ $KILL -TERM $pid"
	eval $KILL -TERM $pid
	$PCP_ECHO_PROG $PCP_ECHO_N "$pid ""$PCP_ECHO_C" >> $tmp.pmies	
    fi

    _unlock
done

# check all the SIGTERM'd pmies really died - if not, use a bigger hammer.
# 
if $SHOWME
then
    :
elif [ $START_PMIE = false -a -s $tmp.pmies ]
then
    pmielist=`cat $tmp.pmies`
    if ps -p "$pmielist" >/dev/null 2>&1
    then
	$VERY_VERBOSE && ( echo; $PCP_ECHO_PROG $PCP_ECHO_N "+ $KILL -KILL `cat $tmp.pmies` ...""$PCP_ECHO_C" )
	eval $KILL -KILL $pmielist >/dev/null 2>&1
	sleep 3		# give them a chance to go
	if ps -f -p "$pmielist" >$tmp.alive 2>&1
	then
	    echo "$prog: Error: pmie process(es) will not die"
	    cat $tmp.alive
	    status=1
	fi
    fi
fi

[ -f $tmp.err ] && status=1
exit
