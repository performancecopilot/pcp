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


class TestMacstatConfigParsing(unittest.TestCase):
    """Test that the full macstat config can be parsed without PM_ERR_NAME errors"""

    def test_all_macstat_keys_are_recognized(self):
        """Test that all keys from macstat config are either in keys or keys_ignore"""
        reporter = pmrep.PMReporter()

        # All keys from the [macstat] config section (excluding metric definitions)
        macstat_config_keys = [
            'header',
            'unitinfo',
            'globals',
            'timestamp',
            'precision',
            'delimiter',
            'repeat_header',
            'groupalign',
            'groupsep',
            'groupsep_data',
            'group.memory',
            'group.memory.prefix',
            'group.memory.label',
        ]

        # Verify each key is either recognized or ignored
        for key in macstat_config_keys:
            is_recognized = key in reporter.keys
            is_ignored = key in reporter.keys_ignore
            is_metric = '=' in key or ('.' in key and not key.startswith('group.'))

            self.assertTrue(
                is_recognized or is_ignored or is_metric,
                f"Key '{key}' from macstat config is not recognized: "
                f"not in keys={is_recognized}, not in keys_ignore={is_ignored}"
            )

    def test_macstat_group_keys_properly_ignored(self):
        """Test that all group.* keys from macstat are in keys_ignore"""
        reporter = pmrep.PMReporter()

        macstat_group_keys = [
            'group.memory',
            'group.memory.prefix',
            'group.memory.label',
        ]

        for key in macstat_group_keys:
            self.assertIn(key, reporter.keys_ignore,
                         f"macstat group key '{key}' should be in keys_ignore")

    def test_macstat_option_keys_in_keys(self):
        """Test that all option keys from macstat are in keys"""
        reporter = pmrep.PMReporter()

        macstat_option_keys = [
            'header',
            'unitinfo',
            'globals',
            'timestamp',
            'precision',
            'delimiter',
            'repeat_header',
            'groupalign',
            'groupsep',
            'groupsep_data',
        ]

        for key in macstat_option_keys:
            self.assertIn(key, reporter.keys,
                         f"macstat option '{key}' should be in keys")

    def test_macstat_metrics_not_in_keys_or_ignore(self):
        """Test that metric definitions are neither in keys nor keys_ignore"""
        reporter = pmrep.PMReporter()

        # Sample metric definitions from macstat (these should be parsed as metrics)
        macstat_metrics = [
            'kernel.all.load',
            'mem.util.free',
            'mem.util.wired',
            'mem.util.active',
            'mem.pageins',
            'mem.pageouts',
            'disk.all.read',
            'disk.all.write',
            'net_in',
            'net_out',
            'usr',
            'sys',
            'idle',
        ]

        for metric in macstat_metrics:
            self.assertNotIn(metric, reporter.keys,
                           f"Metric '{metric}' should not be in keys")
            self.assertNotIn(metric, reporter.keys_ignore,
                           f"Metric '{metric}' should not be in keys_ignore")

    def test_macstat_derived_metric_attributes_not_in_keys(self):
        """Test that derived metric attributes (formula, label, etc.) are not in keys"""
        reporter = pmrep.PMReporter()

        # Derived metric attribute keys from macstat
        derived_attrs = [
            'net_in.label',
            'net_in.formula',
            'net_in.unit',
            'net_in.width',
            'usr.label',
            'usr.formula',
            'usr.unit',
            'usr.width',
        ]

        for attr in derived_attrs:
            self.assertNotIn(attr, reporter.keys,
                           f"Derived metric attribute '{attr}' should not be in keys")
            self.assertNotIn(attr, reporter.keys_ignore,
                           f"Derived metric attribute '{attr}' should not be in keys_ignore")

if __name__ == '__main__':
    unittest.main()
