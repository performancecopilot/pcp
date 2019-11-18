#!/bin/sh -eu

PREV_PWD=$PWD
cd "$(dirname "$0")/.."
. common/env.sh

artifacts_dir="${PREV_PWD}/artifacts/"
host_ips=$(az vmss list-instance-public-ips \
  --resource-group "${AZ_RESOURCE_GROUP}" \
  --name "${AZ_VMSS}" \
  --query "[*].ipAddress" --output tsv)
builder_ip=$(echo ${host_ips} | tr ' ' '\n' | sort -n | head -n 1)

echo Starting build on ${builder_ip}
$SSH pcp@${builder_ip} "GIT_REPO=${GIT_REPO} GIT_COMMIT=${GIT_COMMIT} /usr/local/ci/build.sh"

echo Download artifacts from build server
rsync -a -e "$SSH" pcp@${builder_ip}:artifacts/ "${artifacts_dir}"

echo Transfer artifacts to hosts
for host_ip in ${host_ips}
do
  [ "${host_ip}" = "${builder_ip}" ] && continue
  rsync -a -e "$SSH" "${artifacts_dir}" pcp@${host_ip}:artifacts/
done
