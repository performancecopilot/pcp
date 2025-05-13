#!/usr/bin/env pmpython
#
# Copyright (C) 2025 Red Hat.
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

# pylint: disable=too-many-arguments,too-many-positional-arguments
# pylint: disable=global-statement

import logging
import time
import argparse
import os
import subprocess
import json
from pathlib import Path
from datetime import datetime, UTC

import cpmapi as api
from pcp import pmapi

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
    string = datetime.fromtimestamp(seconds, timezone.utc).isoformat()
    return string.replace('+00:00', 'Z')


def update_time_window(begin: str, end: str, json_path: str, no_op: bool):
    # Update the JSON file to use the archive start and end time
    begin = format_time(begin)
    end = format_time(end)

    # Open and load the json file into a dictionary
    try:
        with open(json_path, "r", encoding="utf-8") as file:
            data = json.load(file)
    except (json.JSONDecodeError, FileNotFoundError) as e:
        logging.error("Error reading JSON file: %s", e)
        return

    # Update the from and to fields
    try:
        data["time"]["from"] = begin
        data["time"]["to"] = end
    except KeyError:
        return # no update required

    if no_op:
        logging.info("Would update the JSON file: %s", json_path)
    else:
        # Write the modified json back into the json file
        with open(json_path, "w", encoding="utf-8") as file:
            json.dump(data, file, indent=4)

        logging.info("Successfully updated the JSON file: %s", json_path)


def setup_time_window(path: Path, i: int, count: int, json_path: str, no_op: bool):
    global minimum_start_time, maximum_finish_time

    archive_path = base_archive_path(path)
    archive_mod_time = os.path.getmtime(path)
    if archive_unchanged(archive_path, archive_mod_time, i, count):
        return

    try:
        ctx = pmapi.pmContext(api.PM_CONTEXT_ARCHIVE, archive_path)
        ctx.pmNewZone('UTC')
        label = ctx.pmGetArchiveLabel()
    except pmapi.pmErr:
        logging.info("Skipping archive %s (no context) [%d/%d]", archive_path, i, count)
        return # .meta exists, but not a PCP archive metadata file
    start = float(label.start)
    if minimum_start_time == 0.0 or start < minimum_start_time:
        logging.info("Updating start to %s from %s [%d/%d]",
                     format_time(start), archive_path, i, count)
        minimum_start_time = start

    finish = float(ctx.pmGetArchiveEnd())
    if finish > maximum_finish_time:
        logging.info("Updating finish to %s from %s [%d/%d]",
                     format_time(finish), archive_path, i, count)
        maximum_finish_time = finish

    # update the time window in the JSON file with archive start and end times
    update_time_window(minimum_start_time, maximum_finish_time, json_path, no_op)


def import_archive(path: Path, i: int, count: int, no_op: bool, import_timeout: int, port: str, time_zone: str):
    archive_path = base_archive_path(path)
    archive_mod_time = os.path.getmtime(path)
    if archive_unchanged(archive_path, archive_mod_time, i, count):
        return

    start_dt = datetime.now()
    try:
        # mainly for testing/debugging
        # if --noop option is set, skip the importing of the archives
        if no_op:
            logging.info("Would run pmseries --load %s... [%d/%d]", archive_path, i, count)
        else:
            # initialize pmseries command, append optional import options
            command_line = ["pmseries", "--load", archive_path]
            if port is not None:
                command_line.extend(["-p", port])
            if time_zone is not None:
                command_line.extend(["-Z", time_zone])
            logging.info("Importing archive %s... [%d/%d]", archive_path, i, count)
            subprocess.run(
                    command_line,
                    stdout=subprocess.PIPE,
                    stderr=subprocess.STDOUT,
                    check=True,
                    universal_newlines=True,
                    timeout=import_timeout,
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


def poll(archives_path: str, jsonfile_path: str, no_op: bool, import_timeout: int, port: str, time_zone: str):
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
            setup_time_window(path, i, len(archive_paths), jsonfile_path, no_op)
        for i, path in enumerate(archive_paths, start=1):
            import_archive(path, i, len(archive_paths), no_op, import_timeout, port, time_zone)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--archives", default="/archives")
    parser.add_argument("--jsonfile",
                        default="/usr/local/var/lib/grafana/dashboards/pcp-archive-analysis.json")
    parser.add_argument("--noop", action="store_true")
    parser.add_argument("--poll-interval", type=int, default=10)
    parser.add_argument("--import-timeout", type=int, default=600)
    parser.add_argument('-p', "--port", type=str)
    parser.add_argument('-Z', "--timezone", type=str)
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
            logging.info("Poll interval: %d", args.poll_interval)
            logging.info("Import timeout: %d", args.import_timeout)
            poll(args.archives, args.jsonfile, args.noop, args.import_timeout, args.port,
                 args.timezone)
            time.sleep(args.poll_interval)
    except KeyboardInterrupt:
        pass  # debugging

if __name__ == "__main__":
    main()
