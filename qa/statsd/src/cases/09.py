#!/usr/bin/env pmpython
# -*- coding: utf-8 -*-

# Exercises hdr histogram aggregation on duration metrics

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
expected_count = 10000000
expected_average = 10000000
expected_median = 10000000
expected_percentile90 = 18000000
expected_percentile95 = 19000000
expected_percentile99 = 19800000
expected_stddev = 5773500.278068

# so that we can guarantee same duration values which may vary when histogram aggregation is used - this is not the point of this test
hdr_duration_aggregation = os.path.join("configs", "single", "duration_aggregation_type", "1", "pmdastatsd.ini")

def run_test():
    utils.print_test_section_separator()
    utils.pmdastatsd_install(hdr_duration_aggregation)
    for i in range(0, 10000001):
        sock.sendto("test_hdr:{}|ms".format(i * 2).encode("utf-8"), (ip, port))
    labels_output = utils.request_metric("statsd.test_hdr")
    output = utils.get_instances(labels_output)
    for k, v in output.items():
        status = False
        number_value = float(v)
        if k == "/average":
            if utils.check_is_in_bounds(expected_average, number_value):
                status = True
        elif k == "/count":
            # TODO: Ask Nathan, if this is OK
            if utils.check_is_in_bounds(expected_count, number_value, 0.5):
                status = True
        elif k == "/max":
            if utils.check_is_in_bounds(expected_max, number_value):
                status = True
        elif k == "/median":
            if utils.check_is_in_bounds(expected_median, number_value):
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
