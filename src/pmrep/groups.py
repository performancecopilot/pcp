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

"""Column grouping for pmrep output

This module provides GroupConfig and GroupHeaderFormatter for creating
vmstat-style grouped column headers in pmrep output.

Example vmstat output with grouped headers:
    --procs-- -----memory----- --swap-- ---io--- --system-- -------cpu-------
     r    b   swpd   free  buff   si so   bi  bo    in   cs  us sy id wa st
"""

import os
try:
    import configparser as ConfigParser
except ImportError:
    import ConfigParser


class GroupConfig:
    """Configuration for a column group

    A group defines a set of related columns that share a common header label.
    For example, vmstat groups 'r' and 'b' columns under '--procs--'.

    Attributes:
        handle: Unique identifier for the group
        columns: List of column names belonging to this group
        label: Display label for the group header (defaults to handle)
        align: Alignment of label within span ('left', 'center', 'right')
        prefix: Optional prefix for column names (not currently used)
    """

    def __init__(self, handle, columns, label=None, **options):
        """Initialize GroupConfig

        Args:
            handle: Unique identifier for the group
            columns: List of column names in this group
            label: Display label (defaults to handle if not specified)
            **options: Optional keyword arguments:
                align: Header alignment - 'left', 'center', or 'right' (default: 'center')
                prefix: Optional prefix for column names
        """
        self.handle = handle
        self.columns = columns
        self.label = label if label is not None else handle
        self.align = options.get('align', 'center')
        self.prefix = options.get('prefix')


class GroupHeaderFormatter:
    """Formats grouped column headers for pmrep output

    This class calculates column spans for groups and formats the group
    header row that appears above the individual column headers.

    Example:
        groups = [
            GroupConfig('procs', ['r', 'b'], label='--procs--'),
            GroupConfig('memory', ['free', 'buff'], label='--memory--')
        ]
        formatter = GroupHeaderFormatter(groups, delimiter=' ')
        header = formatter.format_group_header_row({'r': 3, 'b': 3, 'free': 8, 'buff': 8})
        # Returns: '--procs-- ---memory---'
    """

    def __init__(self, groups, delimiter, groupsep=None):
        """Initialize GroupHeaderFormatter

        Args:
            groups: List of GroupConfig objects defining column groups
            delimiter: String delimiter between columns (used for width calc)
            groupsep: Optional separator string between groups in output
        """
        self.groups = groups
        self.delimiter = delimiter
        self.groupsep = groupsep

    def calculate_spans(self, column_widths):
        """Calculate the span width for each group

        The span width is the sum of column widths plus delimiters between
        columns within the group.

        Args:
            column_widths: Dict mapping column names to their display widths

        Returns:
            List of (label, width, align) tuples for each group
        """
        spans = []
        delimiter_width = len(self.delimiter)

        for group in self.groups:
            # Sum widths of all columns in this group
            total_width = 0
            for i, col in enumerate(group.columns):
                if col in column_widths:
                    total_width += column_widths[col]
                    # Add delimiter width between columns (not after last)
                    if i < len(group.columns) - 1:
                        total_width += delimiter_width

            spans.append((group.label, total_width, group.align))

        return spans

    def format_header(self, spans):
        """Format the group header row from calculated spans

        Args:
            spans: List of (label, width, align) tuples from calculate_spans

        Returns:
            Formatted header string with each label aligned within its span
        """
        if not spans:
            return ''

        parts = []
        for i, (label, width, align) in enumerate(spans):
            # Truncate label if longer than width
            if len(label) > width:
                label = label[:width]

            # Align label within the span width
            if align == 'left':
                formatted = label.ljust(width)
            elif align == 'right':
                formatted = label.rjust(width)
            else:  # center - with consistent padding (extra padding on left for odd totals)
                label_len = len(label)
                total_padding = width - label_len
                left_pad = (total_padding + 1) // 2  # Extra padding on left (not right)
                right_pad = total_padding - left_pad
                formatted = ' ' * left_pad + label + ' ' * right_pad

            parts.append(formatted)

            # Add delimiter or groupsep between groups (except after last)
            # The groupsep replaces the delimiter position to maintain alignment
            if i < len(spans) - 1:
                if self.groupsep:
                    parts.append(self.groupsep)
                else:
                    parts.append(self.delimiter)

        return ''.join(parts)

    def format_group_header_row(self, column_widths):
        """Convenience method to calculate spans and format in one call

        Args:
            column_widths: Dict mapping column names to their display widths

        Returns:
            Formatted group header row string
        """
        spans = self.calculate_spans(column_widths)
        return self.format_header(spans)

    def check_label_widths(self, column_widths):
        """Check for group labels that will be truncated and return warnings

        Args:
            column_widths: Dict mapping column names to their display widths

        Returns:
            List of warning strings for labels wider than their spans
        """
        spans = self.calculate_spans(column_widths)
        warnings = []

        for _, (label, width, _) in zip(self.groups, spans):
            if len(label) > width:
                warnings.append(
                    "Warning: Group label '{}' ({} chars) is wider than its span ({} chars) "
                    "and will be truncated".format(label, len(label), width)
                )

        return warnings


