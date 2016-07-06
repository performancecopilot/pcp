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
from pcp_pidstat import ProcessMemoryUtil

class TestProcessMemoryUtil(unittest.TestCase):
    def setUp(self):
        self.__metric_repository = mock.Mock()
        self.__metric_repository.current_value = mock.Mock(side_effect=self.metric_repo_current_value_side_effect)
        self.__metric_repository.previous_value = mock.Mock(side_effect=self.metric_repo_previous_value_side_effect)

    def metric_repo_current_value_side_effect(self, metric_name,instance):
        if metric_name == 'proc.psinfo.vsize' and instance == 1:
            return 120084
        if metric_name == 'proc.psinfo.rss' and instance == 1:
            return 6272
        if metric_name == 'proc.psinfo.minflt' and instance == 1:
            return 14509
        if metric_name == 'proc.psinfo.maj_flt' and instance == 1:
            return 54
        if metric_name == 'mem.physmem':
            return 3794764
        if metric_name == 'proc.psinfo.cmd' and instance == 1:
            return "test"
        if metric_name == 'proc.psinfo.processor' and instance == 1:
            return 0
        if metric_name == 'proc.id.uid' and instance == 1:
            return 1
        if metric_name == 'proc.psinfo.pid' and instance == 1:
            return 1
        if metric_name == 'proc.psinfo.vsize' and instance == 2:
            return 120084
        if metric_name == 'proc.psinfo.rss' and instance == 2:
            return 6272
        if metric_name == 'proc.psinfo.minflt' and instance == 2:
            return 14509
        if metric_name == 'proc.psinfo.maj_flt' and instance == 2:
            return 54
        return None

    def metric_repo_previous_value_side_effect(self, metric_name,instance):
        if metric_name == 'proc.psinfo.cmin_flt' and instance == 1:
            return 573930
        if metric_name == 'proc.psinfo.minflt' and instance == 1:
            return 14500
        if metric_name == 'proc.psinfo.cmaj_flt' and instance == 1:
            return 645
        if metric_name == 'proc.psinfo.maj_flt' and instance == 1:
            return 50
        if metric_name == 'proc.psinfo.cmin_flt' and instance == 3:
            return 573930
        if metric_name == 'proc.psinfo.minflt' and instance == 3:
            return 14500
        if metric_name == 'proc.psinfo.cmaj_flt' and instance == 3:
            return 645
        if metric_name == 'proc.psinfo.maj_flt' and instance == 3:
            return 50
        return None

    def test_vsize(self):
        process_memory_usage = ProcessMemoryUtil(1,1.34,self.__metric_repository)

        vsize = process_memory_usage.vsize()

        self.assertEquals(vsize, 120084)

    def test_rss(self):
        process_memory_usage = ProcessMemoryUtil(1,1.34,self.__metric_repository)

        rss = process_memory_usage.rss()

        self.assertEquals(rss, 6272)

    def test_mem(self):
        process_memory_usage = ProcessMemoryUtil(1,1.34,self.__metric_repository)

        mem = process_memory_usage.mem()

        self.assertEquals(mem, 0.17)

    def test_min_flt(self):
        process_memory_usage = ProcessMemoryUtil(1,1.34,self.__metric_repository)

        min_flt = process_memory_usage.minflt()

        self.assertEquals(min_flt, 6.72)

    def test_min_flt_if_current_value_is_None(self):
        process_memory_usage = ProcessMemoryUtil(3,1.34,self.__metric_repository)

        min_flt = process_memory_usage.minflt()

        self.assertIsNone(min_flt)

    def test_min_flt_if_previous_value_is_None(self):
        process_memory_usage = ProcessMemoryUtil(2,1.34,self.__metric_repository)

        min_flt = process_memory_usage.minflt()

        self.assertIsNone(min_flt)

    def test_maj_flt(self):
        process_memory_usage = ProcessMemoryUtil(1,1.34,self.__metric_repository)

        maj_flt = process_memory_usage.majflt()

        self.assertEquals(maj_flt, 2.99)

    def test_maj_flt_if_current_value_is_None(self):
        process_memory_usage = ProcessMemoryUtil(3,1.34,self.__metric_repository)

        maj_flt = process_memory_usage.majflt()

        self.assertIsNone(maj_flt)


    def test_maj_flt_if_previous_value_is_None(self):
        process_memory_usage = ProcessMemoryUtil(2,1.34,self.__metric_repository)

        maj_flt = process_memory_usage.majflt()

        self.assertIsNone(maj_flt)

    def test_pid(self):
        process_memory_usage = ProcessMemoryUtil(1,1.34,self.__metric_repository)

        pid = process_memory_usage.pid()

        self.assertEqual(pid,1)

    def test_process_name(self):
        process_memory_usage = ProcessMemoryUtil(1,1.34,self.__metric_repository)

        name = process_memory_usage.process_name()

        self.assertEqual(name,'test')


    def test_user_id(self):
        process_memory_usage = ProcessMemoryUtil(1,1.34,self.__metric_repository)

        user_id = process_memory_usage.user_id()

        self.assertEqual(user_id,1)


if __name__ == '__main__':
    unittest.main()
