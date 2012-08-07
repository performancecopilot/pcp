# Common sh(1) procedures to be used in the Performance Co-Pilot
# PMDA Install and Remove scripts
#
# Copyright (c) 1995-2001,2003 Silicon Graphics, Inc.  All Rights Reserved.
# Portions Copyright (c) 2008 Aconex.  All Rights Reserved.
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
# 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA

# source the PCP configuration environment variables
. $PCP_DIR/etc/pcp.env

tmp=/var/tmp/$$
trap "rm -f $tmp; exit" 0 1 2 3 15

_setup_platform()
{
    case "$PCP_PLATFORM"
    in
	mingw)
	    uid=0	# no permissions we can usefully test here
	    dso_suffix=dll
	    default_pipe_opt=false
	    default_socket_opt=true
	    CHOWN=": skip chown"
	    CHMOD=chmod
	    ;;
	*)
	    eval `id | sed -e 's/(.*//'`
	    dso_suffix=so
	    [ "$PCP_PLATFORM" = darwin ] && dso_suffix=dylib
	    default_pipe_opt=true
	    default_socket_opt=false
	    CHOWN=chown
	    CHMOD=chmod
	    ;;
    esac
}

_setup_localhost()
{
    # Try to catch some truly evil preconditions.  If you cannot reach
    # localhost, all bets are off!
    #
    if which ping >/dev/null 2>&1
    then
	__opt=''
	# Larry Wall style hunt for the version of ping that is first
	# on our path
	#
	ping --help >$tmp 2>&1
	if grep '.-c count' $tmp >/dev/null 2>&1
	then
	    __opt='-c 1 localhost'
	elif grep '.-n count' $tmp >/dev/null 2>&1
	then
	    __opt='-n 1 localhost'
	elif grep 'host .*packetsize .*count' $tmp >/dev/null 2>&1
	then
	    __opt='localhost 56 1'
	elif grep 'host .*data_size.*npackets' $tmp >/dev/null 2>&1
	then
	    __opt='localhost 56 1'
	fi
	if [ -z "$__opt" ]
	then
	    echo "Warning: can't find a ping(1) that I understand ... pushing on"
	else
	    if ping $__opt >$tmp 2>&1
	    then
		:
	    else
		# failing that, try 3 pings ... failure means all 3 were lost,
		# and so there is no hope of continuing
		#
		__opt=`echo "$__opt" | sed -e 's/1/3/'`
		if ping $__opt >/dev/null 2>&1
		then
		    :
		else
		    echo "Error: no route to localhost, pmcd reconfiguration abandoned" 
		    exit 1
		fi
	    fi
	fi
    fi
}

_setup_localhost
_setup_platform
rm -f $tmp

# some useful common variables for Install/Remove scripts
#
# put your PMNS files here
PMNSDIR=$PCP_VAR_DIR/pmns

# pmcd and pcp log files here
if [ ! -z "$PCP_LOGDIR" ]
then
    # this is being discouraged and is no longer documented anywhere
    LOGDIR=$PCP_LOGDIR
else
    if [ -d $PCP_LOG_DIR/pmcd ]
    then
	# the preferred naming scheme
	#
	LOGDIR=$PCP_LOG_DIR/pmcd
    else
	# backwards compatibility for IRIX
	#
	LOGDIR=$PCP_LOG_DIR
    fi
fi

# writeable root of PMNS
NAMESPACE=${PMNS_DEFAULT-$PMNSDIR/root}
PMNSROOT=`basename $NAMESPACE`

# echo without newline - deprecated - use the $PCP_ECHO_* ones from
# /etc/pcp.conf instead, however some old Install and Remove scripts may
# still use $ECHONL, so keep it here
#
ECHONL="echo -n"

# Install control variables
#	Can install as DSO?
dso_opt=false
#	Can install as perl script?
perl_opt=false
#	Can install as daemon?
daemon_opt=true
#	If daemon, pipe?
pipe_opt=$default_pipe_opt
#	If daemon, socket?  and default for Internet sockets?
socket_opt=$default_socket_opt
socket_inet_def=''
#	IPC Protocol for daemon (binary only now)
ipc_prot=binary
#	Delay after install before checking (sec)
check_delay=3
#	Additional command line args to go in $PCP_PMCDCONF_PATH
args=""
#	Source for the pmns
pmns_source=pmns
#	Source for the helptext
help_source=help
#	Assume libpcp_pmda.so.1
pmda_interface=1


# Other variables and constants
#
prog=`basename $0`
tmp=$PCP_TMP_DIR/$$
do_pmda=true
do_check=true
__here=`pwd`
__pmcd_is_dead=false
__echo=false
__verbose=false
__ns_opt=''

trap "rm -f $tmp $tmp.*; exit" 0 1 2 3 15

