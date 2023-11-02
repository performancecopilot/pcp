#!/bin/sh
# SMART & farm drive PMDA test helper - invoke as, e.g.:
# $ cd 001/farm
# $ ../../lsblk.sh

ls *.farm 2>/dev/null | LC_COLLATE=POSIX sort | sed -e 's/.farm//g'
