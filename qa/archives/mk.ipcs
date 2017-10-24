#!/bin/sh
# 
# Recipe for creating the pcp-ipcs archive, for exercising IPC
# metric reporting by pcp-ipcs(1) ...
#

. $PCP_DIR/etc/pcp.env

here=`pwd`
tmp=/tmp/$$

if [ "$PCP_PLATFORM" != "linux" ]
then
    echo "$0: Error: requires Linux kernel IPC metrics"
    exit 1
fi

rm -rf $tmp $here/pcp-ipcs.{0,meta,index}
trap "cd $here; rm -fr $tmp; exit" 0 1 2 3 15

cat <<End-of-File >> $tmp.ipcs.config
log mandatory on once { hinv.pagesize }
log mandatory on 1 sec { ipc }
End-of-File

${PCP_BINADM_DIR}/pmlogger -s 5 -c $tmp.ipcs.config $here/pcp-ipcs

exit
