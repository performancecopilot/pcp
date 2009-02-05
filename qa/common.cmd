#
# common procedures for pcp-gui QA scripts
# ... stolen from PCP QA
#
# Copyright (c) 1997-2002 Silicon Graphics, Inc.  All Rights Reserved.
# Copyright (c) 2006 Ken McDonell.  All Rights Reserved.
#
# $Id: common.cmd,v 1.1 2006/01/20 18:38:28 kenj Exp $
#

. ./common.sh 

[ -z "$DEFAULT_HOST" ] && DEFAULT_HOST=`hostname`

here=`pwd`
rm -f $here/$iam.out

diff=diff
if [ ! -z "$DISPLAY" ]
then
    which tkdiff >/dev/null 2>&1 && diff=tkdiff
    which xdiff >/dev/null 2>&1 && diff=xdiff
    which xxdiff >/dev/null 2>&1 && diff=xxdiff
    which gdiff >/dev/null 2>&1 && diff=gdiff
fi

verbose=false
group=false
xgroup=false
snarf=''
showme=false
sortme=false
have_test_arg=false
rm -f $tmp.list $tmp.tmp $tmp.sed

for r
do
    if $group
    then
	# arg after -g
	group_list=`sed -n <group -e 's/$/ /' -e "/^[0-9][0-9][0-9].* $r /"'{
s/ .*//p
}'`
	if [ -z "$group_list" ]
	then
	    echo "Group \"$r\" is empty or not defined; ignored"
	else
	    [ ! -s $tmp.list ] && touch $tmp.list
	    for t in $group_list
	    do
		if grep -s "^$t\$" $tmp.list >/dev/null
		then
		    :
		else
		    echo "$t" >>$tmp.list
		fi
	    done
    	fi
	group=false
	continue

    elif $xgroup
    then
	# if no test numbers, do everything from group file
	[ ! -s $tmp.list ] && sed -n '/^[0-9][0-9]* /s/ .*//p' group >$tmp.list

	# arg after -x
	group_list=`sed -n <group -e 's/$/ /' -e "/^[0-9].* $r /s/ .*//p"`
	if [ -z "$group_list" ]
	then
	    echo "Group \"$r\" is empty or not defined; ignored"
	else
	    numsed=0
	    rm -f $tmp.sed
	    for t in $group_list
	    do
		if [ $numsed -gt 100 ]
		then
		    sed -f $tmp.sed <$tmp.list >$tmp.tmp
		    mv $tmp.tmp $tmp.list
		    numsed=0
		    rm -f $tmp.sed
		fi
		echo "/^$t\$/d" >>$tmp.sed
		numsed=`expr $numsed + 1`
	    done
	    sed -f $tmp.sed <$tmp.list >$tmp.tmp
	    mv $tmp.tmp $tmp.list
    	fi
	xgroup=false
	continue

    elif [ ! -z "$snarf" ]
    then
	case $snarf
	in
	    d)
		QA_DIR=$r
		;;
	    h)
		QA_HOST=$r
		;;
	    u)
		QA_USER=$r
		;;
	esac
	snarf=''
	continue
    fi

    xpand=true
    range=false
    case "$r"
    in

	-\?)	# usage
	    echo "Usage: $0 [options] [testlist]"'

common options
    -v			verbose

check options
    -g group	include tests from these groups (multiple flags allowed)
    -l		line mode diff [xdiff]
    -n		show me, do not run tests
    -x group	exclude tests from these groups (multiple flags allowed)

show-me options
    -d QA_DIR		[isms/pcp2.0/qa]
    -h QA_HOST		['`hostname`']
    -u QA_USER		[pcpqa]
'
	    exit 0
	    ;;

	-d)	# directory for show-me
	    snarf=d
	    xpand=false
	    ;;

	-g)	# -g group ... pick from group file
	    group=true
	    xpand=false
	    ;;

	-h)	# host for show-me
	    snarf=h
	    xpand=false
	    ;;

	-l)	# line mode for diff, not gdiff over modems
	    diff=diff
	    xpand=false
	    ;;

	-n)	# show me, don't do it
	    showme=true
	    xpand=false
	    ;;

	-u)	# user for show-me
	    snarf=u
	    xpand=false
	    ;;

	-v)
	    verbose=true
	    xpand=false
	    ;;

	-x)	# -x group ... exclude from group file
	    xgroup=true
	    xpand=false
	    ;;

	'[0-9][0-9][0-9] [0-9][0-9][0-9][0-9]')
	    echo "No tests?"
	    status=1
	    exit $status
	    ;;

	[0-9]*-[0-9]*)
	    eval `echo $r | sed -e 's/^/start=/' -e 's/-/ end=/'`
	    range=true
	    ;;

	[0-9]*-)
	    eval `echo $r | sed -e 's/^/start=/' -e 's/-//'`
	    end=`echo [0-9][0-9][0-9] [0-9][0-9][0-9][0-9] | sed -e 's/\[0-9]//g' -e 's/  *$//' -e 's/.* //'`
	    if [ -z "$end" ]
	    then
		echo "No tests in range \"$r\"?"
		status=1
		exit $status
	    fi
	    range=true
	    ;;

	*)
	    start=$r
	    end=$r
	    ;;

    esac

    if $xpand
    then
	start=`echo $start | sed -e 's/^0*\(.\)/\1/'`
	end=`echo $end | sed -e 's/^0*\(.\)/\1/'`
	have_test_arg=true
	$PCP_AWK_PROG </dev/null '
BEGIN	{ for (t='$start'; t<='$end'; t++) printf "%03d\n",t }' \
	| while read id
	do
	    # if test not present, silently forget about it
	    [ -f $id ] || continue
	    if grep -s "^$id " group >/dev/null
	    then
		# in group file ... OK
		echo $id >>$tmp.list
	    else
		# oops
		$range || echo "$id - unknown test, ignored"
	    fi
	done
    fi

done

if [ -s $tmp.list ]
then
    # found some valid test numbers ... this is good
    :
else
    if $have_test_arg
    then
	# had test numbers, but none in group file ... do nothing
	touch $tmp.list
    else
	# no test numbers, do everything from group file
	touch $tmp.list
	sed -n '/^[0-9][0-9]* /s/ .*//p' <group \
	| while read id
	do
	    [ -f $id ] || continue
	    echo $id >>$tmp.list
	done
    fi
fi

list=`sort -n $tmp.list`
rm -f $tmp.list $tmp.tmp $tmp.sed

[ -z "$QA_HOST" ] && QA_HOST=$DEFAULT_HOST
export QA_HOST QA_DIR QA_USER
