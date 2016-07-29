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
from pcp_mpstat import InterruptUsageReporter

class TestHardInterruptUsageReporter(unittest.TestCase):
    def setUp(self):
        interrupts = [ Mock(), Mock()]
        interrupts[0].configure_mock(
                            name = Mock(return_value = 'SOME_INTERRUPT'),
                            value = Mock(return_value = 1.23))
        interrupts[1].configure_mock(
                            name = Mock(return_value = 'ANOTHER_INTERRUPT'),
                            value = Mock(return_value = 2.34))
        self.cpu_interrupt_zero = Mock(
                                cpu_number = Mock(return_value = 0) ,
                                interrupts = interrupts
                                )
        self.cpu_interrupt_one = Mock(
                                cpu_number = Mock(return_value = 1),
                                interrupts = interrupts
                                )
    def test_print_report(self):
        interrupt_usage = Mock()
        printer = Mock()
        options = Mock()
        cpu_interrupts = [self.cpu_interrupt_zero]
        interrupt_usage.get_percpu_interrupts = Mock(return_value = cpu_interrupts)
        cpu_filter = Mock()
        cpu_filter.filter_cpus = Mock(return_value = cpu_interrupts)
        report = InterruptUsageReporter(cpu_filter, printer, options)
        timestamp = '2016-7-18 IST'
        calls = [call('\nTimestamp\tcpu \tSOME_INTERRUPT/s\tANOTHER_INTERRUPT/s\t'),
                call('2016-7-18 IST\t0   \t1.23            \t2.34               \t')]

        report.print_report(interrupt_usage, timestamp)

        printer.assert_has_calls(calls, any_order = False)

    def test_print_report_with_cpu_filter_on(self):
        interrupt_usage = Mock()
        printer = Mock()
        options = Mock()
        cpu_interrupts = [self.cpu_interrupt_zero, self.cpu_interrupt_one]
        interrupt_usage.get_percpu_interrupts = Mock(return_value = cpu_interrupts)
        cpu_filter = Mock()
        cpu_filter.filter_cpus = Mock(return_value = [self.cpu_interrupt_zero])
        report = InterruptUsageReporter(cpu_filter, printer, options)
        timestamp = '2016-7-18 IST'
        calls = [call('\nTimestamp\tcpu \tSOME_INTERRUPT/s\tANOTHER_INTERRUPT/s\t'),
                call('2016-7-18 IST\t0   \t1.23            \t2.34               \t')]

        report.print_report(interrupt_usage, timestamp)

        printer.assert_has_calls(calls, any_order = False)
if __name__ == '__main__':
    unittest.main()
