#!/bin/sh -xe

local_artifacts_dir=/tmp/artifacts

builder_ip=$(az vmss list-instance-public-ips \
  --resource-group "${RESOURCE_GROUP}" \
  --name "${VMSS}" \
  --query "[*].ipAddress | sort(@) | [0]" --output tsv)

ssh -o "StrictHostKeyChecking=no" pcp@${builder_ip} "PCP_COMMIT=${PCP_COMMIT} /usr/local/ci/run_script.sh _checkout.sh ${DISTRIBUTION}"
ssh -o "StrictHostKeyChecking=no" pcp@${builder_ip} "/usr/local/ci/run_script.sh _build.sh ${DISTRIBUTION}"
ssh -o "StrictHostKeyChecking=no" pcp@${builder_ip} "/usr/local/ci/run_script.sh _move_artifacts.sh ${DISTRIBUTION}"

echo Download artifacts from build server
rsync -a -e 'ssh -o StrictHostKeyChecking=no' pcp@${builder_ip}:artifacts/ "${local_artifacts_dir}"
