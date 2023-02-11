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

import mock
import unittest
from pcp_ps import ProcessStatusUtil


class TestProcessStackUtil(unittest.TestCase):
    def setUp(self):
        self.__metric_repository = mock.Mock()
        self.__metric_repository.current_value = mock.Mock(side_effect=self.metric_repo_current_value_side_effect)

    def metric_repo_current_value_side_effect(self, metric_name, instance):
        if metric_name == 'proc.psinfo.cmd' and instance == 1:
            return "test"
        if metric_name == 'proc.id.uid' and instance == 1:
            return 1
        if metric_name == 'proc.psinfo.pid' and instance == 1:
            return 1
        if metric_name == 'proc.id.uid_nm' and instance == 1:
            return "test"
        if metric_name == 'proc.psinfo.cmd' and instance == 1:
            return "test"
        if metric_name == 'proc.psinfo.psargs' and instance == 1:
            return "test"
        if metric_name == 'proc.psinfo.vsize' and instance == 1:
            return 1
        if metric_name == 'proc.psinfo.rss' and instance == 1:
            return 1
        if metric_name == 'mem.physmem' and instance is None:
            return 1
        if metric_name == 'proc.psinfo.sname' and instance == 1:
            return 'R'
        if metric_name == 'proc.psinfo.processor' and instance == 1:
            return 1
        if metric_name == 'proc.psinfo.stime' and instance == 1:
            return 1
        if metric_name == 'proc.psinfo.wchan_s' and instance == 1:
            return "test"
        if metric_name == 'proc.psinfo.priority' and instance == 1:
            return 1
        if metric_name == 'proc.psinfo.utime' and instance == 1:
            return 1
        if metric_name == 'proc.psinfo.guest_time' and instance == 1:
            return 1
        if metric_name == 'proc.psinfo.stime' and instance == 1:
            return 1
        if metric_name == 'proc.psinfo.start_time' and instance == 1:
            return 1
        if metric_name == 'kernel.all.uptime' and instance is None:
            return 1
        if metric_name == 'proc.psinfo.ttyname' and instance == 1:
            return 'tty'
        if metric_name == 'proc.psinfo.policy' and instance == 1:
            return 1

    def test_stack_referenced_size(self):
        self.skipTest(reason="Implement when suitable metric is found")

    #These are blank spaces in assert case been addded 
    #to match the format of function ouput.please don't remove
    def test_username(self):
        process_status_usage = ProcessStatusUtil(1, 1.34, self.__metric_repository)
        name = process_status_usage.user_name()
        self.assertEqual(name, "test      ")

    def test_Processname(self):
        process_status_usage = ProcessStatusUtil(1, 1.34, self.__metric_repository)
        name = process_status_usage.process_name()
        self.assertEqual(name, "test                ")

    def test_process_name_with_args(self):
        process_status_usage = ProcessStatusUtil(1, 1.34, self.__metric_repository)
        name = process_status_usage.process_name_with_args()
        self.assertEqual(name, "test                          ")

    def test_vszie(self):
        process_status_usage = ProcessStatusUtil(1, 1.34, self.__metric_repository)
        vsize = process_status_usage.vsize()
        self.assertEqual(vsize, 1)

    def test_rss(self):
        process_status_usage = ProcessStatusUtil(1, 1.34, self.__metric_repository)
        vsize = process_status_usage.rss()
        self.assertEqual(vsize, 1)

    def test_mem(self):
        process_status_usage = ProcessStatusUtil(1, 1.34, self.__metric_repository)
        vsize = process_status_usage.mem()
        self.assertEqual(vsize, 100)

    def test_pid(self):
        process_status_usage = ProcessStatusUtil(1, 1.34, self.__metric_repository)
        pid = process_status_usage.pid()
        self.assertEqual(pid,'1       ')       

    def test_process_name(self):
        process_status_usage = ProcessStatusUtil(1, 1.34, self.__metric_repository)
        name = process_status_usage.process_name()
        self.assertEqual(name, 'test                ')

    def test_user_id(self):
        process_status_usage = ProcessStatusUtil(1, 1.34, self.__metric_repository)
        user_id = process_status_usage.user_id()
        self.assertEqual(user_id, 1)

    def test_s_name(self):
        process_status_usage = ProcessStatusUtil(1, 1.34, self.__metric_repository)
        name = process_status_usage.s_name()
        self.assertEqual(name, 'R')

    def test_cpu_number(self):
        process_status_usage = ProcessStatusUtil(1, 1.34, self.__metric_repository)
        name = process_status_usage.cpu_number()
        self.assertEqual(name, 1)

    def test_wchan_s(self):
        process_status_usage = ProcessStatusUtil(1, 1.34, self.__metric_repository)
        name = process_status_usage.wchan_s()
        self.assertEqual(name, 'test                          ')

    def test_priority(self):
        process_status_usage = ProcessStatusUtil(1, 1.34, self.__metric_repository)
        name = process_status_usage.priority()
        self.assertEqual(name, 1)

    def test_tty_name(self):
        process_status_usage = ProcessStatusUtil(1, 1.34, self.__metric_repository)
        name = process_status_usage.tty_name()
        self.assertEqual(name, 'tty')

    def test_start_time(self):
        process_status_usage = ProcessStatusUtil(1, 1.34, self.__metric_repository)
        name = process_status_usage.start_time()
        self.assertEqual(name, 1)

    def test_func_state(self):
        process_status_usage = ProcessStatusUtil(1, 1.34, self.__metric_repository)
        name = process_status_usage.func_state()
        self.assertEqual(name, 'N/A')

    def test_policy(self):
        process_status_usage = ProcessStatusUtil(1, 1.34, self.__metric_repository)
        name = process_status_usage.policy()
        self.assertEqual(name, 'FIFO')


if __name__ == '__main__':
    unittest.main()
