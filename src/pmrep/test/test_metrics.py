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

"""Tests for MetricRepository - mockable metric access abstraction for pmrep"""

import sys
import os
import unittest
from unittest.mock import Mock, MagicMock

# Add parent directory to path so we can import modules directly
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from metrics import MetricRepository


class TestMetricRepositoryInit(unittest.TestCase):
    """Tests for MetricRepository initialization"""

    def test_stores_pmconfig(self):
        """MetricRepository stores the pmconfig reference"""
        mock_pmconfig = Mock()
        mock_ts = Mock()
        repo = MetricRepository(mock_pmconfig, mock_ts)

        self.assertIs(repo._pmconfig, mock_pmconfig)

    def test_stores_timestamp_callable(self):
        """MetricRepository stores the timestamp callable"""
        mock_pmconfig = Mock()
        mock_ts = Mock()
        repo = MetricRepository(mock_pmconfig, mock_ts)

        self.assertIs(repo._pmfg_ts, mock_ts)


class TestGetRankedResults(unittest.TestCase):
    """Tests for get_ranked_results delegation"""

    def setUp(self):
        self.mock_pmconfig = Mock()
        self.mock_pmfg_ts = Mock(return_value="12:00:00")
        self.repo = MetricRepository(self.mock_pmconfig, self.mock_pmfg_ts)

    def test_delegates_to_pmconfig(self):
        """get_ranked_results delegates to pmconfig"""
        expected = {'metric': [(0, 'inst', 42)]}
        self.mock_pmconfig.get_ranked_results.return_value = expected

        result = self.repo.get_ranked_results()

        self.assertEqual(result, expected)
        self.mock_pmconfig.get_ranked_results.assert_called_once_with(valid_only=True)

    def test_passes_valid_only_true(self):
        """valid_only=True is passed to pmconfig"""
        self.repo.get_ranked_results(valid_only=True)

        self.mock_pmconfig.get_ranked_results.assert_called_once_with(valid_only=True)

    def test_passes_valid_only_false(self):
        """valid_only=False is passed to pmconfig"""
        self.repo.get_ranked_results(valid_only=False)

        self.mock_pmconfig.get_ranked_results.assert_called_once_with(valid_only=False)

    def test_returns_pmconfig_result(self):
        """Returns exactly what pmconfig returns"""
        complex_result = {
            'cpu.user': [(0, 'cpu0', 10.5), (1, 'cpu1', 20.3)],
            'mem.free': [(None, None, 1024000)]
        }
        self.mock_pmconfig.get_ranked_results.return_value = complex_result

        result = self.repo.get_ranked_results()

        self.assertEqual(result, complex_result)


class TestFetch(unittest.TestCase):
    """Tests for fetch delegation"""

    def setUp(self):
        self.mock_pmconfig = Mock()
        self.mock_pmfg_ts = Mock()
        self.repo = MetricRepository(self.mock_pmconfig, self.mock_pmfg_ts)

    def test_delegates_to_pmconfig(self):
        """fetch delegates to pmconfig.fetch"""
        self.mock_pmconfig.fetch.return_value = 0

        result = self.repo.fetch()

        self.assertEqual(result, 0)
        self.mock_pmconfig.fetch.assert_called_once()

    def test_returns_error_code(self):
        """Returns error code from pmconfig.fetch"""
        self.mock_pmconfig.fetch.return_value = -1

        result = self.repo.fetch()

        self.assertEqual(result, -1)


class TestPause(unittest.TestCase):
    """Tests for pause delegation"""

    def setUp(self):
        self.mock_pmconfig = Mock()
        self.mock_pmfg_ts = Mock()
        self.repo = MetricRepository(self.mock_pmconfig, self.mock_pmfg_ts)

    def test_delegates_to_pmconfig(self):
        """pause delegates to pmconfig.pause"""
        self.repo.pause()

        self.mock_pmconfig.pause.assert_called_once()


