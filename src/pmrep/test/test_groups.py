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

"""Tests for column grouping feature - GroupConfig and GroupHeaderFormatter"""

import sys
import os
import unittest

# Add parent directory to path so we can import modules directly
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from groups import GroupConfig, GroupHeaderFormatter


class TestGroupConfigDefaults(unittest.TestCase):
    """Tests for GroupConfig default values"""

    def test_handle_is_required(self):
        """GroupConfig requires a handle"""
        group = GroupConfig('memory', ['free', 'buff'])
        self.assertEqual(group.handle, 'memory')

    def test_columns_is_required(self):
        """GroupConfig requires columns list"""
        group = GroupConfig('memory', ['free', 'buff'])
        self.assertEqual(group.columns, ['free', 'buff'])

    def test_label_defaults_to_handle(self):
        """Label defaults to handle if not specified"""
        group = GroupConfig('memory', ['free', 'buff'])
        self.assertEqual(group.label, 'memory')

    def test_align_defaults_to_center(self):
        """Align defaults to center"""
        group = GroupConfig('memory', ['free'])
        self.assertEqual(group.align, 'center')

    def test_prefix_defaults_to_none(self):
        """Prefix defaults to None"""
        group = GroupConfig('memory', ['free'])
        self.assertIsNone(group.prefix)


class TestGroupConfigCustomValues(unittest.TestCase):
    """Tests for GroupConfig custom values"""

    def test_custom_label(self):
        """Custom label overrides handle"""
        group = GroupConfig('memory', ['free'], label='mem')
        self.assertEqual(group.label, 'mem')

    def test_align_left(self):
        """Align can be set to left"""
        group = GroupConfig('memory', ['free'], align='left')
        self.assertEqual(group.align, 'left')

    def test_align_right(self):
        """Align can be set to right"""
        group = GroupConfig('memory', ['free'], align='right')
        self.assertEqual(group.align, 'right')

    def test_custom_prefix(self):
        """Custom prefix is stored"""
        group = GroupConfig('memory', ['free'], prefix='MEM')
        self.assertEqual(group.prefix, 'MEM')

    def test_single_column(self):
        """Single column works"""
        group = GroupConfig('cpu', ['user'])
        self.assertEqual(group.columns, ['user'])

    def test_many_columns(self):
        """Many columns work"""
        cols = ['user', 'system', 'idle', 'wait', 'nice']
        group = GroupConfig('cpu', cols)
        self.assertEqual(group.columns, cols)


class TestGroupHeaderFormatterInit(unittest.TestCase):
    """Tests for GroupHeaderFormatter initialization"""

    def test_stores_groups(self):
        """Stores the groups list"""
        groups = [GroupConfig('mem', ['free'])]
        formatter = GroupHeaderFormatter(groups, delimiter=' ')
        self.assertEqual(formatter.groups, groups)

    def test_stores_delimiter(self):
        """Stores the delimiter"""
        formatter = GroupHeaderFormatter([], delimiter='  ')
        self.assertEqual(formatter.delimiter, '  ')

    def test_groupsep_defaults_to_none(self):
        """Group separator defaults to None"""
        formatter = GroupHeaderFormatter([], delimiter=' ')
        self.assertIsNone(formatter.groupsep)

    def test_custom_groupsep(self):
        """Custom group separator is stored"""
        formatter = GroupHeaderFormatter([], delimiter=' ', groupsep='|')
        self.assertEqual(formatter.groupsep, '|')


