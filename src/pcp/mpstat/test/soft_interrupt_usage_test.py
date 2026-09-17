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
from unittest.mock import Mock
from pcp_mpstat import SoftInterruptUsage

class TestSoftInterruptUsage(unittest.TestCase):
    def setUp(self):
        self.metric_repository = Mock()
        self.metric_repository.group.get.return_value = Mock(desc=Mock(indom=1))
        self.metric_repository.group.contextCache.pmGetInDom.return_value = (
            [0, 1, 2, 3],
            ['RCU::cpu0', 'RCU::cpu1', 'RCU::cpu2', 'RCU::cpu3'])

    def test_get_percpu_interrupts(self):
        soft_interrupt_usage = SoftInterruptUsage(1.34, self.metric_repository)

        percpu_interrupts = soft_interrupt_usage.get_percpu_interrupts()

        self.assertEqual(len(percpu_interrupts), 4)

if __name__ == "__main__":
    unittest.main()