# Parse command line args
#
while [ $# -gt 0 ]
do
    case $1
    in
	-e)	# echo user input
	    __echo=true
	    ;;

	-N)	# name space only
	    do_pmda=false
	    ;;

	-n)	# alternate name space
	    if [ $# -lt 2 ]
	    then
		echo "$prog: -n requires a name space file option"
		exit 1
	    fi
	    NAMESPACE=$2
	    PMNSROOT=`basename $NAMESPACE`
	    PMNSDIR=`dirname $NAMESPACE`
	    __ns_opt="-n $2" 
	    shift
	    ;;

	-Q)	# skip check for metrics going away
	    do_check=false
	    ;;

	-R)	# $ROOT
	    if [ "$prog" = "Remove" ]
	    then
		echo "Usage: $prog [-eNQV] [-n namespace]"
		exit 1
	    fi
	    if [ $# -lt 2 ]
	    then
		echo "$prog: -R requires a directory option"
		exit 1
	    fi
	    root=$2
	    shift
	    ;;

	-V)	# verbose
	    __verbose=true
	    ;;

	*)
	    if [ "$prog" = "Install" ]
	    then
		echo "Usage: $prog [-eNQV] [-n namespace] [-R rootdir]"
	    else
		echo "Usage: $prog [-eNQV] [-n namespace]"
	    fi
	    exit 1
	    ;;
    esac
    shift
done

# wait for pmcd to be alive again
# 	Usage: __wait_for_pmcd [can_wait]
#
__wait_for_pmcd()
{
    # 60 seconds default seems like a reasonble max time to get going
    [ -z "$_can_wait" ] && __can_wait=${1-60}
    if pmcd_wait -t $__can_wait
    then
	:
    else
	echo "Arrgghhh ... PMCD failed to start after $__can_wait seconds"
	if [ -f $LOGDIR/pmcd.log ]
	then
	    echo "Here is the PMCD logfile ($LOGDIR/pmcd.log):"
	    ls -l $LOGDIR/pmcd.log
	    cat $LOGDIR/pmcd.log
	else
	    echo "No trace of the PMCD logfile ($LOGDIR/pmcd.log)!"
	fi
	__pmcd_is_dead=true
    fi
}

# try and put pmcd back the way it was
#
__restore_pmcd()
{
    if [ -f $tmp.pmcd.conf.save ]
    then
	__pmcd_is_dead=false
	echo
	echo "Save current PMCD control file in $PCP_PMCDCONF_PATH.prev ..."
	rm -f $PCP_PMCDCONF_PATH.prev
	mv $PCP_PMCDCONF_PATH $PCP_PMCDCONF_PATH.prev
	echo "Restoring previous PMCD control file, and trying to restart PMCD ..."
	cp $tmp.pmcd.conf.save $PCP_PMCDCONF_PATH
	eval $CHOWN root $PCP_PMCDCONF_PATH
	eval $CHMOD 644 $PCP_PMCDCONF_PATH
	rm -f $tmp.pmcd.conf.save
	$PCP_RC_DIR/pcp start
	__wait_for_pmcd
    fi
    if $__pmcd_is_dead
    then
	echo
	echo "Sorry, failed to restart PMCD."
    fi
}

