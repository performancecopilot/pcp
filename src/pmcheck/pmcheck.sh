#!/bin/sh
#
# Check/report-status/enable/disable/ PCP components
#

# common initialization
#
. $PCP_DIR/etc/pcp.env || exit 1

status=0
prog=`basename $0`
tmp=`mktemp -d "$PCP_TMPFILE_DIR/pmcheck.XXXXXXXXX"`
if [ ! -d "$tmp" ]
then
    echo >&2 "$prog: Error: cannot create temp directory: $PCP_TMPFILE_DIR/$prog.XXXXXXXXX"
    exit 9
fi
trap "rm -rf $tmp; exit \$status" 0 1 2 3 15

# common command line processing ... use getopt(1) not pmgetopt(1) because
# we don't want PCPIntro command line arg processing
#
aflag=false
cfile=''
dflag=false
lflag=false
show_me=false
sflag=false
verbose=0
xflag=false
action=''

_usage()
{
    echo >&2 "Usage: $prog [options] [component ...]"
    echo >&2
    echo >&2 "options:"
    echo >&2 "  -a, --activate    activate component(s)"
    echo >&2 "  -c SCRIPT, --file=SCRIPT"
    echo >&2 "                    use this executable SCRIPT file instead of a"
    echo >&2 "                    standard component"
    echo >&2 "  -d, --deactivate  deactivate component(s)"
    echo >&2 "  -l, --list        list components"
    echo >&2 "  -n, --show-me     dry run"
    echo >&2 "  -s, --state       report state of component(s)"
    echo >&2 "  -v, --verbose     increase verbosity"
    echo >&2 "  -x, --trace       run component script with sh -x"
    echo >&2 "  -?, --help        show this usage message"
    echo >&2
    echo >&2 "component is one or more manageable components and may include shell"
    echo >&2 "glob metacharacters, e.g. pmda.* or pm[cl]*"
}

ARGS=`getopt -n $prog -o "ac:dlnsvx?" -l "activate,file:,deactivate,list,show-me,state,verbose,trace,help" -- "$@"`
if [ $? -ne 0 ]
then
    _usage
    status=9
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
		action="$action -a"
		shift
		continue
		;;
	'-c'|'--file')
		if [ ! -f "$2" ]
		then
		    echo >&2 "$prog: Error: \"$2\" does not exist"
		    status=9
		    exit
		fi
		if [ ! -x "$2" ]
		then
		    echo >&2 "$prog: Error: \"$2\" must be an executable script"
		    status=9
		    exit
		fi
		cfile="$2"
		shift; shift
		continue
		;;
	'-d'|'--deactivate')
		dflag=true
		action="$action -d"
		shift
		continue
		;;
	'-l'|'--list')
		lflag=true
		shift
		continue
		;;
	'-n'|'--dry-run')
		show_me=true
		action="$action -n"
		shift
		continue
		;;
	'-s'|'--state')
		sflag=true
		action="$action -s"
		shift
		continue
		;;
	'-v'|'--verbose')
		verbose=`expr $verbose + 1`
		action="$action -v"
		shift
		continue
		;;
	'-x'|'--trace')
		xflag=true
		shift
		continue
		;;
	'-?'|'--help')
		_usage
		exit
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

# semantic checks on command line arguments
#
if [ -n "$cfile" -a "$lflag" = true ]
then
    echo >&2 "$prog: Error: options -c and -l are mutually exclusive"
    status=9
    exit
fi
if [ "$lflag" = true -a $# -gt 0 ]
then
    echo >&2 "$prog: Error: option -l and component arguments are mutually exclusive"
    status=9
    exit
fi
if [ -n "$cfile" -a $# -gt 0 ]
then
    echo >&2 "$prog: Error: option -c and component arguments are mutually exclusive"
    status=9
    exit
fi
if [ -z "$action" -a "$lflag" = false ]
then
    echo >&2 "$prog: Error: one of -a, -d, -s or -l must be specified"
    status=9
    exit
fi
if [ "$aflag" = false -a "$dflag" = false -a "$show_me" = true ]
then
    echo >&2 "$prog: Warning: neither -a nor -d specified, so -n ignored"
    show_me=false
fi

nop=0
$aflag && nop=`expr $nop + 1`
$dflag && nop=`expr $nop + 1`
$lflag && nop=`expr $nop + 1`
$sflag && nop=`expr $nop + 1`
if [ "$nop" -gt 1 ]
then
    echo >&2 "$prog: Error: at most one of -a, -d, -l or -s may be specified"
    status=9
    exit
fi

if [ $# -eq 0 ]
then
    # no components on the command line, do 'em all, but need
    # to skip ones left behind by packaging
    #
    rm -f $tmp/tmp
    set -- `find $PCP_SYSCONF_DIR/pmcheck -type f \
	    | $PCP_AWK_PROG '
/\.dpkg-old/	{ print >"'"$tmp/tmp"'"; next }
/\.dpkg-new/	{ print >"'"$tmp/tmp"'"; next }
		{ print }' \
	    | sed \
		-e "s@$PCP_SYSCONF_DIR/pmcheck/@@" \
	    | LC_COLLATE=POSIX sort`
    if [ -s $tmp/tmp ]
    then
	echo >&2 "$prog: Warning: ignoring component files from package installation: `cat $tmp/tmp`"
	rm -f $tmp/tmp
    fi
fi

_report()
{
    if $show_me
    then
	cat $tmp/out
	return
    fi

    if $sflag
    then
	case "$2"
	in
	    0)	msg="active"
	    	;;
	    1)	msg="could be activated"
	    	;;
	    2)	msg="cannot be activated"
	    	;;
	    3)	msg="not sure"
		;;
	    *)	msg="botch, rc=$2"
	    	;;
	esac
    else
	case "$2"
	in
	    0)	msg="success"
	    	;;
	    1)	msg="failure"
	    	;;
	    *)	msg="botch, rc=$2"
	    	;;
	esac
    fi
    printf "%-12s %-20s" "$1" "$msg"
    if [ -s $tmp/out ]
    then
	first=true
	cat $tmp/out \
	| while read line
	do
	    if $first
	    then
		printf " %s\n" "$line"
		first=false
	    else
		printf "%-12s %-20s %s\n" "" "" "$line"
	    fi
	done
    else
	printf "\n"
    fi
}

if $lflag
then
    # list all known components
    #
    for component
    do
	echo "$component"
    done
    exit
fi

if [ "$cfile" != "" ]
then
    if $xflag
    then
	sh -x "$cfile" $action >$tmp/out
    else
	"$cfile" $action >$tmp/out
    fi
    rc=$?
    _report "$cfile" $rc
else
    for pattern in "$@"
    do
	for script in "$PCP_SYSCONF_DIR"/pmcheck/$pattern
	do
	    component=`basename "$script"`
	    if [ ! -x "$script" ]
	    then
		echo "$prog: Error: $script not found or not executable"
	    else
		if $xflag
		then
		    sh -x "$script" $action $component >$tmp/out
		else
		    "$script" $action $component >$tmp/out
		fi
		rc=$?
		_report "$component" $rc
	    fi
	done
    done
fi
