#!/bin/bash -eu
set -o pipefail

logfile="$(mktemp)"

# NOTE: sudo -i is required to set the $HOME env variable to /var/lib/pcp/testsuite, which is required for some QA tests
cd /var/lib/pcp/testsuite
status=0
sudo -i -u pcpqa ./check "$@" 2>&1 | tee "${logfile}" || status=$?

/usr/local/ci/create_report.py "${logfile}" 1>&2
exit $status
