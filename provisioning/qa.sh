#!/bin/sh

sudo su - pcpqa -c "./check ${1} >runqa.out 2>&1"
sudo cp "/var/lib/pcp/testsuite/runqa.out" /qaresults/
sudo cp "/var/lib/pcp/testsuite/"*.bad /qaresults/
