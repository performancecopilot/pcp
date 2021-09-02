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

debug_output_filename1 = utils.configs["debug_output_filename"][0]
debug_output_filename2 = utils.configs["debug_output_filename"][1]

payloads = [
    "test_labels:0|c", # no label
    "test_labels,tagX=X:1|c", # single label
    "test_labels,tagX=X,tagY=Y:2|c", # two labels
    "test_labels,tagC=C,tagB=B,tagA=A:3|c", # labels that will be ordered
    "test_labels:4|c|#A:A", # labels in dogstatsd-ruby format
    "test_labels,A=A:5|c|#B:B,C:C", # labels in dogstatsd-ruby format combined with standard format
    "test_labels,A=A,A=10:6|c", # labels with non-unique keys, right-most takes precedence
    "test_labels2:0|c", # no label
    "test_labels2,testX=X:1|c", # single label
    "test_labels2,testX=X,testY=Y:2|c", # two labels
    "test_labels2,testC=C,testB=B,testA=A:3|c", # labels that will be ordered
    "test_labels2:4|c|#testA:A", # labels in dogstatsd-ruby format
    "test_labels2,testA=A:5|c|#testB:B,testC:C", # labels in dogstatsd-ruby format combined with standard format
    "test_labels2,testA=A,testA=10:6|c" # labels with non-unique keys, right-most takes precedence
]

def run_test():
    utils.print_test_section_separator()
    utils.pmdastatsd_install(debug_output_filename1)
    for payload in payloads:
        sock.sendto(payload.encode("utf-8"), (ip, port))
    pmdastatsd_pids = utils.get_pmdastatsd_pids()
    time.sleep(5)
    for pid in pmdastatsd_pids:
        utils.send_debug_output_signal(pid)
    time.sleep(5)
    debug_output = utils.get_debug_file("debug")
    print(debug_output)
    utils.pmdastatsd_remove()
    utils.restore_config()
    utils.remove_debug_file("debug")

    utils.pmdastatsd_install(debug_output_filename2)
    for payload in payloads:
        sock.sendto(payload.encode("utf-8"), (ip, port))
    pmdastatsd_pids = utils.get_pmdastatsd_pids()
    time.sleep(5)
    for pid in pmdastatsd_pids:
        utils.send_debug_output_signal(pid)
    time.sleep(5)
    debug_output = utils.get_debug_file("debug_test")
    print(debug_output)
    utils.pmdastatsd_remove()
    utils.restore_config()
    utils.remove_debug_file("debug_test")

# TODO: make test output stable/deterministic before enabling
# run_test()


