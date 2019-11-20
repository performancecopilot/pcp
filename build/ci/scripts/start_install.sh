#!/bin/sh -eu

PREV_PWD=$PWD
cd "$(dirname "$0")/.."
. common/env.sh
. scripts/vmss.env.sh
artifacts_dir="${PREV_PWD}/artifacts/"

echo Host IPs: ${HOST_IPS}
echo Builder IPs: ${BUILDER_IP}

echo
echo Download artifacts from build server
rsync -a -e "$SSH" pcp@${BUILDER_IP}:artifacts/ "${artifacts_dir}"

echo
echo Transfer artifacts to hosts
for host_ip in ${HOST_IPS}
do
  [ "${host_ip}" = "${BUILDER_IP}" ] && continue
  rsync -a -e "$SSH" "${artifacts_dir}" pcp@${host_ip}:artifacts/
done

echo
echo Installing PCP on all hosts
parallel --nonall --tag --line-buffer --eta -S "${HOSTS_SSH}" /usr/local/ci/install.sh

echo
echo Trigger QA initialization steps on all hosts
parallel --nonall --tag --line-buffer --eta -S "${HOSTS_SSH}" /usr/local/ci/test.sh 002