# __pmda_cull name domain
#
__pmda_cull()
{
    # context and integrity checks
    #
    if [ $# -ne 2 ]
    then
	echo "pmdaproc.sh: internal botch: __pmda_cull() called with $# (instead of 2) arguments"
	exit 1
    fi
    [ ! -f $PCP_PMCDCONF_PATH ] && return
    if eval $CHMOD u+w $PCP_PMCDCONF_PATH
    then
	:
    else
	echo "pmdaproc.sh: __pmda_cull: Unable to make $PCP_PMCDCONF_PATH writable"
        exit 1
    fi
    if [ ! -w $PCP_PMCDCONF_PATH ]
    then
	echo "pmdaproc.sh: \"$PCP_PMCDCONF_PATH\" is not writeable"
	exit 1
    fi

    # remove matching entry from $PCP_PMCDCONF_PATH if present
    #
    $PCP_AWK_PROG <$PCP_PMCDCONF_PATH >/tmp/$$.pmcd.conf '
BEGIN					{ status = 0 }
$1 == "'"$1"'" && $2 == "'"$2"'" 	{ status = 1; next }
					{ print }
END					{ exit status }'
    if [ $? -eq 0 ]
    then
	# no match
	:
    else
	
	# log change to the PCP NOTICES file
	#
	pmpost "PMDA cull: from $PCP_PMCDCONF_PATH: $1 $2"

	# save pmcd.conf in case we encounter a problem, and then
	# install updated $PCP_PMCDCONF_PATH
	#
	cp $PCP_PMCDCONF_PATH $tmp.pmcd.conf.save
	cp /tmp/$$.pmcd.conf $PCP_PMCDCONF_PATH
	eval $CHOWN root $PCP_PMCDCONF_PATH
	eval $CHMOD 644 $PCP_PMCDCONF_PATH

	# signal pmcd if it is running
	#
	if pminfo -v pmcd.version >/dev/null 2>&1
	then
	    pmsignal -a -s HUP pmcd >/dev/null 2>&1
	    # allow signal processing to be done before checking status
	    sleep 2
	    __wait_for_pmcd
	    if $__pmcd_is_dead
	    then
		__restore_pmcd
		# give PMCD a chance to get back into original state
	    sleep 3
		__wait_for_pmcd
	    fi
	fi
    fi
    rm -f /tmp/$$.pmcd.conf

    # stop any matching PMDA that is still running
    #
    for __sig in TERM KILL
    do
	__pids=`_get_pids_by_name pmda$1`
	if [ ! -z "$__pids" ]
	then
	    pmsignal -s $__sig $__pids >/dev/null 2>&1
	    # allow signal processing to be done
	    sleep 2
	else
	    break
	fi
    done
}

# __pmda_add "entry for $PCP_PMCDCONF_PATH"
#
__pmda_add()
{
    # context and integrity checks
    #
    if [ $# -ne 1 ]
    then
	echo "pmdaproc.sh: internal botch: __pmda_add() called with $# (instead of 1) arguments"
	exit 1
    fi
    if eval $CHMOD u+w $PCP_PMCDCONF_PATH
    then
	:
    else
	echo "pmdaproc.sh: __pmda_add: Unable to make $PCP_PMCDCONF_PATH writable"
        exit 1
    fi
    if [ ! -w $PCP_PMCDCONF_PATH ]
    then
	echo "pmdaproc.sh: \"$PCP_PMCDCONF_PATH\" is not writeable"
	exit 1
    fi

    # save pmcd.conf in case we encounter a problem
    #
    cp $PCP_PMCDCONF_PATH $tmp.pmcd.conf.save

    myname=`echo $1 | $PCP_AWK_PROG '{print $1}'`
    mydomain=`echo $1 | $PCP_AWK_PROG '{print $2}'`
    # add entry to $PCP_PMCDCONF_PATH
    #
    echo >/tmp/$$.pmcd.access
    $PCP_AWK_PROG <$PCP_PMCDCONF_PATH '
NF==0					{ next }
/^[      ]*\[[   ]*access[       ]*\]/	{ state = 2 }
state == 2				{ print >"'/tmp/$$.pmcd.access'"; next }
$1=="'$myname'" && $2=="'$mydomain'"	{ next }
					{ print >"'/tmp/$$.pmcd.body'"; next }'
    ( cat /tmp/$$.pmcd.body \
      ; echo "$1" \
      ; cat /tmp/$$.pmcd.access \
    ) >$PCP_PMCDCONF_PATH
    rm -f /tmp/$$.pmcd.access /tmp/$$.pmcd.body
    eval $CHOWN root $PCP_PMCDCONF_PATH
    eval $CHMOD 644 $PCP_PMCDCONF_PATH

    # log change to pcplog/NOTICES
    #
    pmpost "PMDA add: to $PCP_PMCDCONF_PATH: $1"

    # signal pmcd if it is running, else start it
    #
    if pminfo -v pmcd.version >/dev/null 2>&1
    then
	pmsignal -a -s HUP pmcd >/dev/null 2>&1
	# allow signal processing to be done before checking status
	sleep 2
	__wait_for_pmcd
	$__pmcd_is_dead && __restore_pmcd
    else
	log=$LOGDIR/pmcd.log
	rm -f $log
	$PCP_RC_DIR/pcp start
	__wait_for_pmcd
	$__pmcd_is_dead && __restore_pmcd
    fi
}

# expect -R root or $ROOT not set in environment
#
__check_root()
{
    if [ "X$root" != X ]
    then
	ROOT="$root"
	export ROOT
    else
	if [ "X$ROOT" != X -a "X$ROOT" != X/ ]
	then
	    echo "Install: \$ROOT was set to \"$ROOT\""
	    echo "          Use -R rootdir to install somewhere other than /"
	    exit 1
	fi
    fi
}

# should be able to extract default domain from domain.h
#
__check_domain()
{
    if [ -f domain.h ]
    then
	__infile=domain.h
    elif [ -f domain.h.perl ]
    then
	__infile=domain.h.perl
    else
	echo "Install: cannot find ./domain.h to determine the Performance Metrics Domain"
	exit 1
    fi
    # $domain is for backwards compatibility, modern PMDAs
    # have something like
    #	#define FOO 123
    #
    domain=''
    eval `$PCP_AWK_PROG <$__infile '
/^#define/ && $3 ~ /^[0-9][0-9]*$/	{ print $2 "=" $3
				      if (seen == 0) {
					print "domain=" $3
					sub(/^PMDA/, "", $2)
					print "SYMDOM=" $2
					seen = 1
				      }
				    }'`
    if [ "X$domain" = X ]
    then
	echo "Install: cannot determine the Performance Metrics Domain from ./domain.h"
	exit 1
    fi
}

# handle optional configuration files that maybe already given in an
# $PCP_PMCDCONF_PATH line or user-supplied or some default or sample
# file
#
# before calling _choose_configfile, optionally define the following
# variables
#
# Name		Default			Use
#
# configdir	$PCP_VAR_DIR/config/$iam	directory for config ... assumed
#					name is $iam.conf in this directory
#
# configfile	""			set if have a preferred choice,
#					e.g. from $PCP_PMCDCONF_PATH
#					this will be set on return if we've
#					found an acceptable config file
#
# default_configfile
#		""			if set, this is the default which
#					will be offered
#
# Note: 
#  If the choice is aborted then $configfile will be set to empty.
#  Therefore, there should be a test for an empty $configfile after
#  the call to this function.
#
_choose_configfile()
{
    configdir=${configdir-$PCP_VAR_DIR/config/$iam}

    if [ ! -d $configdir ]
    then
    	mkdir -p $configdir
    fi

    while true
    do
	echo "Possible configuration files to choose from:"
	# List viable alternatives
	__i=0          # menu item number
	__filelist=""  # list of configuration files
	__choice=""    # the choice of configuration file
	__choice1=""   # the menu item for the 1st possible choice
	__choice2=""   # the menu item for the 2nd possible choice
	__choice3=""   # the menu item for the 3rd possible choice

        if [ ! -z "$configfile" ]
	then
	    if [ -f $configfile ]
	    then
		__i=`expr $__i + 1`
		__choice1=$__i
		__filelist="$__filelist $configfile"
		echo "[$__i] $configfile"
	    fi
	fi

        if [ -f $configdir/$iam.conf ]
	then
	    if echo $__filelist | grep "$configdir/$iam.conf" >/dev/null
	    then
		:
	    else
		__i=`expr $__i + 1`
		__choice2=$__i
		__filelist="$__filelist $configdir/$iam.conf"
		echo "[$__i] $configdir/$iam.conf"
	    fi
	fi

	if [ -f $default_configfile ]
	then
	    if echo $__filelist | grep "$default_configfile" >/dev/null
	    then
		:
	    else
		__i=`expr $__i + 1`
		__choice3=$__i
		__filelist="$__filelist $default_configfile"
		echo "[$__i] $default_configfile"
	    fi
	fi

	__i=`expr $__i + 1`
	__own_choice=$__i
	echo "[$__i] Specify your own configuration file."

	__i=`expr $__i + 1`
	__abort_choice=$__i
	echo "[$__i] None of the above (abandon configuration file selection)."

	$PCP_ECHO_PROG $PCP_ECHO_N "Which configuration file do you want to use ? [1] ""$PCP_ECHO_C"
	read __reply
	$__echo && echo "$__reply"

	# default
	if [ -z "$__reply" ] 
	then
	    __reply=1
	fi

	# Process the reply from the user
	if [ $__reply = $__own_choice ]
	then
	    $PCP_ECHO_PROG $PCP_ECHO_N "Enter the name of the existing configuration file: ""$PCP_ECHO_C"
	    read __choice
	    $__echo && echo "$__choice"
	    if [ ! -f "$__choice" ]
	    then
		echo "Cannot open \"$__choice\"."
		echo ""
		echo "Please choose another configuration file."
		__choice=""
	    fi
	elif [ $__reply = $__abort_choice ]
	then
	    echo "Abandoning configuration file selection."
	    configfile=""
	    return 0
	elif [ "X$__reply" = "X$__choice1" -o "X$__reply" = "X$__choice2" -o "X$__reply" = "X$__choice3" ]
	then
	    # extract nth field as the file
	    __choice=`echo $__filelist | $PCP_AWK_PROG -v n=$__reply '{ print $n }'` 
	else
	    echo "Illegal choice: $__reply"
	    echo ""
	    echo "Please choose number between: 1 and $__i"
	fi

	if [ ! -z "$__choice" ]
	then
	    echo
	    echo "Contents of the selected configuration file:"
	    echo "--------------- start $__choice ---------------"
	    cat $__choice
	    echo "--------------- end $__choice ---------------"
	    echo

	    $PCP_ECHO_PROG $PCP_ECHO_N "Use this configuration file? [y] ""$PCP_ECHO_C"
	    read ans
	    $__echo && echo "$ans"
	    if [ ! -z "$ans" -a "X$ans" != Xy -a "X$ans" != XY ]
	    then
		echo ""
		echo "Please choose another configuration file."
	    else
		break
	    fi
	fi
    done


    __dest=$configdir/$iam.conf
    if [ "$__choice" != "$__dest" ]
    then
	if [ -f $__dest ]
	then
	    echo "Removing old configuration file \"$__dest\""
	    rm -f $__dest
	    if [ -f $__dest ]
	    then
		echo "Error: cannot remove old configuration file \"$__dest\""
		exit 1
	    fi
	fi
	if cp $__choice $__dest
	then
	    :
	else
	    echo "Error: cannot install new configuration file \"$__dest\""
	    exit 1
	fi
	__choice=$__dest
    fi

    configfile=$__choice
}

# choose correct PMDA installation mode
#
# make sure we are installing in the correct style of configuration
#
__choose_mode()
{
    __def=m
    $do_pmda && __def=b
    echo \
'You will need to choose an appropriate configuration for installation of
the "'$iam'" Performance Metrics Domain Agent (PMDA).

  collector	collect performance statistics on this system
  monitor	allow this system to monitor local and/or remote systems
  both		collector and monitor configuration for this system
'
    while true
    do
	$PCP_ECHO_PROG $PCP_ECHO_N 'Please enter c(ollector) or m(onitor) or b(oth) ['$__def'] '"$PCP_ECHO_C"
	read ans
	$__echo && echo "$ans"
	case "$ans"
	in
	    "")	break
		    ;;
	    c|collector|b|both)
		    do_pmda=true
		    break
		    ;;
	    m|monitor)
		    do_pmda=false
		    break
		    ;;
	    *)	echo "Sorry, that is not acceptable response ..."
		    ;;
	esac
    done
}

