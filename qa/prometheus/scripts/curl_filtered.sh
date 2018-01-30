#! /bin/sh
#
# Simple example of a filter to delete a metric from some URL
# Note: the prometheus exposition format used here will have the
# prometheus metric name present on every line. So sed /metricname/d
# does the trick.

. /etc/pcp.conf

( curl -Gq file://$PCP_PMDAS_DIR/prometheus/config.d/some_metric.txt ; \
curl -Gq file://$PCP_PMDAS_DIR/prometheus/config.d/some_other_metric.txt) \
| sed -e '/metric2/d'
