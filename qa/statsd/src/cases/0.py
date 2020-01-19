#!/usr/bin/env pmpython
# -*- coding: utf-8 -*-

# Exercises setting all configuration options via ini files (NOT the settings themselves) and fetching them via pminfo

import sys 
import glob
import os

utils_path = os.path.abspath(os.path.join("utils"))
sys.path.append(utils_path)

import pmdastatsd_test_utils as utils

configs_dir = os.path.join("configs")

print(os.path.basename(__file__))

def exercise_config(config_file):
    utils.print_test_section_separator()
    utils.pmdastatsd_install(config_file)
    utils.print_config_metrics()
    utils.pmdastatsd_remove()
    utils.restore_config()

def run_test():
    for root, dirs, files in os.walk(configs_dir):
        for file in files:
            if file.endswith('.ini'):
                config_file = os.path.join(root, file)
                exercise_config(config_file)

run_test()


