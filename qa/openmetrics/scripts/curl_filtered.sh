#! /bin/sh
#
# Simple example of a filter to delete a metric from some URL
# Note: the OpenMetrics exposition format used here will have
# the metric name present on every line. So sed /metricname/d
# does the trick.

. /etc/pcp.conf

( curl -Gqs file://$PCP_PMDAS_DIR/openmetrics/config.d/some_metric.txt ; \
curl -Gqs file://$PCP_PMDAS_DIR/openmetrics/config.d/some_other_metric.txt) \
| sed -e '/metric2/d'
