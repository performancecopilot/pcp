#!/bin/sh
# SMART drive PMDA test helper - invoke as, e.g.:
# $ cd 001/smart
# $ ../../lsblk.sh

ls *.info 2>/dev/null | LC_COLLATE=POSIX sort | sed -e 's/.info//g'
