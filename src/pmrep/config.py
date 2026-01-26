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

"""Configuration dataclasses for pmrep

These dataclasses provide structured, immutable configuration objects
that replace scattered attributes on PMReporter. Each class owns a
specific configuration domain, following Single Responsibility Principle.
"""

from dataclasses import dataclass, field
from typing import Optional, List


@dataclass(frozen=True)
class OutputConfig:
    """Output-related configuration for pmrep

    Controls how metrics are formatted and where they are written.
    """
    # Output destination
    output: str = "stdout"
    outfile: Optional[str] = None

    # Formatting
    delimiter: str = "  "
    width: int = 0
    width_force: Optional[int] = None
    precision: int = 3
    precision_force: Optional[int] = None
    timefmt: Optional[str] = None

    # Header control
    header: bool = True
    instinfo: bool = True
    unitinfo: bool = True
    timestamp: bool = False
    extheader: bool = False
    extcsv: bool = False
    fixed_header: bool = False
    repeat_header: int = 0
    dynamic_header: bool = False
    separate_header: bool = False


@dataclass(frozen=True)
class FilterConfig:
    """Filtering and ranking configuration for pmrep

    Controls which instances are included and how they are ordered.
    """
    # Ranking
    rank: int = 0
    overall_rank: bool = False
    overall_rank_alt: bool = False

    # Filtering
    limit_filter: int = 0
    limit_filter_force: int = 0
    invert_filter: bool = False
    live_filter: bool = False
    omit_flat: bool = False

    # Sorting/predicates
    predicate: Optional[str] = None
    sort_metric: Optional[str] = None

    # Instance selection
    instances: List[str] = field(default_factory=list)


@dataclass(frozen=True)
class ScaleConfig:
    """Scaling configuration for pmrep

    Controls unit scaling for count, space, and time metrics.
    """
    # Count scaling (e.g., K, M, G)
    count_scale: Optional[str] = None
    count_scale_force: Optional[str] = None

    # Space scaling (e.g., KB, MB, GB)
    space_scale: Optional[str] = None
    space_scale_force: Optional[str] = None

    # Time scaling (e.g., sec, min, hour)
    time_scale: Optional[str] = None
    time_scale_force: Optional[str] = None
