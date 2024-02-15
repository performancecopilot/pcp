#!/usr/bin/env python3
import os
import shutil
import argparse
import re
import json
import csv
from enum import Enum
from collections import defaultdict
from typing import IO, List, Dict, Optional, Tuple
import requests


class Test:
    class Status(str, Enum):
        Passed = "passed"
        Skipped = "skipped"
        Failed = "failed"
        Broken = "broken"
        Triaged = "triaged"

    def __init__(self, name: str):
        self.name = name
        self.groups = []
        self.platform = ""
        self.path = ""
        self.description = ""
        self.start = 0
        self.stop = 0
        self.status = ""
        self.message = ""


FAILED_TEST_STATES = [Test.Status.Failed, Test.Status.Broken, Test.Status.Triaged]


def read_test_durations(timings_path: str) -> Dict[str, List[int]]:
    test_durations = {}
    with open(timings_path, encoding="utf-8") as f:
        for line in f:
            no, start, stop = line.strip().split()
            test_durations[no] = [int(start), int(stop)]
    return test_durations


def read_test_groups(group_path: str) -> Dict[str, List[str]]:
    tests = {}
    with open(group_path, encoding="utf-8") as f:
        for line in f:
            spl = line.rstrip().split()
            if len(spl) >= 2 and spl[0].isdigit():
                tests[spl[0]] = spl[1:]
    return tests


def read_testlog(
    qa_dir: str,
    testartifacts_dir: str,
    groups: Dict[str, List[str]],
    test_durations: Dict[str, List[int]],
    platform: str,
) -> List[Test]:
    tests: List[Test] = []
    testlog_path = os.path.join(testartifacts_dir, "test.log")
    with open(testlog_path, encoding="utf-8", errors="backslashreplace") as testlog_file:
        for line in testlog_file:
            # [xx%] will be displayed if there are more than 9 tests
            #  Xs ... will be displayed if test was already run
            success_m = re.match(r"^(?:\[\d+%\] )?(\d+)(?: \d+s \.\.\.)?\n$", line)
            notrun_m = re.match(r"^(?:\[\d+%\] )?(\d+)(?: \d+s \.\.\.)? \[not run\] (.+)\n$", line)
            failed_m = re.match(r"^(?:\[\d+%\] )?(\d+)(?: \d+s \.\.\.)? ((\- |\[).+)\n$", line)
            cancelled_m = re.match(r"^(?:\[\d+%\] )?(\d+)(?: \d+s \.\.\.)?\Z", line)

            if success_m or notrun_m or failed_m or cancelled_m:
                test_no = (success_m or notrun_m or failed_m or cancelled_m).group(1)

                test = Test(test_no)
                test.groups = groups.get(test_no, [])
                test.platform = platform
                test.path = testartifacts_dir
                test.start, test.stop = test_durations.get(test_no, (0, 0))
                test.description = ""
                try:
                    with open(os.path.join(qa_dir, test_no), encoding="utf-8") as f:
                        next(f)  # skip shebang
                        for test_line in f:
                            if test_line == "\n":
                                break
                            test.description += test_line.strip(" #") + "\n"
                    tests.append(test)
                except IOError:  # non-existant file
                    # false match - it's probably output from the previous test
                    if tests and tests[-1].status in FAILED_TEST_STATES:
                        if len(tests[-1].message) < 10000:  # ignore output for huge diffs
                            tests[-1].message += line
                    continue

            if success_m:
                test.status = Test.Status.Passed
            elif notrun_m:
                test.status = Test.Status.Skipped
                test.message = notrun_m.group(2)
            elif failed_m:
                failed_msg = failed_m.group(2)
                if failed_msg.startswith("- "):
                    failed_msg = failed_msg[2:]

                if "failed" in failed_msg:
                    test.status = Test.Status.Broken
                elif "triaged" in failed_msg:
                    test.status = Test.Status.Triaged
                else:
                    test.status = Test.Status.Failed
                test.message = failed_msg + "\n"
            elif cancelled_m:
                test.status = Test.Status.Broken
                test.message = "test cancelled"
            elif tests and tests[-1].status in FAILED_TEST_STATES:
                # if this line doesn't match any regex, it's probably output from the previous test
                if len(tests[-1].message) < 10000:  # ignore output for huge diffs
                    tests[-1].message += line

    return tests


