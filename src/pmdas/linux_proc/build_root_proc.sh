#!/bin/bash

# Create a new root_proc file that contains hotproc metrics as well
#
# 1. First awk copies metrics that we want
# 2. Sed to change the cluster
# 3. Second awk adds hotproc to root
# 4. Remove the undef that ends up in the middle
# 5. Merge everything together (assumes you have a sane bash)

awk < root_proc.in -f create_hot_pmns.awk \
	| sed -e 's/^proc/hotproc/g' \
	      -e 's/PROC:8:/PROC:52:/g' \
	      -e 's/PROC:9:/PROC:53:/g' \
	      -e 's/PROC:11:/PROC:54:/g' \
	      -e 's/PROC:12:/PROC:55:/g' \
	      -e 's/PROC:24:/PROC:56:/g' \
	      -e 's/PROC:31:/PROC:57:/g' \
	      -e 's/PROC:32:/PROC:58:/g' \
	      -e 's/PROC:51:/PROC:59:/g' \
	| cat <(awk < root_proc.in -f insert_into_root_pmns.awk | sed '/#undef PROC/d') - root_hotproc.in \
	> root_proc
