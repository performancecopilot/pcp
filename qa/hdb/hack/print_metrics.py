#!/usr/bin/env pmpython
""" Generate a markdown table listing all supported metrics.
Use this to update the README when adding new metrics.
"""
import os
import sys

sys.path.insert(1, os.path.join(sys.path[0], ".."))
import pmdahdb  # noqa: E402

if __name__ == "__main__":
    hdb_host = os.getenv("HDB_HOST")
    if not hdb_host:
        raise RuntimeError(
            "hdb host not specified. Set environment variable 'HDB_HOST'",
        )
    hdb_port = os.getenv("HDB_PORT")
    if not hdb_port:
        raise RuntimeError(
            "hdb port not specified. Set environment variable 'HDB_PORT'",
        )
    hdb_user = os.getenv("HDB_USER")
    if not hdb_user:
        raise RuntimeError(
            "hdb user not specified. Set environment variable 'HDB_USER'",
        )
    hdb_password = os.getenv("HDB_PASSWORD")
    if not hdb_password:
        raise RuntimeError(
            "hdb password not specified. Set environment variable 'HDB_PASSWORD'",
        )
    hdb = pmdahdb.HDBConnection(hdb_host, int(hdb_port), hdb_user, hdb_password)
    pmda = pmdahdb.HdbPMDA(hdb)
    for m in sorted(pmda._metric_lookup.values(), key=lambda m: m.name):
        print(f"| {m.name} | {m.desc} |")
