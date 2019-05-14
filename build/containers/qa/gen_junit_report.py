#!/usr/bin/env python
import sys
import os.path
import glob
import re
from xml.sax.saxutils import escape

def read_logfile(testsuite_path, no, ext):
    fn = str(no) + '.' + ext
    try:
        with open(os.path.join(testsuite_path, fn)) as f:
            return f.read()
    except IOError as e:
        return 'could not open {}: {}'.format(fn, e)

def parse_log_line(line, testsuite_path):
    success_m = re.match(r'^\[\d+%\] (\d+)$', line)
    notrun_m = re.match(r'^\[\d+%\] (\d+) \[not run\] (.+)$', line)
    failed_m = re.match(r'^\[\d+%\] (\d+) (.+)$', line)
    if success_m:
        return {
            "no": success_m.group(1),
            "stdout": read_logfile(testsuite_path, success_m.group(1), "out")
        }
    elif notrun_m:
        return {
            "no": notrun_m.group(1),
            "skipped": notrun_m.group(2),
            "stdout": ""
        }
    elif failed_m:
        return {
            "no": failed_m.group(1),
            "failed": failed_m.group(2).replace("- ", ""),
            "failed_reason": read_logfile(testsuite_path, failed_m.group(1), "full"),
            "stdout": read_logfile(testsuite_path, failed_m.group(1), "out.bad")
        }
    else:
        return {}

def parse_testsuite(log_fn, testsuite_path):
    testsuite = {
        "name": os.path.splitext(os.path.basename(log_fn))[0],
        "tests": []
    }
    with open(log_fn) as f:
        for line in f:
            test = parse_log_line(line.rstrip(), testsuite_path)
            if test:
                testsuite["tests"].append(test)
    testsuite["tests_cnt"] = len(testsuite["tests"])
    testsuite["skipped_cnt"] = len([test for test in testsuite["tests"] if "skipped" in test])
    testsuite["failed_cnt"] = len([test for test in testsuite["tests"] if "failed" in test])
    return testsuite

def create_report(log_path, testsuite_path):
    testsuites = []
    for fn in glob.glob(log_path + '/*.log'):
        testsuites.append(parse_testsuite(fn, testsuite_path))

    print('<?xml version="1.0" encoding="UTF-8"?>')
    print('<testsuites>')

    for testsuite in testsuites:
        print('<testsuite name="{name}" tests="{tests_cnt}" skipped="{skipped_cnt}" failures="{failed_cnt}" errors="0">'.format(**testsuite))
        for test in testsuite["tests"]:
            print('<testcase classname="{no}" name="{no}">'.format(**test))
            if "skipped" in test:
                print('<skipped message="{skipped}"/>'.format(**test))
            if "failed" in test:
                print('<failure message="{failed}">{failed_reason}</failure>'.format(failed=test["failed"], failed_reason=escape(test["failed_reason"])))
            print('<system-out>{stdout}</system-out>'.format(stdout=escape(test["stdout"])))
            print('</testcase>')
        print('</testsuite>')

    print('</testsuites>')

if __name__ == '__main__':
    if len(sys.argv) != 3:
        print('usage: {} log_path testsuite_path'.format(sys.argv[0]))
        sys.exit(1)

    create_report(sys.argv[1], sys.argv[2])
