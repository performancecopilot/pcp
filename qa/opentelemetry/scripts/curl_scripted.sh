#! /bin/sh

. /etc/pcp.conf
curl -Gqs file://$PCP_PMDAS_DIR/opentelemetry/config.d/some_script_metric.txt
