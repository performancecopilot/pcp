#!/usr/bin/env python3
#
# Copyright (c) 2025 Red Hat.
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

"""Tests for configuration dataclasses"""

import sys
import os
import unittest

# Add parent directory to path so we can import config.py directly
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from config import OutputConfig, FilterConfig, ScaleConfig


class TestOutputConfig(unittest.TestCase):
    """Tests for OutputConfig dataclass"""

    def test_default_values(self):
        """OutputConfig has sensible defaults"""
        config = OutputConfig()
        self.assertEqual(config.output, "stdout")
        self.assertIsNone(config.outfile)
        self.assertEqual(config.delimiter, "  ")
        self.assertTrue(config.header)
        self.assertTrue(config.instinfo)
        self.assertTrue(config.unitinfo)
        self.assertFalse(config.timestamp)
        self.assertEqual(config.width, 0)
        self.assertEqual(config.precision, 3)

    def test_custom_values(self):
        """OutputConfig accepts custom values"""
        config = OutputConfig(
            output="csv",
            delimiter=",",
            header=False,
            width=10,
            precision=2
        )
        self.assertEqual(config.output, "csv")
        self.assertEqual(config.delimiter, ",")
        self.assertFalse(config.header)
        self.assertEqual(config.width, 10)
        self.assertEqual(config.precision, 2)

    def test_header_options(self):
        """OutputConfig header options are independent"""
        config = OutputConfig(
            extheader=True,
            fixed_header=True,
            repeat_header=10,
            dynamic_header=False,
            separate_header=True
        )
        self.assertTrue(config.extheader)
        self.assertTrue(config.fixed_header)
        self.assertEqual(config.repeat_header, 10)
        self.assertFalse(config.dynamic_header)
        self.assertTrue(config.separate_header)

    def test_force_options(self):
        """OutputConfig force options override regular options"""
        config = OutputConfig(
            width=8,
            width_force=12,
            precision=3,
            precision_force=5
        )
        self.assertEqual(config.width, 8)
        self.assertEqual(config.width_force, 12)
        self.assertEqual(config.precision, 3)
        self.assertEqual(config.precision_force, 5)

    def test_timefmt_default(self):
        """OutputConfig timefmt defaults to None"""
        config = OutputConfig()
        self.assertIsNone(config.timefmt)

    def test_extcsv_default(self):
        """OutputConfig extcsv defaults to False"""
        config = OutputConfig()
        self.assertFalse(config.extcsv)


class TestFilterConfig(unittest.TestCase):
    """Tests for FilterConfig dataclass"""

    def test_default_values(self):
        """FilterConfig has sensible defaults"""
        config = FilterConfig()
        self.assertEqual(config.rank, 0)
        self.assertFalse(config.overall_rank)
        self.assertFalse(config.overall_rank_alt)
        self.assertEqual(config.limit_filter, 0)
        self.assertFalse(config.invert_filter)
        self.assertIsNone(config.predicate)
        self.assertIsNone(config.sort_metric)
        self.assertFalse(config.omit_flat)
        self.assertFalse(config.live_filter)
        self.assertEqual(config.instances, [])

    def test_custom_values(self):
        """FilterConfig accepts custom values"""
        config = FilterConfig(
            rank=5,
            overall_rank=True,
            limit_filter=100,
            invert_filter=True,
            predicate="kernel.all.load",
            sort_metric="mem.util.free"
        )
        self.assertEqual(config.rank, 5)
        self.assertTrue(config.overall_rank)
        self.assertEqual(config.limit_filter, 100)
        self.assertTrue(config.invert_filter)
        self.assertEqual(config.predicate, "kernel.all.load")
        self.assertEqual(config.sort_metric, "mem.util.free")

    def test_instances_list(self):
        """FilterConfig instances is a proper list"""
        config = FilterConfig(instances=["cpu0", "cpu1"])
        self.assertEqual(config.instances, ["cpu0", "cpu1"])

    def test_limit_filter_force(self):
        """FilterConfig has limit_filter_force option"""
        config = FilterConfig(limit_filter=50, limit_filter_force=100)
        self.assertEqual(config.limit_filter, 50)
        self.assertEqual(config.limit_filter_force, 100)


class TestScaleConfig(unittest.TestCase):
    """Tests for ScaleConfig dataclass"""

    def test_default_values(self):
        """ScaleConfig has None defaults"""
        config = ScaleConfig()
        self.assertIsNone(config.count_scale)
        self.assertIsNone(config.count_scale_force)
        self.assertIsNone(config.space_scale)
        self.assertIsNone(config.space_scale_force)
        self.assertIsNone(config.time_scale)
        self.assertIsNone(config.time_scale_force)

    def test_custom_values(self):
        """ScaleConfig accepts custom values"""
        config = ScaleConfig(
            count_scale="K",
            space_scale="MB",
            time_scale="sec"
        )
        self.assertEqual(config.count_scale, "K")
        self.assertEqual(config.space_scale, "MB")
        self.assertEqual(config.time_scale, "sec")

    def test_force_overrides(self):
        """ScaleConfig force options are independent"""
        config = ScaleConfig(
            count_scale="K",
            count_scale_force="M",
            space_scale="MB",
            space_scale_force="GB"
        )
        self.assertEqual(config.count_scale, "K")
        self.assertEqual(config.count_scale_force, "M")
        self.assertEqual(config.space_scale, "MB")
        self.assertEqual(config.space_scale_force, "GB")


class TestConfigImmutability(unittest.TestCase):
    """Tests for config immutability (frozen dataclasses)"""

    def test_output_config_is_frozen(self):
        """OutputConfig should be immutable"""
        config = OutputConfig()
        with self.assertRaises(Exception):
            config.output = "csv"

    def test_filter_config_is_frozen(self):
        """FilterConfig should be immutable"""
        config = FilterConfig()
        with self.assertRaises(Exception):
            config.rank = 10

    def test_scale_config_is_frozen(self):
        """ScaleConfig should be immutable"""
        config = ScaleConfig()
        with self.assertRaises(Exception):
            config.count_scale = "K"


if __name__ == '__main__':
    unittest.main()
