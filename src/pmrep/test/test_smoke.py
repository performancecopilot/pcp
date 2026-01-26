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

import unittest
import sys
import os

# Add parent directory to path so we can import pmrep.py directly
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

# Install PCP mocks BEFORE importing pmrep
from mock_pcp import install_mocks
install_mocks()

# Now we can import pmrep
import pmrep


class TestSmoke(unittest.TestCase):
    """Smoke tests to verify pmrep module can be imported"""

    def test_import_module(self):
        """Verify pmrep module can be imported"""
        self.assertTrue(hasattr(pmrep, 'PMReporter'))

    def test_has_output_constants(self):
        """Verify output target constants are defined"""
        self.assertTrue(hasattr(pmrep, 'OUTPUT_ARCHIVE'))
        self.assertTrue(hasattr(pmrep, 'OUTPUT_CSV'))
        self.assertTrue(hasattr(pmrep, 'OUTPUT_STDOUT'))

    def test_output_constant_values(self):
        """Verify output target constant values"""
        self.assertEqual(pmrep.OUTPUT_ARCHIVE, "archive")
        self.assertEqual(pmrep.OUTPUT_CSV, "csv")
        self.assertEqual(pmrep.OUTPUT_STDOUT, "stdout")

    def test_default_constants(self):
        """Verify default constants are defined"""
        self.assertTrue(hasattr(pmrep, 'CONFVER'))
        self.assertTrue(hasattr(pmrep, 'CSVSEP'))
        self.assertTrue(hasattr(pmrep, 'OUTSEP'))
        self.assertTrue(hasattr(pmrep, 'NO_VAL'))


if __name__ == '__main__':
    unittest.main()
