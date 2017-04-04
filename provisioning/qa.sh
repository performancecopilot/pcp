#!/bin/sh

touch /tmp/runqa.sh
echo "#!/bin/sh" >> /tmp/runqa.sh
echo "cd /var/lib/pcp/testsuite" >> /tmp/runqa.sh
echo "./check #{QA_FLAGS} >/tmp/runqa.out 2>&1" >> /tmp/runqa.sh
echo "cp /tmp/runqa.out /qaresults" >> /tmp/runqa.sh
echo "cp /var/lib/pcp/testsuite/*.bad /qaresults" >> /tmp/runqa.sh

chmod 777 /tmp/runqa.sh
sudo -b -H -u pcpqa sh -c '/tmp/runqa.sh'
