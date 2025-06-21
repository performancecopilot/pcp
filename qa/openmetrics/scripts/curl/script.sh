#! /bin/sh

. /etc/pcp.conf
curl --disable --get --no-progress-meter file://$PCP_PMDAS_DIR/openmetrics/config.d/some_metric.txt
