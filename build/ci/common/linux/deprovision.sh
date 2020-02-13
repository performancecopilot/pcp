#!/bin/sh -eux

waagent -force -deprovision+user
export HISTSIZE=0
sync
