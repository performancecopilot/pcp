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
    local args=`pmgetopt --progname=$prog --config=$tmp/_usage -- "$@"`
    if [ $? -ne 0 ]
    then
	pmgetopt --progname=$prog --config=$tmp/_usage --usage
	status=99
	exit
    fi

    eval set -- "$args"
    unset args

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
    local rc=0
    local action="$1"
    local svc="$2"
    local state=''
    local runlevel=''

    if [ -z "$1" -o -z "$2" ]
    then
	echo >&2 "$prog: _ctl_svc: Error: missing arguments ($1=\"$1\", $2=\"$2\")"
	status=99
	exit
    fi
    case "$action"
    in
	state|start|stop|activate|deactivate)
	    ;;
	*)
	    echo >&2 "$prog: _ctl_svc: Error: bad action argument ($action) for $svc service"
	    status=99
	    exit
	    ;;
    esac

    if [ "$action" = state ]
    then
	if $__use_systemctl
	then
	    systemctl show $svc.service >$tmp/_ctl_svc 2>&1
	    if grep -E '^ActiveState=(active|activating)$' <$tmp/_ctl_svc >/dev/null
	    then
		# active, all good
		:
	    else
		# not active, need to dig a bit ...
		if grep '^ActiveState=inactive' <$tmp/_ctl_svc >/dev/null
		then
		    if grep '^LoadError=.*\.service not found' <$tmp/_ctl_svc >/dev/null
		    then
			rc=2
			echo "No systemd unit file installed"
		    else
			rc=1
		    fi
		else
		    # no clue ...
		    echo "systemd unit $svc.service is not active, activating or inactive?"
		    rc=2
		fi
	    fi
	elif $__use_update_invoke
	then
	    invoke-rc.d $svc status 2>/dev/null >$tmp/_ctl_svc
	    state=`sed -n -e '/^Checking for/s/.*: //p' <$tmp/_ctl_svc`
	    case "$state"
	    in
		running)
		    ;;
		stopped)
		    rc=1
		    ;;
		*)
		    rc=2
		    echo "state ($state) unknown"
		    ;;
	    esac
	elif $__use_rc_script
	then
	    if [ ! -x "$PCP_RC_DIR/$svc" ]
	    then
		rc=2
		echo "$PCP_RC_DIR/$svc script is missing"
	    elif $__use_chkconfig
	    then
		# runlevel emits
		# <previous> 3
		# chconfig emits ...
		# pmcd           	0:off	1:off	2:on	3:on	4:on	5:on	6:off
		# want                                            ^^
		runlevel=`runlevel | cut -d ' ' -f 2`
		state=`chkconfig --list $svc | sed -e "s/.*$runlevel//" -e 's/ .*//'`
		case "$state"
		in
		    on)
			;;
		    off)
			rc=1
			;;
		    *)
			rc=2
			echo "state ($state) from chkcofig unknown"
			;;
		esac
	    fi
	else
	    echo >&2 "$prog: _ctl_svc: Botch: cannot determine how to get state of $svc service"
	    status=99
	    exit
	fi
	return $rc
    fi

    if [ "$action" = activate ]
    then
	if $__use_systemctl
	then
	    if [ "`systemctl is-enabled $svc.service`" != enabled ]
	    then
		if $show_me
		then
		    echo "# systemctl enable $svc.service"
		else
		    if systemctl enable $svc.service >$tmp/_ctl_svc 2>&1
		    then
			:
		    else
			rc=1
			[ "$verbose" -gt 0 ] && echo 'systemctl enable failed: 
'"`cat $tmp/_ctl_svc`"
		    fi
		fi
	    else
		[ "$verbose" -gt 0 ] && echo "already enabled via systemctl"
	    fi
	elif $__use_update_invoke
	then
	    if [ ! -f /etc/rc5.d/S*$svc ]
	    then
		if $show_me
		then
		    echo "# update-rc.d $svc enable"
		else
		    # no exit status from update-rc.d ...
		    #
		    update-rc.d $svc enable >$tmp/_ctl_svc 2>&1
		    if [ ! -f /etc/rc5.d/S*$svc ]
		    then
			rc=1
			[ "$verbose" -gt 0 ] && echo 'update-rc.d enable failed:
'"`cat $tmp/_ctl_svc`"
		    fi
		fi
	    else
		[ "$verbose" -gt 0 ] && echo "already enabled via update-rc.d"
	    fi
	elif $__use_chkconfig
	then
	    runlevel=`runlevel | cut -d ' ' -f 2`
	    state=`chkconfig --list $svc | sed -e "s/.*$runlevel//" -e 's/ .*//'`
	    if [ "$state" == off ]
	    then
		if $show_me
		then
		    echo "# chkconfig -add $svc"
		else
		    if chkconfig --add $svc >$tmp/_ctl_svc 2>&1
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

    if [ "$action" = activate -o "$action" = start ]
    then
	if $__use_systemctl
	then
	    if [ "`systemctl is-active $svc.service`" != active ]
	    then
		if $show_me
		then
		    echo "# systemctl start $svc.service"
		else
		    if systemctl start $svc.service >$tmp/_ctl_svc 2>&1
		    then
			:
		    else
			rc=1
			[ "$verbose" -gt 0 ] && echo 'systemctl start failed:
'"`cat $tmp/_ctl_svc`"
		    fi
		fi
	    else
		[ "$verbose" -gt 0 ] && echo "already started via systemctl"
	    fi
	elif $__use_update_invoke
	then
	    invoke-rc.d $svc status 2>/dev/null >$tmp/_ctl_svc
	    state=`sed -n -e '/^Checking for/s/.*: //p' <$tmp/_ctl_svc`
	    if [ "$state" != running ]
	    then
		if $show_me
		then
		    echo "# invoke-rc.d $svc start"
		else
		    if invoke-rc.d $svc start >$tmp/_ctl_svc 2>&1
		    then
			:
		    else
			rc=1
			[ "$verbose" -gt 0 ] && echo 'invoke-rc.d start failed:
'"`cat $tmp/_ctl_svc`"
		    fi
		fi
	    else
		[ "$verbose" -gt 0 ] && echo "already started via invoke-rc.d"
	    fi
	elif $__use_rc_script
	then
	    $PCP_RC_DIR/$svc status 2>/dev/null >$tmp/_ctl_svc
	    state=`sed -n -e '/^Checking for/s/.*: //p' <$tmp/_ctl_svc`
	    if [ "$state" = stopped ]
		then
		if $show_me
		then
		    echo "# $PCP_RC_DIR/$svc start"
		else
		    if "$PCP_RC_DIR/$svc" start >$tmp/_ctl_svc 2>&1
		    then
			:
		    else
			rc=1
			[ "$verbose" -gt 0 ] && echo "$PCP_RC_DIR/$svc"' start failed:
    '"`cat $tmp/_ctl_svc`"
		    fi
		fi
	    else
		[ "$verbose" -gt 0 ] && echo "already started via $PCP_RC_DIR/$svc"
	    fi
	else
	    echo >&2 "$prog: _ctl_svc: Botch: cannot determine how to start $svc service"
	    status=99
	    exit
	fi
    fi

    if [ "$action" = deactivate -o "$action" = stop ]
    then
	if $__use_systemctl
	then
	    if [ "`systemctl is-active $svc.service`" != inactive ]
	    then
		if $show_me
		then
		    echo "# systemctl stop $svc.service"
		else
		    if systemctl stop $svc.service >$tmp/_ctl_svc 2>&1
		    then
			:
		    else
			rc=1
			[ "$verbose" -gt 0 ] && echo 'systemctl stop failed:
'"`cat $tmp/_ctl_svc`"
		    fi
		fi
	    else
		[ "$verbose" -gt 0 ] && echo "already stopped via systemctl"
	    fi
	elif $__use_update_invoke
	then
	    invoke-rc.d $svc status 2>/dev/null >$tmp/_ctl_svc
	    state=`sed -n -e '/^Checking for/s/.*: //p' <$tmp/_ctl_svc`
	    if [ "$state" = running ]
	    then
		if $show_me
		then
		    echo "# invoke-rc.d $svc stop"
		else
		    if invoke-rc.d $svc stop >$tmp/_ctl_svc 2>&1
		    then
			:
		    else
			rc=1
			[ "$verbose" -gt 0 ] && echo 'invoke-rc.d stop failed:
