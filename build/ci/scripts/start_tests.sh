#!/bin/sh -eu

PREV_PWD=$PWD
cd "$(dirname "$0")/.."
. common/env.sh

test_results_dir="${PREV_PWD}/test-results"
host_ips=$(az vmss list-instance-public-ips \
  --resource-group "${AZ_RESOURCE_GROUP}" \
  --name "${AZ_VMSS}" \
  --query "[*].ipAddress" --output tsv)
hosts_ssh="$(printf "pcp@%s," ${host_ips})"

echo Installing PCP on all hosts
parallel --nonall --eta -S "${hosts_ssh}" /usr/local/ci/install.sh

echo Start distributed QA tests
tests=$(cat ../../qa/group | grep sanity | cut -d' ' -f1 | grep -E '^[0-9]+$')
rm -rf test-results
parallel --jobs 1 --eta --joblog "testlog.txt" --results "${test_results_dir}" \
  -S "${hosts_ssh}" sudo -u pcpqa /usr/local/ci/test.sh ::: "${tests}" || status=$?
cat "${test_results_dir}/log.txt"
exit $status
