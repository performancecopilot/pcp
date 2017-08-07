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
from pcp_pidstat import CpuProcessMemoryUtilReporter

class TestProcessMemoryUtilReporter(unittest.TestCase):
    def setUp(self):
        self.options = Mock(
                        show_process_user = None)

        process_1 = Mock(pid = Mock(return_value = 1),
                        process_name = Mock(return_value = "process_1"),
                        user_name = Mock(return_value='pcp'),
                        user_id = Mock(return_value=1000),
                        minflt = Mock(return_value=9.10),
                        majflt = Mock(return_value=5.34),
                        vsize = Mock(return_value=100),
                        rss = Mock(return_value=200),
                        mem = Mock(return_value=1.23))

        self.processes = [process_1]

    def test_print_report_without_filtering(self):
        process_memory_util = Mock()
        process_filter = Mock()
        printer = Mock()
        process_filter.filter_processes = Mock(return_value=self.processes)
        reporter = CpuProcessMemoryUtilReporter(process_memory_util, process_filter, 1, printer, self.options)

        reporter.print_report(123, "  ", "    ")

        printer.assert_called_with("123    1000\t1\t9.1\t\t5.34\t\t100\t200\t1.23\tprocess_1")

    def test_print_report_with_min_flt_None(self):
        process_memory_util = Mock()
        process_filter = Mock()
        printer = Mock()
        self.processes[0].minflt = Mock(return_value=None)
        process_filter.filter_processes = Mock(return_value=self.processes)
        reporter = CpuProcessMemoryUtilReporter(process_memory_util, process_filter, 1, printer, self.options)

        reporter.print_report(123, "  ", "    ")

        printer.assert_called_with("123    1000\t1\tNone\t\t5.34\t\t100\t200\t1.23\tprocess_1")

    def test_print_report_with_maj_flt_None(self):
        process_memory_util = Mock()
        process_filter = Mock()
        printer = Mock()
        self.processes[0].majflt = Mock(return_value=None)
        process_filter.filter_processes = Mock(return_value=self.processes)
        reporter = CpuProcessMemoryUtilReporter(process_memory_util, process_filter, 1, printer, self.options)

        reporter.print_report(123, "  ", "    ")

        printer.assert_called_with("123    1000\t1\t9.1\t\tNone\t\t100\t200\t1.23\tprocess_1")

    def test_print_report_with_user_name(self):
        self.options.show_process_user = 'pcp'
        process_memory_util = Mock()
        process_filter = Mock()
        printer = Mock()
        process_filter.filter_processes = Mock(return_value=self.processes)
        reporter = CpuProcessMemoryUtilReporter(process_memory_util, process_filter, 1, printer, self.options)

        reporter.print_report(123, "  ", "    ")

        printer.assert_called_with('123    pcp\t1\t9.1\t\t5.34\t\t100\t200\t1.23\tprocess_1')

    def test_print_report_header(self):
        process_memory_util = Mock()
        process_filter = Mock()
        printer = Mock()
        process_filter.filter_processes = Mock(return_value=self.processes)
        reporter = CpuProcessMemoryUtilReporter(process_memory_util, process_filter, 1, printer, self.options)

        reporter.print_report(123, "  ", "    ")

        printer.assert_any_call("Timestamp  UID\tPID\tMinFlt/s\tMajFlt/s\tVSize\tRSS\t%Mem\tCommand")

if __name__ == "__main__":
    unittest.main()
