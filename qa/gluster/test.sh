#!/bin/sh

command="$1"
shift
case "$command"
in
    info|stats)
	testcase="$1"
	cat $GLUSTER_HOME/$testcase
	;;

    start|stop)
	volume="$1"
	sequence="$2"
	filename="$3"
	echo "$command $volume - test $sequence" > "$filename"
	;;

    *)	# just ignore anything else
	;;
esac
