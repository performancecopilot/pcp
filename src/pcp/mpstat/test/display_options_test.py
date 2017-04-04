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
from pcp_mpstat import DisplayOptions

class TestDisplayOptions(unittest.TestCase):
    def setUp(self):
        self.mpstat_options = Mock()
        self.mpstat_options.cpu_list = None
        self.mpstat_options.cpu_filter = False
        self.mpstat_options.options_all = False
        self.mpstat_options.interrupts_filter = False
        self.mpstat_options.interrupt_type = None
        self.mpstat_options.no_options = True

    def test_display_cpu_usage_summary(self):
        display_options = DisplayOptions(self.mpstat_options)

        self.assertTrue(display_options.display_cpu_usage_summary())

    def test_display_total_cpu_usage_with_type_sum(self):
        self.mpstat_options.interrupts_filter = True
        self.mpstat_options.interrupt_type = "SUM"

        display_options = DisplayOptions(self.mpstat_options)

        self.assertTrue(display_options.display_total_cpu_usage())

    def test_display_total_cpu_usage_with_type_all(self):
        self.mpstat_options.interrupts_filter = True
        self.mpstat_options.interrupt_type = "ALL"

        display_options = DisplayOptions(self.mpstat_options)

        self.assertTrue(display_options.display_total_cpu_usage())

    def test_display_hard_interrupt_usage_with_type_cpu(self):
        self.mpstat_options.interrupts_filter = True
        self.mpstat_options.interrupt_type = "CPU"

        display_options = DisplayOptions(self.mpstat_options)

        self.assertTrue(display_options.display_hard_interrupt_usage())

    def test_display_hard_interrupt_usage_with_type_all(self):
        self.mpstat_options.interrupts_filter = True
        self.mpstat_options.interrupt_type = "ALL"

        display_options = DisplayOptions(self.mpstat_options)

        self.assertTrue(display_options.display_hard_interrupt_usage())

    def test_display_soft_interrupt_usage_with_type_scpu(self):
        self.mpstat_options.interrupts_filter = True
        self.mpstat_options.interrupt_type = "SCPU"

        display_options = DisplayOptions(self.mpstat_options)

        self.assertTrue(display_options.display_soft_interrupt_usage())

    def test_display_soft_interrupt_usage_with_type_all(self):
        self.mpstat_options.interrupts_filter = True
        self.mpstat_options.interrupt_type = "ALL"

        display_options = DisplayOptions(self.mpstat_options)

        self.assertTrue(display_options.display_soft_interrupt_usage())

if __name__ == '__main__':
    unittest.main()
