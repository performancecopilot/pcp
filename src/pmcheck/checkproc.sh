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

# common command line processing ... use getopt(1) not pmgetopt(1) because
# we don't want PCPIntro command line arg processing
#
aflag=false
dflag=false
show_me=false
sflag=false
verbose=0

_usage()
{
    echo >&2 "Usage: $prog [options] [component]"
    echo >&2
    echo >&2 "options:"
    echo >&2 "  -a, --activate    activate component"
    echo >&2 "  -d, --deactivate  deactivate component"
    echo >&2 "  -n, --show-me     dry run"
    echo >&2 "  -s, --state       report state of component"
    echo >&2 "  -v, --verbose     increase verbosity"
    echo >&2
}

_do_args()
{
    ARGS=`getopt -n $prog -o "adnsv" -l "activate,deactivate,show-me,state,verbose" -- "$@"`
    if [ $? -ne 0 ]
    then
	_usage
	status=99
	exit
    fi

    eval set -- "$ARGS"
    unset ARGS

    while true
    do
	case "$1"
	in
	    '-a'|'--activate')
		    aflag=true
		    shift
		    continue
		    ;;
	    '-d'|'--deactivate')
		    dflag=true
		    shift
		    continue
		    ;;
	    '-n'|'--show-me')
		    show_me=true
		    shift
		    continue
		    ;;
	    '-s'|'--state')
		    sflag=true
		    shift
		    continue
		    ;;
	    '-v'|'--verbose')
		    verbose=`expr $verbose + 1`
		    shift
		    continue
		    ;;
	    '--')
		    shift
		    break
		    ;;
	    *)
		    echo >&2 "getopt iterator botch \$1=\"$1\" ..."
		    status=99
		    exit
		    ;;
	esac
    done
    if [ $# -eq 0 ]
    then
	component="$prog"
    elif [ $# -eq 1 ]
    then
	component="$1"
    else
	_usage
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
    rc=0
    if [ -z "$1" -o -z "$2" ]
    then
	echo >&2 "$prog: _ctl_svc: Error: missing arguments ($1=\"$1\", $2=\"$2\")"
	status=99
	exit
    fi
    action="$1"
    svc="$2"
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
			status=2
			echo "No systemd unit file installed"
		    else
			status=1
		    fi
		else
		    # no clue ...
		    status=3
		fi
	    fi
	elif $__use_update_invoke
	then
	    invoke-rc.d $svc status 2>/dev/null >$tmp/_ctl_svc
	    __state=`sed -n -e '/^Checking for/s/.*: //p' <$tmp/_ctl_svc`
	    case "$__state"
	    in
		running)
		    ;;
		stopped)
		    status=1
		    ;;
		*)
		    status=3
		    echo "state ($__state) unknown"
		    ;;
	    esac
	else
	    echo >&2 "$prog: _ctl_svc: Botch: cannot determine how to get state of $svc service"
	    status=99
	    exit
	fi
	exit
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
	else
	    echo >&2 "$prog: _ctl_svc: Botch: cannot determine how to activate $svc service"
	    status=99
	    exit
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
	    __state=`sed -n -e '/^Checking for/s/.*: //p' <$tmp/_ctl_svc`
	    if [ "$__state" != running ]
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
	    __state=`sed -n -e '/^Checking for/s/.*: //p' <$tmp/_ctl_svc`
	    if [ "$__state" = running ]
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
	else
	    echo >&2 "$prog: _ctl_svc: Botch: cannot determine how to deactivate $svc service"
	    status=99
	    exit
	fi
    fi

    return $rc
}

# determine what sort of systemd/init/... we're using
#
__use_systemctl=false
__use_update_invoke=false
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
fi

if [ "$__use_systemctl" = false -a "$__use_update_invoke" = false ]
then
    echo >&2 "$prog: Botch: cannot determine how to control services"
    echo >&2 "$prog: Warning: no working systemctl"
    echo >&2 "$prog: Warning: no update-rc.d and invoke-rc.d"
    status=99
    exit
fi
