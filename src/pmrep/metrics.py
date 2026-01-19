#!/usr/bin/env pmpython
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

"""
Metric access abstraction for pmrep.

Provides a mockable interface for metric access, enabling unit testing
without requiring live PCP connections. In production, delegates to pmconfig.
In tests, can be mocked to return predetermined values.
"""


class MetricRepository:
    """
    Abstraction layer for metric access.

    This class wraps pmconfig access to make pmrep testable. In production,
    it delegates to the real pmconfig instance. In tests, this class can be
    replaced with a mock that returns predetermined values.

    This follows the same dependency injection pattern used by mpstat and
    pidstat for testability.
    """

    def __init__(self, pmconfig, pmfg_ts_callable):
        """
        Initialize MetricRepository.

        Args:
            pmconfig: The pmconfig instance to delegate to
            pmfg_ts_callable: Callable that returns the current timestamp
        """
        self._pmconfig = pmconfig
        self._pmfg_ts = pmfg_ts_callable

    def get_ranked_results(self, valid_only=True):
        """
        Get ranked metric results.

        Args:
            valid_only: If True, only return valid metric values

        Returns:
            Dict mapping metric names to list of (instance_id, instance_name, value) tuples
        """
        return self._pmconfig.get_ranked_results(valid_only=valid_only)

    def fetch(self):
        """
        Fetch new metric values.

        Returns:
            0 on success, error code on failure
        """
        return self._pmconfig.fetch()

    def pause(self):
        """Pause between samples according to the configured interval."""
        self._pmconfig.pause()

    def timestamp(self):
        """
        Get the current timestamp.

        Returns:
            The current timestamp from the fetch group
        """
        return self._pmfg_ts()

    @property
    def insts(self):
        """
        Get instance information for all metrics.

        Returns:
            List of (instance_ids, instance_names) tuples per metric
        """
        return self._pmconfig.insts

    @property
    def descs(self):
        """
        Get metric descriptors.

        Returns:
            List of metric descriptors
        """
        return self._pmconfig.descs

    @property
    def pmids(self):
        """
        Get metric PMIDs.

        Returns:
            List of PMIDs for the metrics
        """
        return self._pmconfig.pmids

    @property
    def texts(self):
        """
        Get metric help texts.

        Returns:
            List of help text tuples for each metric
        """
        return self._pmconfig.texts

    @property
    def labels(self):
        """
        Get metric labels.

        Returns:
            List of label information for each metric
        """
        return self._pmconfig.labels

    @property
    def res_labels(self):
        """
        Get result labels.

        Returns:
            Dict of result labels by metric name
        """
        return self._pmconfig.res_labels

    def get_labels_str(self, metric, inst, dynamic, json_fmt):
        """
        Get labels as a formatted string.

        Args:
            metric: The metric name
            inst: The instance
            dynamic: Whether to use dynamic header mode
            json_fmt: Whether to format as JSON

        Returns:
            Formatted labels string
        """
        return self._pmconfig.get_labels_str(metric, inst, dynamic, json_fmt)

    def update_metrics(self, curr_insts=True):
        """
        Update metrics with current instances.

        Args:
            curr_insts: Whether to use current instances
        """
        self._pmconfig.update_metrics(curr_insts=curr_insts)
