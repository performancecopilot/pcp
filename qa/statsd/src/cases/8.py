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
basic_duration_aggregation = os.path.join("configs", "single", "duration_aggregation_type", "0", "pmdastatsd.ini")

def run_test():
    utils.print_test_section_separator()
    utils.pmdastatsd_install(basic_duration_aggregation)
    for payload in payloads:
        sock.sendto(payload.encode("utf-8"), (ip, port))
    for x in range(1, 101):
        sock.sendto("test_labels2:{}|ms".format(x).encode("utf-8"), (ip, port))
    for x in range(1, 101):
        sock.sendto("test_labels2:{}|ms|#label:X".format(x * 2).encode("utf-8"), (ip, port))

    # TODO: maybe create abstractions for instance related operations in tests? 
    labels_result = utils.request_metric("statsd.test_labels")
    expected_number_of_instances = 7
    if labels_result.count("inst [") != expected_number_of_instances:
        print("Unexpected number of instances.")
    else:
        print("Number of instances in result: OK")
    # check if we have all instances we wanted
    if labels_result.find('"/"] value 0') != - 1:
        print("Root instance OK")
    else:
        print("Root instance FAILED")

    if labels_result.find('"/tagX=X::tagY=Y"] value 2') != -1:
        print("/tagX=X::tagY=Y instance OK")
    else:
        print("/tagX=X::tagY=Y instance FAILED")

    if labels_result.find('"/tagA=A::tagB=B::tagC=C"] value 3') != -1:
        print("/tagA=A::tagB=B::tagC=C instance OK")
    else:
        print("/tagA=A::tagB=B::tagC=C instance FAILED")

    if labels_result.find('"/A=A"] value 4') != -1:
        print("/A=A instance OK")
    else:
        print("/A=A instance FAILED")

    if labels_result.find('"/A=A::B=B::C=C"] value 5') != 1:
        print("/A=A::B=B::C=C instance OK")
    else:
        print("/A=A::B=B::C=C instance FAILED")

    if labels_result.find('"/tagX=X"] value 1') != -1:
        print("/tagX=X instance OK")
    else:
        print("/tagX=X instance FAILED")

    if labels_result.find('"/A=10"] value 6') != -1:
        print("/A=10 instance OK")
    else:
        print("/A=10 instance FAILED")

    labels_result = utils.request_metric("statsd.test_labels2")
    expected_number_of_instances = 18
    if labels_result.count("inst [") != expected_number_of_instances:
        print("Unexpected number of instances.")
    else:
        print("Number of instances in result: OK")

    # check if we have all instances we wanted
    if labels_result.find('"/min::label=X"] value 2') != - 1:
        print("/min::label=X instance OK")
    else:
        print("/min::label=X instance FAILED")

    if labels_result.find('"/max::label=X"] value 200') != -1:
        print("/max::label=X instance OK")
    else:
        print("/max::label=X instance FAILED")

    if labels_result.find('"/median::label=X"] value 100') != -1:
        print("/median::label=X instance OK")
    else:
        print("/median::label=X instance FAILED")

    if labels_result.find('"/average::label=X"] value 101') != -1:
        print("/average::label=X instance OK")
    else:
        print("/average::label=X instance FAILED")

    if labels_result.find('"/percentile90::label=X"] value 180') != 1:
        print("/percentile90::label=X instance OK")
    else:
        print("/percentile90::label=X instance FAILED")

    if labels_result.find('"/percentile95::label=X"] value 190') != -1:
        print("/percentile95::label=X instance OK")
    else:
        print("/percentile95::label=X instance FAILED")

    if labels_result.find('"/percentile99::label=X"] value 198') != -1:
        print("/percentile99::label=X instance OK")
    else:
        print("/percentile99::label=X instance FAILED")

    if labels_result.find('"/count::label=X"] value 10') != -1:
        print("/count::label=X instance OK")
    else:
        print("/count::label=X instance FAILED")

    if labels_result.find('"/std_deviation::label=X"] value 57.73214009544424') != -1:
        print("/std_deviation::label=X instance OK")
    else:
        print("/std_deviation::label=X instance FAILED")

    utils.pmdastatsd_remove()
    utils.restore_config()

run_test()
