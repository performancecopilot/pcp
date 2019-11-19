#!/bin/sh -eu

PREV_PWD=$PWD
cd "$(dirname "$0")/.."
. common/env.sh

test_jobs_file="${PREV_PWD}/jobs.txt"
test_junit_file="${PREV_PWD}/tests.xml"
test_results_dir="${PREV_PWD}/test-results"
host_ips=$(az vmss list-instance-public-ips \
  --resource-group "${AZ_RESOURCE_GROUP}" \
  --name "${AZ_VMSS}" \
  --query "[*].ipAddress" --output tsv)
hosts_ssh="$(printf "$SSH pcp@%s," ${host_ips})"
echo Host IPs: ${host_ips}

echo Installing PCP on all hosts
parallel --nonall --eta -S "${hosts_ssh}" /usr/local/ci/install.sh

echo Start distributed QA tests
tests=$(cat ../../qa/group | grep sanity | cut -d' ' -f1 | grep -E '^[0-9]+$')
status=0
parallel --jobs 1 --eta --joblog "${test_jobs_file}" --results "${test_results_dir}" \
  -S "${hosts_ssh}" sudo -u pcpqa /usr/local/ci/test.sh ::: "${tests}" || status=$?

echo Generate JUnit output
./build/ci/scripts/create_junitreport.py "${test_results_dir}" > "${test_junit_file}"

echo Job Logs:
cat "${test_jobs_file}"
exit $status