'"`cat $tmp/_ctl_svc`"
		    fi
		fi
	    else
		[ "$verbose" -gt 0 ] && echo "already stopped via invoke-rc.d"
	    fi
	elif $__use_rc_script
	then
	    $PCP_RC_DIR/$svc status 2>/dev/null >$tmp/_ctl_svc
	    state=`sed -n -e '/^Checking for/s/.*: //p' <$tmp/_ctl_svc`
	    if [ "$state" = running ]
	    then
		if $show_me
		then
		    echo "# $PCP_RC_DIR/$svc stop"
		else
		    if "$PCP_RC_DIR/$svc" stop >$tmp/_ctl_svc 2>&1
		    then
			:
		    else
			rc=1
			[ "$verbose" -gt 0 ] && echo "$PCP_RC_DIR/$svc"' stop failed:
    '"`cat $tmp/_ctl_svc`"
		    fi
		fi
	    else
		[ "$verbose" -gt 0 ] && echo "already stopped via $PCP_RC_DIR/$svc"
	    fi
	else
	    echo >&2 "$prog: _ctl_svc: Botch: cannot determine how to stop $svc service"
	    status=99
	    exit
	fi
    fi

    if [ "$action" = deactivate ]
    then
	if $__use_systemctl
	then
	    if [ "`systemctl is-enabled $svc.service`" != disabled ]
	    then
		if $show_me
		then
		    echo "# systemctl disable $svc.service"
		else
		    if systemctl disable $svc.service >$tmp/_ctl_svc 2>&1
		    then
			:
		    else
			rc=1
			[ "$verbose" -gt 0 ] && echo 'systemctl disable failed:
'"`cat $tmp/_ctl_svc`"
		    fi
		fi
	    else
		[ "$verbose" -gt 0 ] && echo "already disabled via systemctl"
	    fi
	elif $__use_update_invoke
	then
	    if [ -f /etc/rc5.d/S*$svc ]
	    then
		if $show_me
		then
		    echo "# update-rc.d $svc disable"
		else
		    # no exit status from update-rc.d ...
		    #
		    update-rc.d $svc disable >$tmp/_ctl_svc 2>&1
		    if [ -f /etc/rc5.d/S*$svc ]
		    then
			rc=1
			[ "$verbose" -gt 0 ] && echo 'update-rc.d disable failed:
