# Helper procedures for pmcheck plugins
#
# Exit code 99 indicates a fatal botch in the setup or the execution
# environment.

# common initialization
#
. $PCP_DIR/etc/pcp.env || exit 99

status=0
prog=`basename $0`
tmp=`mktemp -d "$PCP_TMPFILE_DIR/pmcheck-$prog.XXXXXXXXX"`
if [ ! -d "$tmp" ]
then
    echo >&2 "$prog: Error: cannot create temp directory: $PCP_TMPFILE_DIR/$prog.XXXXXXXXX"
    exit 99
fi
trap "rm -rf $tmp; exit \$status" 0 1 2 3 15

# common command line processing ... use pmgetopt(1) for portability
#
aflag=false
dflag=false
lflag=false
show_me=false
sflag=false
verbose=0

cat <<'End-of-File' >$tmp/_usage
# getopts: adlnsv
# usage: [options] component

options:
  -a, --activate    activate component(s)
  -d, --deactivate  deactivate component(s)
  -l, --list        report descsription with -v
  -n, --show-me     dry run
  -s, --state       report state of component(s)
  -v, --verbose     increase verbosity
End-of-File

_do_args()
{
    __args="`pmgetopt --progname=$prog --config=$tmp/_usage -- "$@"`"
    if [ $? -ne 0 ]
    then
	pmgetopt --progname=$prog --config=$tmp/_usage --usage
	status=99
	exit
    fi

    eval set -- "$__args"
    unset __args

    while [ $# -gt 0 ]
    do
	case "$1"
	in
	    -a)	# activate
		aflag=true
		;;
	    -d)	# deactivate
		dflag=true
		;;
	    -l)	# list
		lflag=true
		;;
	    -n)	# dry run
		show_me=true
		;;
	    -s)	# state
		sflag=true
		;;
	    -v)	# verbose
		verbose=`expr $verbose + 1`
		;;
	    --)	# end of opts
		shift
		break
		;;
	    *)
		echo >&2 "getopt iterator botch \$1=\"$1\" ..."
		status=99
		exit
		;;
	esac
	shift
    done

    if [ $# -eq 0 ]
    then
	component="$prog"
    elif [ $# -eq 1 ]
    then
	component="$1"
    else
	pmgetopt --progname=$prog --config=$tmp/_usage --usage
	status=99
	exit
    fi
}

# control systemd/init/... services
# Usage: _ctl_svc action service
# where action is one of
# - state
# - start
# - stop
# - activate (enable and start)
# - deactivate (stop and disable)
# a systemd/init/... service
#
_ctl_svc()
{
    __rc=0
    __action="$1"
    __svc="$2"
    __state=''
    __runlevel=''

    if [ -z "$1" -o -z "$2" ]
    then
	echo >&2 "$prog: _ctl_svc: Error: missing arguments ($1=\"$1\", $2=\"$2\")"
	status=99
	exit
    fi
    case "$__action"
    in
	state|start|stop|activate|deactivate)
	    ;;
	*)
	    echo >&2 "$prog: _ctl_svc: Error: bad action argument ($__action) for $__svc service"
	    status=99
	    exit
	    ;;
    esac

    if [ "$__action" = state ]
    then
	if $__use_systemctl
	then
	    systemctl show $__svc.service >$tmp/_ctl_svc 2>&1
	    if grep -q '^ActiveState=' <$tmp/_ctl_svc
	    then
		eval `grep '^ActiveState=' <$tmp/_ctl_svc`
	    else
		ActiveState=unknown
	    fi
	    if [ "$ActiveState" = active -o "$ActiveState" = activating ]
	    then
		# active, all good
		:
	    else
		# not active, need to dig a bit ...
		if [ "$ActiveState" = inactive ]
		then
		    if grep '^LoadError=.*\.service not found' <$tmp/_ctl_svc >/dev/null
		    then
			__rc=2
			echo "systemd's $__svc.service unit is not installed"
		    else
			__rc=1
		    fi
		elif [ "$ActiveState" = failed ]
		then
		    __rc=2
		    echo "systemd's $__svc.service unit has failed"
		else
		    # no clue ...
		    __rc=2
		    echo "systemd $__svc.service unit is status ($ActiveState) unexpected"
		fi
	    fi
	elif $__use_update_invoke
	then
	    invoke-rc.d $__svc status >$tmp/_ctl_svc 2>&1
	    __state=`sed -n -e '/^Checking for/s/.*: //p' <$tmp/_ctl_svc`
	    if [ -z "$__state" ]
	    then
		# some, like redis-server just say ... is running.
		#
		__state=`sed -n -e "/^$__svc is \([^ ][^ .]*\).*/s//\1/p" <$tmp/_ctl_svc`
	    fi
	    case "$__state"
	    in
		running)
		    ;;
		stopped)
		    __rc=1
		    ;;
		*)
		    __rc=2
		    if [ ! -f /etc/init.d/$__svc ]
		    then
			echo "/etc/init.d/$__svc is not installed"
		    elif [ -n "$__state" ]
		    then
			echo "state ($__state) unknown"
		    else
			cat $tmp/_ctl_svc
		    fi
		    ;;
	    esac
	elif $__use_rc_script
	then
	    if [ ! -x "$PCP_RC_DIR/$__svc" ]
	    then
		__rc=2
		echo "$PCP_RC_DIR/$__svc script is missing"
	    elif $__use_chkconfig
	    then
		# runlevel emits
		# <previous> 3
		# or (in CI containers especially)
		# unknown
		# in which case we punt on 3
		# chconfig emits ...
		# pmcd           	0:off	1:off	2:on	3:on	4:on	5:on	6:off
		# want                                            ^^
		__runlevel=`runlevel | cut -d ' ' -f 2`
		[ "$__runlevel" = unknown ] && __runlevel=3
		__state=`chkconfig --list $__svc 2>/dev/null | sed -e "s/.*$__runlevel://" -e 's/[ 	].*//'`
		case "$__state"
		in
		    on)
			;;
		    off)
			__rc=1
			;;
		    *)
			__rc=2
			echo "state ($__state) from chkconfig unknown"
			;;
		esac
	    fi
	else
	    echo >&2 "$prog: _ctl_svc: Botch: cannot determine how to get state of $__svc service"
	    status=99
	    exit
	fi
	return $__rc
    fi

    if [ "$__action" = activate ]
    then
	if $__use_systemctl
	then
	    if [ "`systemctl is-enabled $__svc.service`" != enabled ]
	    then
		if $show_me
		then
		    echo "# systemctl enable $__svc.service"
		else
		    if systemctl enable $__svc.service >$tmp/_ctl_svc 2>&1
		    then
			:
		    else
			__rc=1
			[ "$verbose" -gt 0 ] && echo 'systemctl enable failed: 
