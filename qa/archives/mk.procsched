#!/bin/sh
# 
# Recipe for creating the procsched archive
#

. $PCP_DIR/etc/pcp.env

tmp=/tmp/$$
rm -rf $tmp

if pmprobe proc.psinfo.nice 2>&1 | grep -q 'Unknown metric name'
then
    echo "Arrg, proc PMDA is apparently not installed"
    exit 1
fi

trap "rm -rf $tmp; exit" 0 1 2 3 15

pid=$$
cat <<End-of-File >$tmp.config
log mandatory on 1sec {
    proc.psinfo.nice [ 1 $pid ]
    proc.psinfo.priority[ 1 $pid ]
}
End-of-File
${PCP_BINADM_DIR}/pmlogger -s 5 -c $tmp.config procsched

exit
