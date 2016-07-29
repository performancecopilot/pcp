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
import sys
import unittest
if sys.version_info[0] < 3:
    from mock import Mock
else:
    from unittest.mock import Mock
from pcp_mpstat import TotalInterruptUsage

class TestTotalInterruptUsage(unittest.TestCase):
    def setUp(self):
        self.metric_repository = Mock()
        self.metric_repository.current_value = Mock(return_value=2.35)
    def current_value_side_effect(self, metric_name, instance):
        if metric_name == 'kernel.all.intr' and instance is None:
            return 2.45
        return None

    def previous_value_side_effect(self, metric_name, instance):
        if metric_name == 'kernel.all.intr' and instance is None:
            return 2.24
        return None

    def test_total_interrupt_per_delta_time(self):
        metric_repository = Mock()
        metric_repository.current_value = Mock(side_effect=self.current_value_side_effect)
        metric_repository.previous_value = Mock(side_effect=self.previous_value_side_effect)
        total_interrupt_usage = TotalInterruptUsage(1.34, metric_repository)

        interrupt_usage = total_interrupt_usage.total_interrupt_per_delta_time()

        self.assertEqual(interrupt_usage, 0.16)

    def test_total_interrupt_per_delta_time_if_current_value_is_none(self):
        metric_repository = Mock()
        metric_repository.current_value = Mock(return_value= None)
        metric_repository.previous_value = Mock(side_effect=self.previous_value_side_effect)
        total_interrupt_usage = TotalInterruptUsage(1.34, metric_repository)

        interrupt_usage = total_interrupt_usage.total_interrupt_per_delta_time()

        self.assertIsNone(interrupt_usage)

    def test_total_interrupt_per_delta_time_if_previous_value_is_none(self):
        metric_repository = Mock()
        metric_repository.current_value = Mock(side_effect=self.current_value_side_effect)
        metric_repository.previous_value = Mock(return_value= None)
        total_interrupt_usage = TotalInterruptUsage(1.34, metric_repository)

        interrupt_usage = total_interrupt_usage.total_interrupt_per_delta_time()

        self.assertIsNone(interrupt_usage)

if __name__ == '__main__':
    unittest.main()
