#!/bin/sh -u

cd /var/lib/pcp/testsuite
sudo -u pcpqa ./check $1 2>&1
status=$?

if [ $status -ne 0 ]; then
    tail -n+1 $1.out.bad $1.full 1>&2
fi

exit $status
