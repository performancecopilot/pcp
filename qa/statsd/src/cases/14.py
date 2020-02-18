#!/usr/bin/env pmpython
# -*- coding: utf-8 -*-

# Exercises max_udp_packet_size_option option

import sys
import socket
import glob
import os
import time

utils_path = os.path.abspath(os.path.join("utils"))
sys.path.append(utils_path)

import pmdastatsd_test_utils as utils

utils.print_test_file_separator()
print(os.path.basename(__file__))

ip = "0.0.0.0"
port = 8125
sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

# 1472
maxudp_1 = utils.configs["max_udp_packet_size"][0]
# 2944
maxudp_2 = utils.configs["max_udp_packet_size"][1]
# 10
maxudp_3 = utils.configs["max_udp_packet_size"][2]

payloads = [
    "test_payload_" + ("x" * 1472) + ":0|c",
    "test_payload2_" + ("x" * 3000) + ":0|c",
    "test:1|g",
    "test_payload:2|ms",
]

def run_test():
    utils.print_test_section_separator()
    utils.pmdastatsd_install(maxudp_1)
    for payload in payloads:
        sock.sendto(payload.encode("utf-8"), (ip, port))
    time.sleep(1)
    utils.print_metric("statsd.pmda.metrics_tracked")
    utils.pmdastatsd_remove()
    utils.restore_config()
    utils.print_test_section_separator()
    utils.pmdastatsd_install(maxudp_2)
    for payload in payloads:
        sock.sendto(payload.encode("utf-8"), (ip, port))
    time.sleep(1)
    utils.print_metric("statsd.pmda.metrics_tracked")
    utils.pmdastatsd_remove()
    utils.restore_config()
    utils.print_test_section_separator()
    utils.pmdastatsd_install(maxudp_3)
    for payload in payloads:
        sock.sendto(payload.encode("utf-8"), (ip, port))
    time.sleep(1)
    utils.print_metric("statsd.pmda.metrics_tracked")
    utils.pmdastatsd_remove()
    utils.restore_config()

run_test()


