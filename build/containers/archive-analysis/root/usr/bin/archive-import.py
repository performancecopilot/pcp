#!/usr/bin/env python3
import logging
import time
import argparse
import os
import sys
from datetime import datetime
import subprocess
from pathlib import Path

POLL_INTERVAL_SEC = 10
IMPORT_TIMEOUT_SEC = 10 * 60  # 10 minutes
imported_archives = {}


def import_archive(path: Path, i: int, count: int):
    archive_mod_time = os.path.getmtime(path)
    archive_path = str(path.with_suffix(""))  # strip .meta from path

    prev_mod_time = imported_archives.get(archive_path)
    if prev_mod_time and prev_mod_time == archive_mod_time:
        logging.info("Skipping archive %s (no changes) [%d/%d]", archive_path, i, count)
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

    logging.basicConfig(level=logging.INFO, format="%(message)s")
    logging.info("Starting Performance Co-Pilot archive import...")
    logging.info("Dashboard: http://localhost:3000/d/pcp-archive-analysis/pcp-archive-analysis (when using default instructions)")

    while True:
        poll(args.archives)
        time.sleep(POLL_INTERVAL_SEC)


if __name__ == "__main__":
    main()