class TestCalculateSpans(unittest.TestCase):
    """Tests for calculate_spans method"""

    def test_single_group_single_column(self):
        """Single group with single column"""
        groups = [GroupConfig('mem', ['free'], label='mem')]
        formatter = GroupHeaderFormatter(groups, delimiter=' ')

        spans = formatter.calculate_spans({'free': 8})

        self.assertEqual(len(spans), 1)
        self.assertEqual(spans[0][0], 'mem')  # label
        self.assertEqual(spans[0][1], 8)       # width = just column width
        self.assertEqual(spans[0][2], 'center')  # default align

    def test_single_group_two_columns(self):
        """Single group with two columns - includes delimiter"""
        groups = [GroupConfig('memory', ['free', 'buff'], label='mem')]
        formatter = GroupHeaderFormatter(groups, delimiter='  ')

        spans = formatter.calculate_spans({'free': 8, 'buff': 8})

        self.assertEqual(len(spans), 1)
        self.assertEqual(spans[0][0], 'mem')
        # Width = 8 + 2 (delimiter) + 8 = 18
        self.assertEqual(spans[0][1], 18)

    def test_single_group_three_columns(self):
        """Single group with three columns"""
        groups = [GroupConfig('memory', ['free', 'buff', 'cache'], label='memory')]
        formatter = GroupHeaderFormatter(groups, delimiter=' ')

        spans = formatter.calculate_spans({'free': 8, 'buff': 8, 'cache': 8})

        # Width = 8 + 1 + 8 + 1 + 8 = 26
        self.assertEqual(spans[0][1], 26)

    def test_multiple_groups(self):
        """Multiple groups return multiple spans"""
        groups = [
            GroupConfig('procs', ['r', 'b'], label='procs'),
            GroupConfig('memory', ['free', 'buff', 'cache'], label='memory')
        ]
        formatter = GroupHeaderFormatter(groups, delimiter=' ')

        spans = formatter.calculate_spans({
            'r': 3, 'b': 3,
            'free': 8, 'buff': 8, 'cache': 8
        })

        self.assertEqual(len(spans), 2)
        self.assertEqual(spans[0][0], 'procs')
        self.assertEqual(spans[0][1], 7)   # 3 + 1 + 3
        self.assertEqual(spans[1][0], 'memory')
        self.assertEqual(spans[1][1], 26)  # 8 + 1 + 8 + 1 + 8

    def test_preserves_align(self):
        """Alignment is preserved in span"""
        groups = [GroupConfig('mem', ['free'], align='left')]
        formatter = GroupHeaderFormatter(groups, delimiter=' ')

        spans = formatter.calculate_spans({'free': 8})

        self.assertEqual(spans[0][2], 'left')

    def test_different_column_widths(self):
        """Different column widths are summed correctly"""
        groups = [GroupConfig('stats', ['min', 'max', 'avg'])]
        formatter = GroupHeaderFormatter(groups, delimiter=' ')

        spans = formatter.calculate_spans({'min': 5, 'max': 5, 'avg': 10})

        # Width = 5 + 1 + 5 + 1 + 10 = 22
        self.assertEqual(spans[0][1], 22)

    def test_empty_groups(self):
        """Empty groups list returns empty spans"""
        formatter = GroupHeaderFormatter([], delimiter=' ')

        spans = formatter.calculate_spans({'free': 8})

        self.assertEqual(spans, [])