def read_platforms(qa_dir: str, artifacts_path: str) -> Tuple[List[str], List[Test]]:
    groups = read_test_groups(os.path.join(qa_dir, "group"))
    platforms = set()
    tests = []
    for artifact_dir in os.listdir(artifacts_path):
        if artifact_dir.startswith("build-"):
            platform = artifact_dir[len("build-") :]
            platforms.add(platform)
        elif artifact_dir.startswith("test-"):
            platform = artifact_dir[len("test-") :]
            platforms.add(platform)

            testartifacts_dir = os.path.join(artifacts_path, artifact_dir)
            test_timings_file = os.path.join(testartifacts_dir, "check.timings")

            test_durations = read_test_durations(test_timings_file)
            tests.extend(read_testlog(qa_dir, testartifacts_dir, groups, test_durations, platform))
    return list(platforms), tests


def write_test_report_csv(tests: List[Test], out: IO):
    writer = csv.writer(out)
    writer.writerow(["Name", "Groups", "Platform", "Start", "Stop", "Duration", "Status"])
    for test in tests:
        writer.writerow(
            [
                test.name,
                ",".join(test.groups),
                test.platform,
                test.start,
                test.stop,
                test.stop - test.start,
                test.status,
            ]
        )


def test_summary_short_platform(platform: str) -> str:
    return (
        platform.replace("-container", "")
        .replace("fedora", "f")
        .replace("centos-stream", "el")
        .replace("centos", "el")
        .replace("debian", "deb")
        .replace("ubuntu", "ubu")
    )


def test_summary_tests(tests: List[Test]):
    tests_grouped = defaultdict(dict)
    platform_set = set()
    has_failed_tests = False
    for test in tests:
        tests_grouped[test.name][test.platform] = test
        platform_set.add(test.platform)
        if not has_failed_tests and test.status in FAILED_TEST_STATES:
            has_failed_tests = True

    if not has_failed_tests:
        return ""

    platforms = sorted(platform_set)
    platform_short = {platform: test_summary_short_platform(platform) for platform in platforms}

    summary = "=== Failed Tests ===\n\n"
    summary += " Test "
    for platform in platforms:
        summary += f" {platform_short[platform]}"
    summary += "  Groups\n"

    for test_name in sorted(tests_grouped, key=int):
        test_line = f" {int(test_name):4d} "
        for platform in platforms:
            col_width = len(platform_short[platform])
            test_line += " "

            test = tests_grouped[test_name].get(platform)
            if test and test.status == Test.Status.Passed:
                test_line += " ".center(col_width)
            elif test and test.status in [Test.Status.Failed, Test.Status.Broken]:
                test_line += "X".center(col_width)
            elif test and test.status == Test.Status.Triaged:
                test_line += "T".center(col_width)
            else:
                test_line += "-".center(col_width)

        groups = sorted(list(tests_grouped[test_name].values())[0].groups)
        test_line += f"  {' '.join(groups)}"

        # skip tests without any failures
        if "X" in test_line or "T" in test_line:
            summary += test_line + "\n"

    summary += "\nLegend: ( ) Passed, (X) Failure, (T) Triaged, (-) Skipped\n\n\n"
    return summary