# choose an IPC method
#
__choose_ipc()
{
    _dir=$1
    ipc_type=''
    $pipe_opt && ipc_type=pipe
    $socket_opt && ipc_type=socket
    $pipe_opt && $socket_opt && ipc_type=''
    if [ -z "$ipc_type" ]
    then
	while true
	do
	    $PCP_ECHO_PROG $PCP_ECHO_N "PMCD should communicate with the $iam daemon via a pipe or a socket? [pipe] ""$PCP_ECHO_C"
	    read ipc_type
	    $__echo && echo "$ipc_type"
	    if  [ "X$ipc_type" = Xpipe -o "X$ipc_type" = X ]
	    then
		ipc_type=pipe
		break
	    elif [ "X$ipc_type" = Xsocket ]
	    then
		break
	    else
		echo "Must choose one of \"pipe\" or \"socket\", please try again"
	    fi
	done
    fi

    if [ $ipc_type = pipe ]
    then
	# This defaults to binary unless the Install file
	# specifies ipc_prot="binary notready"   -- See pmcd(1)
	type="pipe	$ipc_prot 		$_dir/$pmda_name"
    else
	while true
	do
	    $PCP_ECHO_PROG $PCP_ECHO_N "Use Internet or Unix domain sockets? [Internet] ""$PCP_ECHO_C"
	    read ans
	    $__echo && echo "$ans"
	    if [ "X$ans" = XInternet -o "X$ans" = X ]
	    then
		$PCP_ECHO_PROG $PCP_ECHO_N "Internet port number or service name? [$socket_inet_def] ""$PCP_ECHO_C"
		read port
		$__echo && echo "$port"
		[ "X$port" = X ] && port=$socket_inet_def
		case $port
		in
		    [0-9]*)
			    ;;
		    *)
			    if grep "^$port[ 	]*[0-9]*/tcp" /etc/services >/dev/null 2>&1
			    then
				:
			    else
				echo "Warning: there is no tcp service for \"$port\" in /etc/services!"
			    fi
			    ;;
		esac
		type="socket	inet $port	$_dir/$pmda_name"
		args="-i $port $args"
		break
	    elif [ "X$ans" = XUnix ]
	    then
		$PCP_ECHO_PROG $PCP_ECHO_N "Unix FIFO name? ""$PCP_ECHO_C"
		read fifo
		$__echo && echo "$fifo"
		if [ "X$fifo" = X ]
		then
		    echo "Must provide a name, please try again"
		else
		    type="socket	unix $fifo	$_dir/$pmda_name"
		    args="-u $fifo $args"
		    break
		fi
	    else
		echo "Must choose one of \"Unix\" or \"Internet\", please try again"
	    fi
	done
    fi
}

