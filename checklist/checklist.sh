#! /bin/sh

# starts the checklist services

PMWEBD_PORT=44444 # choose any random one
BROWSER="/usr/bin/firefox --new-instance"
# BROWSER="/usr/bin/google-chrome"
PCP_ENV=/etc/pcp.env
CHECKLISTDIR=`dirname $0`
CHECKLIST=$CHECKLISTDIR/checklist.json
WEBAPPSDIR=`cd $CHECKLISTDIR/..; pwd`

. $PCP_ENV

set -e

h=${1-local:}
# many pcp clients (undocumentedly) can take their -h VALUE from here
PCP_HOST=$h
export PCP_HOST
hostname=`pmprobe -v pmcd.hostname | cut -f2 -d'"'`
echo "Starting checklist for PCP_HOST=$PCP_HOST $hostname".
echo

tmpdir=`mktemp -d`
echo "For use by checklist pid $$" > $tmpdir/README
echo "Created temporary directory $tmpdir"

# baby systemd -- or maybe instead systemd-run --scope=user ...
pids=""
trap 'kill $pids >/dev/null 2>&1; rm -rf "$tmpdir"; exit 0' 0 1 2 3 5 9 15 

# start private logger (pmrep) from metrics in json file
jq '.nodes[] | .pcp_metrics_log // .pcp_metrics | select (. != null) | .[] ' < $CHECKLIST | while read metric
do
    metric=`eval echo $metric` # undo quoting
    if pminfo $metric >/dev/null
    then
        echo $metric
    fi
done > $tmpdir.metrics
metrics=`cat $tmpdir.metrics`
refresh="1" # XXX: parametrize
$PCP_BIN_DIR/pmrep -F ${tmpdir}/${hostname} -o archive -t $refresh $metrics &
pids="$pids $!"
$PCP_BINADM_DIR/pmwebd -i $refresh -G -X -R ${WEBAPPSDIR} -p ${PMWEBD_PORT} -A ${tmpdir} -P &
pids="$pids $!"

echo "Started service pids $pids"

cmd="$BROWSER http://localhost:${PMWEBD_PORT}/grafana/index.html#/dashboard/script/checklist.js"
echo "Starting web browser:"
echo "$cmd"
eval $cmd

wait