def test_summary_platforms(platforms: List[str], tests: List[Test]):
    platform_stats = {platform: {"passed": 0, "failed": 0, "skipped": 0, "cancelled": 0} for platform in platforms}
    for test in tests:
        if test.status == Test.Status.Passed:
            platform_stats[test.platform]["passed"] += 1
        elif test.status == Test.Status.Skipped:
            platform_stats[test.platform]["skipped"] += 1
        elif test.status == Test.Status.Broken and test.message == "test cancelled":
            platform_stats[test.platform]["cancelled"] += 1
        elif test.status in FAILED_TEST_STATES:
            platform_stats[test.platform]["failed"] += 1

    summary = "=== Platform Summary ===\n\n"
    summary += f"{'Platform':25}  Pass Fail Skip\n"
    for platform, stats in sorted(platform_stats.items()):
        summary += f"{platform:25}  {stats['passed']:4d} {stats['failed']:4d} {stats['skipped']:4d}"
        if stats["passed"] == 0:
            summary += "  build broken\n"
        elif stats["cancelled"] > 0:
            summary += "  QA ran into a timeout\n"
        elif stats["failed"] == 0:
            summary += "  ✓\n"
        else:
            summary += "\n"
    summary += "\n\n"
    return summary


def write_test_summary(platforms: List[str], tests: List[Test], out: IO, build_url: str, report_url: str):
    summary = test_summary_tests(tests)
    summary += test_summary_platforms(platforms, tests)
    summary += f"Build:  {build_url}\n"
    summary += f"Report: {report_url}\n"
    out.write(summary)


def write_allure_test_result(test: Test, allure_results_path: str, source_url: Optional[str]):
    allure_result = {
        "name": f"QA {test.name}",
        "fullName": f"QA #{test.name} on {test.platform}",
        "historyId": f"{test.name}@{test.platform}",
        "description": test.description,
        # Allure doesn't have a "triaged" test status, therefore we use the "unknown" status
        "status": "unknown" if test.status == Test.Status.Triaged else test.status,
        "statusDetails": {"message": test.message, "flaky": "flakey" in test.groups},
        "attachments": [],
        "start": test.start * 1000,
        "stop": test.stop * 1000,
        "labels": [
            {"name": "suite", "value": test.platform},
            {"name": "host", "value": test.platform},
        ],
        "links": [
            {
                "type": "test_case",
                "url": f"{source_url}/qa/{test.name}" if source_url else "",
                "name": "Source",
            },
            {
                "type": "test_case",
                "url": f"{source_url}/qa/{test.name}.out" if source_url else "",
                "name": "Expected output",
            },
        ],
    }

    for group in test.groups:
        allure_result["labels"].append({"name": "epic", "value": group})

    if test.status in FAILED_TEST_STATES:
        allure_result["description"] += (
            f"**To reproduce this test, please run:**\n\n"
            f"    build/ci/ci-run.py {test.platform} reproduce"
        )

        out_bad = os.path.join(test.path, f"{test.name}.out.bad")
        if os.path.exists(out_bad):
            out_bad_attachment_fn = f"{test.platform}-{test.name}.out.bad-attachment.txt"
            shutil.copyfile(out_bad, os.path.join(allure_results_path, out_bad_attachment_fn))
            allure_result["attachments"].append(
                {
                    "type": "text/plain",
                    "name": f"{test.name}.out.bad",
                    "source": out_bad_attachment_fn,
                }
            )

        out_full = os.path.join(test.path, f"{test.name}.full")
        if os.path.exists(out_full):
            out_full_attachment_fn = f"{test.platform}-{test.name}.full-attachment.txt"
            shutil.copyfile(out_full, os.path.join(allure_results_path, out_full_attachment_fn))
            allure_result["attachments"].append(
                {
                    "type": "text/plain",
                    "name": f"{test.name}.full",
                    "source": out_full_attachment_fn,
                }
            )

    with open(os.path.join(allure_results_path, f"{test.platform}-{test.name}-result.json"), "w") as f:
        json.dump(allure_result, f)


# pylint: disable=too-many-arguments
def write_allure_results(
    allure_results_path: str,
    tests: List[Test],
    source_url: Optional[str],
    build_name: Optional[str],
    build_url: Optional[str],
    report_url: Optional[str],
):
    os.makedirs(allure_results_path, exist_ok=True)
    for test in tests:
        write_allure_test_result(test, allure_results_path, source_url)

    executor = {
        "name": "GitHub Actions",
        "type": "github",
        "buildName": build_name,
        "buildUrl": build_url,
        "reportUrl": report_url,
    }
    with open(os.path.join(allure_results_path, "executor.json"), "w") as f:
        json.dump(executor, f)


