#!/bin/sh -eu

PREV_PWD=$PWD
cd "$(dirname "$0")/.."
. scripts/env.sh
. scripts/vmss.env.sh
tests_job_file="${PREV_PWD}/jobs.txt"
tests_junit_file="${PREV_PWD}/tests.xml"
tests_results_dir="${PREV_PWD}/test-results"

echo Start distributed QA tests
tests=$(cat ../../qa/group | grep -E "$2" | cut -d' ' -f1 | grep -E '^[0-9]+$')
status=0
parallel --jobs 1 --eta --joblog "${tests_job_file}" --results "${tests_results_dir}" \
  -S "${HOSTS_SSH}" /usr/local/ci/test.sh ::: "${tests}" > /dev/null || status=$?

echo
echo Generate JUnit output
./scripts/create_junitreport.py "${tests_job_file}" "${tests_results_dir}" > "${tests_junit_file}"

echo
echo All tests:
cat "${tests_job_file}"

echo
echo Failed tests:
cat "${tests_job_file}" | awk -F '\t' '$7 != 0 {print}'

exit $status
