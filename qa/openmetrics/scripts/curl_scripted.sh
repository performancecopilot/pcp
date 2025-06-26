#! /bin/sh

. /etc/pcp.conf
curl -Gqs file://$PCP_PMDAS_DIR/openmetrics/config.d/some_metric.txt
curl -Gqs file://$PCP_PMDAS_DIR/openmetrics/config.d/some_other_metric.txt
