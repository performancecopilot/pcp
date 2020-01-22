#!/usr/bin/env pmpython
# -*- coding: utf-8 -*-

# Exercises agent to make sure that metrics don't randomly disappear and that PMNS is stable

import sys
import socket
import glob
import os
from random import uniform
from threading import Thread

utils_path = os.path.abspath(os.path.join("utils"))
sys.path.append(utils_path)

import pmdastatsd_test_utils as utils

utils.print_test_file_separator()
print(os.path.basename(__file__))

ip = "0.0.0.0"
port = 8125
sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
n = 1000000

def run_test():
    utils.print_test_section_separator()
    utils.pmdastatsd_install()
    pminfo_metrics = utils.pminfo("statsd") 
    before_metric_count = len(pminfo_metrics.split('\n')) - 1
    def thread_fun1():
        for i in range(1, n + 1):
            sock.sendto("test_thread1_counter:1|c".encode("utf-8"), (ip, port))
    def thread_fun2():
        for i in range(1, n + 1):
            sock.sendto("test_thread2_counter:2|c".encode("utf-8"), (ip, port))
    def thread_fun3():
        for i in range(1, n + 1):
            sock.sendto("test_thread3_gauge:+{}|g".format(uniform(1.0, 100.0)).encode("utf-8"), (ip, port))
    def thread_fun4():
        for i in range(1, n + 1):
            sock.sendto("test_thread4_ms:{}|ms".format(uniform(0, 1)).encode("utf-8"), (ip, port))
    thread1 = Thread(target=thread_fun1)
    thread2 = Thread(target=thread_fun2)
    thread3 = Thread(target=thread_fun3)
    thread4 = Thread(target=thread_fun4)

    thread1.start()
    thread2.start()
    thread3.start()
    thread4.start()

    thread1.join()
    thread2.join()
    thread3.join()
    thread4.join()

    after_result = utils.pminfo("statsd")
    after_metric_tracked_count = len(after_result.split('\n')) - 1
    if before_metric_count + 4 == after_metric_tracked_count:
        print("OK. +4 metrics, none lost.")
    else:
        print("FAIL, some metrics were lost / none were added.")
    
    utils.pmdastatsd_remove()
    utils.restore_config()

run_test()
