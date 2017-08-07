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
from pcp_pidstat import CpuProcessPrioritiesReporter

class TestProcessPriorityReporter(unittest.TestCase):
    def setUp(self):
        self.options = Mock(
                        show_process_user = None)

        process_1 = Mock(pid = Mock(return_value = 1),
                        process_name = Mock(return_value = "process_1"),
                        user_name = Mock(return_value='pcp'),
                        user_id = Mock(return_value=1000),
                        priority = Mock(return_value=99),
                        policy = Mock(return_value='FIFO'))

        self.processes = [process_1]

    def test_print_report_without_filtering(self):
        process_priority = Mock()
        process_filter = Mock()
        printer = Mock()
        process_filter.filter_processes = Mock(return_value=self.processes)
        reporter = CpuProcessPrioritiesReporter(process_priority, process_filter, printer, self.options)

        reporter.print_report(123, "  ", "    ")

        printer.assert_called_with("123    1000\t1\t99\tFIFO\tprocess_1")

    def test_print_report_with_user_name(self):
        self.options.show_process_user = 'pcp'
        process_priority = Mock()
        process_filter = Mock()
        printer = Mock()
        process_filter.filter_processes = Mock(return_value=self.processes)
        reporter = CpuProcessPrioritiesReporter(process_priority, process_filter, printer, self.options)

        reporter.print_report(123, "  ", "    ")

        printer.assert_called_with("123    pcp\t1\t99\tFIFO\tprocess_1")

    def test_print_report_header(self):
        process_priority = Mock()
        process_filter = Mock()
        printer = Mock()
        process_filter.filter_processes = Mock(return_value=self.processes)
        reporter = CpuProcessPrioritiesReporter(process_priority, process_filter, printer, self.options)

        reporter.print_report(123, "  ", "    ")

        printer.assert_any_call("Timestamp  UID\tPID\tprio\tpolicy\tCommand")

if __name__ == "__main__":
    unittest.main()
