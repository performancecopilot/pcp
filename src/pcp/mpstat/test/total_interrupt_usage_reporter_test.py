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
    from mock import call
else:
    from unittest.mock import Mock
    from unittest.mock import call
from pcp_mpstat import TotalInterruptUsageReporter

class TestTotalInterruptUsageReporter(unittest.TestCase):

    def test_print_report_without_cpu_filter(self):
        timestamp = "2016-19-07 IST"
        total_interrupt_usage = Mock()
        total_interrupt_usage.total_interrupt_per_delta_time = Mock(return_value = 1.23)
        options = Mock()
        options.cpu_list = None
        options.cpu_filter = False
        printer = Mock()
        cpu_filter = Mock()
        report = TotalInterruptUsageReporter(cpu_filter, printer, options)

        report.print_report(total_interrupt_usage, timestamp)

        printer.assert_called_with('2016-19-07 IST\tall  \t1.23 ')

    def test_print_report_with_filtered_cpus(self):
        timestamp = "2016-19-07 IST"
        total_interrupt_usage = Mock()
        total_interrupt_usage.total_interrupt_per_delta_time = Mock(return_value = 1.23)
        options = Mock()
        options.cpu_list = 'ALL'
        options.cpu_filter = True
        printer = Mock()
        cpu_filter = Mock()
        cpu_interrupt = Mock()
        cpu_interrupt.cpu_number = Mock(return_value=0)
        cpu_interrupt.value = Mock(return_value=2.4)
        cpu_filter.filter_cpus = Mock(return_value=[cpu_interrupt])
        report = TotalInterruptUsageReporter(cpu_filter, printer, options)

        report.print_report(total_interrupt_usage, timestamp)

        calls = [call('\nTimestamp\tCPU  \tintr/s'),
                call('2016-19-07 IST\tall  \t1.23 '),
                call('2016-19-07 IST\t0    \t2.4  ')]

        printer.assert_has_calls(calls)

if __name__ == "__main__":
    unittest.main()
