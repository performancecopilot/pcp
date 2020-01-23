#!/bin/sh -eu

cd "$(dirname "$0")/.."
. scripts/env.sh
. scripts/vmss.env.sh
artifacts_dir="$2/"

echo Host IPs: ${HOST_IPS}
echo Builder IPs: ${BUILDER_IP}

echo
echo Download artifacts from build server
rsync -av -e "$SSH" pcp@${BUILDER_IP}:artifacts/ "${artifacts_dir}"

for host_ip in ${HOST_IPS}
do
  [ "${host_ip}" = "${BUILDER_IP}" ] && continue
  echo
  echo Transfer artifacts to host ${host_ip}
  rsync -av -e "$SSH" "${artifacts_dir}" pcp@${host_ip}:artifacts/
done

echo
echo Installing PCP on all hosts
parallel --nonall --tag --line-buffer --eta -S "${HOSTS_SSH}" /usr/local/ci/install.sh

echo
echo Trigger QA initialization steps on all hosts
parallel --nonall --tag --line-buffer --eta -S "${HOSTS_SSH}" /usr/local/ci/test.sh 002
