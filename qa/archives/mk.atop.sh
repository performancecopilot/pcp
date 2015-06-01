#!/bin/sh
# 
# Recipe for creating the pcp-atop archive
#

. $PCP_DIR/etc/pcp.env

here=`pwd`
tmp=/tmp/$$
rm -rf $tmp

PMLOGCONF=$PCP_BINADM_DIR/pmlogconf
PMLOGGER=$PCP_BINADM_DIR/pmlogger
PMSLEEP=$PCP_BINADM_DIR/pmsleep
MKAF=$PCP_BINADM_DIR/mkaf

if which curl >/dev/null 2>&1
then
    :
else
    echo "Arrgh, curl binary is apparently not installed"
    exit 1
fi

if pmprobe apache 2>&1 | grep -q 'Unknown metric name'
then
    echo "Arrgh, apache PMDA is apparently not installed"
    exit 1
fi

trap "rm -rf $tmp; exit" 0 1 2 3 15

mkdir -p $tmp/config
cp $PCP_VAR_DIR/config/pmlogconf/tools/atop* $tmp/config
# create an empty pmlogconf configuration
echo "#pmlogconf 2.0" > $tmp.config 
echo "#+ groupdir $tmp/config" >> $tmp.config
# interactive - set 1 second interval, and log everything!
$PMLOGCONF -d $tmp/config $tmp.config

rm -f pcp-atop.*
$PMLOGGER -t 1 -s 5 -c $tmp.config -l $tmp.log pcp-atop &

#
# Do some work to make kernel and apache stats move ...
#

# apache, and misc net traffic
curl http://localhost:80/status >/dev/null 2>&1
$PMSLEEP 0.2
curl http://localhost:80/status >/dev/null 2>&1
$PMSLEEP 0.8
curl http://www.google.com/ >/dev/null 2>&1
$PMSLEEP 0.5

# some disk I/O and cpu time
find /usr/bin >/dev/null 2>&1 &
$PMSLEEP 0.05
sum /usr/bin/bash >/dev/null &
$PMSLEEP 1.5
sum /usr/bin/ls >/dev/null &

wait
echo "pmlogger log:"
cat $tmp.log

$MKAF pcp-atop.* > pcp-atop.folio
