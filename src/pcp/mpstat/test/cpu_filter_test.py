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
from pcp_mpstat import CpuFilter

class TestCpuFilter(unittest.TestCase):
    def setUp(self):
        self.mpstat_options = Mock()
        self.mpstat_options.cpu_list = None
        self.cpus = [Mock(
                        cpu_number = Mock(return_value = 0),
                        cpu_online = Mock(return_value = 1)),
                    Mock(
                            cpu_number = Mock(return_value = 1),
                            cpu_online = Mock(return_value = 1)),
                    Mock(
                            cpu_number = Mock(return_value = 2),
                            cpu_online = Mock(return_value = 0)),
                    ]

    def test_filter_cpus_all(self):
        self.mpstat_options.cpu_list = "ALL"
        cpu_filter = CpuFilter(self.mpstat_options)

        cpu_list = cpu_filter.filter_cpus(self.cpus)

        self.assertEqual(len(cpu_list), 3)

    def test_filter_cpus_on(self):
        self.mpstat_options.cpu_list = "ON"
        cpu_filter = CpuFilter(self.mpstat_options)

        cpu_list = cpu_filter.filter_cpus(self.cpus)

        self.assertEqual(len(cpu_list), 2)

    def test_filter_cpus_list(self):
        self.mpstat_options.cpu_list = [1]
        cpu_filter = CpuFilter(self.mpstat_options)

        cpu_list = cpu_filter.filter_cpus(self.cpus)

        self.assertEqual(len(cpu_list), 1)

if __name__ == '__main__':
    unittest.main()