'"`cat $tmp/_ctl_svc`"
		    fi
		fi
	    else
		[ "$verbose" -gt 0 ] && echo "already enabled via systemctl"
	    fi
	elif $__use_update_invoke
	then
	    if [ ! -f /etc/rc5.d/S*$__svc ]
	    then
		if $show_me
		then
		    echo "# update-rc.d $__svc enable"
		else
		    # no exit status from update-rc.d ...
		    #
		    update-rc.d $__svc enable >$tmp/_ctl_svc 2>&1
		    if [ ! -f /etc/rc5.d/S*$__svc ]
		    then
			__rc=1
			[ "$verbose" -gt 0 ] && echo 'update-rc.d enable failed:
'"`cat $tmp/_ctl_svc`"
		    fi
		fi
	    else
		[ "$verbose" -gt 0 ] && echo "already enabled via update-rc.d"
	    fi
	elif $__use_chkconfig
	then
	    __runlevel=`runlevel | cut -d ' ' -f 2`
	    [ "$__runlevel" = unknown ] && __runlevel=3
	    __state=`chkconfig --list $__svc 2>/dev/null | sed -e "s/.*$__runlevel://" -e 's/[	].*//'`
	    if [ "$__state" = off -o -z "$__state" ]
	    then
		if $show_me
		then
		    echo "# chkconfig -add $__svc"
		else
		    if chkconfig --add $__svc >$tmp/_ctl_svc 2>&1
		    then
			:
		    else
			[ "$verbose" -gt 0 ] && echo 'chconfig --add failed:
    '"`cat $tmp/_ctl_svc`"
		    fi
		fi
	    else
		[ "$verbose" -gt 0 ] && echo "already enabled via chkconfig"
	    fi
	fi
    fi

    if [ "$__action" = activate -o "$__action" = start ]
    then
	if $__use_systemctl
	then
	    if [ "`systemctl is-active $__svc.service`" != active ]
	    then
		if $show_me
		then
		    echo "# systemctl start $__svc.service"
		else
		    if systemctl start $__svc.service >$tmp/_ctl_svc 2>&1
		    then
			:
		    else
			__rc=1
			[ "$verbose" -gt 0 ] && echo 'systemctl start failed:
