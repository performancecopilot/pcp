#!/bin/sh
#
# Copyright (c) 2008 Aconex.  All Rights Reserved.
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
# You should have received a copy of the GNU General Public License along
# with this program; if not, write to the Free Software Foundation, Inc.,
# 51 Franklin Street, Fifth Floor, Boston, MA 02110, USA
# 

. $PCP_DIR/etc/pcp.env

tmp=`mktemp -d /tmp/pcp.XXXXXXXXX` || exit 1
trap "rm -rf $tmp; exit 0" 0 1 2 3 15
prog=`basename $0`

LOCALHOST=`pmhostname`
CONFIGDIR=$PCP_VAR_DIR/config/pmsnap
CONTROL=$CONFIGDIR/control
[ -z "$PCP_PMSNAPCONTROL_PATH" ] || CONTROL="$PCP_PMSNAPCONTROL_PATH"

_usage()
{
    cat << EOF
Usage: $prog [-dNV] [-C dir] [-c regex] [-f format] [-n regex] [-o dir]
    -C    alternate directory for control file(s)
    -c    matches the Config field in the pmsnap control file
    -f    output image format (default png)
    -N    show me, but do not execute commands
    -n    matches the Name field in the pmsnap control file
    -o    default directory for output images (default ".")
    -V    verbose trace of actions (for debugging)
    --    pass all following options directly to pmchart

If no patterns are given then all lines in the pmsnap control file
	$CONTROL
will be processed.  If any patterns are given then only lines which
match all patterns will be processed.

Patterns are full regular expressions, see egrep(1) and may require
appropriate quotes to avoid shell meta character expansion.
EOF
    exit 1
}

_fixpath() 
{
    echo $1 $2 |\
    $PCP_AWK_PROG '{
	if (substr($2, 1, 1) == "/")
	    print $2
	else
	    printf "%s/%s\n", $1, $2
    }'
}

_warning()
{
    echo
    echo "$prog: $CONTROL[$line]:"
    echo "$*" | fmt \
| sed -e '1{
s/^/Warning: /
n
}' -e 's/^/         /'
}

_error()
{
    echo
    echo "$prog: $CONTROL[$line]:"
    echo "$*" | fmt \
| sed -e '1{
s/^/Error: /
n
}' -e 's/^/       /'
}

_debug()
{
    echo
    echo "$prog: $CONTROL[$line]:"
    echo "$*" | fmt \
| sed -e '1{
s/^/Debug: /
n
}' -e 's/^/       /'
}

serverId()
{
    id=42
    while [ -f /tmp/.X$id-lock ]; do
	id=`expr $id + 1`
    done
    echo $id
}

startXvfb()
{
    [ -x /usr/bin/Xvfb ] || return

    geom="2048x2048"
    args="-nolisten tcp -screen 0 ${geom}x8"
    server=`serverId`
    cookie=`mcookie`
    XAUTHORITY=$tmp/xauth xauth add ":$server" . "$cookie" >/dev/null 2>&1
    XAUTHORITY=$tmp/xauth /usr/bin/Xvfb ":$server" $args >/dev/null 2>&1 &
    pid=$!
    pmsleep 0.2
    kill -0 $pid 2>/dev/null || return
    export XAUTHORITY=$tmp/xauth
    export DISPLAY=":$server"
    echo $pid
}

stopXvfb()
{
    pid="$1"
    [ -z "$pid" -o "$pid" -le 0 ] && return
    kill $pid
}

line=0
archives=""
configs='.*'
names='.*'
dir="."
format="png"
verbose=false
passthru=false
version="1.0"

# option parsing
#
SHOWME=false
PMLC="echo flush | pmlc >/dev/null 2>&1"
while getopts a:C:c:f:Nn:o:V-? c
do
    case $c
    in
	C)	CONFIGDIR="$OPTARG"
		;;
	c)	configs="$OPTARG"
		;;
	f)	format="$OPTARG"
		;;
	o)	dir="$OPTARG"
		;;
	N)	SHOWME=true
		PMLC="echo '+ echo flush | pmlc'"
		;;
	n)	names="$OPTARG"
		;;
	V)	verbose=true
		;;
	-)	passthru=true
		;;
	?)	_usage
		;;
    esac
    [ $passthru = true ] && break
done

