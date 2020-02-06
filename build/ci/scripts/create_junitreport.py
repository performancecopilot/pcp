#!/usr/bin/env python3
import sys
import re
import os
import xml.etree.ElementTree as ET


def read_testcase(testcase: ET.Element, testcase_path: str):
    system_out = ET.SubElement(testcase, "system-out")
    system_out.text = open(os.path.join(testcase_path, "stdout"), encoding="utf-8").read()

    system_err = ET.SubElement(testcase, "system-err")
    system_err.text = open(os.path.join(testcase_path, "stderr"), encoding="utf-8").read()

    success_m = re.search(r'^Passed all \d+ tests$', system_out.text, re.MULTILINE)
    notrun_m = re.search(r'^(\d+) \[not run\] (.+)', system_out.text, re.MULTILINE)
    if success_m:
        pass
    elif notrun_m:
        skipped = ET.SubElement(testcase, "skipped")
        skipped.set("message", notrun_m.group(2))  # error message
    else:
        failure = ET.SubElement(testcase, "failure")
        failure.set("message", "Test failed")  # error message
        failure.text = system_out.text or system_err.text  # stack trace


def create_report(job_file_path: str, results_dir: str):
    testsuites = ET.Element("testsuites")
    testsuite = ET.SubElement(testsuites, "testsuite", {"name": "tests"})  # name is ignored

    with open(job_file_path, encoding="utf-8") as job_file:
        next(job_file)  # skip header
        for job_line in job_file:
            fields = job_line.rstrip().split("\t")
            command = fields[8]
            test_no = command.split()[-1]
            job_runtime = fields[3].lstrip()

            testcase = ET.SubElement(testsuite, "testcase")
            testcase.set("name", test_no)  # title
            testcase.set("classname", "qa/" + test_no)  # test file
            testcase.set("time", job_runtime)  # duration
            read_testcase(testcase, os.path.join(results_dir, "1", test_no))

    tree = ET.ElementTree(testsuites)
    tree.write(sys.stdout, encoding="unicode", xml_declaration=True)


if __name__ == "__main__":
    if len(sys.argv) != 3:
        print("usage: {} tests-job-file tests-results-directory".format(sys.argv[0]))
        sys.exit(1)

    create_report(sys.argv[1], sys.argv[2])
