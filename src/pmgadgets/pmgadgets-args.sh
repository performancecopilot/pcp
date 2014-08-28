#
# Copyright (c) 2014 Red Hat.
# Copyright (c) 1996-2002 Silicon Graphics, Inc.  All Rights Reserved.
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

#
# Note on error handling
#
# We assume this is sourced from a shell script that uses $status to pass
# the exit status back to the parent.
#

# source the PCP configuration environment variables
. $PCP_DIR/etc/pcp.env

prog=`basename $0`

#
# Standard usage and command-line argument parsing for pmgadgets front ends.
# This file should be included by pmgadgets front end scripts to present a
# consistent interface. See pmgadgets(1) for more information.
#

#
# The front end scripts should call _pmgadgets_usage after their own usage 
# information in a subroutine called _usage. The _usage subroutine may be 
# called by either _pmgadgets_note or _pmgadgets_args.
#

_pmgadgets_usage()
{
    echo '
  -C                     check configuration file and exit
  -h host                metrics source is PMCD on host
  -n pmnsfile            use an alternative PMNS
  -t interval            sample interval [default 2.0 seconds]
  -z                     set reporting timezone to local time of metrics source
  -Z timezone            set reporting timezone

  -zoom factor           make the gadgets bigger by a factor of 1, 2, 3 or 4
  -infofont fontname     use fontname for text in info dialogs
  -defaultfont fontname  use fontname for label gadgets

  -display display-string
  -geometry geometry-string
  -name name-string
  -title title-string'
}

#
# One of the first actions of a front end script should be to call 
# _pmgadgets_args. It sets the following variables:
#
# host		the host specified with -h.
# interval	the update interval specified by the user, 0 indicates it
#		was not specified. The caller must pass this on to pmgadgets,
#               unlike $host which is included in $args.
# args          The list of args that pmgadgets will comprehend and use.
# otherArgs     The arguments pmgadgets will not understand and should be
#               handled by the front end script.
# titleArg	The title the user prefers. If empty, the title should be
#               provided by the front end script.
# prog		The name of the program.
# namespace	The namespace (including the flag) if specified, else empty
#               eg "-n foo"
# msource	The metrics source, whether live or an archive, include the
#               flag. eg "-h blah"
#

_pmgadgets_args()
{

host=""
args=""
otherArgs=""
titleArg=""
namespace=""
interval=0
msource=""

if [ $# -eq 1 -a '$1' = '-\?' ]
then
   _usage
   status=0
   exit
fi

while [ $# -gt 0 ]
do
    case $1
    in
	-g*|-d*|-fg|-bg|-name)
	    # assume an X11 argument
	    if [ $# -lt 2 ]
	    then
		_pmgadgets_note Usage-Error error "$prog: X-11 option $1 requires one argument"
		#NOTREACHED
	    fi
	    args="$args $1 '$2'"
	    shift
	    ;;

	-title)
	    # assume an X11 argument
	    if [ $# -lt 2 ]
	    then
		echo "$prog: $1 requires one argument"
		_pmgadgets_note Usage-Error error "$prog: Option $1 requires one argument"
		#NOTREACHED
	    fi
	    titleArg="$2"
	    shift
	    ;;

	-D|-Z|-delta|-zoom|-infofont|-defaultfont)
	    if [ $# -lt 2 ]
	    then
		_pmgadgets_note Usage-Error error "$prog: Option $1 requires one argument"
		#NOTREACHED
	    fi
	    args="$args $1 '$2'"
	    shift
	    ;;

	-D*|-Z*|-z|-C)
	    args="$args $1"
	    ;;

	-h)
	    if [ "X$host" != X ]
	    then
		_pmgadgets_note Usage-Error error "$prog: Only one -h option may be specified"
		#NOTREACHED
	    fi
	    if [ $# -lt 2 ]
	    then
		_pmgadgets_note Usage-Error error "$prog: Option $1 requires one argument"
		#NOTREACHED
	    fi
	    host=$2
	    args="$args -h $2"
	    msource="-h $host"
	    shift
	    ;;

	-n)
	    if [ $# -lt 2 ]
	    then
		_pmgadgets_note Usage-Error error "$prog: Option $1 requires one argument"
		#NOTREACHED
	    fi
	    namespace="-n $2"
	    args="$args -n $2"
	    shift
	    ;;

	-t)
	    if [ $# -lt 2 ]
	    then
		_pmgadgets_note Usage-Error error "$prog: Option $1 requires one argument"
		#NOTREACHED
	    fi
	    interval=$2
	    shift
	    ;;

	*)
	    otherArgs="$otherArgs $1"
	    ;;

    esac
    shift
done

if [ -z "$host" ]
then
    host=`pmhostname`
    msource="-h $host"
fi
}

# standard fatal error reporting
# Usage: _pmgadgets_error message goes in here
#        _pmgadgets_error -f file
#
_pmgadgets_error()
{
    _pmgadgets_note Error error $*
}

# standard warning
# Usage: _pmgadgets_warning message goes in here
#        _pmgadgets_warning -f file
#
_pmgadgets_warning()
{
    _pmgadgets_note Warning warning $*
}

