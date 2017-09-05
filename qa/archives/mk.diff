#!/bin/sh
# 
# Recipe for creating the diff1 and diff2 archives
#

. $PCP_DIR/etc/pcp.env

here=`pwd`
tmp=/tmp/$$
rm -rf $tmp

cat <<End-of-File >$tmp.config
log mandatory on 1sec {
    kernel.all.load
    sample.bin
    sample.seconds
    sample.milliseconds
    sample.long.write_me
    sample.ulong.write_me
    sample.longlong.write_me
    sample.ulonglong.write_me
    sample.float.write_me
    sample.double.write_me
    sampledso.long.write_me
    sampledso.ulong.write_me
    sampledso.longlong.write_me
    sampledso.ulonglong.write_me
    sampledso.float.write_me
    sampledso.double.write_me
}
End-of-File

# diff1
pmstore sample.long.write_me 0
pmstore sample.ulong.write_me 3
pmstore sampledso.ulong.write_me 3
pmstore sample.longlong.write_me 2
pmstore sample.ulonglong.write_me 1000
pmstore sample.float.write_me 50
pmstore sample.double.write_me 2000.0
pmstore sampledso.double.write_me 2010.0
pmstore sampledso.long.write_me 10
rm -f diff1.0 diff1.meta diff1.index
${PCP_BINADM_DIR}/pmlogger -s 5 -c $tmp.config diff1

# diff1
# [inf]
pmstore sample.long.write_me 10
# [=100]
pmstore sample.ulong.write_me 300
# [>100]
pmstore sampledso.ulong.write_me 301
# [60]
pmstore sample.longlong.write_me 120
# [1]
pmstore sample.ulonglong.write_me 1000
# [1/5]
pmstore sample.float.write_me 10
# [=1/1000]
pmstore sample.double.write_me 2.0
# [<1/1000]
pmstore sampledso.double.write_me 2.0
# [1/inf]
pmstore sampledso.long.write_me 0
rm -f diff2.0 diff2.meta diff2.index
${PCP_BINADM_DIR}/pmlogger -s 5 -c $tmp.config diff2

