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
from pcp_mpstat import CpuUtilReporter

class TestCpuUtilReporter(unittest.TestCase):
    def setUp(self):
        self.cpu_usage_total = Mock(
                            user_time = Mock(return_value = 1.23),
                            nice_time = Mock(return_value = 2.34),
                            sys_time = Mock(return_value =  3.45),
                            iowait_time = Mock(return_value = 4.56),
                            irq_hard = Mock(return_value = 5.67),
                            irq_soft = Mock(return_value = 6.78),
                            steal = Mock(return_value = 7.89),
                            guest_time = Mock(return_value = 8.90),
                            guest_nice = Mock(return_value = 1.34),
                            idle_time = Mock(return_value = 2.45)
                        )
        self.cpu_usage_1 = Mock(
                            cpu_number = Mock(return_value = 1),
                            user_time = Mock(return_value = 1.43),
                            nice_time = Mock(return_value = 2.35),
                            sys_time = Mock(return_value =  2.45),
                            iowait_time = Mock(return_value = 3.76),
                            irq_hard = Mock(return_value = 6.45),
                            irq_soft = Mock(return_value = 2.58),
                            steal = Mock(return_value = 2.59),
                            guest_time = Mock(return_value = 5.60),
                            guest_nice = Mock(return_value = 2.34),
                            idle_time = Mock(return_value = 6.67)
                        )
        self.cpu_usage_2 = Mock(
                            cpu_number = Mock(return_value = 2),
                            user_time = Mock(return_value = 2.43),
                            nice_time = Mock(return_value = 3.35),
                            sys_time = Mock(return_value =  5.45),
                            iowait_time = Mock(return_value = 2.76),
                            irq_hard = Mock(return_value = 7.45),
                            irq_soft = Mock(return_value = 3.58),
                            steal = Mock(return_value = 6.59),
                            guest_time = Mock(return_value = 2.60),
                            guest_nice = Mock(return_value = 7.34),
                            idle_time = Mock(return_value = 3.67)
                        )

    def test_print_report_with_no_options(self):
        options = Mock()
        options.cpu_list = None
        cpu_filter = Mock()
        cpu_filter.filter_cpus = Mock(return_value = [])
        cpu_util = Mock()
        cpu_util.get_totalcpu_util = Mock(return_value = self.cpu_usage_total)
        printer = Mock()
        report = CpuUtilReporter(cpu_filter, printer, options)
        timestamp = '2016-7-18 IST'

        report.print_report(cpu_util, timestamp)

        printer.assert_called_with('2016-7-18 IST\tall\t1.23 \t2.34  \t3.45 \t4.56    \t5.67 \t6.78  \t7.89   \t8.9    \t1.34  \t2.45  ')

    def test_print_report_with_online_cpus(self):
        options = Mock()
        options.cpu_list = "ON"
        options.cpu_filter = True
        cpu_filter = Mock()
        cpu_filter.filter_cpus = Mock(return_value = [self.cpu_usage_1, self.cpu_usage_2])
        printer = Mock()
        cpu_util = Mock()
        cpu_util.get_totalcpu_util = Mock(return_value = self.cpu_usage_total)
        timestamp = '2016-7-18 IST'
        report = CpuUtilReporter(cpu_filter, printer, options)

        report.print_report(cpu_util, timestamp)

        calls = [call('\nTimestamp \tCPU\t%usr \t%nice \t%sys \t%iowait \t%irq \t%soft \t%steal \t%guest \t%nice \t%idle '),
                call('2016-7-18 IST\tall\t1.23 \t2.34  \t3.45 \t4.56    \t5.67 \t6.78  \t7.89   \t8.9    \t1.34  \t2.45  '),
                call('2016-7-18 IST\t1  \t1.43 \t2.35  \t2.45 \t3.76    \t6.45 \t2.58  \t2.59   \t5.6    \t2.34  \t6.67  '),
                call('2016-7-18 IST\t2  \t2.43 \t3.35  \t5.45 \t2.76    \t7.45 \t3.58  \t6.59   \t2.6    \t7.34  \t3.67  ')]

        printer.assert_has_calls(calls)

    def test_print_report_with_option_all(self):
        options = Mock()
        options.cpu_list = "ALL"
        options.cpu_filter = True
        cpu_filter = Mock()
        cpu_filter.filter_cpus = Mock(return_value = [self.cpu_usage_1, self.cpu_usage_2])
        printer = Mock()
        cpu_util = Mock()
        cpu_util.get_totalcpu_util = Mock(return_value = self.cpu_usage_total)
        timestamp = '2016-7-18 IST'
        report = CpuUtilReporter(cpu_filter, printer, options)

        report.print_report(cpu_util, timestamp)

        calls = [call('\nTimestamp \tCPU\t%usr \t%nice \t%sys \t%iowait \t%irq \t%soft \t%steal \t%guest \t%nice \t%idle '),
                call('2016-7-18 IST\tall\t1.23 \t2.34  \t3.45 \t4.56    \t5.67 \t6.78  \t7.89   \t8.9    \t1.34  \t2.45  '),
                call('2016-7-18 IST\t1  \t1.43 \t2.35  \t2.45 \t3.76    \t6.45 \t2.58  \t2.59   \t5.6    \t2.34  \t6.67  '),
                call('2016-7-18 IST\t2  \t2.43 \t3.35  \t5.45 \t2.76    \t7.45 \t3.58  \t6.59   \t2.6    \t7.34  \t3.67  ')]

        printer.assert_has_calls(calls)

    def test_print_report_if_header_prints_once(self):
        options = Mock()
        options.cpu_list = None
        options.cpu_filter = False
        cpu_filter = Mock()
        cpu_filter.filter_cpus = Mock(return_value = [self.cpu_usage_1])
        printer = Mock()
        cpu_util = Mock()
        cpu_util.get_totalcpu_util = Mock(return_value = self.cpu_usage_total)
        timestamp = '2016-7-18 IST'
        report = CpuUtilReporter(cpu_filter, printer, options)

        report.print_report(cpu_util, timestamp)
        report.print_report(cpu_util, timestamp)

        calls = [call('\nTimestamp \tCPU\t%usr \t%nice \t%sys \t%iowait \t%irq \t%soft \t%steal \t%guest \t%nice \t%idle '),
                call('2016-7-18 IST\tall\t1.23 \t2.34  \t3.45 \t4.56    \t5.67 \t6.78  \t7.89   \t8.9    \t1.34  \t2.45  '),
                call('2016-7-18 IST\tall\t1.23 \t2.34  \t3.45 \t4.56    \t5.67 \t6.78  \t7.89   \t8.9    \t1.34  \t2.45  ')]

        printer.assert_has_calls(calls)

    def test_print_report_if_header_prints_every_time_if_ALL_used(self):
        options = Mock()
        options.cpu_list = "ALL"
        options.cpu_filter = True
        cpu_filter = Mock()
        cpu_filter.filter_cpus = Mock(return_value = [self.cpu_usage_1])
        printer = Mock()
        cpu_util = Mock()
        cpu_util.get_totalcpu_util = Mock(return_value = self.cpu_usage_total)
        timestamp = '2016-7-18 IST'
        report = CpuUtilReporter(cpu_filter, printer, options)

        report.print_report(cpu_util, timestamp)
        report.print_report(cpu_util, timestamp)

        calls = [call('\nTimestamp \tCPU\t%usr \t%nice \t%sys \t%iowait \t%irq \t%soft \t%steal \t%guest \t%nice \t%idle '),
            call('2016-7-18 IST\tall\t1.23 \t2.34  \t3.45 \t4.56    \t5.67 \t6.78  \t7.89   \t8.9    \t1.34  \t2.45  '),
            call('2016-7-18 IST\t1  \t1.43 \t2.35  \t2.45 \t3.76    \t6.45 \t2.58  \t2.59   \t5.6    \t2.34  \t6.67  '),
            call('\nTimestamp \tCPU\t%usr \t%nice \t%sys \t%iowait \t%irq \t%soft \t%steal \t%guest \t%nice \t%idle '),
            call('2016-7-18 IST\tall\t1.23 \t2.34  \t3.45 \t4.56    \t5.67 \t6.78  \t7.89   \t8.9    \t1.34  \t2.45  '),
            call('2016-7-18 IST\t1  \t1.43 \t2.35  \t2.45 \t3.76    \t6.45 \t2.58  \t2.59   \t5.6    \t2.34  \t6.67  ')]

        printer.assert_has_calls(calls)


if __name__ == "__main__":
    unittest.main()
