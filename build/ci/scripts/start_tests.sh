#!/bin/sh -eu

cd "$(dirname "$0")/.."
. scripts/env.sh
. scripts/env.vmss.sh
tests="$2"
tests_dir="$3"
tests_job_file="${tests_dir}/jobs.txt"
tests_junit_file="${tests_dir}/tests.xml"
tests_results_dir="${tests_dir}/test-results"

echo Start distributed QA tests
tests=$(cat ../../qa/group | grep -E "${tests}" | grep -oP '^[0-9]+(?= )' || true)
[ -z "${tests}" ] && { echo "No tests matching '${tests}', exiting."; exit 0; }

status=0
mkdir -p "${tests_dir}"
parallel --jobs 1 --max-args 30 --eta --joblog "${tests_job_file}" --results "${tests_results_dir}/{}/" \
  -S "${AZ_VMSS_HOSTS_SSH}" /usr/local/ci/test.sh ::: "${tests}" > /dev/null || status=$?

echo
echo Generate JUnit output
cat << EOF > "${tests_junit_file}"
<?xml version="1.0" encoding="UTF-8"?>
<testsuites>
  <testsuite name="tests">
    $(cat "${tests_results_dir}"/*/stderr)
  </testsuite>
</testsuites>
EOF

echo
echo All test jobs:
cat "${tests_job_file}"

echo
echo Failed test jobs:
cat "${tests_job_file}" | awk -F '\t' '$7 != 0 {print}'

exit $status
