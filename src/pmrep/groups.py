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

    def __init__(self, handle, columns, label=None, align='center', prefix=None):
        """Initialize GroupConfig

        Args:
            handle: Unique identifier for the group
            columns: List of column names in this group
            label: Display label (defaults to handle if not specified)
            align: Header alignment - 'left', 'center', or 'right'
            prefix: Optional prefix for column names
        """
        self.handle = handle
        self.columns = columns
        self.label = label if label is not None else handle
        self.align = align
        self.prefix = prefix


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
            else:  # center
                formatted = label.center(width)

            parts.append(formatted)

            # Add group separator between groups (not after last)
            if self.groupsep and i < len(spans) - 1:
                parts.append(self.groupsep)

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
