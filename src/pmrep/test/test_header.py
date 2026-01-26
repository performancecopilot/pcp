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

"""Tests for HeaderFormatter class - header generation for pmrep stdout output"""

import sys
import os
import unittest

# Add parent directory to path so we can import modules directly
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from header import HeaderFormatter


class TestHeaderFormatterInit(unittest.TestCase):
    """Tests for HeaderFormatter initialization"""

    def test_default_delimiter(self):
        """Default delimiter is two spaces"""
        formatter = HeaderFormatter()
        self.assertEqual(formatter.delimiter, "  ")

    def test_custom_delimiter(self):
        """Custom delimiter is accepted"""
        formatter = HeaderFormatter(delimiter=",")
        self.assertEqual(formatter.delimiter, ",")

    def test_default_no_timestamp(self):
        """Timestamp is disabled by default"""
        formatter = HeaderFormatter()
        self.assertEqual(formatter.timestamp_width, 0)

    def test_custom_timestamp_width(self):
        """Custom timestamp width is accepted"""
        formatter = HeaderFormatter(timestamp_width=8)
        self.assertEqual(formatter.timestamp_width, 8)


class TestBuildFormatString(unittest.TestCase):
    """Tests for build_format_string method"""

    def test_single_column_no_timestamp(self):
        """Single column without timestamp produces correct format"""
        formatter = HeaderFormatter(delimiter="  ", timestamp_width=0)
        fmt = formatter.build_format_string([8])

        # Format should have placeholder for timestamp (empty) + delimiter + one column
        # {0:}{1}{2:>8.8}
        self.assertIn("{0:}", fmt)  # Empty timestamp placeholder
        self.assertIn(":>8.8}", fmt)  # 8-wide right-aligned column

    def test_single_column_with_timestamp(self):
        """Single column with timestamp produces correct format"""
        formatter = HeaderFormatter(delimiter="  ", timestamp_width=8)
        fmt = formatter.build_format_string([6])

        # Format should have timestamp + delimiter + column
        self.assertIn("{0:<8}", fmt)  # 8-wide left-aligned timestamp
        self.assertIn(":>6.6}", fmt)  # 6-wide right-aligned column

    def test_multiple_columns(self):
        """Multiple columns produce correct format with delimiters"""
        formatter = HeaderFormatter(delimiter=" ", timestamp_width=0)
        fmt = formatter.build_format_string([5, 8, 6])

        # Should have placeholders for each column with delimiters between
        self.assertIn(":>5.5}", fmt)
        self.assertIn(":>8.8}", fmt)
        self.assertIn(":>6.6}", fmt)

    def test_empty_column_list(self):
        """Empty column list produces timestamp-only format"""
        formatter = HeaderFormatter(delimiter="  ", timestamp_width=8)
        fmt = formatter.build_format_string([])

        self.assertIn("{0:<8}", fmt)
        # No column specifiers
        self.assertNotIn(":>", fmt)

    def test_format_string_can_be_used(self):
        """Generated format string works with Python str.format()"""
        formatter = HeaderFormatter(delimiter=" ", timestamp_width=0)
        fmt = formatter.build_format_string([5, 5])

        # Build values list: [timestamp, delim, col1, delim, col2]
        values = ["", " ", "name1", " ", "name2"]
        result = fmt.format(*values)

        self.assertIn("name1", result)
        self.assertIn("name2", result)


class TestBuildHeaderValues(unittest.TestCase):
    """Tests for build_header_values method"""

    def test_single_value_no_timestamp(self):
        """Single value without timestamp"""
        formatter = HeaderFormatter(delimiter="  ", timestamp_width=0)
        values = formatter.build_header_values(["cpu"])

        # Should be: ["", delimiter, "cpu"]
        self.assertEqual(values, ["", "  ", "cpu"])

    def test_single_value_with_timestamp(self):
        """Timestamp placeholder is empty in header"""
        formatter = HeaderFormatter(delimiter="  ", timestamp_width=8)
        values = formatter.build_header_values(["cpu"])

        # Timestamp is empty in header row: ["", delimiter, "cpu"]
        self.assertEqual(values[0], "")  # Empty timestamp
        self.assertEqual(values[-1], "cpu")  # Value

    def test_multiple_values(self):
        """Multiple values with delimiters between"""
        formatter = HeaderFormatter(delimiter=",", timestamp_width=0)
        values = formatter.build_header_values(["cpu", "mem", "disk"])

        # Should be: ["", ",", "cpu", ",", "mem", ",", "disk"]
        self.assertEqual(len(values), 7)
        self.assertEqual(values[2], "cpu")
        self.assertEqual(values[4], "mem")
        self.assertEqual(values[6], "disk")

    def test_empty_values(self):
        """Empty values list produces minimal output"""
        formatter = HeaderFormatter(delimiter="  ", timestamp_width=0)
        values = formatter.build_header_values([])

        # Should just have empty timestamp placeholder
        self.assertEqual(values, ["", "  "])


class TestFormatHeaderRow(unittest.TestCase):
    """Tests for format_header_row convenience method"""

    def test_formats_complete_row(self):
        """Formats a complete header row string"""
        formatter = HeaderFormatter(delimiter=" ", timestamp_width=0)
        row = formatter.format_header_row([5, 5], ["name1", "name2"])

        self.assertIn("name1", row)
        self.assertIn("name2", row)

    def test_respects_column_widths(self):
        """Values are right-aligned to column widths"""
        formatter = HeaderFormatter(delimiter=" ", timestamp_width=0)
        row = formatter.format_header_row([8], ["cpu"])

        # "cpu" right-aligned in 8 chars = 5 spaces + "cpu"
        self.assertIn("     cpu", row)

    def test_truncates_long_values(self):
        """Values longer than width are truncated"""
        formatter = HeaderFormatter(delimiter=" ", timestamp_width=0)
        row = formatter.format_header_row([3], ["longname"])

        # Should be truncated to 3 chars
        self.assertIn("lon", row)
        self.assertNotIn("longname", row)


class TestIntegration(unittest.TestCase):
    """Integration tests for realistic header scenarios"""

    def test_vmstat_style_header(self):
        """vmstat-style header with multiple columns"""
        formatter = HeaderFormatter(delimiter=" ", timestamp_width=8)
        widths = [4, 4, 8, 8, 8, 5, 5]
        names = ["r", "b", "free", "buff", "cache", "si", "so"]

        row = formatter.format_header_row(widths, names)

        # All column names should appear
        for name in names:
            self.assertIn(name, row)

    def test_csv_style_delimiter(self):
        """CSV-style output with comma delimiter"""
        formatter = HeaderFormatter(delimiter=",", timestamp_width=0)
        widths = [10, 10, 10]
        names = ["metric.a", "metric.b", "metric.c"]

        row = formatter.format_header_row(widths, names)

        # Should have commas between values
        self.assertIn(",", row)


if __name__ == '__main__':
    unittest.main()
