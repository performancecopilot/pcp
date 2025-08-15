#!/usr/bin/env pmpython
# -*- coding: utf-8 -*-

# Exercises hdr histogram aggregation on duration metrics
# Since agent works with UDP datagrams, we have to take into account the fact that not all payloads will get processed and will get lost.
# Following test assumes that at least 10% of datagrams gets processed and measued values are within 35% +/- of expected values

import sys
import socket
import glob
import os

utils_path = os.path.abspath(os.path.join("utils"))
sys.path.append(utils_path)

import pmdastatsd_test_utils as utils

utils.print_test_file_separator()
print(os.path.basename(__file__))

ip = "0.0.0.0"
port = 8125
sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

expected_min = 0 
expected_max = 20000000
expected_count_max = 10000000 # since we use UDP not all datagrams are expected to be processed
expected_count_min = 10000000 * 0.1 # assume that at least 10 % of all send datagrams gets processed
expected_average = 10000000
expected_median = 10000000
expected_percentile90 = 18000000
expected_percentile95 = 19000000
expected_percentile99 = 19800000
expected_stddev = 5773500.278068

hdr_duration_aggregation = utils.configs["duration_aggregation_type"][1]

def run_test():
    utils.print_test_section_separator()
    utils.pmdastatsd_install(hdr_duration_aggregation)
    for i in range(0, expected_count_max):
        sock.sendto("test_hdr:{}|ms".format(i * 2).encode("utf-8"), (ip, port))
    labels_output = utils.request_metric("statsd.test_hdr")
    output = utils.get_instances(labels_output)
    for k, v in output.items():
        status = False
        number_value = float(v)
        sys.stderr.write(k + ' = ' + str(number_value) + '\n')
        if k == "/average":
            if utils.check_is_in_bounds(expected_average, number_value):
                status = True
        elif k == "/count":
            if utils.check_is_in_range(expected_count_max, expected_count_min, number_value):
                status = True
        elif k == "/max":
            if utils.check_is_in_bounds(expected_max, number_value):
                status = True
        elif k == "/median":
            if utils.check_is_in_bounds(expected_median, number_value, 0.42):
                status = True
        elif k == "/min":
            if utils.check_is_in_bounds(expected_min, number_value):
                status = True
        elif k == "/percentile90":
            if utils.check_is_in_bounds(expected_percentile90, number_value):
                status = True
        elif k == "/percentile95":
            if utils.check_is_in_bounds(expected_percentile95, number_value):
                status = True
        elif k == "/percentile99":
            if utils.check_is_in_bounds(expected_percentile99, number_value):
                status = True
        elif k == "/std_deviation":
            if utils.check_is_in_bounds(expected_stddev, number_value):
                status = True
        if status:
            print(k, "OK")
        else:
            print(k, v)
    utils.pmdastatsd_remove()
    utils.restore_config()

run_test()
