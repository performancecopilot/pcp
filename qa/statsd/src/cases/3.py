#!/usr/bin/env pmpython
# -*- coding: utf-8 -*-

# Exercises default values of hardcoded metrics, all should equal 0

import sys 
import glob
import os

utils_path = os.path.abspath(os.path.join("utils"))
sys.path.append(utils_path)

import pmdastatsd_test_utils as utils

utils.print_test_file_separator()
print(os.path.basename(__file__))

def run_test():
    utils.print_test_section_separator()
    utils.pmdastatsd_install()
    utils.print_metric('statsd.pmda.received')
    utils.print_metric('statsd.pmda.parsed')
    utils.print_metric('statsd.pmda.aggregated')
    utils.print_metric('statsd.pmda.dropped')
    utils.print_metric('statsd.pmda.time_spent_parsing')
    utils.print_metric('statsd.pmda.time_spent_aggregating')
    utils.print_metric('statsd.pmda.metrics_tracked')
    utils.pmdastatsd_remove()

run_test()
