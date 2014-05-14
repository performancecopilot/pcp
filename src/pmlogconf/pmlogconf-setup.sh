#!/bin/sh
#
# pmlogconf-setup - parse and process a group file to produce an
# initial configuration file control line
#
# Copyright (c) 2014 Red Hat.
# Copyright (c) 2010 Ken McDonell.  All Rights Reserved.
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

# Get standard environment
. $PCP_DIR/etc/pcp.env

status=1
tmp=`mktemp -d /tmp/pcp.XXXXXXXXX` || exit 1
trap "rm -rf $tmp; exit \$status" 0 1 2 3 15
#debug# tmp=`pwd`/tmp-setup
prog=`basename $0`

cat > $tmp/usage << EOF
# Usage: [options] file

Options:
  --host
  -v,--verbose   increase diagnostic verbosity
  --help
EOF

_usage()
{
    pmgetopt --progname=$prog --config=$tmp/usage --usage
    exit 1
}

HOST=local:
verbose=false

ARGS=`pmgetopt --progname=$prog --config=$tmp/usage -- "$@"`
[ $? != 0 ] && exit 1

eval set -- "$ARGS"
while [ $# -gt 0 ]
do
    case "$1"
    in
	-h)	# host to contact for "probe" tests
		HOST="$2"
		shift
		;;

	-v)	# verbose
		verbose=true
		;;

	--)	shift
		break
		;;

	-\?)	# eh?
		_usage
		# NOTREACHED
		;;
    esac
    shift
done

[ $# -eq 1 ] || _usage

# find "probe metric [condition] [state_rule]" line to determine action
# or
# find "force state" line
#
metric=''
options=''
force=''
eval `sed -n <"$1" \
    -e '/^#/d' \
    -e 's/?/"\\\\?"/g' \
    -e 's/\*/"\\\\*"/g' \
    -e 's/\[/"\\\\["/g' \
    -e '/^probe[ 	]/{
s/^probe[ 	]*/metric="/
s/$/ /
s/[ 	]/" options="/
s/ *$/"/
p
q
}' \
    -e '/^force[ 	]/{
s/^force[ 	]*/force='"'"'/
s/ *$/'"'"'/
p
}'`
$verbose && $PCP_ECHO_PROG "$1: " >&2
if [ -n "$force" -a -n "$metric" ]
then
    $PCP_ECHO_PROG "$1: Warning: \"probe\" and \"force\" control lines ... ignoring \"force\"" >&2
    force=''
fi
if [ -z "$force" -a -z "$metric" ]
then
    $PCP_ECHO_PROG "$1: Warning: neither \"probe\" nor \"force\" control lines ... use \"force available\"" >&2
    force=available
fi

$verbose && [ -n "$metric" ] && $PCP_ECHO_PROG $PCP_ECHO_N "probe $metric $options""$PCP_ECHO_C" >&2
$verbose && [ -n "$force" ] && $PCP_ECHO_PROG $PCP_ECHO_N "force $force""$PCP_ECHO_C" >&2
rm -f $tmp/err
if [ -n "$metric" ]
then
    # probe
    echo "$options"
    echo "data `pmprobe -h $HOST -v $metric`"
    # need to handle these variants of pmprobe output
    # sample.string.hullo 1 "hullo world!"
    # sample.colour 3 101 202 303
    #
else
    # force
    $PCP_ECHO_PROG "force $force"
fi \
| sed \
    -e '/^data /!{
s/ //g
}' \
    -e '/^data [^"]*$/{
s/^data //
s/ //g
}' \
    -e '/^data .*"/{
s/^data //
s/ "//
s/" "//g
s/"$//
s/ //
}' \
| $PCP_AWK_PROG -F '
BEGIN		{ # conditions
	      i = 0
	      exists = ++i; condition[exists] = "exists"
	      values = ++i; condition[values] = "values"
	      force = ++i; conditon[force] = "-"
	      regexp = ++i; condition[regexp] = "~" 
	      notregexp = ++i; condition[notregexp] = "!~" 
	      gt = ++i; condition[gt] = ">";
	      ge = ++i; condition[ge] = ">=";
	      eq = ++i; condition[eq] = "==";
	      neq = ++i; condition[neq] = "!=";
	      le = ++i; condition[le] = "<=";
	      lt = ++i; condition[lt] = "<";
	      # states
	      include = 100; state[include] = "include"
	      exclude = 101; state[exclude] = "exclude"
	      available = 102; state[available] = "available"
	    }
NR == 1 && $1 == "force" && NF == 2 {
	      # force variant
	      action = -1
	      for (i in state) {
		if ($2 == state[i]) {
		    action = i
		    break
		}
	      }
	      if (action == -1) {
		print "force state \"" $2 "\" not recognized" >"'$tmp/err'"
		exit
	      }
	      printf "probe=1 action=%d\n",action >>"'$tmp/out'"
	      exit
	    }
