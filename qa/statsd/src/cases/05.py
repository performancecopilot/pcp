#!/usr/bin/env pmpython
# -*- coding: utf-8 -*-

# Exercises counter metric type

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

payloads = [
    # thrown away
    "-1",
    "-1wqeqe",
    "-20weqe0",
    "-wqewqe20",
    # ok
    "0",
    "1",
    "10", 
    "100",
    "1000",
    "10000",
    "+1",
    "+10",
    "+100",
    "+1000",
    "+10000",
    "0.1",
    "0.01",
    "0.001",
    "0.0001",
    "0.00001"
]

basis_parser_config = utils.configs["parser_type"][0]
ragel_parser_config = utils.configs["parser_type"][1]

testconfigs = [basis_parser_config, ragel_parser_config]

def run_test():
    for testconfig in testconfigs:
        utils.print_test_section_separator()
        utils.pmdastatsd_install(testconfig)
        utils.print_metric("statsd.pmda.dropped")
        for payload in payloads:
            sock.sendto("test_counter:{}|c".format(payload).encode("utf-8"), (ip, port))
        utils.print_metric('statsd.test_counter')
        test_payload = sys.float_info.max
        sock.sendto("test_counter2:{}|c".format(test_payload).encode("utf-8"), (ip, port))
        utils.print_metric("statsd.test_counter2")
        utils.print_metric("statsd.pmda.dropped")
        sock.sendto("test_counter2:{}|c".format(test_payload).encode("utf-8"), (ip, port))
        utils.print_metric("statsd.test_counter2")
        utils.print_metric("statsd.pmda.dropped")
        utils.pmdastatsd_remove()
        utils.restore_config()

run_test()