class TestTimestamp(unittest.TestCase):
    """Tests for timestamp method"""

    def setUp(self):
        self.mock_pmconfig = Mock()
        self.mock_pmfg_ts = Mock(return_value="12:00:00")
        self.repo = MetricRepository(self.mock_pmconfig, self.mock_pmfg_ts)

    def test_calls_timestamp_callable(self):
        """timestamp calls the pmfg_ts callable"""
        result = self.repo.timestamp()

        self.assertEqual(result, "12:00:00")
        self.mock_pmfg_ts.assert_called_once()

    def test_returns_datetime_object(self):
        """timestamp can return datetime objects"""
        from datetime import datetime
        ts = datetime(2024, 1, 15, 12, 30, 45)
        self.mock_pmfg_ts.return_value = ts

        result = self.repo.timestamp()

        self.assertEqual(result, ts)


class TestInstsProperty(unittest.TestCase):
    """Tests for insts property"""

    def setUp(self):
        self.mock_pmconfig = Mock()
        self.mock_pmfg_ts = Mock()
        self.repo = MetricRepository(self.mock_pmconfig, self.mock_pmfg_ts)

    def test_returns_pmconfig_insts(self):
        """insts property returns pmconfig.insts"""
        expected_insts = [([0, 1], ['cpu0', 'cpu1'])]
        self.mock_pmconfig.insts = expected_insts

        result = self.repo.insts

        self.assertEqual(result, expected_insts)

    def test_empty_insts(self):
        """insts property handles empty list"""
        self.mock_pmconfig.insts = []

        result = self.repo.insts

        self.assertEqual(result, [])


class TestDescsProperty(unittest.TestCase):
    """Tests for descs property"""

    def setUp(self):
        self.mock_pmconfig = Mock()
        self.mock_pmfg_ts = Mock()
        self.repo = MetricRepository(self.mock_pmconfig, self.mock_pmfg_ts)

    def test_returns_pmconfig_descs(self):
        """descs property returns pmconfig.descs"""
        mock_desc = Mock()
        mock_desc.contents.indom = 0
        expected_descs = [mock_desc]
        self.mock_pmconfig.descs = expected_descs

        result = self.repo.descs

        self.assertEqual(result, expected_descs)


class TestPmidsProperty(unittest.TestCase):
    """Tests for pmids property"""

    def setUp(self):
        self.mock_pmconfig = Mock()
        self.mock_pmfg_ts = Mock()
        self.repo = MetricRepository(self.mock_pmconfig, self.mock_pmfg_ts)

    def test_returns_pmconfig_pmids(self):
        """pmids property returns pmconfig.pmids"""
        expected_pmids = [123, 456, 789]
        self.mock_pmconfig.pmids = expected_pmids

        result = self.repo.pmids

        self.assertEqual(result, expected_pmids)


class TestTextsProperty(unittest.TestCase):
    """Tests for texts property"""

    def setUp(self):
        self.mock_pmconfig = Mock()
        self.mock_pmfg_ts = Mock()
        self.repo = MetricRepository(self.mock_pmconfig, self.mock_pmfg_ts)

    def test_returns_pmconfig_texts(self):
        """texts property returns pmconfig.texts"""
        expected_texts = [("help", "long help", None, None)]
        self.mock_pmconfig.texts = expected_texts

        result = self.repo.texts

        self.assertEqual(result, expected_texts)


class TestLabelsProperty(unittest.TestCase):
    """Tests for labels property"""

    def setUp(self):
        self.mock_pmconfig = Mock()
        self.mock_pmfg_ts = Mock()
        self.repo = MetricRepository(self.mock_pmconfig, self.mock_pmfg_ts)

    def test_returns_pmconfig_labels(self):
        """labels property returns pmconfig.labels"""
        expected_labels = [({0: {'hostname': 'localhost'}}, {})]
        self.mock_pmconfig.labels = expected_labels

        result = self.repo.labels

        self.assertEqual(result, expected_labels)