class TestFormatHeader(unittest.TestCase):
    """Tests for format_header method"""

    def test_center_aligned_single_span(self):
        """Center-aligned label is centered in span width"""
        groups = [GroupConfig('mem', ['a', 'b'], align='center')]
        formatter = GroupHeaderFormatter(groups, delimiter=' ')

        header = formatter.format_header([('mem', 10, 'center')])

        self.assertEqual(len(header), 10)
        self.assertIn('mem', header)
        # 'mem' is 3 chars, 10 - 3 = 7 spaces, centered = 3 or 4 on each side
        self.assertTrue(header.strip() == 'mem')

    def test_left_aligned(self):
        """Left-aligned label starts at beginning"""
        groups = [GroupConfig('mem', ['a'], align='left')]
        formatter = GroupHeaderFormatter(groups, delimiter=' ')

        header = formatter.format_header([('mem', 10, 'left')])

        self.assertTrue(header.startswith('mem'))
        self.assertEqual(len(header), 10)

    def test_right_aligned(self):
        """Right-aligned label ends at end"""
        groups = [GroupConfig('mem', ['a'], align='right')]
        formatter = GroupHeaderFormatter(groups, delimiter=' ')

        header = formatter.format_header([('mem', 10, 'right')])

        self.assertTrue(header.endswith('mem'))
        self.assertEqual(len(header), 10)

    def test_multiple_spans_without_separator(self):
        """Multiple spans without separator include delimiter for column spacing"""
        groups = [
            GroupConfig('a', ['x'], label='A'),
            GroupConfig('b', ['y'], label='B')
        ]
        formatter = GroupHeaderFormatter(groups, delimiter=' ')

        header = formatter.format_header([('A', 5, 'center'), ('B', 5, 'center')])

        # Length = 5 (span A) + 1 (delimiter) + 5 (span B) = 11
        self.assertEqual(len(header), 11)
        self.assertIn('A', header)
        self.assertIn('B', header)

    def test_multiple_spans_with_separator(self):
        """Multiple spans with separator include separator between"""
        groups = [
            GroupConfig('a', ['x'], label='A'),
            GroupConfig('b', ['y'], label='B')
        ]
        formatter = GroupHeaderFormatter(groups, delimiter=' ', groupsep='|')

        header = formatter.format_header([('A', 5, 'center'), ('B', 5, 'center')])

        self.assertIn('|', header)

    def test_label_truncated_if_too_long(self):
        """Label is truncated if longer than span width"""
        groups = [GroupConfig('verylongname', ['a'])]
        formatter = GroupHeaderFormatter(groups, delimiter=' ')

        header = formatter.format_header([('verylongname', 5, 'center')])

        self.assertEqual(len(header), 5)
        self.assertNotIn('verylongname', header)

    def test_empty_spans(self):
        """Empty spans returns empty string"""
        formatter = GroupHeaderFormatter([], delimiter=' ')

        header = formatter.format_header([])

        self.assertEqual(header, '')


class TestFormatGroupHeaderRow(unittest.TestCase):
    """Tests for format_group_header_row convenience method"""

    def test_combines_calculate_and_format(self):
        """Combines calculate_spans and format_header"""
        groups = [GroupConfig('memory', ['free', 'buff'], label='mem')]
        formatter = GroupHeaderFormatter(groups, delimiter='  ')

        header = formatter.format_group_header_row({'free': 8, 'buff': 8})

        self.assertIn('mem', header)
        self.assertEqual(len(header), 18)  # 8 + 2 + 8

    def test_multiple_groups(self):
        """Works with multiple groups"""
        groups = [
            GroupConfig('procs', ['r', 'b'], label='--procs--'),
            GroupConfig('memory', ['free'], label='--mem--')
        ]
        formatter = GroupHeaderFormatter(groups, delimiter=' ')

        header = formatter.format_group_header_row({
            'r': 5, 'b': 5,
            'free': 10
        })

        self.assertIn('--procs--', header)
        self.assertIn('--mem--', header)


class TestIntegration(unittest.TestCase):
    """Integration tests for vmstat-like output"""

    def test_vmstat_style_groups(self):
        """vmstat-style grouped header with realistic widths"""
        groups = [
            GroupConfig('procs', ['r', 'b'], label='procs'),
            GroupConfig('memory', ['swpd', 'free', 'buff', 'cache'], label='memory'),
            GroupConfig('swap', ['si', 'so'], label='swap'),
            GroupConfig('io', ['bi', 'bo'], label='io'),
            GroupConfig('system', ['in', 'cs'], label='system'),
            GroupConfig('cpu', ['us', 'sy', 'id', 'wa', 'st'], label='cpu')
        ]
        formatter = GroupHeaderFormatter(groups, delimiter=' ')

        widths = {
            'r': 4, 'b': 4,
            'swpd': 7, 'free': 7, 'buff': 7, 'cache': 7,
            'si': 5, 'so': 5,
            'bi': 6, 'bo': 6,
            'in': 5, 'cs': 5,
            'us': 3, 'sy': 3, 'id': 3, 'wa': 3, 'st': 3
        }

        header = formatter.format_group_header_row(widths)

        # All group labels should appear (labels fit within column spans)
        for group in groups:
            self.assertIn(group.label, header)


if __name__ == '__main__':
    unittest.main()
