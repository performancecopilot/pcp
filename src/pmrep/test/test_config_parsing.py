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

    def test_keys_ignore_exists(self):
        """Test that keys_ignore attribute exists for pattern-based keys"""
        reporter = pmrep.PMReporter()
        self.assertTrue(hasattr(reporter, 'keys_ignore'),
                       "PMReporter should have keys_ignore attribute")

    def test_group_definition_keys_are_ignored(self):
        """Test that group.<handle> keys are in keys_ignore"""
        reporter = pmrep.PMReporter()
        self.assertIn('group.memory', reporter.keys_ignore,
                     "group.memory should be in keys_ignore")
        self.assertIn('group.procs', reporter.keys_ignore,
                     "group.procs should be in keys_ignore")
        self.assertIn('group.cpu', reporter.keys_ignore,
                     "group.cpu should be in keys_ignore")

    def test_group_prefix_keys_are_ignored(self):
        """Test that group.<handle>.prefix keys are in keys_ignore"""
        reporter = pmrep.PMReporter()
        self.assertIn('group.memory.prefix', reporter.keys_ignore,
                     "group.memory.prefix should be in keys_ignore")

    def test_group_label_keys_are_ignored(self):
        """Test that group.<handle>.label keys are in keys_ignore"""
        reporter = pmrep.PMReporter()
        self.assertIn('group.memory.label', reporter.keys_ignore,
                     "group.memory.label should be in keys_ignore")

    def test_group_align_keys_are_ignored(self):
        """Test that group.<handle>.align keys are in keys_ignore"""
        reporter = pmrep.PMReporter()
        self.assertIn('group.cpu.align', reporter.keys_ignore,
                     "group.cpu.align should be in keys_ignore")

    def test_non_group_keys_not_ignored(self):
        """Test that non-group keys are not in keys_ignore"""
        reporter = pmrep.PMReporter()
        self.assertNotIn('header', reporter.keys_ignore,
                        "header should not be in keys_ignore")
        self.assertNotIn('groupalign', reporter.keys_ignore,
                        "groupalign should not be in keys_ignore")
        self.assertNotIn('mem.util.free', reporter.keys_ignore,
                        "mem.util.free should not be in keys_ignore")

if __name__ == '__main__':
    unittest.main()