NR == 1		{
	      op = exists	# default predicate
	      yes = available	# default success action
	      no = exclude	# default failure action
	      if (NF > 0) {
		have_condition = 0
		for (i in condition) {
		    if ($1 == condition[i]) {
			have_condition = 1
			op = i
			yes = available	# default success action
			no = exclude	# default failure action
			break
		    }
		}
		if (have_condition == 0 && $1 != "?") {
		    print "condition operator \"" $1 "\" not recognized" >"'$tmp/err'"
		    exit
		}
		if (op == exists || op == values) {
		    if (have_condition)
			actarg = 2
		    else
			actarg = 1
		}
		else {
		    if (NF < 2) {
			print "missing condition operand after " condition[op] " operator" >"'$tmp/err'"
			exit
		    }
		    oprnd = $2
		    actarg = 3
		}
		if (NF < actarg) next
		str = $actarg
		for (i = actarg+1; i <= NF; i++) {
		    str = str " " $i
		}
		if (NF >= actarg && $actarg != "?") {
		    print "expected \"?\" after condition, found \"" str "\"" >"'$tmp/err'"
		    exit
		}
		if (NF >= actarg && NF < actarg+3) {
		    print "missing state rule components: \"" str "\"" >"'$tmp/err'"
		    exit
		}
		if (NF >= actarg && NF > actarg+3) {
		    print "extra state rule components: \"" str "\"" >"'$tmp/err'"
		    exit
		}
		actarg++
		yes = -1
		for (i in state) {
		    if ($actarg == state[i]) {
			yes = i
			break
		    }
		}
		if (yes == -1) {
		    print "sucess state \"" $actarg "\" not recognized" >"'$tmp/err'"
		    exit
		}
		actarg++
		if ($actarg != ":") {
		    print "expected \":\" in state rule, found \"" $actarg "\"" >"'$tmp/err'"
		    exit
		}
		actarg++
		no = -1
		for (i in state) {
		    if ($actarg == state[i]) {
			no = i
			break
		    }
		}
		if (no == -1) {
		    print "failure state \"" $actarg "\" not recognized" >"'$tmp/err'"
		    exit
		}
	      } 
	    }
NR == 2		{
	      #debug# printf "op: %d %s pmprobe: %s",op, condition[op],$0
	      probe = 0
	      if ($2 < 0) {
		# error from pmprobe
		;
	      }
	      else {
		if (op == exists) {
		    probe = 1
		}
		else if (op == values) {
		    if ($2 > 0) probe = 1
		}
		else if (op == regexp) {
		    for (i = 3; i <= NF; i++) {
			if ($i ~ oprnd) {
			    probe = 1
			    break
			}
		    }
		}
		else if (op == notregexp) {
		    for (i = 3; i <= NF; i++) {
			if ($i !~ oprnd) {
			    probe = 1
			    break
			}
		    }
		}
		else if (op == gt) {
		    for (i = 3; i <= NF; i++) {
			if ($i > oprnd) {
			    probe = 1
			    break
			}
		    }
		}
		else if (op == ge) {
		    for (i = 3; i <= NF; i++) {
			if ($i >= oprnd) {
			    probe = 1
			    break
			}
		    }
		}
		else if (op == eq) {
		    for (i = 3; i <= NF; i++) {
			if ($i == oprnd) {
			    probe = 1
			    break
			}
		    }
		}
		else if (op == neq) {
		    for (i = 3; i <= NF; i++) {
			if ($i != oprnd) {
			    probe = 1
			    break
			}
		    }
		}
		else if (op == le) {
		    for (i = 3; i <= NF; i++) {
			if ($i <= oprnd) {
			    probe = 1
			    break
			}
		    }
		}
		else if (op == lt) {
		    for (i = 3; i <= NF; i++) {
			if ($i < oprnd) {
			    probe = 1
			    break
			}
		    }
		}
	      }
	      if (probe == 1) 
		action = yes
	      else
		action = no
	      printf "probe=%d action=%d\n",probe,action >>"'$tmp/out'"
	    }'

if [ -f $tmp/err ]
then
    $verbose && $PCP_ECHO_PROG >&2
    $PCP_ECHO_PROG "$1: Error: `cat $tmp/err`" >&2
elif [ -f $tmp/out ]
then
    probe=''
    action=''
    eval `cat $tmp/out`
    if $verbose
    then
	case $probe
	in
	    0)
		probe_s="failure"
		;;
	    1)
		probe_s="success"
		;;
	    *)
		probe_s="unknown ($probe)"
		;;
	esac
	case $action
	in
	    100)
		action_s="include"
		;;
	    101)
		action_s="exclude"
		;;
	    102)
		action_s="available"
		;;
	    *)
		action_s="unknown ($action)"
		;;
	esac
	$PCP_ECHO_PROG " -> probe=$probe_s action=$action_s" >&2
    fi
    if [ $action = 100 -o $action = 102 ]
    then
	mode='n'
	[ $action = 100 ] && mode='y'
	delta=`sed -n <"$1" -e /'^delta[ 	]/s/delta[ 	]*//p'`
	[ -z "$delta" ] && delta='default'
	echo "#+ $1:$mode:$delta:"
    else
	echo "#+ $1:x::"
    fi
    status=0
else
    $verbose && $PCP_ECHO_PROG >&2
    $PCP_ECHO_PROG "$1: Botch: no errors and no probe results ... try verbose mode" >&2
fi

exit
