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

"""Tests for configuration option parsing"""
import unittest
import sys
import os
from io import StringIO

# Add parent directory to path to import pmrep
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

# Install PCP mocks BEFORE importing pmrep
from mock_pcp import install_mocks
install_mocks()

# Now we can import pmrep
import pmrep

class TestGroupConfigParsing(unittest.TestCase):
    """Test that group configuration options are recognized as valid keys"""

    def setUp(self):
        """Save original sys.argv and stderr"""
        self.original_argv = sys.argv
        self.original_stderr = sys.stderr
        sys.stderr = StringIO()  # Capture error output

    def tearDown(self):
        """Restore sys.argv and stderr"""
        sys.argv = self.original_argv
        sys.stderr = self.original_stderr

    def test_groupalign_is_valid_key(self):
        """Test that groupalign is recognized as a configuration option"""
        reporter = pmrep.PMReporter()
        self.assertIn('groupalign', reporter.keys,
                     "groupalign should be in valid configuration keys")

    def test_groupheader_is_valid_key(self):
        """Test that groupheader is recognized as a configuration option"""
        reporter = pmrep.PMReporter()
        self.assertIn('groupheader', reporter.keys,
                     "groupheader should be in valid configuration keys")

    def test_groupsep_is_valid_key(self):
        """Test that groupsep is recognized as a configuration option"""
        reporter = pmrep.PMReporter()
        self.assertIn('groupsep', reporter.keys,
                     "groupsep should be in valid configuration keys")

    def test_groupsep_data_is_valid_key(self):
        """Test that groupsep_data is recognized as a configuration option"""
        reporter = pmrep.PMReporter()
        self.assertIn('groupsep_data', reporter.keys,
                     "groupsep_data should be in valid configuration keys")

    def test_all_group_keys_present(self):
        """Test that all four group configuration keys are present"""
        reporter = pmrep.PMReporter()
        required_keys = ['groupalign', 'groupheader', 'groupsep', 'groupsep_data']
        for key in required_keys:
            self.assertIn(key, reporter.keys,
                         f"{key} missing from configuration keys")

if __name__ == '__main__':
    unittest.main()
