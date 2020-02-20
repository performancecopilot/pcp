#!/bin/sh -eux

# file /usr/bin/dstat from package pcp-system-tools-5.0.3-1.x86_64 collides with file from package dstat-0.7.0-3.el6_9.1.noarch
yum remove -y dstat
