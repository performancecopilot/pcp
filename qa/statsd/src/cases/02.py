#!/usr/bin/env pmpython
# -*- coding: utf-8 -*-

# Exercises setting all configuration options via ini files (NOT the settings themselves) and fetching them via pminfo

import sys 
import glob
import os

utils_path = os.path.abspath(os.path.join("utils"))
sys.path.append(utils_path)

import pmdastatsd_test_utils as utils

configs = [
    utils.configs["default"],
    utils.configs["empty"],
    utils.configs["debug_output_filename"][0],
    utils.configs["debug_output_filename"][1],
    utils.configs["duration_aggregation_type"][0],
    utils.configs["duration_aggregation_type"][1],
    utils.configs["max_udp_packet_size"][0],
    utils.configs["max_udp_packet_size"][1],
    utils.configs["max_udp_packet_size"][2],
    utils.configs["max_unprocessed_packets"][0],
    utils.configs["max_unprocessed_packets"][1],
    utils.configs["parser_type"][0],
    utils.configs["parser_type"][1],
    utils.configs["verbose"][0],
    utils.configs["verbose"][1],
    utils.configs["verbose"][2]
]

utils.print_test_file_separator()
print(os.path.basename(__file__))

def exercise_config(config):
    utils.print_test_section_separator()
    utils.pmdastatsd_install(config)
    utils.print_config_metrics()
    utils.pmdastatsd_remove()
    utils.restore_config()

def run_test():
    for config in configs:
        exercise_config(config)

run_test()


