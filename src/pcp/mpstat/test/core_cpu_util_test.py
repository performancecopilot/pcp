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
from pcp_mpstat import CoreCpuUtil
class TestCoreCpuUtil(unittest.TestCase):
    def setUp(self):
        self.__metric_repository = Mock()
        self.__metric_repository.current_value = Mock(side_effect=self.metric_repo_current_value_side_effect)
        self.__metric_repository.previous_value = Mock(side_effect=self.metric_repo_previous_value_side_effect)

    def metric_repo_current_value_side_effect(self, metric_name,instance):
        if metric_name == 'hinv.ncpu':
            return 2
        if metric_name == 'hinv.cpu.online':
            return 2
        if metric_name == 'kernel.all.cpu.user' and instance is None:
            return 1234
        if metric_name == 'kernel.all.cpu.sys' and instance is None:
            return 1123
        if metric_name == 'kernel.all.cpu.nice' and instance is None:
            return 1223
        if metric_name == 'kernel.all.cpu.guest' and instance is None:
            return 1233
        if metric_name == 'kernel.all.cpu.wait.total' and instance is None:
            return 1212
        if metric_name == 'kernel.all.cpu.irq.hard' and instance is None:
            return 12
        if metric_name == 'kernel.all.cpu.irq.soft' and instance is None:
            return 1133
        if metric_name == 'kernel.all.cpu.steal' and instance is None:
            return 3423
        if metric_name == 'kernel.all.cpu.idle' and instance is None:
            return 1122
        if metric_name == 'kernel.all.cpu.guest_nice' and instance is None:
            return 1123
        if metric_name == 'kernel.percpu.cpu.user' and instance == 0:
            return 1234
        if metric_name == 'kernel.percpu.cpu.sys' and instance == 0:
            return 1123
        if metric_name == 'kernel.percpu.cpu.nice' and instance == 0:
            return 1223
        if metric_name == 'kernel.percpu.cpu.guest' and instance == 0:
            return 1233
        if metric_name == 'kernel.percpu.cpu.wait.total' and instance == 0:
            return 1212
        if metric_name == 'kernel.percpu.cpu.irq.hard' and instance == 0:
            return 12
        if metric_name == 'kernel.percpu.cpu.irq.soft' and instance == 0:
            return 1133
        if metric_name == 'kernel.percpu.cpu.steal' and instance == 0:
            return 3423
        if metric_name == 'kernel.percpu.cpu.idle' and instance == 0:
            return 1122
        if metric_name == 'kernel.percpu.cpu.guest_nice' and instance == 0:
            return 1123
        if metric_name == 'kernel.percpu.cpu.user' and instance == 1:
            return 1234
        if metric_name == 'kernel.percpu.cpu.sys' and instance == 1:
            return 1123
        if metric_name == 'kernel.percpu.cpu.nice' and instance == 1:
            return 1223
        if metric_name == 'kernel.percpu.cpu.guest' and instance == 1:
            return 1233
        if metric_name == 'kernel.percpu.cpu.wait.total' and instance == 1:
            return 1212
        if metric_name == 'kernel.percpu.cpu.irq.hard' and instance == 1:
            return 12
        if metric_name == 'kernel.percpu.cpu.irq.soft' and instance == 1:
            return 1133
        if metric_name == 'kernel.percpu.cpu.steal' and instance == 1:
            return 3423
        if metric_name == 'kernel.percpu.cpu.idle' and instance == 1:
            return 1122
        if metric_name == 'kernel.percpu.cpu.guest_nice' and instance == 1:
            return 1123
        return None

    def metric_repo_previous_value_side_effect(self, metric_name,instance):
        if metric_name == 'kernel.all.cpu.user' and instance is None:
            return 1230
        if metric_name == 'kernel.all.cpu.sys' and instance is None:
            return 1112
        if metric_name == 'kernel.all.cpu.nice' and instance is None:
            return 1215
        if metric_name == 'kernel.all.cpu.guest' and instance is None:
            return 1225
        if metric_name == 'kernel.all.cpu.wait.total' and instance is None:
            return 1204
        if metric_name == 'kernel.all.cpu.irq.hard' and instance is None:
            return 10
        if metric_name == 'kernel.all.cpu.irq.soft' and instance is None:
            return 1131
        if metric_name == 'kernel.all.cpu.steal' and instance is None:
            return 3415
        if metric_name == 'kernel.all.cpu.idle' and instance is None:
            return 1119
        if metric_name == 'kernel.all.cpu.guest_nice' and instance is None:
            return 1116
        if metric_name == 'kernel.percpu.cpu.user' and instance == 0:
            return 1230
        if metric_name == 'kernel.percpu.cpu.sys' and instance == 0:
            return 1112
        if metric_name == 'kernel.percpu.cpu.nice' and instance == 0:
            return 1215
        if metric_name == 'kernel.percpu.cpu.guest' and instance == 0:
            return 1225
        if metric_name == 'kernel.percpu.cpu.wait.total' and instance == 0:
            return 1204
        if metric_name == 'kernel.percpu.cpu.irq.hard' and instance == 0:
            return 10
        if metric_name == 'kernel.percpu.cpu.irq.soft' and instance == 0:
            return 1131
        if metric_name == 'kernel.percpu.cpu.steal' and instance == 0:
            return 3415
        if metric_name == 'kernel.percpu.cpu.idle' and instance == 0:
            return 1119
        if metric_name == 'kernel.percpu.cpu.guest_nice' and instance == 0:
            return 1116
        return None

    def test_cpu_online(self):
        cpu_util = CoreCpuUtil(None, 1.34, self.__metric_repository)

        test_cpu_online  = cpu_util.cpu_online()

        self.assertEqual(test_cpu_online, 2)

    def test_user_time(self):
        cpu_util = CoreCpuUtil(0, 1.34, self.__metric_repository)

        user_time  = cpu_util.user_time()

        self.assertEqual(user_time,0.3)

    def test_nice_time(self):
        cpu_util = CoreCpuUtil(0, 1.34, self.__metric_repository)

        nice_time  = cpu_util.nice_time()

        self.assertEqual(nice_time,0.6)

    def test_sys_time(self):
        cpu_util = CoreCpuUtil(0, 1.34, self.__metric_repository)

        sys_time  = cpu_util.sys_time()

        self.assertEqual(sys_time,0.82)

    def test_iowait_time(self):
        cpu_util = CoreCpuUtil(0, 1.34, self.__metric_repository)

        iowait_time  = cpu_util.iowait_time()

        self.assertEqual(iowait_time,0.6)

    def test_irq_hard(self):
        cpu_util = CoreCpuUtil(0, 1.34, self.__metric_repository)

        irq_hard  = cpu_util.irq_hard()

        self.assertEqual(irq_hard,0.15  )

    def test_irq_soft(self):
        cpu_util = CoreCpuUtil(0, 1.34, self.__metric_repository)

        irq_soft  = cpu_util.irq_soft()

        self.assertEqual(irq_soft,0.15)

    def test_guest_time(self):
        cpu_util = CoreCpuUtil(0, 1.34, self.__metric_repository)

        guest_time  = cpu_util.guest_time()

        self.assertEqual(guest_time,0.6)

    def test_guest_nice(self):
        cpu_util = CoreCpuUtil(0, 1.34, self.__metric_repository)

        guest_nice  = cpu_util.guest_nice()

        self.assertEqual(guest_nice,0.52)

    def test_idle_time(self):
        cpu_util = CoreCpuUtil(0, 1.34, self.__metric_repository)

        idle_time  = cpu_util.idle_time()

        self.assertEqual(idle_time,0.22)

    def test_user_time_for_all_cpus(self):
        cpu_util = CoreCpuUtil(None, 1.34, self.__metric_repository)

        user_time  = cpu_util.user_time()

        self.assertEqual(user_time,0.15)

    def test_nice_time_for_all_cpus(self):
        cpu_util = CoreCpuUtil(None, 1.34, self.__metric_repository)

        nice_time  = cpu_util.nice_time()

        self.assertEqual(nice_time,0.3)

    def test_sys_time_for_all_cpus(self):
        cpu_util = CoreCpuUtil(None, 1.34, self.__metric_repository)

        sys_time  = cpu_util.sys_time()

        self.assertEqual(sys_time,0.41)

    def test_iowait_time_for_all_cpus(self):
        cpu_util = CoreCpuUtil(None, 1.34, self.__metric_repository)

        iowait_time  = cpu_util.iowait_time()

        self.assertEqual(iowait_time,0.3)

    def test_irq_hard_for_all_cpus(self):
        cpu_util = CoreCpuUtil(None, 1.34, self.__metric_repository)

        irq_hard  = cpu_util.irq_hard()

        self.assertEqual(irq_hard,0.07)

    def test_irq_soft_for_all_cpus(self):
        cpu_util = CoreCpuUtil(None, 1.34, self.__metric_repository)

        irq_soft  = cpu_util.irq_soft()

        self.assertEqual(irq_soft,0.07)

    def test_guest_time_for_all_cpus(self):
        cpu_util = CoreCpuUtil(None, 1.34, self.__metric_repository)

        guest_time  = cpu_util.guest_time()

        self.assertEqual(guest_time,0.3)

    def test_guest_nice_for_all_cpus(self):
        cpu_util = CoreCpuUtil(None, 1.34, self.__metric_repository)

        guest_nice  = cpu_util.guest_nice()

        self.assertEqual(guest_nice,0.26)

    def test_idle_time_for_all_cpus(self):
        cpu_util = CoreCpuUtil(None, 1.34, self.__metric_repository)

        idle_time  = cpu_util.idle_time()

        self.assertEqual(idle_time,0.11)

    def test_user_time_if_current_value_is_none(self):
        cpu_util = CoreCpuUtil(2, 1.34, self.__metric_repository)

        user_time  = cpu_util.user_time()

        self.assertIsNone(user_time)

    def test_nice_time_if_current_value_is_none(self):
        cpu_util = CoreCpuUtil(2, 1.34, self.__metric_repository)

        nice_time  = cpu_util.nice_time()

        self.assertIsNone(nice_time)

    def test_sys_time_if_current_value_is_none(self):
        cpu_util = CoreCpuUtil(2, 1.34, self.__metric_repository)

        sys_time  = cpu_util.sys_time()

        self.assertIsNone(sys_time)

    def test_iowait_time_if_current_value_is_none(self):
        cpu_util = CoreCpuUtil(2, 1.34, self.__metric_repository)

        iowait_time  = cpu_util.iowait_time()

        self.assertIsNone(iowait_time)

    def test_irq_hard_if_current_value_is_none(self):
        cpu_util = CoreCpuUtil(2, 1.34, self.__metric_repository)

        irq_hard  = cpu_util.irq_hard()

        self.assertIsNone(irq_hard)

    def test_irq_soft_if_current_value_is_none(self):
        cpu_util = CoreCpuUtil(2, 1.34, self.__metric_repository)

        irq_soft  = cpu_util.irq_soft()

        self.assertIsNone(irq_soft)

    def test_guest_time_if_current_value_is_none(self):
        cpu_util = CoreCpuUtil(2, 1.34, self.__metric_repository)

        guest_time  = cpu_util.guest_time()

        self.assertIsNone(guest_time)

    def test_guest_nice_if_current_value_is_none(self):
        cpu_util = CoreCpuUtil(2, 1.34, self.__metric_repository)

        guest_nice  = cpu_util.guest_nice()

        self.assertIsNone(guest_nice)

    def test_idle_time_if_current_value_is_none(self):
        cpu_util = CoreCpuUtil(2, 1.34, self.__metric_repository)

        idle_time  = cpu_util.idle_time()

        self.assertIsNone(idle_time)

    def test_user_time_if_previous_value_is_none(self):
        cpu_util = CoreCpuUtil(1, 1.34, self.__metric_repository)

        user_time  = cpu_util.user_time()

        self.assertIsNone(user_time)

    def test_nice_time_if_previous_value_is_none(self):
        cpu_util = CoreCpuUtil(1, 1.34, self.__metric_repository)

        nice_time  = cpu_util.nice_time()

        self.assertIsNone(nice_time)

    def test_sys_time_if_previous_value_is_none(self):
        cpu_util = CoreCpuUtil(1, 1.34, self.__metric_repository)

        sys_time  = cpu_util.sys_time()

        self.assertIsNone(sys_time)

    def test_iowait_time_if_previous_value_is_none(self):
        cpu_util = CoreCpuUtil(1, 1.34, self.__metric_repository)

        iowait_time  = cpu_util.iowait_time()

        self.assertIsNone(iowait_time)

    def test_irq_hard_if_previous_value_is_none(self):
        cpu_util = CoreCpuUtil(1, 1.34, self.__metric_repository)

        irq_hard  = cpu_util.irq_hard()

        self.assertIsNone(irq_hard)

    def test_irq_soft_if_previous_value_is_none(self):
        cpu_util = CoreCpuUtil(1, 1.34, self.__metric_repository)

        irq_soft  = cpu_util.irq_soft()

        self.assertIsNone(irq_soft)

    def test_guest_time_if_previous_value_is_none(self):
        cpu_util = CoreCpuUtil(1, 1.34, self.__metric_repository)

        guest_time  = cpu_util.guest_time()

        self.assertIsNone(guest_time)

    def test_guest_nice_if_previous_value_is_none(self):
        cpu_util = CoreCpuUtil(1, 1.34, self.__metric_repository)

        guest_nice  = cpu_util.guest_nice()

        self.assertIsNone(guest_nice)

    def test_idle_time_if_previous_value_is_none(self):
        cpu_util = CoreCpuUtil(1, 1.34, self.__metric_repository)

        idle_time  = cpu_util.idle_time()

        self.assertIsNone(idle_time)



if __name__ == '__main__':
    unittest.main()
