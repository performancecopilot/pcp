#!/bin/sh
# 
# Recipe for creating the gap and gap2 archives.
#
#	gap: 10 sec data, 5 sec gap, 10 sec data
#	gap2: 10 sec data, 5 sec gap, 10 sec data, 5 sec gap, 10 sec data
#	- sample.longlong.million (semantics instant) is logged only once
#	  in the first block of data
#	- hinv.ncpu (semantics discrete) is logged only once in each block
#	  of data
#	- otherwise the logging interval is 1sec
#	- sample.bin (semantics instant) instances change across gaps
#	- pmcd.pdu_in.* (semantics counter)
#	- proc.nprocs (semantics instant) and variable
#	- pmcd.numagents (semantics instant) and constant
#

. $PCP_DIR/etc/pcp.env

here=`pwd`
tmp=/tmp/$$
rm -rf $tmp
mkdir -p $tmp/tmp
cd $tmp

trap "cd $here; rm -fr $tmp; exit" 0 1 2 3 15

cat <<End-of-File >tmp/A.config
log mandatory on once { hinv.ncpu, sample.longlong.million }
log mandatory on 1sec { pmcd.pdu_in, proc.nprocs, pmcd.numagents }
End-of-File
cp tmp/A.config tmp/B.config
cp tmp/A.config tmp/C.config
echo 'log mandatory on 1sec { sample.bin["bin-100","bin-200","bin-400"] }' >>tmp/A.config
echo 'log mandatory on 1sec { sample.bin["bin-100","bin-300","bin-400"] }' >>tmp/B.config
echo 'log mandatory on 1sec { sample.bin["bin-100","bin-400"] }' >>tmp/C.config

${PCP_BINADM_DIR}/pmlogger -s 10 -c tmp/A.config tmp/A
sleep 5
${PCP_BINADM_DIR}/pmlogger -s 10 -c tmp/B.config tmp/B
sleep 5
${PCP_BINADM_DIR}/pmlogger -s 10 -c tmp/C.config tmp/C

rm -f $here/gap.index $here/gap.meta $here/gap.0
${PCP_BINADM_DIR}/pmlogextract tmp/A tmp/B $here/gap

rm -f $here/gap2.index $here/gap2.meta $here/gap2.0
${PCP_BINADM_DIR}/pmlogextract tmp/A tmp/B tmp/C $here/gap2

exit
