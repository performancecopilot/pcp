#!/bin/sh -u

# NOTE: sudo -i is required to set the $HOME env variable to /var/lib/pcp/testsuite, which is required for some QA tests

cd /var/lib/pcp/testsuite
sudo -i -u pcpqa ./check $1 2>&1
status=$?

if [ $status -ne 0 ]; then
    tail -n+1 $1.out.bad $1.full 1>&2
fi

exit $status
