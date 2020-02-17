#!/usr/bin/env pmpython
# -*- coding: utf-8 -*-

# Exercises labels

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
    "test_labels:0|c", # no label
    "test_labels,tagX=X:1|c", # single label
    "test_labels,tagX=X,tagY=Y:2|c", # two labels
    "test_labels,tagC=C,tagB=B,tagA=A:3|c", # labels that will be ordered
    "test_labels:4|c|#A:A", # labels in dogstatsd-ruby format
    "test_labels,A=A:5|c|#B:B,C:C", # labels in dogstatsd-ruby format combined with standard format
    "test_labels,A=A,A=10:6|c" # labels with non-unique keys, right-most takes precedence
]

# so that we can guarantee same duration values which may vary when histogram aggregation is used - this is not the point of this test
basic_duration_aggregation = utils.configs["duration_aggregation_type"][0]

def run_test():
    utils.print_test_section_separator()
    utils.pmdastatsd_install(basic_duration_aggregation)
    for payload in payloads:
        sock.sendto(payload.encode("utf-8"), (ip, port))
    for x in range(1, 101):
        sock.sendto("test_labels2:{}|ms".format(x).encode("utf-8"), (ip, port))
    for x in range(1, 101):
        sock.sendto("test_labels2:{}|ms|#label:X".format(x * 2).encode("utf-8"), (ip, port))
    labels_output = utils.request_metric("statsd.test_labels")
    output = utils.get_instances(labels_output)
    for k, v in output.items():
        print(k, v)
    labels_output = utils.request_metric("statsd.test_labels2")
    output = utils.get_instances(labels_output)
    for k, v in output.items():
        print(k, v)
    utils.pmdastatsd_remove()
    utils.restore_config()

run_test()
