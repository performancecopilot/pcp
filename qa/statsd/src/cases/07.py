#!/usr/bin/env pmpython
# -*- coding: utf-8 -*-

# Exercises duration metric type, basic aggregation

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

basic_duration_aggregation = utils.configs["duration_aggregation_type"][0]

def run_test():
    utils.print_test_section_separator()
    utils.pmdastatsd_install(basic_duration_aggregation)
    for x in range(1, 101):
        sock.sendto("test_duration:{}|ms".format(x).encode("utf-8"), (ip, port))
    utils.print_metric("statsd.test_duration")
    utils.pmdastatsd_remove()
    utils.restore_config()

run_test()
