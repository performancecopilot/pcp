#!/bin/sh -e

local_artifacts_dir=/tmp/artifacts

builder_ip=$(az vmss list-instance-public-ips \
  --resource-group "${RESOURCE_GROUP}" \
  --name "${VMSS}" \
  --query "[*].ipAddress | sort(@) | [0]" --output tsv)
agent_ips=$(az vmss list-instance-public-ips \
  --resource-group "${RESOURCE_GROUP}" \
  --name "${VMSS}" \
  --query "[*].ipAddress" --output tsv)
agents_ssh="$(printf "pcp@%s," ${agent_ips})"

echo Transfer artifacts to agents
for agent_ip in ${agent_ips}
do
  [ "${agent_ip}" = "${builder_ip}" ] && continue
  rsync -a -e 'ssh -o StrictHostKeyChecking=no' "${local_artifacts_dir}" pcp@${agent_ip}:artifacts/
done

echo Install PCP on agents
parallel --nonall -S "${agents_ssh}" /usr/local/ci/run_script.sh _install_pcp.sh ${DISTRIBUTION}

echo Start distributed QA tests
parallel --jobs 1 --linebuffer -S "${agents_ssh}" /usr/local/ci/run_script.sh _test.sh ${DISTRIBUTION} ::: sanity pmda.bcc pmda.bpftrace
