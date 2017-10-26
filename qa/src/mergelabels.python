#!/usr/bin/env pmpython
#
# Copyright (C) 2017 Ronak Jain.
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
#

import sys
import cpmapi
import argparse
from pcp import pmapi

parser = argparse.ArgumentParser(description='Python test for mergeLabels')
parser.add_argument('labels', metavar='labels', type=str, nargs='+',
                    help='labels for merging')
args = parser.parse_args()

try:
	label = pmapi.pmContext.pmMergeLabels(args.labels)
	print(label)
except Exception as e:
	print(e)
	sys.exit(1)