#!/usr/bin/env pmpython
# -*- coding: utf-8 -*-

# Exercises installation and removal of pmdastatsd

import sys 
import glob
import os

utils_path = os.path.abspath(os.path.join("utils"))
sys.path.append(utils_path)

import pmdastatsd_test_utils as utils

utils.print_test_file_separator()
print(os.path.basename(__file__))

def run_test():
    install_output = utils.pmdastatsd_install()
    print(install_output)
    remove_output = utils.pmdastatsd_remove()
    print(remove_output)

run_test()