'"`cat $tmp/_ctl_svc`"
		    fi
		fi
	    else
		[ "$verbose" -gt 0 ] && echo "already disabled via update-rc.d"
	    fi
	elif $__use_chkconfig
	then
	    runlevel=`runlevel | cut -d ' ' -f 2`
	    state=`chkconfig --list $svc | sed -e "s/.*$runlevel//" -e 's/ .*//'`
	    if [ "$state" == on ]
	    then
		if $show_me
		then
		    echo "# chkconfig -del $svc"
		else
		    if chkconfig --del $svc >$tmp/_ctl_svc 2>&1
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

    return $rc
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
    local action="$1"
    local name="$2"
    local pre=0
    local rc=0
    local domain=0
    local pid=0
    local here=`pwd`
    local input=''

    if [ -z "$1" -o -z "$2" ]
    then
	echo >&2 "$prog: _ctl_pmda: Error: missing arguments ($1=\"$1\" $2=\"$2\")"
	status=99
	exit
    fi
    case "$name"
    in
	pmda-*)		# strip pmda- prefix from component name
			name=`echo "$name" | sed -e s'/^pmda-//'`
			;;
    esac
    # need a working pmcd
    #
    pminfo -f pmcd.agent.status >$tmp/tmp 2>/dev/null
    if [ ! -s $tmp/tmp ]
    then
	[ "$verbose" -gt 0 ] && echo "need to activate pmcd"
	pre=1
    fi
    # need the PMDA pieces to be installed
    #
    if [ ! -d "$PCP_VAR_DIR/pmdas/$name" ]
    then
	if [ "$verbose" -gt 0 ]
	then
	    echo "need to install the PCP package for the $name PMDA"
	fi
	pre=2
    elif [ "$action" = activate -a -n "$3" -a ! -x "$PCP_VAR_DIR/pmdas/$name/$3" ]
    then
	[ "$verbose" -gt 0 ] && echo "need to install the package for the $name PMDA"
	pre=2
    fi
    if [ $pre -eq 0 ]
    then
	# OK so far, check PMDA installed & running status
	# but don't issue -v verbage yet
	#
	if ! grep "\"$name\"]" <$tmp/tmp >/dev/null
	then
	    if [ "$action" != activate ]
	    then
		[ "$verbose" -gt 0 ] && echo "need to run the $name PMDA's Install script"
	    fi
	    pre=3
	elif ! grep "\"$name\"] value 0" <$tmp/tmp >/dev/null
	then
	    # pmcd.agent.status != 0
	    #
	    if [ "$verbose" -gt 0 ]
	    then
		rc=`sed -n <$tmp/tmp -e "/.*\"$name\"] value /s///p"`
		echo "$pmda PMDA has failed (exit status=$rc)"
	    fi
	    pre=4
	fi
	domain=`sed -n <$tmp/tmp -e "/.* inst \[\([0-9][0-9]*\).*\"$name\"] value .*/s//\1/p"`
    fi

    # now $pre values map to these cases:
    #  0  PMDA is installed and running (no output yet with -v)
    # (-v output already generated for the cases below)
    #  1  pmcd not running
    #  2  package providing the PMDA is not installed
    #  3  PMDA is not Installed
    #  4  PMDA is Installed, but has exited or failed
    #
    rc=0
    case "$action"
    in

	state)
	    case "$pre"
	    in
		0)	# PMDA installed and OK
			if [ $verbose -gt 0 ]
			then
			    pid=`$PCP_PS_PROG $PCP_PS_ALL_FLAGS | $PCP_AWK_PROG '/\/pmda'"$name'"' / { print $2 }'`
			    if [ -n "$pid" -a -n "$domain" ]
			    then
				echo "PID $pid, `pminfo -m | grep "PMID: $domain\\." | wc -l | sed -e 's/  *//g'` metrics"
			    elif [ -n "$pid" ]
			    then
				echo "PID $pid"
			    elif [ -n "$domain" ]
			    then
				echo "`pminfo -m | grep "PMID: $domain\\." | wc -l | sed -e 's/  *//g'` metrics"
			    fi
			fi
			;;
		1|2)	# pmcd not running or PMDA package not installed
			rc=2
			;;
		3|4)	# PMDA not Installed or PMDA has failed
			rc=1
			;;
	    esac
	    ;;

	activate)
	    case "$pre"
	    in
		0)	# PMDA installed and OK
			[ $verbose -gt 0 ] && echo "$name PMDA already installed and active"
			;;
		1|2)	# pmcd not running or PMDA package not installed
			;;
		3)	# PMDA not Installed
			rm -f $tmp/out
			if $show_me
			then
			    echo "$ cd $PCP_VAR_DIR/pmdas/$name"
			else
			    if cd $PCP_VAR_DIR/pmdas/$name
			    then
				:
			    else
				status=99
				exit
			    fi
			fi
			input='/dev/null'
			if [ -n "$4" ]
			then
			    # Install need's input
			    #
			    input="$4"
			fi
			if $show_me
			then
			    echo "# ./Install <$input"
			else
			    if ./Install <$input >$tmp/out 2>&1
			    then
				:
			    else
				cat $tmp/out
				rc=1
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
				rc=1
			    fi
			fi
			;;
		*)	rc=1
			;;
	    esac
	    ;;

	deactivate)
	    case "$pre"
	    in
		0|4)	# PMDA installed and OK or PMDA has failed
			rm -f $tmp/out
			if $show_me
			then
			    echo "$ cd $PCP_VAR_DIR/pmdas/$name"
			else
			    if cd $PCP_VAR_DIR/pmdas/$name
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
				rc=1
			    fi
			fi
			;;
		1|2)	# pmcd not running or PMDA package not installed
			rc=1
			;;
		3)	# PMDA not Installed
			[ $verbose -gt 0 ] && echo "$name PMDA already deactivated"
			;;
	    esac
	    ;;

	*)
	    echo >&2 "$prog: _ctl_pmda: Error: bad action argument ($action)"
	    status=99
	    exit
	    ;;
    esac

    return $rc
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
