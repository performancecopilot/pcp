#!/bin/sh
. $PCP_DIR/etc/pcp.env
exec $PCP_PMDAS_DIR/cisco/pmdacisco -n parse -C $@