'"`cat $tmp/_ctl_svc`"
		    fi
		fi
	    else
		[ "$verbose" -gt 0 ] && echo "already started via systemctl"
	    fi
	elif $__use_update_invoke
	then
	    invoke-rc.d $__svc status 2>/dev/null >$tmp/_ctl_svc
	    __state=`sed -n -e '/^Checking for/s/.*: //p' <$tmp/_ctl_svc`
	    if [ "$__state" != running ]
	    then
		if $show_me
		then
		    echo "# invoke-rc.d $__svc start"
		else
		    if invoke-rc.d $__svc start >$tmp/_ctl_svc 2>&1
		    then
			:
		    else
			__rc=1
			[ "$verbose" -gt 0 ] && echo 'invoke-rc.d start failed:
'"`cat $tmp/_ctl_svc`"
		    fi
		fi
	    else
		[ "$verbose" -gt 0 ] && echo "already started via invoke-rc.d"
	    fi
	elif $__use_rc_script
	then
	    $PCP_RC_DIR/$__svc status 2>/dev/null >$tmp/_ctl_svc
	    __state=`sed -n -e '/^Checking for/s/.*: //p' <$tmp/_ctl_svc`
	    if [ "$__state" = stopped ]
		then
		if $show_me
		then
		    echo "# $PCP_RC_DIR/$__svc start"
		else
		    if "$PCP_RC_DIR/$__svc" start >$tmp/_ctl_svc 2>&1
		    then
			:
		    else
			__rc=1
			[ "$verbose" -gt 0 ] && echo "$PCP_RC_DIR/$__svc"' start failed:
    '"`cat $tmp/_ctl_svc`"
		    fi
		fi
	    else
		[ "$verbose" -gt 0 ] && echo "already started via $PCP_RC_DIR/$__svc"
	    fi
	else
	    echo >&2 "$prog: _ctl_svc: Botch: cannot determine how to start $__svc service"
	    status=99
	    exit
	fi
    fi

    if [ "$__action" = deactivate -o "$__action" = stop ]
    then
	if $__use_systemctl
	then
	    if [ "`systemctl is-active $__svc.service`" != inactive ]
	    then
		if $show_me
		then
		    echo "# systemctl stop $__svc.service"
		else
		    if systemctl stop $__svc.service >$tmp/_ctl_svc 2>&1
		    then
			:
		    else
			__rc=1
			[ "$verbose" -gt 0 ] && echo 'systemctl stop failed:
