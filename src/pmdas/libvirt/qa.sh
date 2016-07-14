#!/bin/sh
# PCP QA Test No. XXX
# Exercise the libvirt PMDA - install, remove and values.
#
# Copyright (c) 2016 Red Hat.
#
# Expectations:
#   1) libvirtd installed and running
#     - typically from libvirt package
#   2) libvirt Python API available
#     - typically from libvirt-python
#   3) optionally one or more VMs up
#   4) libvirt.hv.* metrics always expected
#   5) with VMs available at least:
#     - libvirt.dominfo.uuid
#     - libvirt.dominfo.name
#     - libvirt.dominfo.memory.{boot,current}
#     - libvirt.dominfo.vcpu.*
#     - libvirt.dominfo.type
#     - libvirt.dominfo.os.type
#     - libvirt.domstats.mem.*
#

seq=`basename $0`
echo "QA output created by $seq"

# get standard environment, filters and checks
. ./common.product
. ./common.filter
. ./common.check

export PCP_PMDAS_DIR=/var/lib/pcp/pmdas

[ -d $PCP_PMDAS_DIR/libvirt ] || _notrun "libvirt PMDA directory is not installed"

status=1	# failure is the default!
$sudo rm -rf $tmp.* $seq.full

pmdalibvirt_remove()
{
    echo
    echo "=== remove libvirt agent ==="
    $sudo ./Remove >$tmp.out 2>&1
    _filter_pmda_remove <$tmp.out
}

pmdalibvirt_install()
{
    # start from known starting points
    cd $PCP_PMDAS_DIR/libvirt
    $sudo ./Remove >/dev/null 2>&1
    $sudo $PCP_RC_DIR/pmcd stop

    cat <<EOF >$tmp.config
[pmda]
oldapi = False
user = root
uri = qemu:///system
EOF
    echo "pmdalibvirt config:" >> $here/$seq.full
    cat $tmp.config >> $here/$seq.full

    [ -f $PCP_PMDAS_DIR/libvirt/libvirt.conf ] && \
    $sudo cp $PCP_PMDAS_DIR/libvirt/libvirt.conf $tmp.backup
    $sudo cp $tmp.config $PCP_PMDAS_DIR/libvirt/libvirt.conf

    echo
    echo "=== libvirt agent installation ==="
    $sudo ./Install </dev/null >$tmp.out 2>&1
    # Check metrics have appeared ... X metrics and Y values
    _filter_pmda_install <$tmp.out \
    | sed \
        -e '/^Waiting for pmcd/s/\.\.\.[. ]*$/DOTS/' \
    | $PCP_AWK_PROG '
/Check libvirt metrics have appeared/  { if ($7 >= 67 && $7 <= 67) $7 = "X"
                                          if ($10 >= 14 && $10 <= 50) $10 = "Y"
                                       }
/ warnings, /                          { if ($9 >= 67 && $9 <= 67) $9 = "X"
                                          if ($12 >= 14 && $12 <= 50) $12 = "Y"
                                       }
                                       { print }'
}

pmdalibvirt_cleanup()
{
    if [ -f $tmp.backup ]; then
	$sudo cp $tmp.backup $PCP_PMDAS_DIR/libvirt/libvirt.conf
	$sudo rm $tmp.backup
    else
	$sudo rm -f $PCP_PMDAS_DIR/libvirt/libvirt.conf
    fi
    _cleanup_pmda libvirt
}

_prepare_pmda libvirt
trap "pmdalibvirt_cleanup; exit \$status" 0 1 2 3 15

# real QA test starts here
pmdalibvirt_install

echo
echo "=== extract metric values ==="
echo "from pmprobe:" >>$here/$seq.full
pmprobe -v libvirt \
| LC_COLLATE=POSIX sort

pmdalibvirt_remove
status=0
exit
