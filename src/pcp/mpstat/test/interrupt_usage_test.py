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
from pcp_mpstat import InterruptUsage

class TestInterruptUsage(unittest.TestCase):
    def setUp(self):
        self.metric_repository = Mock()
        self.metric_repository.current_value = Mock(side_effect = self.current_value_side_effect)
        self.metric_repository.previous_value = Mock(side_effect = self.previous_value_side_effect)

    def current_value_side_effect(self, metric, instance):
        if metric == 'kernel.percpu.interrupts.line12' and instance == 0:
            return 1234
        if metric == 'kernel.percpu.interrupts.PIW' and instance == 0:
            return 2345
        if metric == 'kernel.percpu.interrupts.line0' and instance == 2:
            return 1243
        return None

    def previous_value_side_effect(self, metric, instance):
        if metric == 'kernel.percpu.interrupts.line12'and instance == 1:
            return 1232
        if metric == 'kernel.percpu.interrupts.PIW' and instance == 1:
            return 2341
        if metric == 'kernel.percpu.interrupts.line0' and instance == 2:
            return 1241
        return None

    def test_if_name_has_line_in_it(self):
        interrupt_usage = InterruptUsage(1.34, self.metric_repository, 'kernel.percpu.interrupts.line12', 0)

        name = interrupt_usage.name()

        self.assertEqual(name, '12')

    def test_if_name_does_not_have_line_in_it(self):
        interrupt_usage = InterruptUsage(1.34, self.metric_repository, 'kernel.percpu.interrupts.PIW', 0)

        name = interrupt_usage.name()

        self.assertEqual(name, 'PIW')

    def test_value_if_not_none(self):
        interrupt_usage = InterruptUsage(1.34, self.metric_repository, 'kernel.percpu.interrupts.line0', 2)

        value = interrupt_usage.value()

        self.assertEqual(value, 1.49)

    def test_value_if_current_value_is_none(self):
        interrupt_usage = InterruptUsage(1.34, self.metric_repository, 'kernel.percpu.interrupts.line12', 1)

        value = interrupt_usage.value()

        self.assertIsNone(value)

    def test_value_if_previous_value_is_none(self):
        interrupt_usage = InterruptUsage(1.34, self.metric_repository, 'kernel.percpu.interrupts.line12', 0)

        value = interrupt_usage.value()

        self.assertIsNone(value)

if __name__ == '__main__':
    unittest.main()
