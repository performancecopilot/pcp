#!/usr/bin/env pmpython
# -*- coding: utf-8 -*-

# Exercises gauge metric type

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
    "-0.1",
    "-0.01",
    "-0.001",
    "-0.0001",
    "-0.00001"
]

basis_parser_config = utils.configs["parser_type"][0]
ragel_parser_config = utils.configs["parser_type"][1]

testconfigs = [basis_parser_config, ragel_parser_config]

def run_test():
    for testconfig in testconfigs:
        utils.print_test_section_separator()
        utils.pmdastatsd_install(testconfig)
        for payload in payloads:
            sock.sendto("test_gauge:{}|g".format(payload).encode("utf-8"), (ip, port))
        utils.print_metric("statsd.pmda.dropped")
        utils.print_metric('statsd.test_gauge')
        overflow_payload = sys.float_info.max
        sock.sendto("test_gauge2:+{}|g".format(overflow_payload).encode("utf-8"), (ip, port))
        utils.print_metric("statsd.pmda.dropped")
        utils.print_metric("statsd.test_gauge2")
        sock.sendto("test_gauge2:+{}|g".format(overflow_payload).encode("utf-8"), (ip, port))
        utils.print_metric("statsd.pmda.dropped")
        utils.print_metric("statsd.test_gauge2")
        underflow_payload = sys.float_info.max * -1.0
        sock.sendto("test_gauge3:{}|g".format(underflow_payload).encode("utf-8"), (ip, port))
        utils.print_metric("statsd.pmda.dropped")
        utils.print_metric("statsd.test_gauge3")
        sock.sendto("test_gauge3:{}|g".format(underflow_payload).encode("utf-8"), (ip, port))
        utils.print_metric("statsd.pmda.dropped")
        utils.print_metric("statsd.test_gauge3")
        utils.pmdastatsd_remove()
        utils.restore_config()

run_test()


