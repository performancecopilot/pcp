#!/usr/bin/env python3
import logging
import time
import argparse
import os
import sys
import subprocess
import cpmapi as api
from pcp import pmapi
from pathlib import Path
from datetime import datetime, UTC

POLL_INTERVAL_SEC = 10
IMPORT_TIMEOUT_SEC = 10 * 60  # 10 minutes

imported_archives = {}
minimum_start_time = 0.0
maximum_finish_time = 0.0


def format_time(seconds):
    # From a floating point number of seconds since the epoch,
    # produce a time string in the format Grafana is expecting.
    string = datetime.fromtimestamp(seconds, UTC).isoformat()
    return string.replace('+00:00', 'Z')


def import_archive(path: Path, i: int, count: int):
    global imported_archives, minimum_start_time, maximum_finish_time

    archive_mod_time = os.path.getmtime(path)
    archive_path = str(path.with_suffix(""))  # strip .meta from path

    prev_mod_time = imported_archives.get(archive_path)
    if prev_mod_time and prev_mod_time == archive_mod_time:
        logging.info("Skipping archive %s (no changes) [%d/%d]", archive_path, i, count)
        return

    ctx = pmapi.pmContext(api.PM_CONTEXT_ARCHIVE, archive_path)
    ctx.pmNewZone('UTC')

    try:
        label = ctx.pmGetHighResArchiveLabel()
    except pmapi.pmErr:
        logging.info("Skipping archive %s (no context) [%d/%d]", archive_path, i, count)
        return # .meta exists, but not a PCP archive metadata file
    start = float(label.start)
    if minimum_start_time == 0.0 or start < minimum_start_time:
        minimum_start_time = start
        logging.info("Updated start: %s" % format_time(minimum_start_time))

    finish = float(ctx.pmGetHighResArchiveEnd())
    if finish > maximum_finish_time:
        maximum_finish_time = finish
        logging.info("Updated finish: %s" % format_time(maximum_finish_time))

    start_dt = datetime.now()
    try:
        logging.info("Importing archive %s... [%d/%d]", archive_path, i, count)
        subprocess.run(
            ["pmseries", "--load", archive_path],
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            check=True,
            text=True,
            timeout=IMPORT_TIMEOUT_SEC,
        )
    except subprocess.CalledProcessError as e:
        logging.error("Error: %s", e.stdout)
    except subprocess.TimeoutExpired:
        logging.error("Timeout")
    else:
        total_sec = (datetime.now() - start_dt).total_seconds()
        minutes, seconds = divmod(total_sec, 60)
        logging.info("Successfully imported archive in %d:%dm.", minutes, seconds)
        imported_archives[archive_path] = archive_mod_time


def poll(archives_path: str):
    logging.info("Searching for new or updated archives...")

    if not os.access(archives_path, os.R_OK):
        logging.error("Cannot read directory: %s", archives_path)
        logging.error("Please use '--security-opt label=disable' when starting the container.")
        return

    archive_paths = list(Path(archives_path).rglob("*.meta"))
    if not archive_paths:
        logging.warning("No archives found.")
    else:
        for i, path in enumerate(archive_paths, start=1):
            import_archive(path, i, len(archive_paths))


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--archives", default="/archives")
    args = parser.parse_args()
    dash = 'http://localhost:3000/d/pcp-archive-analysis/pcp-archive-analysis'
    logging.basicConfig(level=logging.INFO, format="%(message)s")
    logging.info("Starting Performance Co-Pilot archive import...")
    logging.info("Dashboard: %s (when using default instructions)" % dash)

    # ensure archive timestamps for Grafana handled in UTC
    os.environ['TZ'] = 'UTC'
    time.tzset()

    try:
        while True:
            poll(args.archives)
            time.sleep(POLL_INTERVAL_SEC)
    except KeyboardInterrupt:
        pass  # debugging

if __name__ == "__main__":
    main()
