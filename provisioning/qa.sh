#!/bin/sh
sudo su - pcpqa -c "./check ${1} >runqa.out 2>&1"
sudo cp "/var/lib/pcp/testsuite/runqa.out" /qaresults/
if [ `ls /var/lib/pcp/testsuite/*.bad >/dev/null 2>&1` ] ; then
    for file in /var/lib/pcp/testsuite/*.bad
    do
        sudo cp "$file" /qaresults/
    done
    exit 1
elif [ ! -f '/var/lib/pcp/testsuite/runqa.out' ] ; then
    echo "No Tests run"
    exit 1
else
    echo "All Tests Passed."
    exit 0
fi