def send_slack_notification(
    platforms: List[str],
    tests: List[Test],
    slack_channel: str,
    build_url: str,
    report_url: str,
    summary_url: str,
):
    platform_stats = defaultdict(lambda: {"passed": 0, "failed": 0, "skipped": 0, "cancelled": 0})
    for test in tests:
        if test.status == Test.Status.Passed:
            platform_stats[test.platform]["passed"] += 1
        elif test.status == Test.Status.Skipped:
            platform_stats[test.platform]["skipped"] += 1
        elif test.status == Test.Status.Broken and test.message == "test cancelled":
            platform_stats[test.platform]["cancelled"] += 1
        elif test.status in FAILED_TEST_STATES:
            platform_stats[test.platform]["failed"] += 1

    platform_texts = []
    for platform in sorted(platforms):
        if platform in platform_stats:
            stats = platform_stats[platform]
            platform_stats_text = f"Passed: {stats['passed']}, Failed: {stats['failed']}, Skipped: {stats['skipped']}"
            if stats["cancelled"] > 0:
                platform_stats_text = f"CANCELLED ({platform_stats_text})"

            if stats["cancelled"] > 0 or stats["passed"] < 1000:
                symbol = ":x:"
            elif stats["failed"] > 0:
                symbol = ":warning:"
            else:
                symbol = ":heavy_check_mark:"
        else:
            platform_stats_text = "Build BROKEN"
            symbol = ":x:"
        platform_texts.append(f"*{platform}*:\n{symbol} {platform_stats_text}")

    platform_blocks = []
    # slack only allows max 10 fields inside one block
    for i in range(0, len(platform_texts), 10):
        platform_blocks.append(
            {
                "type": "section",
                "fields": [{"type": "mrkdwn", "text": text} for text in platform_texts[i : i + 10]],
            }
        )

    payload = {
        "channel": slack_channel,
        "attachments": [
            {
                "blocks": [
                    {
                        "type": "section",
                        "text": {"type": "mrkdwn", "text": "*Daily QA:*"},
                    }
                ]
                + platform_blocks
                + [
                    {
                        "type": "section",
                        "text": {
                            "type": "mrkdwn",
                            "text": f"*<{build_url}|View build logs>*\n"
                            f"*<{report_url}|View QA report>*\n"
                            f"*<{summary_url}|View test summary>*",
                        },
                    }
                ]
            }
        ],
    }

    print("Sending slack notification...")
    r = requests.post(
        "https://slack.com/api/chat.postMessage",
        headers={"Authorization": f"Bearer {os.environ['SLACK_BOT_TOKEN']}"},
        json=payload,
    )
    print(r.text)
    r.raise_for_status()


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--qa", default="./qa")
    parser.add_argument("--artifacts")
    parser.add_argument("--summary", type=argparse.FileType("w"))
    parser.add_argument("--csv", type=argparse.FileType("w"))
    parser.add_argument("--allure-results", dest="allure_results")
    parser.add_argument("--source-url", dest="source_url")  # require for allure report
    parser.add_argument("--build-name", dest="build_name")  # require for allure report
    parser.add_argument("--build-url", dest="build_url")  # required for allure report and slack message
    parser.add_argument("--report-url", dest="report_url")  # required for allure report and slack message
    parser.add_argument("--slack-channel", dest="slack_channel")  # required for slack message
    parser.add_argument("--summary-url", dest="summary_url")  # required for slack message
    args = parser.parse_args()

    platforms, tests = read_platforms(args.qa, args.artifacts)

    if args.summary:
        write_test_summary(platforms, tests, args.summary, args.build_url, args.report_url)

    if args.csv:
        write_test_report_csv(tests, args.csv)

    if args.allure_results:
        write_allure_results(
            args.allure_results, tests, args.source_url, args.build_name, args.build_url, args.report_url
        )

    if args.slack_channel:
        print()
        send_slack_notification(platforms, tests, args.slack_channel, args.build_url, args.report_url, args.summary_url)


if __name__ == "__main__":
    main()
