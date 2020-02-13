#!/bin/sh -eu

cd "$(dirname "$0")/.."
. scripts/env.sh
. scripts/env.vmss.sh
artifacts_dir="$2/"

echo Host IPs: "${AZ_VMSS_IPS}"
echo Builder IPs: "${AZ_VMSS_BUILDER_IP}"

echo
echo Download artifacts from build server
rsync -av -e "$SSH" "pcp@${AZ_VMSS_BUILDER_IP}:artifacts/" "${artifacts_dir}"

for host_ip in ${AZ_VMSS_IPS}
do
  [ "${host_ip}" = "${AZ_VMSS_BUILDER_IP}" ] && continue
  echo
  echo Transfer artifacts to host "${host_ip}"
  rsync -av -e "$SSH" "${artifacts_dir}" "pcp@${host_ip}:artifacts/"
done

echo
echo Installing PCP on all hosts
parallel --nonall --tag --line-buffer --eta -S "${AZ_VMSS_HOSTS_SSH}" /usr/local/ci/install.sh

echo
echo Trigger QA initialization steps on all hosts
parallel --nonall --tag --line-buffer --eta -S "${AZ_VMSS_HOSTS_SSH}" /usr/local/ci/test.sh 002