'"`cat $tmp/_ctl_svc`"
		    fi
		fi
	    else
		[ "$verbose" -gt 0 ] && echo "already stopped via systemctl"
	    fi
	elif $__use_update_invoke
	then
	    invoke-rc.d $__svc status 2>/dev/null >$tmp/_ctl_svc
	    __state=`sed -n -e '/^Checking for/s/.*: //p' <$tmp/_ctl_svc`
	    if [ "$__state" = running ]
	    then
		if $show_me
		then
		    echo "# invoke-rc.d $__svc stop"
		else
		    if invoke-rc.d $__svc stop >$tmp/_ctl_svc 2>&1
		    then
			:
		    else
			__rc=1
			[ "$verbose" -gt 0 ] && echo 'invoke-rc.d stop failed:
'"`cat $tmp/_ctl_svc`"
		    fi
		fi
	    else
		[ "$verbose" -gt 0 ] && echo "already stopped via invoke-rc.d"
	    fi
	elif $__use_rc_script
	then
	    $PCP_RC_DIR/$__svc status 2>/dev/null >$tmp/_ctl_svc
	    __state=`sed -n -e '/^Checking for/s/.*: //p' <$tmp/_ctl_svc`
	    if [ "$__state" = running ]
	    then
		if $show_me
		then
		    echo "# $PCP_RC_DIR/$__svc stop"
		else
		    if "$PCP_RC_DIR/$__svc" stop >$tmp/_ctl_svc 2>&1
		    then
			:
		    else
			__rc=1
			[ "$verbose" -gt 0 ] && echo "$PCP_RC_DIR/$__svc"' stop failed:
    '"`cat $tmp/_ctl_svc`"
		    fi
		fi
	    else
		[ "$verbose" -gt 0 ] && echo "already stopped via $PCP_RC_DIR/$__svc"
	    fi
	else
	    echo >&2 "$prog: _ctl_svc: Botch: cannot determine how to stop $__svc service"
	    status=99
	    exit
	fi
    fi

    if [ "$__action" = deactivate ]
    then
	if $__use_systemctl
	then
	    if [ "`systemctl is-enabled $__svc.service`" != disabled ]
	    then
		if $show_me
		then
		    echo "# systemctl disable $__svc.service"
		else
		    if systemctl disable $__svc.service >$tmp/_ctl_svc 2>&1
		    then
			:
		    else
			__rc=1
			[ "$verbose" -gt 0 ] && echo 'systemctl disable failed:
'"`cat $tmp/_ctl_svc`"
		    fi
		fi
	    else
		[ "$verbose" -gt 0 ] && echo "already disabled via systemctl"
	    fi
	elif $__use_update_invoke
	then
	    if [ -f /etc/rc5.d/S*$__svc ]
	    then
		if $show_me
		then
		    echo "# update-rc.d $__svc disable"
		else
		    # no exit status from update-rc.d ...
		    #
		    update-rc.d $__svc disable >$tmp/_ctl_svc 2>&1
		    if [ -f /etc/rc5.d/S*$__svc ]
		    then
			__rc=1
			[ "$verbose" -gt 0 ] && echo 'update-rc.d disable failed:
'"`cat $tmp/_ctl_svc`"
		    fi
		fi
	    else
		[ "$verbose" -gt 0 ] && echo "already disabled via update-rc.d"
	    fi
	elif $__use_chkconfig
	then
	    __runlevel=`runlevel | cut -d ' ' -f 2`
	    [ "$__runlevel" = unknown ] && __runlevel=3
	    __state=`chkconfig --list $__svc 2>&1 | sed -e "s/.*$__runlevel://" -e 's/[ 	].*//'`
	    if [ "$__state" = on ]
	    then
		if $show_me
		then
		    echo "# chkconfig -del $__svc"
		else
		    if chkconfig --del $__svc >$tmp/_ctl_svc 2>&1
		    then
			:
		    else
			[ "$verbose" -gt 0 ] && echo 'chconfig --del failed:
    '"`cat $tmp/_ctl_svc`"
		    fi
		fi
	    else
		[ "$verbose" -gt 0 ] && echo "already disabled via chkconfig"
	    fi
	fi
    fi

    return $__rc
}