def parse_group_definitions(config_path, section, default_groupalign='center'):
    """Parse group.* definitions from config file and build GroupConfig objects

    Args:
        config_path: Path to config file or directory containing config files
        section: Config section name (e.g., 'macstat', 'vmstat-grouped')
        default_groupalign: Default alignment for groups (default: 'center')

    Returns:
        List of GroupConfig objects in config file order, or empty list if no groups
    """
    # Get config files to read
    conf_files = []
    if config_path:
        if os.path.isfile(config_path):
            conf_files.append(config_path)
        elif os.path.isdir(config_path):
            for f in sorted(os.listdir(config_path)):
                fn = os.path.join(config_path, f)
                if fn.endswith(".conf") and os.access(fn, os.R_OK) and os.path.isfile(fn):
                    conf_files.append(fn)

    if not conf_files or not section:
        return []

    # Read config file
    config = ConfigParser.ConfigParser()
    config.optionxform = str  # Preserve case
    for conf_file in conf_files:
        config.read(conf_file)

    if not config.has_section(section):
        return []

    # Find all group.* keys - preserve order from config file
    group_handles = []
    seen = set()
    for key in config.options(section):
        if key.startswith('group.'):
            parts = key.split('.')
            if len(parts) >= 2 and parts[1] not in seen:
                group_handles.append(parts[1])
                seen.add(parts[1])

    # Read groupalign option if present (override default)
    if config.has_option(section, 'groupalign'):
        default_groupalign = config.get(section, 'groupalign')

    # Parse each group
    group_configs = []
    for handle in group_handles:
        # Get group definition (list of columns)
        group_key = 'group.{}'.format(handle)
        if not config.has_option(section, group_key):
            continue

        columns_raw = config.get(section, group_key)
        columns = [col.strip() for col in columns_raw.split(',')]

        # Get group options
        prefix_key = '{}.prefix'.format(group_key)
        label_key = '{}.label'.format(group_key)
        align_key = '{}.align'.format(group_key)

        prefix = config.get(section, prefix_key) if config.has_option(section, prefix_key) else None
        label = config.get(section, label_key) if config.has_option(section, label_key) else handle
        align = config.get(section, align_key) if config.has_option(section, align_key) else default_groupalign

        # Apply prefix resolution and alias resolution
        resolved_columns = []
        for col in columns:
            if prefix:
                # Prefix specified - prepend to create full metric name
                # Don't resolve aliases when prefix is present
                resolved_columns.append('{}.{}'.format(prefix, col))
            else:
                # No prefix - check if column is an alias that needs resolution
                # If the column name is defined in config, resolve it to actual metric
                # (e.g., usrp = kernel.all.cpu.usrp)
                if config.has_option(section, col):
                    metric_spec = config.get(section, col)
                    # The first part before any comma/space is the metric name
                    actual_metric = metric_spec.split(',')[0].strip()
                    resolved_columns.append(actual_metric)
                else:
                    # Not an alias - use as-is
                    resolved_columns.append(col)

        # Create GroupConfig object
        group_config = GroupConfig(
            handle=handle,
            columns=resolved_columns,
            label=label,
            align=align,
            prefix=prefix
        )
        group_configs.append(group_config)

    return group_configs
