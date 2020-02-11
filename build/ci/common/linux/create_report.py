#!/usr/bin/env pmpython
#
# This script creates JUnit <testcase> XML elements from the output of the qa/check script,
# including timings from check.time and debug information from *.out.bad and *.full files

# Note: This script runs on each host, therefore it needs to support Python 2.6.6 (for RHEL 6)
# Script is meant to be run with PCP's testsuite dir as working directory.

import sys
import re
import xml.etree.ElementTree as ET


def read_logfile(path):
    try:
        with open(path) as f:
            return f.read()
    except IOError as e:
        return 'could not open {}: {}'.format(path, e)


def read_test_durations():
    test_durations = {}
    with open("check.time") as f:
        for line in f:
            no, duration = line.strip().split()
            test_durations[no] = duration
    return test_durations


def create_testcases(test_output_file, test_durations):
    testcases = []
    testcase = None
    for line in test_output_file:
        # [xx%] will be displayed if there are more than 9 tests
        #  Xs ... will be displayed if test was already run
        success_m = re.match(r'^(?:\[\d+%\] )?(\d+)(?: \d+s \.\.\.)?$', line)
        notrun_m = re.match(r'^(?:\[\d+%\] )?(\d+)(?: \d+s \.\.\.)? \[not run\] (.+)$', line)
        failed_m = re.match(r'^(?:\[\d+%\] )?(\d+)(?: \d+s \.\.\.)? ((\- |\[).+)$', line)

        if success_m or notrun_m or failed_m:
            test_no = (success_m or notrun_m or failed_m).group(1)
            testcase = ET.Element("testcase")
            testcase.set("name", test_no)  # title
            testcase.set("classname", "qa/" + test_no)  # test file
            if test_no in test_durations:
                testcase.set("time", test_durations[test_no])  # duration

        if success_m:
            testcases.append(testcase)
        elif notrun_m:
            skipped = ET.SubElement(testcase, "skipped")
            skipped.set("message", notrun_m.group(2))  # error message
            testcases.append(testcase)
        elif failed_m:
            failed_msg = failed_m.group(2)
            if failed_msg.startswith("- "):
                failed_msg = failed_msg[2:]

            failure = ET.SubElement(testcase, "failure")
            failure.set("message", failed_msg)  # error message
            failure.text = ""  # stack trace

            system_out = ET.SubElement(testcase, "system-out")
            system_out.text = read_logfile(test_no + ".out.bad")
            system_err = ET.SubElement(testcase, "system-err")
            system_err.text = read_logfile(test_no + ".full")
            testcases.append(testcase)
        elif testcase is not None:
            # if this line doesn't match any regex, it's probably output from the previous test
            last_failure = testcase.find("failure")
            if last_failure is not None:
                last_failure.text += line
    return testcases


def create_report(test_output_path, output):
    test_durations = read_test_durations()
    with open(test_output_path) as test_output_file:
        testcases = create_testcases(test_output_file, test_durations)

    for testcase in testcases:
        if sys.version_info > (3,):
            print(ET.tostring(testcase, "unicode"))
        else:
            print(ET.tostring(testcase))


if __name__ == "__main__":
    if len(sys.argv) != 2:
        print("usage: {} test-output.log".format(sys.argv[0]))
        sys.exit(1)

    create_report(sys.argv[1], sys.stdout)
