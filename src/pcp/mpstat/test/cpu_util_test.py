#!/usr/bin/env pmpython
#
# Copyright (C) 2016 Sitaram Shelke.
#
# This program is free software; you can redistribute it and/or modify it
# under the terms of the GNU General Public License as published by the
# Free Software Foundation; either version 2 of the License, or (at your
# option) any later version.
#
# This program is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
# or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
# for more details.
#

import sys
import unittest
if sys.version_info[0] < 3:
    from mock import Mock
else:
    from unittest.mock import Mock
from pcp_mpstat import CpuUtil

class TestCpuUtil(unittest.TestCase):

    def current_values_side_effect(self, metric):
        if metric == 'hinv.map.cpu_num':
            return {1: 0, 2: 1}

    def test_get_percpu_util(self):
        metric_repository = Mock()
        cpu_util = CpuUtil(1.34, metric_repository)
        metric_repository.current_values = Mock(side_effect=self.current_values_side_effect)

        cpu_list = cpu_util.get_percpu_util()

        self.assertEqual(len(cpu_list),2)

    def test_get_totalcpu_util(self):
        metric_repository = Mock()
        cpu_util = CpuUtil(1.34, metric_repository)
        metric_repository.current_values = Mock(side_effect=self.current_values_side_effect)

        cpu_util = cpu_util.get_totalcpu_util()

        self.assertIsNotNone(cpu_util)

if __name__ == '__main__':
    unittest.main()
