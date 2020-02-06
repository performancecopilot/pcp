#!/bin/sh -eu

cd "$(dirname "$0")/.."
. scripts/env.sh
. scripts/env.vmss.sh

echo Copying sources to build server
rsync -a -e "$SSH" ../../ pcp@${AZ_VMSS_BUILDER_IP}:pcp/

echo Start build
$SSH pcp@${AZ_VMSS_BUILDER_IP} /usr/local/ci/build.sh
