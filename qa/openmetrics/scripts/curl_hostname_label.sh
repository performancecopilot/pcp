#! /bin/sh
#
# Simple example of a curl fetch to a remote URL, to replace the
# hostname label to that of a remote host. The PCP label hierarchy
# rules mean the new hostname label will override localhost for
# the metric (or the host running pmcd/PMDA as the case may be).
#
# A simple sed script inserts the hostname="remotehost" label
# in front of the label set for every instance of the metric.

. /etc/pcp.conf

# here for QA purposes we're fetching from a local file
# and just pretending it came from a remote host.
curl -Gqs file://$PCP_PMDAS_DIR/openmetrics/config.d/some_metric.txt 2>/dev/null | \
sed -e 's/[a-z0-9]*=/hostname="remotehost",&/'
