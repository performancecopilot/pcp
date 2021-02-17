#!/usr/bin/env python3
import os
import argparse
import re
import json
import shutil
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
        self.host = ""
        self.platform = ""
        self.runner = ""
        self.path = ""
        self.description = ""
        self.start = 0
        self.stop = 0
        self.status = ""
        self.message = ""


def read_test_durations(timings_path: str) -> Dict[str, List[int]]:
    test_durations = {}
    with open(timings_path) as f:
        for line in f:
            no, start, stop = line.strip().split()
            test_durations[no] = [int(start), int(stop)]
    return test_durations


def read_test_groups(group_path: str) -> Dict[str, List[str]]:
    tests = {}
    with open(group_path) as f:
        for line in f:
            spl = line.rstrip().split()
            if len(spl) >= 2 and spl[0].isdigit():
                tests[spl[0]] = spl[1:]
    return tests


def read_testlog(qa_dir: str, testartifacts_dir: str, groups: Dict[str, List[str]],
                 test_durations: Dict[str, List[int]], host: str) -> List[Test]:
    tests: List[Test] = []
    testlog_path = os.path.join(testartifacts_dir, 'test.log')
    with open(testlog_path) as testlog_file:
        for line in testlog_file:
            # [xx%] will be displayed if there are more than 9 tests
            #  Xs ... will be displayed if test was already run
            success_m = re.match(r'^(?:\[\d+%\] )?(\d+)(?: \d+s \.\.\.)?\n$', line)
            notrun_m = re.match(r'^(?:\[\d+%\] )?(\d+)(?: \d+s \.\.\.)? \[not run\] (.+)\n$', line)
            failed_m = re.match(r'^(?:\[\d+%\] )?(\d+)(?: \d+s \.\.\.)? ((\- |\[).+)\n$', line)
            cancelled_m = re.match(r'^(?:\[\d+%\] )?(\d+)(?: \d+s \.\.\.)?\Z', line)

            if success_m or notrun_m or failed_m or cancelled_m:
                test_no = (success_m or notrun_m or failed_m or cancelled_m).group(1)
                platform, runner = host.split('-')

                test = Test(test_no)
                test.groups = groups[test_no]
                test.host = host
                test.platform = platform
                test.runner = runner
                test.path = testartifacts_dir
                test.start, test.stop = test_durations[test_no]
                test.description = ""
                with open(os.path.join(qa_dir, test_no)) as f:
                    next(f)  # skip shebang
                    for test_line in f:
                        if test_line == '\n':
                            break
                        test.description += test_line.strip(' #') + '\n'
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

                test.status = Test.Status.Broken if 'failed' in failed_msg else Test.Status.Failed
                test.message = failed_msg + "\n"
            elif cancelled_m:
                test.status = Test.Status.Broken
                test.message = "test cancelled"
            elif tests and tests[-1].status in [Test.Status.Failed, Test.Status.Broken]:
                # if this line doesn't match any regex, it's probably output from the previous test
                tests[-1].message += line
    return tests


def read_hosts(qa_dir: str, artifacts_path: str) -> List[Test]:
    groups = read_test_groups(os.path.join(qa_dir, 'group'))
    hosts = set()
    tests = []
    for artifact_dir in os.listdir(artifacts_path):
        if artifact_dir.startswith('build-'):
            host = artifact_dir[len('build-'):]
            hosts.add(host)
        elif artifact_dir.startswith('test-'):
            host = artifact_dir[len('test-'):]
            hosts.add(host)

            testartifacts_dir = os.path.join(artifacts_path, artifact_dir)
            test_timings_file = os.path.join(testartifacts_dir, 'check.timings')

            test_durations = read_test_durations(test_timings_file)
            tests.extend(read_testlog(qa_dir, testartifacts_dir, groups, test_durations, host))
    return list(hosts), tests


def write_allure_result(test: Test, commit: str, allure_results_path: str):
    allure_result = {
        "name": f"QA {test.name}",
        "fullName": f"QA #{test.name} on {test.host}",
        "historyId": f"{test.name}@{test.host}",
        "description": test.description,
        "status": test.status,
        "statusDetails": {
            "message": test.message,
            "flaky": "flakey" in test.groups
        },
        "attachments": [],
        "start": test.start * 1000,
        "stop": test.stop * 1000,
        "labels": [{
            "name": "suite",
            "value": test.host
        }, {
            "name": "host",
            "value": test.host
        }],
        "links": [{
            "type": "test_case",
            "url": f"https://github.com/performancecopilot/pcp/blob/{commit}/qa/{test.name}",
            "name": "Source"
        }, {
            "type": "test_case",
            "url": f"https://github.com/performancecopilot/pcp/blob/{commit}/qa/{test.name}.out",
            "name": "Expected output"
        }]
    }

    for group in test.groups:
        allure_result["labels"].append({"name": "epic", "value": group})

    if test.status in [Test.Status.Failed, Test.Status.Broken]:
        allure_result[
            "description"] += (f"**To reproduce this test in a container, please run:**\n\n"
                               f"    ./build/ci/run.py --runner {test.runner} --platform {test.platform} reproduce")

        out_bad = os.path.join(test.path, f"{test.name}.out.bad")
        if os.path.exists(out_bad):
            out_bad_attachment_fn = f"{test.host}-{test.name}.out.bad-attachment.txt"
            shutil.copyfile(out_bad, os.path.join(allure_results_path, out_bad_attachment_fn))
            allure_result["attachments"].append({
                "type": "text/plain",
                "name": f"{test.name}.out.bad",
                "source": out_bad_attachment_fn
            })

        out_full = os.path.join(test.path, f"{test.name}.full")
        if os.path.exists(out_full):
            out_full_attachment_fn = f"{test.host}-{test.name}.full-attachment.txt"
            shutil.copyfile(out_full, os.path.join(allure_results_path, out_full_attachment_fn))
            allure_result["attachments"].append({
                "type": "text/plain",
                "name": f"{test.name}.full",
                "source": out_full_attachment_fn
            })

    with open(os.path.join(allure_results_path, f"{test.host}-{test.name}-result.json"), 'w') as f:
        json.dump(allure_result, f)


