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
import unittest
from pcp_pidstat import ProcessFilter

class TestProcessFilter(unittest.TestCase):
    def setUp(self):
        self.options = Mock(process_name = None,
                        show_process_memory_util = False,
                        show_process_priority = False,
                        show_process_stack_util = False,
                        per_processor_usage = False,
                        show_process_user = False,
                        filtered_process_user = None,
                        pid_filter = None,
                        pid_list = [])

        self.process_1 = Mock(pid = Mock(return_value = 1),
                        process_name = Mock(return_value = "process_1"),
                        user_name = Mock(return_value='pcp'),
                        vsize = Mock(return_value=136),
                        priority = Mock(return_value=99),
                        stack_size = Mock(return_value=123),)
        self.process_2 = Mock(pid = Mock(return_value = 2),
                        process_name = Mock(return_value = "process_two"),
                        user_name = Mock(return_value='pcp1'),
                        vsize = Mock(return_value=136),
                        priority = Mock(return_value=0),
                        stack_size = Mock(return_value=0),)
        self.process_3 = Mock(pid = Mock(return_value = 3),
                        process_name = Mock(return_value = "proc_3"),
                        user_name = Mock(return_value='pcp1'),
                        vsize = Mock(return_value=0),
                        priority = Mock(return_value=99),
                        stack_size = Mock(return_value=0),)
        self.process_4 = Mock(pid = Mock(return_value = 4),
                        process_name = Mock(return_value = "a_short_process"),
                        user_name = Mock(return_value='pcp'),
                        vsize = Mock(return_value=0),
                        priority = Mock(return_value=0),
                        stack_size = Mock(return_value=50),)

        self.processes = [self.process_1, self.process_2, self.process_3, self.process_4]

    def test_filter_processes_for_given_user_name(self):
        self.options.filtered_process_user = 'pcp1'
        processs_filter = ProcessFilter(self.options)

        test_filtered_processes = processs_filter.filter_processes(self.processes)

        self.assertEqual(test_filtered_processes,[self.process_2,self.process_3])

    def test_filter_processes_for_given_process_name(self):
        self.options.process_name = 'process'
        processs_filter = ProcessFilter(self.options)

        test_filtered_processes = processs_filter.filter_processes(self.processes)

        self.assertEqual(test_filtered_processes,[self.process_1, self.process_2, self.process_4])

    def test_filter_processes_for_given_pid_list(self):
        self.options.pid_filter = 'ALL'
        self.options.pid_list = [1,4]
        processs_filter = ProcessFilter(self.options)

        test_filtered_processes = processs_filter.filter_processes(self.processes)

        self.assertEqual(test_filtered_processes,[self.process_1,self.process_4])

    def test_filter_processes_for_process_vsize(self):
        self.options.show_process_memory_util = True
        processs_filter = ProcessFilter(self.options)

        test_filtered_processes = processs_filter.filter_processes(self.processes)

        self.assertEqual(test_filtered_processes,[self.process_1,self.process_2])

    def test_filter_processes_for_process_priority(self):
        self.options.show_process_priority = True
        processs_filter = ProcessFilter(self.options)

        test_filtered_processes = processs_filter.filter_processes(self.processes)

        self.assertEqual(test_filtered_processes,[self.process_1,self.process_3])

    def test_filter_processes_for_process_stack_size(self):
        self.options.show_process_stack_util = True
        processs_filter = ProcessFilter(self.options)

        test_filtered_processes = processs_filter.filter_processes(self.processes)

        self.assertEqual(test_filtered_processes,[self.process_1,self.process_4])

if __name__ == "__main__":
    unittest.main()
