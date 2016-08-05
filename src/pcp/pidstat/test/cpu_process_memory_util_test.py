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

from mock import Mock
import  mock
import unittest
from pcp_pidstat import CpuProcessMemoryUtil


class TestCpuProcessMemoryUtil(unittest.TestCase):

    def current_values_side_effect(self, metric):
        if metric == 'proc.psinfo.pid':
            return {1: 1, 2: 2, 5: 5, 10: 10}

    def test_get_processes(self):
        metric_repository = mock.Mock()
        cpu_process_memory_util = CpuProcessMemoryUtil(metric_repository)
        metric_repository.current_values = mock.Mock(side_effect=self.current_values_side_effect)

        processes_list = cpu_process_memory_util.get_processes(1.34)

        self.assertEquals(len(processes_list),4)

if __name__ == '__main__':
    unittest.main()
