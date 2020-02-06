#!/bin/sh -u

test_no="$1"

# NOTE: sudo -i is required to set the $HOME env variable to /var/lib/pcp/testsuite, which is required for some QA tests
cd /var/lib/pcp/testsuite
sudo -i -u pcpqa ./check "${test_no}" 2>&1
status=$?

if [ $status -ne 0 ]; then
    tail -n+1 "${test_no}.out" "${test_no}.out.bad" "${test_no}.full" 1>&2
fi

exit $status