# filter pmprobe -i output of the format:
#	postgresql.active.is_in_recovery -12351 Missing metric value(s)
#	postgresql.statio.sys_sequences.blks_hit 0
#	disk.partitions.read 13 ?0 ?1 ?2 ?3 ?4 ?5 ?6 ?7 ?8 ?9 ?10 ?11 ?12
# to produce a summary of metrics, values and warnings
#
__filter()
{
    $PCP_AWK_PROG '
/^'$1'/         { metric++
                  if ($2 < 0)
		    warn++
		  else
		    value += $2
		  next
                }
                { warn++ }
END             { if (warn) printf "%d warnings, ",warn
                  printf "%d metrics and %d values\n",metric,value
                }'
}

_setup()
{
    # some more configuration controls
    pmns_name=${pmns_name-$iam}
    pmda_name=pmda$iam
    dso_name="${PCP_PMDAS_DIR}/${iam}/pmda_${iam}.${dso_suffix}"
    dso_name="$dso_name"
    dso_entry=${iam}_init
    pmda_dir="${PCP_PMDAS_DIR}/${iam}"

    _check_userroot
    _check_directory

    # automatically generate files for those lazy Perl programmers
    #
    if $perl_opt
    then
	perl_name="${PCP_PMDAS_DIR}/${iam}/pmda${iam}.pl"
	perl_name="$perl_name"
	perl_pmns="${PCP_PMDAS_DIR}/${iam}/pmns.perl"
	perl_dom="${PCP_PMDAS_DIR}/${iam}/domain.h.perl"
	perl -e 'use PCP::PMDA' 2>/dev/null
	if test $? -eq 0
	then
	    eval PCP_PERL_DOMAIN=1 perl "$perl_name" > "$perl_dom"
	    eval PCP_PERL_PMNS=1 perl "$perl_name" > "$perl_pmns"
	elif $dso_opt || $daemon_opt
	then
	    :	# we have an alternative, so continue on
	else
	    echo 'Perl PCP::PMDA module is not installed, install it and try again'
	    exit 1
	fi
    fi

    # Juggle pmns and domain.h in case perl pmda install was done here
    # last time
    #
    for file in pmns domain.h
    do
	[ -f $file.save ] && mv $file.save $file
    done

    # Set $domain and $SYMDOM from domain.h
    #
    __check_domain

    case $prog
    in
	*Install*)
	    # Check that $ROOT is not set, we have a default domain value and
	    # choose the installation mode (collector, monitor or both)
	    #
	    __check_root
	    __choose_mode
	    ;;
    esac
}

