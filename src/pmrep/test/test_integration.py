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

"""Integration tests for pmrep - testing end-to-end behavior with mocked PCP"""

import sys
import os
import unittest
from io import StringIO
from unittest.mock import Mock, MagicMock, patch

# Add parent directory to path so we can import modules directly
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

# Install PCP mocks BEFORE importing pmrep
from mock_pcp import install_mocks
install_mocks()

# Now we can import pmrep
import pmrep


class TestGroupHeaderIntegration(unittest.TestCase):
    """Integration tests for group header rendering in pmrep output"""

    def setUp(self):
        """Set up test fixtures - create PMReporter with mocked dependencies"""
        self.output = StringIO()
        self.reporter = pmrep.PMReporter()

        # Mock the writer to capture output
        self.reporter.writer = self.output

        # Set up basic configuration
        self.reporter.header = 1  # Enable headers
        self.reporter.delimiter = "  "  # Two-space delimiter
        self.reporter.format = "{}"  # Simple format for testing
        self.reporter.colxrow = None  # Not in colxrow mode
        self.reporter.separate_header = False
        self.reporter.instinfo = 0  # Disable instance info for simpler test
        self.reporter.unitinfo = 0  # Disable unit info for simpler test
        self.reporter.include_labels = False

        # Mock metrics dictionary - simple case with two groups
        self.reporter.metrics = {
            'mem.util.free': ('free', '', ('', ''), 0, 0),
            'mem.util.bufmem': ('buff', '', ('', ''), 0, 0),
            'swap.pagesin': ('si', '', ('', ''), 0, 0),
            'swap.pagesout': ('so', '', ('', ''), 0, 0)
        }

        # Mock pmconfig
        mock_desc = Mock()
        mock_desc.contents.indom = 0xffffffff  # PM_INDOM_NULL
        self.reporter.pmconfig.descs = [mock_desc, mock_desc, mock_desc, mock_desc]
        # insts format: [[instances], [names]]
        self.reporter.pmconfig.insts = [
            [[], []],  # mem.util.free - no instances
            [[], []],  # mem.util.bufmem - no instances
            [[], []],  # swap.pagesin - no instances
            [[], []]   # swap.pagesout - no instances
        ]
        self.reporter.dynamic_header = False

    def test_group_headers_not_rendered_initially(self):
        """TDD FAILING TEST: Proves group headers are NOT currently rendered

        This test MUST FAIL initially, proving the bug exists.
        After integration is complete, this test MUST PASS.
        """
        # Configure group definitions (what user would put in config file)
        # group.memory = free, bufmem
        # group.swap = pagesin, pagesout
        self.reporter.groupheader = True  # Enable group headers
        self.reporter.groupalign = 'center'
        self.reporter.groupsep = None

        # IMPORTANT: This is what we need to implement - parsing group config
        # For now, we manually set what SHOULD be parsed from config
        # In real implementation, this would come from config file parsing
        from groups import GroupConfig, GroupHeaderFormatter
        self.reporter.group_configs = [
            GroupConfig('memory', ['free', 'buff'], label='memory', align='center'),
            GroupConfig('swap', ['si', 'so'], label='swap', align='center')
        ]

        # Set up column widths (normally calculated by prepare_stdout_std)
        self.reporter.column_widths = {
            'free': 8,
            'buff': 8,
            'si': 5,
            'so': 5
        }

        # Initialize the group formatter with the configs
        self.reporter.group_formatter = GroupHeaderFormatter(
            self.reporter.group_configs,
            delimiter=self.reporter.delimiter,
            groupsep=self.reporter.groupsep
        )

        # Call write_header_stdout to generate output
        self.reporter.write_header_stdout(repeat=False, results={})

        # Get the captured output
        output = self.output.getvalue()
        lines = output.strip().split('\n')

        # DEBUG: Print what we got so we can see the failure
        print("\n=== CAPTURED OUTPUT ===")
        print(output)
        print("=== END OUTPUT ===\n")

        # Assert: Group header should be the FIRST line
        # Group header should contain 'memory' and 'swap' labels
        self.assertGreater(len(lines), 0, "Expected at least one header line")

        first_line = lines[0]
        self.assertIn('memory', first_line,
                     "Group header should contain 'memory' label")
        self.assertIn('swap', first_line,
                     "Group header should contain 'swap' label")

        # The second line should be the metric names header
        if len(lines) > 1:
            second_line = lines[1]
            self.assertIn('free', second_line,
                         "Metric header should contain column names")

    def test_group_headers_with_separator(self):
        """TDD FAILING TEST: Group headers with separator character

        Tests that groupsep='|' properly appears between groups.
        MUST FAIL initially, then PASS after integration.
        """
        from groups import GroupConfig, GroupHeaderFormatter

        self.reporter.groupheader = True
        self.reporter.groupalign = 'center'
        self.reporter.groupsep = ' | '  # Separator with spaces

        self.reporter.group_configs = [
            GroupConfig('memory', ['free', 'buff'], label='--memory--'),
            GroupConfig('swap', ['si', 'so'], label='--swap--')
        ]

        self.reporter.column_widths = {
            'free': 8,
            'buff': 8,
            'si': 5,
            'so': 5
        }

        # Initialize the group formatter
        self.reporter.group_formatter = GroupHeaderFormatter(
            self.reporter.group_configs,
            delimiter=self.reporter.delimiter,
            groupsep=self.reporter.groupsep
        )

        self.reporter.write_header_stdout(repeat=False, results={})
        output = self.output.getvalue()
        lines = output.strip().split('\n')

        print("\n=== SEPARATOR TEST OUTPUT ===")
        print(output)
        print("=== END OUTPUT ===\n")

        # First line should be group header with separator
        self.assertGreater(len(lines), 0)
        first_line = lines[0]

        self.assertIn('--memory--', first_line)
        self.assertIn('--swap--', first_line)
        self.assertIn('|', first_line, "Separator should appear between groups")

    def test_no_group_header_when_disabled(self):
        """TDD TEST: When groupheader=False, no group header should be rendered

        This test should PASS even before integration (backwards compat).
        """
        from groups import GroupConfig

        self.reporter.groupheader = False  # Explicitly disabled
        self.reporter.group_configs = [
            GroupConfig('memory', ['free', 'buff'], label='memory')
        ]
        self.reporter.column_widths = {'free': 8, 'buff': 8}

        self.reporter.write_header_stdout(repeat=False, results={})
        output = self.output.getvalue()
        lines = output.strip().split('\n')

        # Should only have metric names header, not group header
        # So 'memory' should NOT appear (it's the group label, not a metric)
        for line in lines:
            if 'memory' in line:
                # If 'memory' appears, it should be as a metric name format,
                # not as a standalone group header
                # This is a bit tricky - we're asserting the ABSENCE of group header
                pass

        # The key test: there should be exactly 1 line (just metric names)
        # Not 2 lines (group header + metric names)
        self.assertEqual(len(lines), 1,
                        "With groupheader=False, should only have metric names header")


if __name__ == '__main__':
    unittest.main()
