#!/usr/bin/env python
"""This script generates a JUnit report from PCP's test runner log file"""
import sys
import os.path
import glob
import re
from xml.sax.saxutils import escape

def read_logfile(testsuite_path, no, ext):
    """read a logfile (and return an error message if file could not be found)"""
    fn = str(no) + '.' + ext
    try:
        with open(os.path.join(testsuite_path, fn)) as f:
            return f.read()
    except IOError as e:
        return 'could not open {}: {}'.format(fn, e)

def parse_logfile_tests(logfile, testsuite_path):
    """parse tests from a logfile"""
    tests = []
    for line in logfile:
        success_m = re.match(r'^\[\d+%\] (\d+)$', line)
        notrun_m = re.match(r'^\[\d+%\] (\d+) \[not run\] (.+)$', line)
        failed_m = re.match(r'^\[\d+%\] (\d+) (.+)$', line)
        if success_m:
            tests.append({
                "no": success_m.group(1),
                "stdout": read_logfile(testsuite_path, success_m.group(1), "out")
            })
        elif notrun_m:
            tests.append({
                "no": notrun_m.group(1),
                "skipped": notrun_m.group(2),
            })
        elif failed_m:
            tests.append({
                "no": failed_m.group(1),
                "failed": failed_m.group(2).replace("- ", ""),
                "failed_reason": "",
                "stdout": read_logfile(testsuite_path, failed_m.group(1), "out.bad"),
                "stderr": read_logfile(testsuite_path, failed_m.group(1), "full")
            })
        else:
            # if this line doesn't match any regex, it's probably output from the previous test
            if tests and "failed_reason" in tests[-1]:
                tests[-1]["failed_reason"] += line
    return tests

def parse_logfile(log_fn, testsuite_path):
    """parse a logfile and calculate summary properties"""
    testsuite = {
        "name": os.path.splitext(os.path.basename(log_fn))[0],
        "tests": []
    }
    with open(log_fn) as logfile:
        testsuite["tests"] = parse_logfile_tests(logfile, testsuite_path)
        logfile.seek(0)
        testsuite["stdout"] = logfile.read()
    testsuite["tests_cnt"] = len(testsuite["tests"])
    testsuite["skipped_cnt"] = len([test for test in testsuite["tests"] if "skipped" in test])
    testsuite["failed_cnt"] = len([test for test in testsuite["tests"] if "failed" in test])
    return testsuite

def create_report(log_path, testsuite_path):
    """generates a JUnit report"""
    testsuites = []
    for fn in glob.glob(log_path + '/*.log'):
        testsuites.append(parse_logfile(fn, testsuite_path))

    print('<?xml version="1.0" encoding="UTF-8"?>')
    print('<testsuites>')

    for testsuite in testsuites:
        print('<testsuite name="{name}" tests="{tests_cnt}" skipped="{skipped_cnt}" '
              'failures="{failed_cnt}" errors="0">'.format(**testsuite))
        for test in testsuite["tests"]:
            print('<testcase classname="{no}" name="{no}">'.format(**test))
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

        print('<system-out>{stdout}</system-out>'.format(stdout=escape(testsuite["stdout"])))
        print('</testsuite>')

    print('</testsuites>')

if __name__ == '__main__':
    if len(sys.argv) != 3:
        print('usage: {} log_path testsuite_path'.format(sys.argv[0]))
        sys.exit(1)

    create_report(sys.argv[1], sys.argv[2])
