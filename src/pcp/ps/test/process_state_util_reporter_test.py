#!/usr/bin/env pmpython
#
# Copyright (c) 2022 Oracle and/or its affiliates.
# DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
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
from pcp_ps import ProcessStatusReporter


class TestProcessStateReporter(unittest.TestCase):
    def setUp(self):
        self.options = Mock(
            show_process_user=None)

        process_1 = Mock(pid=Mock(return_value=1),
                         process_name=Mock(return_value="process_1"),
                         user_name=Mock(return_value='pcp'),
                         user_id=Mock(return_value=1000),
                         stack_size=Mock(return_value=136),
                         tty_name=Mock(return_value="tty"),
                         total_time=Mock(return_value=100))

        self.processes = [process_1]

    def test_print_report_with_user_name(self):
        self.options.show_all_process = True
        self.options.print_count=1
        process_stack_util = Mock()
        process_filter = Mock()
        printer = Mock()
        process_filter.filter_processes = Mock(return_value=self.processes)
        reporter = ProcessStatusReporter(process_stack_util, process_filter, 1.34, printer, self.options)

        reporter.print_report(123, "  ", "    ")

        printer.assert_called_with("123    1\t\ttty\t100\tprocess_1")


if __name__ == "__main__":
    unittest.main()
