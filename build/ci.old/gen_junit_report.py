#!/usr/bin/env python3
"""This script generates a JUnit report from PCP's test runner log file"""
import sys
import re
from xml.sax.saxutils import escape

def read_logfile(no, ext):
    """read a logfile (and return an error message if file could not be found)"""
    fn = str(no) + '.' + ext
    try:
        with open(fn) as f:
            return f.read()
    except IOError as e:
        return 'could not open {}: {}'.format(fn, e)

def parse_testouput_tests():
    """parse tests from testoutput"""
    tests = []
    for line in sys.stdin:
        # [xx%] will only be displayed if there are more than 9 tests
        success_m = re.match(r'^(?:\[\d+%\] )?(\d+)$', line)
        notrun_m = re.match(r'^(?:\[\d+%\] )?(\d+) \[not run\] (.+)$', line)
        failed_m = re.match(r'^(?:\[\d+%\] )?(\d+) (.+)$', line)
        if success_m:
            tests.append({
                "no": success_m.group(1),
                "stdout": read_logfile(success_m.group(1), "out")
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
                "stdout": read_logfile(failed_m.group(1), "out.bad"),
                "stderr": read_logfile(failed_m.group(1), "full")
            })
        else:
            # if this line doesn't match any regex, it's probably output from the previous test
            if tests and "failed_reason" in tests[-1]:
                tests[-1]["failed_reason"] += line
    return tests

def parse_testouput():
    """parse test output and calculate summary properties"""
    testsuite = {
        "name": "tests",
        "tests": parse_testouput_tests()
    }

    test_durations = {}
    with open('check.time') as f:
        for line in f:
            no, duration = line.strip().split(' ')
            test_durations[no] = int(duration)
    for test in testsuite["tests"]:
        test["time"] = test_durations.get(test["no"], 0)

    testsuite["tests_cnt"] = len(testsuite["tests"])
    testsuite["skipped_cnt"] = len([test for test in testsuite["tests"] if "skipped" in test])
    testsuite["failed_cnt"] = len([test for test in testsuite["tests"] if "failed" in test])
    testsuite["time"] = sum([test["time"] for test in testsuite["tests"]])
    return testsuite

def create_report():
    """generates a JUnit report"""
    testsuites = []
    testsuites.append(parse_testouput())

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
    create_report()