# standard info
# Usage: _pmgadgets_info message goes in here
#        _pmgadgets_info -f file
#
_pmgadgets_info()
{
    _pmgadgets_note Info info $*
}

# generic notifier
# Usage: _pmgadgets_note tag icon args ...
#
_pmgadgets_note()
{
    tag=$1; shift
    icon=$1; shift
    button=""
    [ $tag = Error ] && button="-B Quit"
    [ $tag = Usage-Error ] && button="-B Quit -b Usage"

    [ X"$PCP_STDERR" = XDISPLAY -a -z "$DISPLAY" ] && unset PCP_STDERR

    if [ $# -eq 2 -a "X$1" = X-f ]
    then
	case "$PCP_STDERR"
	in
	    DISPLAY)
		ans=`$PCP_XCONFIRM_PROG -icon $icon -file $2 -useslider -header "$tag $prog" $button 2>&1`
		;;
	    '')
		echo "$tag:" >&2
		cat $2 >&2
		ans=Quit
		;;
	    *)
		echo "$tag:" >>$PCP_STDERR
		cat $2 >>$PCP_STDERR
		ans=Quit
		;;
	esac
    else
	case "$PCP_STDERR"
	in
	    DISPLAY)
		ans=`$PCP_XCONFIRM_PROG -icon $icon -t "$*" -noframe -header "$tag $prog" $button 2>&1`
		;;
	    '')
		echo "$tag: $*" >&2
		ans=Quit
		;;
	    *)
		echo "$tag: $*" >>$PCP_STDERR
		ans=Quit
		;;
	esac
    fi

    if [ $tag = Usage-Error ]
    then
	[ $ans = Usage ] && _usage
	tag=Error
    fi

    if [ $tag = Error ]
    then
	status=1
	exit
    fi
}

# Fetch metrics
#
# $1                    - pmprobe flag
# $2                    - metric name
# number                - number of values
# $tmp/pmgadgets_result - values
#
_pmgadgets_fetch()
{
    if pmprobe $namespace $msource $1 $2 > $tmp/pmgadgets_fetch 2>&1
    then
	tr < $tmp/pmgadgets_fetch ' ' '\012' \
	| tee $tmp/pmgadgets_output \
	| sed \
		-e 's/"//g' \
		-e '1,2d' \
	> $tmp/pmgadgets_result

	number=`sed -n -e 2p < $tmp/pmgadgets_output`
	[ $number -le 0 ] && return 1
    else
        number=0
	rm -f $tmp/pmgadgets_output $tmp/pmgadgets_result
 	return 1
    fi

    return 0
}

# Fetch the metric values
#
# $1                    - metric name
# number                - number of values
# $tmp/pmgadgets_result - values
#
_pmgadgets_fetch_values()
{
    _pmgadgets_fetch -v $1
    return $?
}

# Fetch the metric instance list
#
# $1                    - metric name
# number                - number of instances
# $tmp/pmgadgets_result - instances
#
_pmgadgets_fetch_indom()
{
    _pmgadgets_fetch -I $1
    return $?
}

# Convert pmprobe/pminfo error message into something useful and
# consistent
#
_pmgadgets_fetch_mesg()
{
    $PCP_AWK_PROG '
$1 == "pmprobe:"	{ $1 = "'$prog':"; print; exit }
$1 == "pminfo:"		{ $1 = "'$prog':"; print; exit }
$1 == "Error:"		{ $1 = ":"; 
			  printf("%s: %s%s\n", "'$prog'", metric, $0); exit }
$1 == "inst"		{ exit }
NF == 1			{ metric = $1; next }
NF == 0			{ next }
NF == 2 && $2 == "0"	{ printf("%s: %s: No values available\n", "'$prog'", $1); exit}
			{ $2 = ":"; print "'$prog': " $0; exit}' \
    | sed "s/ : /: /" \
    | fmt
}

# Generate error metric for failed fetch
#
_pmgadgets_fetch_fail()
{
    cat $tmp/pmgadgets_fetch | _pmgadgets_fetch_mesg >> $tmp/msg
    echo "$prog: Failed to $1 from host \"$host\"" | fmt >> $tmp/msg
    _pmgadgets_error -f $tmp/msg
    # NOTREACHED
}

# Generate warning message for failed fetch
#
_pmgadgets_fetch_warn()
{
    cat $tmp/pmgadgets_fetch | _pmgadgets_fetch_mesg >> $tmp/msg
    echo "$prog: Failed to $1 from host \"$host\"" | fmt >> $tmp/msg
    _pmgadgets_warning -f $tmp/msg
}

# Check that $OPTARG for option $1 is a positive integer
# ...note the creative use of unary - to prevent leading signs
#
_pmgadgets_unsigned()
{
    if [ "X-$OPTARG" != "X`expr 0 + -$OPTARG 2>/dev/null`" ]
    then
	_pmgadgets_error "$prog: -$1 option must have a positive integral argument"
	# NOTREACHED
    fi
}
