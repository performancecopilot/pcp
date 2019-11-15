#!/bin/sh -xe

cd /var/lib/pcp/testsuite
./check -g "$1"
