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

"""Tests for pure formatting functions extracted from pmrep"""

import sys
import os
import unittest

# Add parent directory to path so we can import pmrep.py directly
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

# Install PCP mocks BEFORE importing pmrep
from mock_pcp import install_mocks
install_mocks()

# Now we can import pmrep and its pure functions
from pmrep import parse_non_number, remove_delimiter, option_override


class TestParseNonNumber(unittest.TestCase):
    """Tests for parse_non_number function"""

    def test_positive_infinity_wide(self):
        """Positive infinity with sufficient width returns 'inf'"""
        self.assertEqual(parse_non_number(float('inf'), 8), 'inf')

    def test_positive_infinity_exact_width(self):
        """Positive infinity with exact width (3) returns 'inf'"""
        self.assertEqual(parse_non_number(float('inf'), 3), 'inf')

    def test_positive_infinity_narrow_width(self):
        """Positive infinity with insufficient width returns truncation marker"""
        result = parse_non_number(float('inf'), 2)
        self.assertEqual(result, '...')

    def test_negative_infinity_wide(self):
        """Negative infinity with sufficient width returns '-inf'"""
        self.assertEqual(parse_non_number(float('-inf'), 8), '-inf')

    def test_negative_infinity_exact_width(self):
        """Negative infinity with exact width (4) returns '-inf'"""
        self.assertEqual(parse_non_number(float('-inf'), 4), '-inf')

    def test_negative_infinity_narrow_width(self):
        """Negative infinity with insufficient width returns truncation marker"""
        result = parse_non_number(float('-inf'), 3)
        self.assertEqual(result, '...')

    def test_nan_wide(self):
        """NaN with sufficient width returns 'NaN'"""
        self.assertEqual(parse_non_number(float('nan'), 8), 'NaN')

    def test_nan_exact_width(self):
        """NaN with exact width (3) returns 'NaN'"""
        self.assertEqual(parse_non_number(float('nan'), 3), 'NaN')

    def test_nan_narrow_width(self):
        """NaN with insufficient width returns truncation marker"""
        result = parse_non_number(float('nan'), 2)
        self.assertEqual(result, '...')

    def test_regular_float_passthrough(self):
        """Regular float values pass through unchanged"""
        self.assertEqual(parse_non_number(42.5, 8), 42.5)

    def test_integer_passthrough(self):
        """Integer values pass through unchanged"""
        self.assertEqual(parse_non_number(42, 8), 42)

    def test_zero_passthrough(self):
        """Zero passes through unchanged"""
        self.assertEqual(parse_non_number(0.0, 8), 0.0)

    def test_negative_float_passthrough(self):
        """Negative float values pass through unchanged"""
        self.assertEqual(parse_non_number(-123.456, 8), -123.456)

    def test_default_width(self):
        """Default width parameter is 8"""
        self.assertEqual(parse_non_number(float('inf')), 'inf')


class TestRemoveDelimiter(unittest.TestCase):
    """Tests for remove_delimiter function"""

    def test_replaces_comma_with_underscore(self):
        """Comma delimiter in string is replaced with underscore"""
        result = remove_delimiter("foo,bar", ",")
        self.assertEqual(result, "foo_bar")

    def test_replaces_underscore_with_space(self):
        """Underscore delimiter in string is replaced with space"""
        result = remove_delimiter("foo_bar", "_")
        self.assertEqual(result, "foo bar")

    def test_replaces_semicolon_with_underscore(self):
        """Non-underscore delimiters are replaced with underscore"""
        result = remove_delimiter("foo;bar", ";")
        self.assertEqual(result, "foo_bar")

    def test_no_delimiter_in_string(self):
        """String without delimiter passes through unchanged"""
        result = remove_delimiter("foobar", ",")
        self.assertEqual(result, "foobar")

    def test_whitespace_delimiter_passthrough(self):
        """Whitespace delimiters do not trigger replacement"""
        result = remove_delimiter("foo bar", " ")
        self.assertEqual(result, "foo bar")

    def test_tab_delimiter_passthrough(self):
        """Tab delimiter does not trigger replacement"""
        result = remove_delimiter("foo\tbar", "\t")
        self.assertEqual(result, "foo\tbar")

    def test_non_string_integer_passthrough(self):
        """Integer values pass through unchanged"""
        result = remove_delimiter(42, ",")
        self.assertEqual(result, 42)

    def test_non_string_float_passthrough(self):
        """Float values pass through unchanged"""
        result = remove_delimiter(3.14, ",")
        self.assertEqual(result, 3.14)

    def test_none_delimiter_passthrough(self):
        """None delimiter does not trigger replacement"""
        result = remove_delimiter("foo,bar", None)
        self.assertEqual(result, "foo,bar")

    def test_empty_delimiter_passthrough(self):
        """Empty delimiter does not trigger replacement"""
        result = remove_delimiter("foo,bar", "")
        self.assertEqual(result, "foo,bar")

    def test_multiple_occurrences(self):
        """All occurrences of delimiter are replaced"""
        result = remove_delimiter("a,b,c,d", ",")
        self.assertEqual(result, "a_b_c_d")


class TestOptionOverride(unittest.TestCase):
    """Tests for option_override function"""

    def test_g_returns_1(self):
        """Option 'g' returns 1 (override)"""
        self.assertEqual(option_override('g'), 1)

    def test_H_returns_1(self):
        """Option 'H' returns 1 (override)"""
        self.assertEqual(option_override('H'), 1)

    def test_K_returns_1(self):
        """Option 'K' returns 1 (override)"""
        self.assertEqual(option_override('K'), 1)

    def test_n_returns_1(self):
        """Option 'n' returns 1 (override)"""
        self.assertEqual(option_override('n'), 1)

    def test_N_returns_1(self):
        """Option 'N' returns 1 (override)"""
        self.assertEqual(option_override('N'), 1)

    def test_p_returns_1(self):
        """Option 'p' returns 1 (override)"""
        self.assertEqual(option_override('p'), 1)

    def test_other_options_return_0(self):
        """Options not in override list return 0"""
        self.assertEqual(option_override('a'), 0)
        self.assertEqual(option_override('b'), 0)
        self.assertEqual(option_override('x'), 0)
        self.assertEqual(option_override('z'), 0)


if __name__ == '__main__':
    unittest.main()
