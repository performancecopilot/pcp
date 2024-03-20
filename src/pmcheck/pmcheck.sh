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

# common command line processing ... use pmgetopt(1) for portability
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

cat <<'End-of-File' >$tmp/_usage
# getopts: ac:dlnsvx?
# usage: [options] [component ...]

options:
  -a, --activate    activate component(s)
  -c=SCRIPT, --file=SCRIPT use this executable SCRIPT file instead of a standard component
  -d, --deactivate  deactivate component(s)
  -l, --list        list components
  -n, --show-me     dry run
  -s, --state       report state of component(s)
  -v, --verbose     increase verbosity
  -x, --trace       run component script with sh -x
  -?, --help        show this usage message
# end

component is one or more manageable components and may include shell
glob metacharacters, e.g. pmda.* or pm[cl]*
End-of-File

__args=`pmgetopt --progname=$prog --config=$tmp/_usage -- "$@"`
if [ $? -ne 0 ]
then
    pmgetopt --progname=$prog --config=$tmp/_usage --usage
    status=9
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
		action="$action -a"
		;;
	-c)	# alternate component script
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
		shift
		;;
	-d)	# deactivate
		dflag=true
		action="$action -d"
		;;
	-l)	# list
		lflag=true
		action="$action -l"
		;;
	-n)	# dry run
		show_me=true
		action="$action -n"
		;;
	-s)	# state
		sflag=true
		action="$action -s"
		;;
	-v)	# verbose
		verbose=`expr $verbose + 1`
		action="$action -v"
		;;
	-x)	# sh -x tracing
		xflag=true
		;;
	--)	# end of opts
		shift
		break
		;;
	-\?)	# help or error
		pmgetopt --progname=$prog --config=$tmp/_usage --usage
		exit
		;;
	-\*)	# botch
		echo >&2 "$prog: Error: unrecognized option ($1)"
		status=9
		exit
    esac
    shift
done

# semantic checks on command line arguments
#
if [ -n "$cfile" -a "$lflag" = true ]
then
    echo >&2 "$prog: Error: options -c and -l are mutually exclusive"
    status=9
    exit
fi
if [ -n "$cfile" -a $# -gt 0 ]
then
    echo >&2 "$prog: Error: option -c and component arguments are mutually exclusive"
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
if [ "$nop" -eq 0 ]
then
    # default to -s
    #
    sflag=true
    action="$action -s"
fi
if [ "$nop" -gt 1 ]
then
    echo >&2 "$prog: Error: at most one of -a, -d, -l or -s may be specified"
    status=9
    exit
fi

if [ $# -eq 0 ]
then
    # no components on the command line, do 'em all
    #
    rm -f $tmp/tmp
    set -- `find $PCP_SHARE_DIR/lib/pmcheck -type f \
	    | sed \
		-e "s@$PCP_SHARE_DIR/lib/pmcheck/@@" \
	    | LC_COLLATE=POSIX sort`
    if [ -s $tmp/tmp ]
    then
	echo >&2 "$prog: Warning: ignoring component files from package installation: `cat $tmp/tmp`"
	rm -f $tmp/tmp
    fi
fi

# reporting for -a, -d and -s
#
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
    printf "%-15s %-20s" "$1" "$msg"
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
		printf "%-15s %-20s %s\n" "" "" "$line"
	    fi
	done
    else
	printf "\n"
    fi
}

# reporting for -l
#
_report_list()
{
    printf "%-15s" "$1"
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
		printf "%-15s %s\n" "" "$line"
	    fi
	done
    else
	printf "\n"
    fi
}

if $lflag
then
    # list components
    #
    for component
    do
	script="$PCP_SHARE_DIR/lib"/pmcheck/$component
	if [ ! -x "$script" ]
	then
	    echo "$prog: Error: $script not found or not executable"
	else
	    rm -f $tmp/out
	    if [ $verbose -gt 0 ]
	    then
		if $xflag
		then
		    sh -x "$script" $action $component >$tmp/out
		else
		    "$script" $action $component >$tmp/out
		fi
	    fi
	    _report_list "$component"
	fi
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
	for script in "$PCP_SHARE_DIR/lib"/pmcheck/$pattern
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
