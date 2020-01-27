#!/usr/bin/env pmpython
# -*- coding: utf-8 -*-

# Exercises debug_output_filename option

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

debug_output_filename1 = os.path.join("configs", "single", "debug_output_filename", "0", "pmdastatsd.ini")
debug_output_filename2 = os.path.join("configs", "single", "debug_output_filename", "1", "pmdastatsd.ini")

payloads = [
    "test_labels:0|c", # no label
    "test_labels,tagX=X:1|c", # single label
    "test_labels,tagX=X,tagY=Y:2|c", # two labels
    "test_labels,tagC=C,tagB=B,tagA=A:3|c", # labels that will be ordered
    "test_labels:4|c|#A:A", # labels in dogstatsd-ruby format
    "test_labels,A=A:5|c|#B:B,C:C", # labels in dogstatsd-ruby format combined with standard format
    "test_labels,A=A,A=10:6|c" # labels with non-unique keys, right-most takes precedence
    "test_labels2:0|c", # no label
    "test_labels2,tagX=X:1|c", # single label
    "test_labels2,tagX=X,tagY=Y:2|c", # two labels
    "test_labels2,tagC=C,tagB=B,tagA=A:3|c", # labels that will be ordered
    "test_labels2:4|c|#A:A", # labels in dogstatsd-ruby format
    "test_labels2,A=A:5|c|#B:B,C:C", # labels in dogstatsd-ruby format combined with standard format
    "test_labels2,A=A,A=10:6|c" # labels with non-unique keys, right-most takes precedence
]

def run_test():
    utils.print_test_section_separator()
    utils.pmdastatsd_install(debug_output_filename1)
    for payload in payloads:
        sock.sendto(payload.encode("utf-8"), (ip, port))
    time.sleep(3)
    pmdastatsd_pids = utils.get_pmdastatsd_pids()
    for pid in pmdastatsd_pids:
        utils.send_debug_output_signal(pid)
    time.sleep(3)
    debug_output = utils.get_debug_file("debug")
    print(debug_output)
    utils.pmdastatsd_remove()
    utils.restore_config()
    utils.remove_debug_file("debug")

    utils.pmdastatsd_install(debug_output_filename2)
    for payload in payloads:
        sock.sendto(payload.encode("utf-8"), (ip, port))
    time.sleep(3)
    pmdastatsd_pids = utils.get_pmdastatsd_pids()
    for pid in pmdastatsd_pids:
        utils.send_debug_output_signal(pid)
    time.sleep(3)
    debug_output = utils.get_debug_file("debug_test")
    print(debug_output)
    utils.pmdastatsd_remove()
    utils.restore_config()
    utils.remove_debug_file("debug_test")

run_test()