def print_test_summary(tests: List[Test]):
    print("TEST SUMMARY")
    print()

    grouped_by_name = defaultdict(list)
    for test in tests:
        grouped_by_name[test.name].append(test)

    print("Most failed tests:")
    failed_tests = {name: len([t for t in tests if t.status in [Test.Status.Failed, Test.Status.Broken]])
                    for name, tests in grouped_by_name.items()}
    failed_tests_sorted = sorted([(name, count) for name, count in failed_tests.items() if count > 0],
                                 key=lambda t: t[1], reverse=True)
    print(f"{'Test':>5}  {'Count':>5}")
    for name, count in failed_tests_sorted[:10]:
        print(f"{name:>5}  {count:>5}")

    num_failed_tests = sum(failed_tests.values())
    if num_failed_tests:
        print(f"\n::error::{num_failed_tests} unique QA test failures.")


def send_slack_notification(slack_channel: str, github_run_url: str, qa_report_url: str, hosts: List[str],
                            tests: List[Test]):
    host_stats = defaultdict(lambda: {"passed": 0, "failed": 0, "skipped": 0, "cancelled": 0})
    for test in tests:
        if test.status == Test.Status.Passed:
            host_stats[test.host]["passed"] += 1
        elif test.status == Test.Status.Skipped:
            host_stats[test.host]["skipped"] += 1
        elif test.status == Test.Status.Broken and test.message == "test cancelled":
            host_stats[test.host]["cancelled"] += 1
        elif test.status in [Test.Status.Failed, Test.Status.Broken]:
            host_stats[test.host]["failed"] += 1

    host_texts = []
    for host in sorted(hosts):
        if host in host_stats:
            stats = host_stats[host]
            host_stats_text = f"Passed: {stats['passed']}, Failed: {stats['failed']}, Skipped: {stats['skipped']}"
            if stats['cancelled'] > 0:
                host_stats_text = f"CANCELLED ({host_stats_text})"

            if stats['cancelled'] > 0 or stats['passed'] < 1000:
                symbol = ":x:"
            elif stats['failed'] > 0:
                symbol = ":warning:"
            else:
                symbol = ":heavy_check_mark:"
        else:
            host_stats_text = "Build BROKEN"
            symbol = ":x:"
        host_texts.append(f"*{host}*:\n{symbol} {host_stats_text}")

    host_blocks = []
    # slack only allows max 10 fields inside one block
    for i in range(0, len(host_texts), 10):
        host_blocks.append({
            "type": "section",
            "fields": [{
                "type": "mrkdwn",
                "text": text
            } for text in host_texts[i:i + 10]]
        })

    payload = {
        "channel": slack_channel,
        "attachments": [{
            "blocks": [{
                "type": "section",
                "text": {
                    "type": "mrkdwn",
                    "text": "*Daily QA:*"
                }
            }] + host_blocks + [{
                "type": "section",
                "text": {
                    "type": "mrkdwn",
                    "text": f"*<{github_run_url}|View build logs>*\n*<{qa_report_url}|View QA report>*"
                }
            }]
        }]
    }

    print("Sending slack notification...")
    r = requests.post('https://slack.com/api/chat.postMessage', headers={
        "Authorization": f"Bearer {os.environ['SLACK_BOT_TOKEN']}"
    }, json=payload)
    print(r.text)
    r.raise_for_status()


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--qa', default='./qa')
    parser.add_argument('--artifacts', default='./artifacts')
    parser.add_argument('--commit', required=True)
    parser.add_argument('--allure-results', dest='allure_results')
    parser.add_argument('--slack-channel', dest='slack_channel')
    parser.add_argument('--github-run-url', dest='github_run_url')
    parser.add_argument('--qa-report-url', dest='qa_report_url')
    args = parser.parse_args()

    hosts, tests = read_hosts(args.qa, args.artifacts)
    print_test_summary(tests)

    if args.allure_results:
        os.makedirs(args.allure_results, exist_ok=True)
        for test in tests:
            write_allure_result(test, args.commit, args.allure_results)

    if args.slack_channel:
        print()
        send_slack_notification(args.slack_channel, args.github_run_url, args.qa_report_url, hosts, tests)


if __name__ == '__main__':
    main()