class TestResLabelsProperty(unittest.TestCase):
    """Tests for res_labels property"""

    def setUp(self):
        self.mock_pmconfig = Mock()
        self.mock_pmfg_ts = Mock()
        self.repo = MetricRepository(self.mock_pmconfig, self.mock_pmfg_ts)

    def test_returns_pmconfig_res_labels(self):
        """res_labels property returns pmconfig.res_labels"""
        expected = {'metric': ({}, {})}
        self.mock_pmconfig.res_labels = expected

        result = self.repo.res_labels

        self.assertEqual(result, expected)


class TestGetLabelsStr(unittest.TestCase):
    """Tests for get_labels_str delegation"""

    def setUp(self):
        self.mock_pmconfig = Mock()
        self.mock_pmfg_ts = Mock()
        self.repo = MetricRepository(self.mock_pmconfig, self.mock_pmfg_ts)

    def test_delegates_to_pmconfig(self):
        """get_labels_str delegates to pmconfig"""
        self.mock_pmconfig.get_labels_str.return_value = '{"hostname":"localhost"}'

        result = self.repo.get_labels_str('metric', 0, True, True)

        self.assertEqual(result, '{"hostname":"localhost"}')
        self.mock_pmconfig.get_labels_str.assert_called_once_with('metric', 0, True, True)

    def test_passes_all_arguments(self):
        """All arguments are passed to pmconfig"""
        self.repo.get_labels_str('cpu.user', 5, False, False)

        self.mock_pmconfig.get_labels_str.assert_called_once_with('cpu.user', 5, False, False)


class TestUpdateMetrics(unittest.TestCase):
    """Tests for update_metrics delegation"""

    def setUp(self):
        self.mock_pmconfig = Mock()
        self.mock_pmfg_ts = Mock()
        self.repo = MetricRepository(self.mock_pmconfig, self.mock_pmfg_ts)

    def test_delegates_to_pmconfig(self):
        """update_metrics delegates to pmconfig"""
        self.repo.update_metrics()

        self.mock_pmconfig.update_metrics.assert_called_once_with(curr_insts=True)

    def test_passes_curr_insts_false(self):
        """curr_insts=False is passed to pmconfig"""
        self.repo.update_metrics(curr_insts=False)

        self.mock_pmconfig.update_metrics.assert_called_once_with(curr_insts=False)


class TestIntegration(unittest.TestCase):
    """Integration tests demonstrating typical usage patterns"""

    def test_typical_fetch_cycle(self):
        """Simulates a typical pmrep fetch-and-report cycle"""
        mock_pmconfig = Mock()
        mock_pmfg_ts = Mock()
        repo = MetricRepository(mock_pmconfig, mock_pmfg_ts)

        # Setup mock return values
        from datetime import datetime
        ts = datetime(2024, 1, 15, 12, 0, 0)
        mock_pmfg_ts.return_value = ts
        mock_pmconfig.fetch.return_value = 0
        mock_pmconfig.get_ranked_results.return_value = {
            'cpu.user': [(0, 'cpu0', 45.2)],
            'mem.free': [(None, None, 2048000)]
        }
        mock_pmconfig.insts = [
            ([0], ['cpu0']),
            ([None], [None])
        ]

        # Typical cycle: fetch -> get timestamp -> get results
        fetch_result = repo.fetch()
        timestamp = repo.timestamp()
        results = repo.get_ranked_results()

        self.assertEqual(fetch_result, 0)
        self.assertEqual(timestamp, ts)
        self.assertEqual(len(results), 2)
        self.assertIn('cpu.user', results)

    def test_mockable_for_testing(self):
        """Demonstrates how MetricRepository enables mocking for tests"""
        # Create a mock repository directly (what tests would do)
        mock_repo = Mock(spec=MetricRepository)
        mock_repo.get_ranked_results.return_value = {'test.metric': [(0, 'inst', 100)]}
        mock_repo.fetch.return_value = 0
        mock_repo.insts = [([0], ['inst'])]

        # Code under test would use the mock exactly like the real thing
        mock_repo.fetch()
        results = mock_repo.get_ranked_results()

        self.assertEqual(results['test.metric'][0][2], 100)


if __name__ == '__main__':
    unittest.main()