_install_views()
{
    viewer="$1"
    have_views=false

    [ `echo *.$viewer` != "*.$viewer" ] && have_views=true
    if [ -d $PCP_VAR_DIR/config/$viewer ]
    then
	$have_views && echo "Installing $viewer view(s) ..."
	for __i in *.$viewer
	do
	    if [ "$__i" != "*.$viewer" ]
	    then
		__dest=$PCP_VAR_DIR/config/$viewer/`basename $__i .$viewer`
		rm -f $__dest
		cp $__i $__dest
	    fi
	done
    else
	$have_views && \
	echo "Skip installing $viewer view(s) ... no \"$PCP_VAR_DIR/config/$viewer\" directory"
    fi
}

# Configurable PMDA installation
#
# before calling _install,
# 1. set $iam
# 2. set one/some/all of $dso_opt, $perl_opt, or $daemon_opt to true
#    (optional, $daemon_opt is true by default)
# 3. if $daemon_opt set one or both of $pipe_opt or $socket_opt true
#    (optional, $pipe_opt is true by default)
# 4. if $socket_opt and there is an default Internet socket, set
#    $socket_inet_def

_install()
{
    if [ -z "$iam" ]
    then
	echo 'Botch: must define $iam before calling _install()'
	exit 1
    fi

    if $do_pmda
    then
	if $dso_opt || $perl_opt || $daemon_opt
	then
	    :
	else
	    echo 'Botch: must set at least one of $dso_opt, $perl_opt or $daemon_opt to "true"'
	    exit 1
	fi
	if $daemon_opt
	then
	    if $pipe_opt || $socket_opt
	    then
		:
	    else
		echo 'Botch: must set at least one of $pipe_opt or $socket_opt to "true"'
		exit 1
	    fi
	fi

	# Select a PMDA style (dso/perl/deamon), and for daemons the
	# IPC method for communication between PMCD and the PMDA.
	#
	pmda_options=''
	pmda_default_option=''
	pmda_multiple_options=false

	if $dso_opt
	then
	    pmda_options="dso"
	    pmda_default_option="dso"
	fi
	if $perl_opt
	then
	    pmda_default_option="perl"
	    if test -n "$pmda_options"
	    then
		pmda_options="perl or $pmda_options"
		pmda_multiple_options=true
	    else
		pmda_options="perl"
	    fi
	fi
	if $daemon_opt
	then
	    pmda_default_option="daemon"
	    if test -n "$pmda_options"
	    then
		pmda_options="daemon or $pmda_options"
		pmda_multiple_options=true
	    else
		pmda_options="daemon"
	    fi
	fi

	pmda_type="$pmda_default_option"
	if $pmda_multiple_options
	then
	    while true
	    do
		$PCP_ECHO_PROG $PCP_ECHO_N "Install $iam as a $pmda_options agent? [$pmda_default_option] ""$PCP_ECHO_C"
		read pmda_type
		$__echo && echo "$pmda_type"
		if [ "X$pmda_type" = Xdaemon -o "X$pmda_type" = X ]
		then
		    pmda_type=daemon
		    break
		elif [ "X$pmda_type" = Xdso ]
		then
		    break
		elif [ "X$pmda_type" = Xperl ]
		then
		    perl -e 'use PCP::PMDA' 2>/dev/null
		    if test $? -ne 0
		    then
			echo 'Perl PCP::PMDA module is not installed, install it and try again'
		    else
			break
		    fi
		else
		    echo "Must choose one of $pmda_options, please try again"
		fi
	    done
	fi
	if [ "$pmda_type" = daemon ]
	then
	    __choose_ipc $pmda_dir
	    args="-d $domain $args"
	elif [ "$pmda_type" = perl ]
	then
	    type="pipe	binary		perl $perl_name"
	    args=""
	else
	    type="dso	$dso_entry	$dso_name"
	    args=""
	fi

	# Install binaries
	#
	if [ -f Makefile -o -f makefile -o -f GNUmakefile ]
	then
	    if [ ! -f "$PCP_MAKE_PROG" -o ! -f "$PCP_INC_DIR/pmda.h" ]
	    then
		echo "$prog: Arrgh, PCP devel environment required to install this PMDA"
		exit 1
	    fi

	    echo "Installing files ..."
	    if $PCP_MAKE_PROG install
	    then
		:
	    else
		echo "$prog: Arrgh, \"$PCP_MAKE_PROG install\" failed!"
		exit 1
	    fi
	fi

	# Fix domain in help for instance domains (if any)
	#
	if [ -f $help_source ]
	then
	    case $pmda_interface
	    in
		1)
			help_version=1
			;;
		*)	# PMDA_INTERFACE_2 or later
			help_version=2
			;;
	    esac
	    sed -e "/^@ $SYMDOM\./s/$SYMDOM\./$domain./" <$help_source \
	    | newhelp -n root -v $help_version -o $help_source
	fi
    fi

    if $do_pmda
    then
	if [ "X$pmda_type" = Xperl ]
	then
	    # Juggle pmns and domain.h ... save originals and
	    # use *.perl ones created earlier
	    for file in pmns domain.h
	    do
		if [ ! -f $file.perl ]
		then
		    echo "Botch: $file.perl missing ... giving up"
		    exit 1
		fi
		if [ -f $file ]
		then
		    if diff $file.perl $file >/dev/null
		    then
			:
		    else
			[ ! -f $file.save ] && mv $file $file.save
			mv $file.perl $file
		    fi
		else
		    mv $file.perl $file
		fi
	    done
	fi
    else
	# Maybe PMNS only install, and only implementation may be a
	# Perl one ... simpler juggling needed here
	#
	for file in pmns domain.h
	do
	    if [ ! -f $file -a -f $file.perl ]
	    then
		mv $file.perl $file
	    fi
	done
    fi

    $PCP_SHARE_DIR/lib/lockpmns $NAMESPACE
    trap "$PCP_SHARE_DIR/lib/unlockpmns \$NAMESPACE; rm -f $tmp $tmp.*; exit" 0 1 2 3 15

    echo "Updating the Performance Metrics Name Space (PMNS) ..."

    # Install the namespace
    #

    for __n in $pmns_name
    do
	if pminfo $__ns_opt $__n >/dev/null 2>&1
	then
            cd $PMNSDIR
	    if pmnsdel -n $PMNSROOT $__n >$tmp 2>&1
	    then
		pmsignal -a -s HUP pmcd >/dev/null 2>&1
		# Make sure the PMNS timestamp will be different the next
		# time the PMNS is updated (for Linux only 1 sec resolution)
		sleep 2
	    else
		if grep 'Non-terminal "'"$__n"'" not found' $tmp >/dev/null
		then
		    :
		elif grep 'Error: metricpath "'"$__n"'" not defined' $tmp >/dev/null
		then
		    :
		else
		    echo "$prog: failed to delete \"$__n\" from the PMNS"
		    cat $tmp
		    exit 1
		fi
	    fi
            cd $__here
	fi

	# Put the default domain number into the namespace file
	#
	# If there is only one namespace, then the pmns file will
	# be named "pmns".  If there are multiple metric trees,
	# subsequent pmns files will be named "pmns.<metricname>"
	#
	# the string "pmns" can be overridden by the Install/Remove
	# scripts by altering $pmns_source
	#
	if [ "$__n" = "$iam" -o "$__n" = "$pmns_name" ]
	then
	    __s=$pmns_source
	else
	    __s=$pmns_source.$__n
	fi
	sed -e "s/$SYMDOM:/$domain:/" <$__s >$PMNSDIR/$__n

        cd $PMNSDIR
	if pmnsadd -n $PMNSROOT $__n
	then
	    pmsignal -a -s HUP pmcd >/dev/null 2>&1
	    # Make sure the PMNS timestamp will be different the next
	    # time the PMNS is updated (for Linux only 1 sec resolution)
	    sleep 2
	else
	    echo "$prog: failed to add the PMNS entries for \"$__n\" ..."
	    echo
	    ls -l
	    exit 1
	fi
        cd $__here
    done

    trap "rm -f $tmp $tmp.*; exit" 0 1 2 3 15
    $PCP_SHARE_DIR/lib/unlockpmns $NAMESPACE

    _install_views pmchart
    _install_views kmchart
    _install_views pmview

    if $do_pmda
    then
	# Terminate old PMDA
	#
	echo "Terminate PMDA if already installed ..."
	__pmda_cull $iam $domain

	# Add PMDA to pmcd's configuration file
	#
	echo "Updating the PMCD control file, and notifying PMCD ..."
	__pmda_add "$iam	$domain	$type $args"

	# Check that the agent is running OK
	#
	if $do_check
	then
	    [ "$check_delay" -gt 5 ] && echo "Wait $check_delay seconds for the $iam agent to initialize ..."
	    sleep $check_delay
	    for __n in $pmns_name
	    do
		$PCP_ECHO_PROG $PCP_ECHO_N "Check $__n metrics have appeared ... ""$PCP_ECHO_C"
		pmprobe -i $__ns_opt $__n | tee $tmp.verbose | __filter $__n
		if $__verbose
		then
		    echo "pminfo output ..."
		    cat $tmp.verbose
		fi
	    done
	fi
    else
	echo "Skipping PMDA install and PMCD re-configuration"
    fi
}

