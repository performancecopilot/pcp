#!/bin/sh
# 
# Recipe for creating the gap and gap2 archives.
#
#	gap: 10 sec data, 5 sec gap, 10 sec data
#	gap2: 10 sec data, 5 sec gap, 10 sec data, 5 sec gap, 10 sec data
#	sample.bin instances change across gaps
#

. $PCP_DIR/etc/pcp.env

here=`pwd`
tmp=/tmp/$$
rm -rf $tmp
mkdir -p $tmp/tmp
cd $tmp

trap "cd $here; rm -fr $tmp; exit" 0 1 2 3 15

echo 'log mandatory on 1sec { pmcd.pdu_in,pmcd.numagents,hinv.ncpu,sample.bin["bin-100","bin-200","bin-400"] }' >tmp/A.config
echo 'log mandatory on 1sec { pmcd.pdu_in,pmcd.numagents,hinv.ncpu,sample.bin["bin-100","bin-300","bin-400"] }' >tmp/B.config
echo 'log mandatory on 1sec { pmcd.pdu_in,pmcd.numagents,hinv.ncpu,sample.bin["bin-100","bin-400"] }' >tmp/C.config

${PCP_BINADM_DIR}/pmlogger -s 10 -c tmp/A.config tmp/A
sleep 5
${PCP_BINADM_DIR}/pmlogger -s 10 -c tmp/B.config tmp/B
sleep 5
${PCP_BINADM_DIR}/pmlogger -s 10 -c tmp/C.config tmp/C

rm -f gap.index gap.meta gap.0
${PCP_BINADM_DIR}/pmlogextract tmp/A tmp/B $here/gap

rm -f gap2.index gap2.meta gap2.0
${PCP_BINADM_DIR}/pmlogextract tmp/A tmp/B tmp/C $here/gap2

exit
