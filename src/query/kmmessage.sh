#!/bin/sh
source /etc/pcp.conf
export PATH="$PCP_BIN_DIR:"`dirname $0`
exec kmquery -noprint $@
