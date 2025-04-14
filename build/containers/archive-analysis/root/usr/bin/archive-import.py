#!/usr/bin/env pmpython
#
# Copyright (C) 2025 Marko Myllynen <myllynen@redhat.com>
#
# This program is free software; you can redistribute it and/or modify it
# under the terms of the GNU General Public License as published by the
# Free Software Foundation; either version 2 of the License, or (at your
# option) any later version.
#
# This program is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
# or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
# for more details.

# pylint: disable=global-statement

import logging
import time
import argparse
import os
import subprocess
from pathlib import Path
from datetime import datetime, UTC

import cpmapi as api
from pcp import pmapi

POLL_INTERVAL_SEC = 10
IMPORT_TIMEOUT_SEC = 10 * 60  # 10 minutes

imported_archives = {}
minimum_start_time = 0.0
maximum_finish_time = 0.0


def base_archive_path(path: Path):
    # From a path, return canonical PMAPI archive context string
    # (i.e. removing optional compression and .meta suffixes)
    while len(path.suffixes) != 0:
        if '.meta' not in path.suffixes:  # stripped too far, bail out now
            break
        path = path.with_suffix("")  # strip .meta, optional .gz/.xz/... from path
    return str(path)


def archive_unchanged(archive_path: str, archive_time: float, i: int, count: int):
    # Test whether we have fully imported this archive already
    previous_time = imported_archives.get(archive_path)
    if previous_time and previous_time == archive_time:
        logging.info("Skipping archive %s (no changes) [%d/%d]", archive_path, i, count)
        return True
    return False


def format_time(seconds: float):
    # From a floating point number of seconds since the epoch,
    # produce a time string in the format Grafana is expecting.
    string = datetime.fromtimestamp(seconds, UTC).isoformat()
    return string.replace('+00:00', 'Z')


def setup_grafana(path: Path, i: int, count: int):
    global minimum_start_time, maximum_finish_time

    archive_path = base_archive_path(path)
    archive_mod_time = os.path.getmtime(path)
    if archive_unchanged(archive_path, archive_mod_time, i, count):
        return

    try:
        ctx = pmapi.pmContext(api.PM_CONTEXT_ARCHIVE, archive_path)
        ctx.pmNewZone('UTC')
        label = ctx.pmGetHighResArchiveLabel()
    except pmapi.pmErr:
        logging.info("Skipping archive %s (no context) [%d/%d]", archive_path, i, count)
        return # .meta exists, but not a PCP archive metadata file
    start = float(label.start)
    if minimum_start_time == 0.0 or start < minimum_start_time:
        logging.info("Updating start to %s from %s [%d/%d]",
                     format_time(minimum_start_time), archive_path, i, count)
        minimum_start_time = start

    finish = float(ctx.pmGetHighResArchiveEnd())
    if finish > maximum_finish_time:
        logging.info("Updating finish to %s from %s [%d/%d]",
                     format_time(maximum_finish_time), archive_path, i, count)
        maximum_finish_time = finish


def import_archive(path: Path, i: int, count: int):
    archive_path = base_archive_path(path)
    archive_mod_time = os.path.getmtime(path)
    if archive_unchanged(archive_path, archive_mod_time, i, count):
        return

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

    archive_paths = list(Path(archives_path).rglob("*.meta*"))
    if not archive_paths:
        logging.warning("No archives found.")
    else:
        # prepare the dashboard with an initial (quick) pass over all archives
        # because the import_archive process may be loading large data volumes
        for i, path in enumerate(archive_paths, start=1):
            setup_grafana(path, i, len(archive_paths))
        for i, path in enumerate(archive_paths, start=1):
            import_archive(path, i, len(archive_paths))


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--archives", default="/archives")
    args = parser.parse_args()
    dash = 'http://localhost:3000/d/pcp-archive-analysis/pcp-archive-analysis'
    logging.basicConfig(level=logging.INFO, format="%(message)s")
    logging.info("Starting Performance Co-Pilot archive import...")
    logging.info("Dashboard: %s (when using default instructions)", dash)

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