if [ $passthru = false ]
then
    shift `expr $OPTIND - 1`
    [ $# -ne 0 ] && _usage
fi
commonargs="$commonargs $@"

CONTROL=$CONFIGDIR/control
if [ ! -f "$CONTROL" ]
then
    echo "$prog: Error: cannot find control file \"$CONTROL\""
    exit 1
fi

if [ ! -d "$dir" ]
then
    echo "$prog: Error: directory \"$dir\" does not exist"
    exit 1
fi
cd $dir
dir=`pwd`
$SHOWME && echo "+ cd $dir"

$SHOWME || xvfb=`startXvfb`

# escape / in patterns so $PCP_AWK_PROG is not confused
#
names=`echo "$names" | sed -e 's;/;\\\\/;g'`
configs=`echo "$configs" | sed -e 's;/;\\\\/;g'`

sed -e 's/LOCALHOSTNAME/'$LOCALHOST'/g' < $CONTROL \
| $PCP_AWK_PROG '
/^\#/ || /^\$/ || NF < 4	{
    print
    next
}
{
    if ($1 ~ /'$names'/ && $3 ~ /'$configs'/) {
	printf "%s %s %s '\''", $1, $2, $3
	for (i=4; i<= NF; i++)
	    printf "%s ", $i
	printf "'\''\n"
    }
}' \
| while read aname afolio aview args
do
    line=`expr $line + 1`
    case "$aname"
    in
        \#*|'') # comment or empty
                continue
                ;;

	\$*)	# in-line variable assignment
		$SHOWME && echo "# $aname $afolio $aview $args"
		cmd=`echo "$aname $afolio $aview $args" \
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

    if [ -z "$afolio" -o -z "$aview" -o -z "$args" ]
    then
	_error "insufficient fields in control file record"
	continue
    fi

    # strip first and last single quote added by awk prior to read
    #
    args=`echo $args | sed -e "s/^'//" -e "s/'$//"`

    name=`_fixpath $dir $aname`
    if [ -d $PCP_LOG_DIR/pmlogger ]; then
	folio=`_fixpath $PCP_LOG_DIR/pmlogger $afolio`
    fi
    view=`_fixpath $dir $aview`

    # make sure output directory exists
    #
    outdir=`dirname $name`
    if [ ! -d $outdir ]
    then
	if $SHOWME
	then
	    echo "+ mkdir -p $outdir"
	else
	    mkdir -p $outdir
	    if [ ! -d $outdir ]
	    then
		_error "cannot create directory \"$outdir\" for output file"
	    else
		$verbose && _debug "created image output directory \"$outdir\""
	    fi
	fi
    fi

    if $SHOWME
    then
	:
    else
	if [ ! -w $outdir ]
	then
	    _error "cannot write in directory \"$outdir\" to create output file"
	    exit 1
	fi
    fi

    if $verbose
    then
	echo
	_debug "name=\"$name\""
	_debug "folio=\"$folio\""
	_debug "view=\"$view\""
	_debug "args=\"$args\""
	_debug "commonargs=\"$commonargs\""
    fi

    if [ ! -f "$view" ]
    then
	if [ ! -f $CONFIGDIR/$aview ]
	then
	    if [ ! -f $PCP_VAR_DIR/config/pmchart/$aview ]
	    then
		if [ ! -f $PCP_VAR_DIR/config/kmchart/$aview ]
		then
		    _warning "could not find \"$view\", \"$CONFIGDIR/$aview\", \"$PCP_VAR_DIR/config/pmchart/$aview, or \"$PCP_VAR_DIR/config/kmchart/$aview\""
		    continue
		else
		    view=$PCP_VAR_DIR/config/kmchart/$aview
		fi
	    else
		view=$PCP_VAR_DIR/config/pmchart/$aview
	    fi
	else
	    view=$CONFIGDIR/$aview
	fi
    fi

    if [ ! -f $folio ]
    then
	# oops, no folio
	_error "cannot find archive folio \"$folio\""
	echo "Skipping entry from control file ..."
	continue
    fi

    f=$folio
    $SHOWME && echo "+ pmafm $f selection"
    pmafm "$f" selection >$tmp/out 2>&1

    if [ $? -ne 0 ]
    then
	if [ ! -f $folio.meta ]
	then
	    _warning "\"$folio\" is neither a PCP folio nor a PCP archive"
	    ls -l $folio
	    continue
	else
	    contents=$folio
	fi
    else
	contents=`sed -n -e '/Archive:/s///gp' <$tmp/out`
	if [ -z "$contents" ]
	then
	    _warning "cannot determine archive name from PCP folio \"$folio\""
	    continue
	fi
    fi

    arch=""

    for c in $contents
    do
	if [ -f $c.meta ]
	then
	    host=`pminfo -f -a $c pmcd.pmlogger.host \
	    | sed -e 's/"//g' -e 's/\[//g' \
	    | $PCP_AWK_PROG '$1=="inst" {print $6; exit}'`

	    port=`pminfo -f -a $c pmcd.pmlogger.port \
	    | sed -e 's/"//g' -e 's/\[//g' \
	    | $PCP_AWK_PROG '$1=="inst" {print $6; exit}'`

	    if [ ! -z "$host" -a ! -z "$port" ]
	    then
		echo "$host $port" >> $tmp/flush
	    else
		if $verbose
		then
		    _warning "could not extract host and pmlogger control port from archive \"$c\" ... this archive may not be flushed"
		fi
	    fi

	    if [ -z "$arch" ]
	    then
		arch=$c
	    else
		arch=$arch","$c
	    fi
	else
	    _warning "cannot find or open archive \"$c\""
	fi
    done

    if [ -z "$arch" ]
    then
	_warning "no archives could be found for \"$contents\""
	continue
    fi

    sort -u $tmp/flush | while read host port
    do
	eval $PMLC -h $host -p $port
    done

    if $SHOWME
    then
	echo "+ pmchart -c $view -o $name.$format -a $arch $commonargs $args"
    else
	eval pmchart -c "$view" -o "$tmp/name.$format" -a "$arch" $commonargs $args > $tmp/err 2>&1
	sts=$?

	if [ $sts -eq 0 -a -f $tmp/name.$format ]
	then
	    # success, update the image
	    mv $tmp/name.$format $name.$format
	    $verbose && _debug "created \"$name.$format\""
	    if [ -s "$tmp/err" ]
	    then
		_warning "the output image \"$name.$format\" was created but there were warning messages from pmchart, as follows:"
		cat $tmp/err
		echo "-------------"
	    fi
	elif grep 'too short to satisfy the requested starting alignment' $tmp/err >/dev/null
	then
	    # archive's duration is too short for chart configuration ...
	    # silently ignore this one, as we'll probably succeed next time
	    # or soon thereafter
	    if $verbose
	    then
		_debug "archive \"$arch\" too short for pmchart args \"$args\""
	    fi
	else
	    # failed, remove the image and report the error
	    _warning "the output image \"$name.$format\" was not created. There were error messages from pmchart, as follows:"
	    cat $tmp/err
	    echo "-------------"
	    rm -f $name.$format
	fi
    fi

    rm -f $tmp/err
done

$SHOWME || stopXvfb "$xvfb"

exit $status