# control pmdas
# Usage: _ctl_pmda action name [arg ...]
# where action is one of
# - state name
# - activate name pmdaname [Install-input-file]
# - deactivate name
#
_ctl_pmda()
{
    __action="$1"
    __name="$2"
    __pre=0
    __rc=0
    __domain=0
    __pid=0
    __here=`pwd`
    __input=''

    if [ -z "$1" -o -z "$2" ]
    then
	echo >&2 "$prog: _ctl_pmda: Error: missing arguments ($1=\"$1\" $2=\"$2\")"
	status=99
	exit
    fi
    case "$__name"
    in
	pmda-*)		# strip pmda- prefix from component name
			__name=`echo "$__name" | sed -e s'/^pmda-//'`
			;;
    esac
    # need a working pmcd
    #
    pminfo -f pmcd.agent.status >$tmp/tmp 2>/dev/null
    if [ ! -s $tmp/tmp ]
    then
	[ "$verbose" -gt 0 ] && echo "need to activate pmcd"
	__pre=1
    fi
    # need the PMDA pieces to be installed
    #
    if [ ! -d "$PCP_VAR_DIR/pmdas/$__name" ]
    then
	if [ "$verbose" -gt 0 ]
	then
	    echo "need to install the PCP package for the $__name PMDA"
	fi
	__pre=2
    elif [ "$__action" = activate -a -n "$3" -a ! -f "$PCP_VAR_DIR/pmdas/$__name/$3" ]
    then
	[ "$verbose" -gt 0 ] && echo "need to install the package for the $__name PMDA"
	__pre=2
    fi
    if [ $__pre -eq 0 ]
    then
	# OK so far, check PMDA installed & running status
	# but don't issue -v verbage yet
	#
	if ! grep "\"$__name\"]" <$tmp/tmp >/dev/null
	then
	    if [ "$__action" != activate ]
	    then
		[ "$verbose" -gt 0 ] && echo "need to run the $__name PMDA's Install script"
	    fi
	    __pre=3
	elif ! grep "\"$__name\"] value 0" <$tmp/tmp >/dev/null
	then
	    # pmcd.agent.status != 0
	    #
	    if [ "$verbose" -gt 0 ]
	    then
		__rc=`sed -n <$tmp/tmp -e "/.*\"$__name\"] value /s///p"`
		echo "$pmda PMDA has failed (exit status=$__rc)"
	    fi
	    __pre=4
	fi
	__domain=`sed -n <$tmp/tmp -e "/.* inst \[\([0-9][0-9]*\).*\"$__name\"] value .*/s//\1/p"`
    fi

    # now $__pre values map to these cases:
    #  0  PMDA is installed and running (no output yet with -v)
    # (-v output already generated for the cases below)
    #  1  pmcd not running
    #  2  package providing the PMDA is not installed
    #  3  PMDA is not Installed
    #  4  PMDA is Installed, but has exited or failed
    #
    __rc=0
    case "$__action"
    in

	state)
	    case "$__pre"
	    in
		0)	# PMDA installed and OK
			if [ $verbose -gt 0 ]
			then
			    __pid=`$PCP_PS_PROG $PCP_PS_ALL_FLAGS | $PCP_AWK_PROG '/\/pmda'"$__name'"' / { print $2 }'`
			    if [ -n "$__pid" -a -n "$__domain" ]
			    then
				echo "PID $__pid, `pminfo -m | grep "PMID: $__domain\\." | wc -l | sed -e 's/  *//g'` metrics"
			    elif [ -n "$__pid" ]
			    then
				echo "PID $__pid"
			    elif [ -n "$__domain" ]
			    then
				echo "`pminfo -m | grep "PMID: $__domain\\." | wc -l | sed -e 's/  *//g'` metrics"
			    fi
			fi
			;;
		1|2)	# pmcd not running or PMDA package not installed
			__rc=2
			;;
		3|4)	# PMDA not Installed or PMDA has failed
			__rc=1
			;;
	    esac
	    ;;

	activate)
	    case "$__pre"
	    in
		0)	# PMDA installed and OK
			[ $verbose -gt 0 ] && echo "$__name PMDA already installed and active"
			;;
		1|2)	# pmcd not running or PMDA package not installed
			;;
		3)	# PMDA not Installed
			rm -f $tmp/out
			if $show_me
			then
			    echo "$ cd $PCP_VAR_DIR/pmdas/$__name"
			else
			    if cd $PCP_VAR_DIR/pmdas/$__name
			    then
				:
			    else
				status=99
				exit
			    fi
			fi
			__input='/dev/null'
			if [ -n "$4" ]
			then
			    # Install need's input
			    #
			    __input="$4"
			fi
			if $show_me
			then
			    echo "# ./Install <$__input"
			else
			    if ./Install <$__input >$tmp/out 2>&1
			    then
				:
			    else
				cat $tmp/out
				__rc=1
			    fi
			fi
			;;
		4)	# PMDA has failed
			if $show_me
			then
			    echo "# $PCP_BINADM_DIR/pmsignal -s HUP pmcd"
			else
			    if "$PCP_BINADM_DIR/pmsignal" -s HUP pmcd >$tmp/out 2>&1
			    then
				# TODO check if this worked or the PMDA failed again?
				:
			    else
				cat $tmp/out
				__rc=1
			    fi
			fi
			;;
		*)	__rc=1
			;;
	    esac
	    ;;

	deactivate)
	    case "$__pre"
	    in
		0|4)	# PMDA installed and OK or PMDA has failed
			rm -f $tmp/out
			if $show_me
			then
			    echo "$ cd $PCP_VAR_DIR/pmdas/$__name"
			else
			    if cd $PCP_VAR_DIR/pmdas/$__name
			    then
				:
			    else
				status=99
				exit
			    fi
			fi
			# Remove always works fine with </dev/null
			#
			if $show_me
			then
			    echo "# ./Remove </dev/null"
			else
			    if ./Remove </dev/null >$tmp/out 2>&1
			    then
				:
			    else
				cat $tmp/out
				__rc=1
			    fi
			fi
			;;
		1|2)	# pmcd not running or PMDA package not installed
			__rc=1
			;;
		3)	# PMDA not Installed
			[ $verbose -gt 0 ] && echo "$__name PMDA already deactivated"
			;;
	    esac
	    ;;

	*)
	    echo >&2 "$prog: _ctl_pmda: Error: bad action argument ($__action)"
	    status=99
	    exit
	    ;;
    esac

    return $__rc
}

