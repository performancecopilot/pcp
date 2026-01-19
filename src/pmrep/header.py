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

"""Header formatting for pmrep stdout output

This module provides HeaderFormatter, a class that generates format strings
and header row content for pmrep's stdout output mode. It encapsulates the
format string building logic that was previously embedded in PMReporter.
"""


class HeaderFormatter:
    """Builds format strings and header rows for pmrep stdout output

    This class encapsulates the logic for:
    - Building Python format strings with proper column widths
    - Generating header value lists (names, instances, units)
    - Formatting complete header row strings

    The format string structure follows pmrep conventions:
    - Position 0: timestamp (empty for header rows)
    - Position 1: delimiter after timestamp
    - Position 2, 4, 6...: column values
    - Position 3, 5, 7...: delimiters between columns
    """

    def __init__(self, delimiter="  ", timestamp_width=0):
        """Initialize HeaderFormatter

        Args:
            delimiter: String to place between columns (default: two spaces)
            timestamp_width: Width for timestamp column, 0 to disable timestamp
        """
        self.delimiter = delimiter
        self.timestamp_width = timestamp_width

    def build_format_string(self, column_widths):
        """Build a Python format string for the given column widths

        Args:
            column_widths: List of integer widths for each column

        Returns:
            A format string suitable for str.format() with positional arguments
        """
        index = 0

        # Timestamp placeholder
        if self.timestamp_width == 0:
            fmt = "{0:}{1}"
            index = 2
        else:
            fmt = "{0:<" + str(self.timestamp_width) + "}{1}"
            index = 2

        # Add each column with delimiter between
        for width in column_widths:
            w = str(width)
            fmt += "{" + str(index) + ":>" + w + "." + w + "}"
            index += 1
            fmt += "{" + str(index) + "}"
            index += 1

        # Remove trailing delimiter placeholder if we have columns
        if column_widths:
            placeholder_len = len(str(index - 1)) + 2  # "{N}" length
            fmt = fmt[:-placeholder_len]

        return fmt

    def build_header_values(self, values):
        """Build the values list for a header row

        Args:
            values: List of column values (names, instances, or units)

        Returns:
            List suitable for format string: [timestamp, delim, val1, delim, val2, ...]
        """
        result = ["", self.delimiter]  # Timestamp empty for headers

        for value in values:
            result.append(value)
            result.append(self.delimiter)

        # Remove trailing delimiter if we have values
        if values:
            result.pop()

        return result

    def format_header_row(self, column_widths, values):
        """Format a complete header row string

        Convenience method that builds format string and values, then formats.

        Args:
            column_widths: List of integer widths for each column
            values: List of column values to format

        Returns:
            Formatted header row string
        """
        fmt = self.build_format_string(column_widths)
        header_values = self.build_header_values(values)
        return fmt.format(*header_values)
