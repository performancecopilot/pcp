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

import mock
import unittest
from pcp_pidstat import ProcessCpuUsage

class TestProcessCpuUsage(unittest.TestCase):
    def setUp(self):
        self.__metric_repository = mock.Mock()
        self.__metric_repository.current_value = mock.Mock(side_effect=self.metric_repo_current_value_side_effect)
        self.__metric_repository.previous_value = mock.Mock(side_effect=self.metric_repo_previous_value_side_effect)

    def metric_repo_current_value_side_effect(self, metric_name,instance):
        if metric_name == 'proc.psinfo.utime' and instance == 1:
            return 112233
        if metric_name == 'proc.psinfo.guest_time' and instance == 1:
            return 112213
        if metric_name == 'proc.psinfo.stime' and instance == 1:
            return 112243
        if metric_name == 'proc.psinfo.pid' and instance == 1:
            return 1
        if metric_name == 'proc.psinfo.cmd' and instance == 1:
            return "test"
        if metric_name == 'proc.psinfo.processor' and instance == 1:
            return 0
        if metric_name == 'proc.id.uid' and instance == 1:
            return 1
        if metric_name == 'proc.id.uid_nm' and instance == 1:
            return "pcp"
        if metric_name == 'proc.psinfo.utime' and instance == 2:
            return 112233
        if metric_name == 'proc.psinfo.guest_time' and instance == 2:
            return 112213
        if metric_name == 'proc.psinfo.stime' and instance == 2:
            return 112243
        if metric_name == 'proc.psinfo.pid' and instance == 2:
            return 1
        if metric_name == 'proc.psinfo.cmd' and instance == 2:
            return "test"
        if metric_name == 'proc.psinfo.processor' and instance == 2:
            return 0
        if metric_name == 'proc.id.uid' and instance == 2:
            return 1
        if metric_name == 'proc.id.uid_nm' and instance == 2:
            return "pcp"
        return None

    def metric_repo_previous_value_side_effect(self, metric_name,instance):
        if metric_name == 'proc.psinfo.utime' and instance == 1:
            return 112223
        if metric_name == 'proc.psinfo.guest_time' and instance == 1:
            return 112203
        if metric_name == 'proc.psinfo.stime' and instance == 1:
            return 112233
        if metric_name == 'proc.psinfo.pid' and instance == 1:
            return 1
        if metric_name == 'proc.psinfo.cmd' and instance == 1:
            return "test"
        if metric_name == 'proc.psinfo.processor' and instance == 1:
            return 0
        if metric_name == 'proc.id.uid' and instance == 1:
            return 1
        if metric_name == 'proc.id.uid_nm' and instance == 1:
            return "pcp"
        if metric_name == 'proc.psinfo.utime' and instance == 3:
            return 112223
        if metric_name == 'proc.psinfo.guest_time' and instance == 3:
            return 112203
        if metric_name == 'proc.psinfo.stime' and instance == 3:
            return 112233
        if metric_name == 'proc.psinfo.pid' and instance == 3:
            return 1
        if metric_name == 'proc.psinfo.cmd' and instance == 3:
            return "test"
        if metric_name == 'proc.psinfo.processor' and instance == 3:
            return 0
        if metric_name == 'proc.id.uid' and instance == 3:
            return 1
        if metric_name == 'proc.id.uid_nm' and instance == 3:
            return "pcp"
        return None

    def test_user_percent(self):
        process_cpu_usage = ProcessCpuUsage(1,1.34,self.__metric_repository)

        user_percent = process_cpu_usage.user_percent()

        self.assertEquals(user_percent, 0.75)

    def test_user_percent_if_current_value_is_None(self):
        process_cpu_usage = ProcessCpuUsage(3,1.34,self.__metric_repository)

        user_percent = process_cpu_usage.user_percent()

        self.assertIsNone(user_percent)

    def test_user_percent_if_previous_value_is_None(self):
        process_cpu_usage = ProcessCpuUsage(2,1.34,self.__metric_repository)

        user_percent = process_cpu_usage.user_percent()

        self.assertIsNone(user_percent)

    def test_guest_percent(self):
        process_cpu_usage = ProcessCpuUsage(1,1.34,self.__metric_repository)

        guest_percent = process_cpu_usage.guest_percent()

        self.assertEquals(guest_percent, 0.75)

    def test_guest_percent_if_current_value_is_None(self):
        process_cpu_usage = ProcessCpuUsage(3,1.34,self.__metric_repository)

        guest_percent = process_cpu_usage.guest_percent()

        self.assertIsNone(guest_percent)

    def test_guest_percent_if_previous_value_is_None(self):
        process_cpu_usage = ProcessCpuUsage(2,1.34,self.__metric_repository)

        guest_percent = process_cpu_usage.guest_percent()

        self.assertIsNone(guest_percent)

    def test_system_percent(self):
        process_cpu_usage = ProcessCpuUsage(1,1.34,self.__metric_repository)

        system_percent = process_cpu_usage.system_percent()

        self.assertEquals(system_percent, 0.75)

    def test_system_percent_if_current_value_is_None(self):
        process_cpu_usage = ProcessCpuUsage(3,1.34,self.__metric_repository)

        system_percent = process_cpu_usage.system_percent()

        self.assertIsNone(system_percent, None)

    def test_system_percent_if_previous_value_is_None(self):
        process_cpu_usage = ProcessCpuUsage(2,1.34,self.__metric_repository)

        system_percent = process_cpu_usage.system_percent()

        self.assertIsNone(system_percent, None)

    def test_total_percent(self):
        process_cpu_usage = ProcessCpuUsage(1,1.34,self.__metric_repository)

        total_percent = process_cpu_usage.total_percent()

        self.assertEquals(total_percent, 2.25)

    def test_total_percent_if_current_value_None(self):
        process_cpu_usage = ProcessCpuUsage(3,1.34,self.__metric_repository)

        total_percent = process_cpu_usage.total_percent()

        self.assertIsNone(total_percent, None)

    def test_total_percent_if_previous_value_None(self):
        process_cpu_usage = ProcessCpuUsage(2,1.34,self.__metric_repository)

        total_percent = process_cpu_usage.total_percent()

        self.assertIsNone(total_percent, None)

    def test_pid(self):
        process_cpu_usage = ProcessCpuUsage(1,1.34,self.__metric_repository)

        pid = process_cpu_usage.pid()

        self.assertEqual(pid,1)

    def test_process_name(self):
        process_cpu_usage = ProcessCpuUsage(1,1.34,self.__metric_repository)

        name = process_cpu_usage.process_name()

        self.assertEqual(name,'test')

    def test_cpu_number(self):
        process_cpu_usage = ProcessCpuUsage(1,1.34,self.__metric_repository)

        number = process_cpu_usage.cpu_number()

        self.assertEqual(number,0)

    def test_user_id(self):
        process_cpu_usage = ProcessCpuUsage(1,1.34,self.__metric_repository)

        user_id = process_cpu_usage.user_id()

        self.assertEqual(user_id,1)

    def test_user_name(self):
        process_cpu_usage = ProcessCpuUsage(1,1.34,self.__metric_repository)

        user_name = process_cpu_usage.user_name()

        self.assertEqual(user_name,'pcp')


if __name__ == '__main__':
    unittest.main()