# determine what sort of systemd/init/... we're using
#
__use_systemctl=false
__use_update_invoke=false
__use_chkconfig=false
__use_rc_script=false
if which systemctl >/dev/null 2>&1
then
    # we have a systemctl executable, but it might be disabled,
    # e.g. on MX Linux
    #
    if systemctl -q is-active local-fs.target >/dev/null 2>&1
    then
	# real and working systemctl
	#
	__use_systemctl=true
    fi
fi
if [ "$__use_systemctl" = false ]
then
    if which update-rc.d >/dev/null 2>&1
    then
	if which invoke-rc.d >/dev/null 2>&1
	then
	    __use_update_invoke=true
	fi
    fi
    if [ "$__use_update_invoke" = false ]
    then
	if which chkconfig >/dev/null 2>&1
	then
	    __use_chkconfig=true
	fi
	if [ -d $PCP_RC_DIR ]
	then
	    __use_rc_script=true
	fi
    fi
fi

if [ "$__use_systemctl" = false -a "$__use_update_invoke" = false -a "$__use_rc_script" = false ]
then
    echo >&2 "$prog: Botch: cannot determine how to control services"
    echo >&2 "$prog: Warning: no working systemctl"
    echo >&2 "$prog: Warning: no update-rc.d and invoke-rc.d"
    echo >&2 "$prog: Warning: no PCP \"rc\" dir ($PCP_RC_DIR)"
    status=99
    exit
fi
