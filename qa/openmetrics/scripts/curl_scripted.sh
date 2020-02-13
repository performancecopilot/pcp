#! /bin/sh

. /etc/pcp.conf
curl -Gq file://$PCP_PMDAS_DIR/openmetrics/config.d/some_metric.txt
curl -Gq file://$PCP_PMDAS_DIR/openmetrics/config.d/some_other_metric.txt
