#!/usr/bin/env python3
"""This script generates a JUnit report from PCP's test runner log file"""
import sys
import re
import os
from xml.sax.saxutils import escape

def parse_results_dir(results_dir):
    """parse tests from testoutput"""
    tests = []

    for group in os.listdir(results_dir):
        for test_no in os.listdir(os.path.join(results_dir, group)):
            path = os.path.join(results_dir, group, test_no)
            test = {
                "no": int(test_no),
                "stdout": open(os.path.join(path, "stdout")).read(),
                "stderr": open(os.path.join(path, "stderr")).read()
            }
            success_m = re.search(r'^Passed all \d+ tests$', test["stdout"], re.MULTILINE)
            notrun_m = re.search(r'^(\d+) \[not run\] (.+)', test["stdout"], re.MULTILINE)

            if success_m:
                pass
            elif notrun_m:
                test["skipped"] = notrun_m.group(2)
            else:
                test["failed"] = "Test failed"
                test["failed_reason"] = test["stdout"]
            tests.append(test)

    return tests

def parse_testsuite(job_file, results_dir):
    """parse test output and calculate summary properties"""
    testsuite = {
        "name": "tests",
        "tests": parse_results_dir(results_dir)
    }

    test_durations = {}
    with open(job_file) as f:
        next(f) # skip header
        for line in f:
            spl = line.split("\t")
            no = int(spl[8].split(" ")[1])
            test_durations[no] = float(spl[3])
    for test in testsuite["tests"]:
        test["time"] = test_durations.get(test["no"], 0)

    testsuite["tests_cnt"] = len(testsuite["tests"])
    testsuite["skipped_cnt"] = len([test for test in testsuite["tests"] if "skipped" in test])
    testsuite["failed_cnt"] = len([test for test in testsuite["tests"] if "failed" in test])
    testsuite["time"] = sum([test["time"] for test in testsuite["tests"]])
    return testsuite

def create_report(job_file, results_dir):
    """generates a JUnit report"""
    testsuites = []
    testsuites.append(parse_testsuite(job_file, results_dir))

    print('<?xml version="1.0" encoding="UTF-8"?>')
    print('<testsuites>')

    for testsuite in testsuites:
        print('<testsuite name="{name}" tests="{tests_cnt}" skipped="{skipped_cnt}" '
              'failures="{failed_cnt}" errors="0" time="{time}">'.format(**testsuite))
        for test in testsuite["tests"]:
            print('<testcase classname="{no}" name="{no}" time="{time}">'.format(**test))
            if "skipped" in test:
                print('<skipped message="{skipped}"/>'.format(**test))
            if "failed" in test:
                print('<failure message="{failed}">{failed_reason}</failure>'.format(
                    failed=test["failed"], failed_reason=escape(test["failed_reason"])))
            if "stdout" in test:
                print('<system-out>{stdout}</system-out>'.format(stdout=escape(test["stdout"])))
            if "stderr" in test:
                print('<system-err>{stderr}</system-err>'.format(stderr=escape(test["stderr"])))
            print('</testcase>')
        print('</testsuite>')

    print('</testsuites>')

if __name__ == '__main__':
    if len(sys.argv) != 3:
        print("usage: {} tests-job-file tests-results-directory".format(sys.argv[0]))
        sys.exit(1)
    create_report(sys.argv[1], sys.argv[2])