_remove()
{
    # Update the namespace
    #

    $PCP_SHARE_DIR/lib/lockpmns $NAMESPACE
    trap "$PCP_SHARE_DIR/lib/unlockpmns \$NAMESPACE; rm -f $tmp $tmp.*; exit" 0 1 2 3 15

    echo "Culling the Performance Metrics Name Space ..."
    cd $PMNSDIR

    for __n in $pmns_name
    do
	$PCP_ECHO_PROG $PCP_ECHO_N "$__n ... ""$PCP_ECHO_C"
	if pmnsdel -n $PMNSROOT $__n >$tmp 2>&1
	then
	    rm -f $PMNSDIR/$__n
	    pmsignal -a -s HUP pmcd >/dev/null 2>&1
	    sleep 2
	    echo "done"
	else
	    if grep 'Non-terminal "'"$__n"'" not found' $tmp >/dev/null
	    then
		echo "not found in Name Space, this is OK"
	    elif grep 'Error: metricpath "'"$__n"'" not defined' $tmp >/dev/null
	    then
		echo "not found in Name Space, this is OK"
	    else
		echo "error"
		cat $tmp
		exit
	    fi
	fi
    done

    # Remove the PMDA and help files
    #
    cd $__here

    if $do_pmda
    then
	echo "Updating the PMCD control file, and notifying PMCD ..."
	__pmda_cull $iam $domain

	if [ -f Makefile -o -f makefile -o -f GNUmakefile ]
	then
	    echo "Removing files ..."
	    $PCP_MAKE_PROG clobber >/dev/null
	fi
	for __i in *.pmchart
	do
	    if [ "$__i" != "*.pmchart" ]
	    then
		__dest=$PCP_VAR_DIR/config/pmchart/`basename $__i .pmchart`
		rm -f $__dest
	    fi
	done
	for __i in *.kmchart
	do
	    if [ "$__i" != "*.kmchart" ]
	    then
		__dest=$PCP_VAR_DIR/config/kmchart/`basename $__i .kmchart`
		rm -f $__dest
	    fi
	done

	if $do_check
	then
	    for __n in $pmns_name
	    do
		$PCP_ECHO_PROG $PCP_ECHO_N "Check $__n metrics have gone away ... ""$PCP_ECHO_C"
		if pminfo -n $NAMESPACE -f $__n >$tmp 2>&1
		then
		    echo "Arrgh, something has gone wrong!"
		    cat $tmp
		else
		    echo "OK"
		fi
	    done
	fi
    else
	echo "Skipping PMDA removal and PMCD re-configuration"
    fi

    trap "rm -f $tmp $tmp.*; exit" 0 1 2 3 15
    $PCP_SHARE_DIR/lib/unlockpmns $NAMESPACE
}

_check_userroot()
{
    if [ "$uid" -ne 0 ]
    then
	if [ -n "$PCP_DIR" ]
	then
	    : running in a non-default installation, do not need to be root
	else
	    echo "Error: You must be root (uid 0) to update the PCP collector configuration."
	    exit 1
	fi
    fi
}

_check_directory()
{
    case "$__here"
    in
	*/pmdas/$iam)
	    ;;
	*)
	    echo "Error: expect current directory to be .../pmdas/$iam, not $__here"
	    echo "       (typical location is $PCP_PMDAS_DIR/$iam on this platform)"
	    exit 1
	    ;;
    esac
}

# preferred public interfaces
#
pmdaSetup()
{
    _setup
}

pmdaChooseConfigFile()
{
    _choose_configfile
}

pmdaInstall()
{
    _install
}

pmdaRemove()
{
    _remove
}
