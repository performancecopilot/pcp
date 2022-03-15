#!/usr/bin/env python3
import sys
import os
import shutil
import argparse
import re
import json
import csv
from enum import Enum
from collections import defaultdict
from typing import List, Dict
import requests


class Test:
    class Status(str, Enum):
        Passed = "passed"
        Skipped = "skipped"
        Failed = "failed"
        Broken = "broken"

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
                test.groups = groups[test_no]
                test.platform = platform
                test.path = testartifacts_dir
                test.start, test.stop = test_durations[test_no]
                test.description = ""
                with open(os.path.join(qa_dir, test_no), encoding="utf-8") as f:
                    next(f)  # skip shebang
                    for test_line in f:
                        if test_line == "\n":
                            break
                        test.description += test_line.strip(" #") + "\n"
                tests.append(test)

            if success_m:
                test.status = Test.Status.Passed
            elif notrun_m:
                test.status = Test.Status.Skipped
                test.message = notrun_m.group(2)
            elif failed_m:
                failed_msg = failed_m.group(2)
                if failed_msg.startswith("- "):
                    failed_msg = failed_msg[2:]

                test.status = Test.Status.Broken if "failed" in failed_msg else Test.Status.Failed
                test.message = failed_msg + "\n"
            elif cancelled_m:
                test.status = Test.Status.Broken
                test.message = "test cancelled"
            elif tests and tests[-1].status in [Test.Status.Failed, Test.Status.Broken]:
                # if this line doesn't match any regex, it's probably output from the previous test
                if len(tests[-1].message) < 10000:  # ignore output for huge diffs
                    tests[-1].message += line

    return tests


def read_platforms(qa_dir: str, artifacts_path: str) -> List[Test]:
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


def print_test_report_csv(tests: List[Test], f):
    writer = csv.writer(f)
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


def print_test_summary(tests: List[Test]):
    print("TEST SUMMARY")

    failed_tests = [t for t in tests if t.status in [Test.Status.Failed, Test.Status.Broken]]
    if not failed_tests:
        print("No test failures.")
        return

    print("\nAll failed tests:")
    failed_tests_by_name = defaultdict(list)
    for test in failed_tests:
        failed_tests_by_name[test.name].append(test)
    failed_tests_by_name_count = {name: len(tests) for name, tests in failed_tests_by_name.items()}
    print(" ".join(sorted(failed_tests_by_name.keys(), key=int)))

    failed_tests_by_platform = defaultdict(list)
    for test in failed_tests:
        failed_tests_by_platform[test.platform].append(test)
    print()
    for platform in sorted(failed_tests_by_platform):
        print(f"{platform}:", " ".join(map(lambda t: t.name, failed_tests_by_platform[platform])))

    print("\nMost failed tests:")
    failed_tests_by_name_sorted = sorted(
        failed_tests_by_name_count.items(),
        key=lambda t: t[1],
        reverse=True,
    )
    print(f"{'Test':>5}  {'Count':>5}")
    for name, count in failed_tests_by_name_sorted[:10]:
        print(f"{name:>5}  {count:>5}")

    print(f"\n::error::{len(failed_tests_by_name)} test failures ({len(failed_tests)} unique)")


def write_allure_result(test: Test, commit: str, allure_results_path: str):
    allure_result = {
        "name": f"QA {test.name}",
        "fullName": f"QA #{test.name} on {test.platform}",
        "historyId": f"{test.name}@{test.platform}",
        "description": test.description,
        "status": test.status,
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
                "url": f"https://github.com/performancecopilot/pcp/blob/{commit}/qa/{test.name}",
                "name": "Source",
            },
            {
                "type": "test_case",
                "url": f"https://github.com/performancecopilot/pcp/blob/{commit}/qa/{test.name}.out",
                "name": "Expected output",
            },
        ],
    }

    for group in test.groups:
        allure_result["labels"].append({"name": "epic", "value": group})

    if test.status in [Test.Status.Failed, Test.Status.Broken]:
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


def send_slack_notification(
    slack_channel: str,
    github_run_url: str,
    qa_report_url: str,
    platforms: List[str],
    tests: List[Test],
):
    platform_stats = defaultdict(lambda: {"passed": 0, "failed": 0, "skipped": 0, "cancelled": 0})
    for test in tests:
        if test.status == Test.Status.Passed:
            platform_stats[test.platform]["passed"] += 1
        elif test.status == Test.Status.Skipped:
            platform_stats[test.platform]["skipped"] += 1
        elif test.status == Test.Status.Broken and test.message == "test cancelled":
            platform_stats[test.platform]["cancelled"] += 1
        elif test.status in [Test.Status.Failed, Test.Status.Broken]:
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
                            "text": f"*<{github_run_url}|View build logs>*\n*<{qa_report_url}|View QA report>*",
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
    parser.add_argument("--commit")
    parser.add_argument("--summary", action="store_true")
    parser.add_argument("--csv", type=argparse.FileType("w"))
    parser.add_argument("--allure-results", dest="allure_results")
    parser.add_argument("--slack-channel", dest="slack_channel")  # required only for slack message
    parser.add_argument("--github-run-url", dest="github_run_url")  # required only for slack message
    parser.add_argument("--qa-report-url", dest="qa_report_url")  # required only for slack message
    args = parser.parse_args()

    platforms, tests = read_platforms(args.qa, args.artifacts)

    if args.summary:
        print_test_summary(tests)

    if args.csv:
        print_test_report_csv(tests, args.csv)

    if args.allure_results:
        if not args.commit:
            print("Please specify a commit hash when generating an allure report.", file=sys.stderr)
            sys.exit(1)

        os.makedirs(args.allure_results, exist_ok=True)
        for test in tests:
            write_allure_result(test, args.commit, args.allure_results)

    if args.slack_channel:
        print()
        send_slack_notification(args.slack_channel, args.github_run_url, args.qa_report_url, platforms, tests)


if __name__ == "__main__":
    main()
