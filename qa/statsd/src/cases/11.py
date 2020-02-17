#!/usr/bin/env pmpython
# -*- coding: utf-8 -*-

# Exercises ability of the agent to listen on configured port

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
port_a = 8125
port_b = 8126
sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

port_a_config = utils.configs["port"][0]
port_b_config = utils.configs["port"][1]

def run_test():
    utils.print_test_section_separator()
    utils.pmdastatsd_install(port_a_config)
    sock.sendto("test_port_a:1|c".encode("utf-8"), (ip, port_a))
    print("Testing port {}".format(port_a))
    utils.print_metric("statsd.test_port_a")
    utils.pmdastatsd_remove()
    utils.restore_config()
    utils.pmdastatsd_install(port_b_config)
    sock.sendto("test_port_b:1|c".encode("utf-8"), (ip, port_b))
    print("Testing port {}".format(port_b))
    utils.print_metric("statsd.test_port_b")
    utils.pmdastatsd_remove()
    utils.restore_config()

run_test()
