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
from pcp_pidstat import ProcessPriority

class TestProcessPriority(unittest.TestCase):
    def setUp(self):
        self.__metric_repository = mock.Mock()
        self.__metric_repository.current_value = mock.Mock(side_effect=self.metric_repo_current_value_side_effect)

    def metric_repo_current_value_side_effect(self, metric_name,instance):
        if metric_name == 'proc.psinfo.pid' and instance == 1:
            return 1
        if metric_name == 'proc.id.uid' and instance == 1:
            return 0
        if metric_name == 'proc.psinfo.rt_priority' and instance == 1:
            return 99
        if metric_name == 'proc.psinfo.cmd' and instance == 1:
            return "test"
        if metric_name == 'proc.psinfo.policy' and instance == 1:
            return 1

    def test_pid(self):
        process_priority = ProcessPriority(1,self.__metric_repository)

        pid = process_priority.pid()

        self.assertEqual(pid,1)

    def test_process_name(self):
        process_priority = ProcessPriority(1,self.__metric_repository)

        name = process_priority.process_name()

        self.assertEqual(name,'test')

    def test_policy(self):
        process_priority = ProcessPriority(1,self.__metric_repository)

        policy = process_priority.policy()

        self.assertEqual(policy,'FIFO')

    def test_user_id(self):
        process_priority = ProcessPriority(1,self.__metric_repository)

        user_id = process_priority.user_id()

        self.assertEqual(user_id,0)

    def test_priority(self):
        process_priority = ProcessPriority(1,self.__metric_repository)

        priority = process_priority.priority()

        self.assertEqual(priority,99)

if __name__ == '__main__':
    unittest.main()
