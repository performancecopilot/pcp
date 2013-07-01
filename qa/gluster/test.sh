#!/bin/sh

command=$1
shift

case "$command"
in
    info)
       volume=$1
       cat $GLUSTER_HOME/info-$volume
       ;;

    profile)
       volume=$1
       shift
       subcommand=$1
       [ $subcommand == profile ] && cat $GLUSTER_HOME/profile-$1
       # ignore start/stop for now, but a more sophisticated test
       # might use 'em at some point.
       ;;

    *) ;;
esac
