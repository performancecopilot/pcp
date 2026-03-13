#!/usr/bin/env pmpython
#
# Copyright (C) 2014-2018 Red Hat.
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
# pylint: disable=bad-continuation,consider-using-enumerate
#
""" Display NUMA memory allocation statistucs """

import os
import re
import signal
import sys
import time

from pcp import pmapi
from pcp import pmcc
from cpmapi import PM_CONTEXT_ARCHIVE

if sys.version >= '3':
    long = int  # python2 to python3 portability (no long() in python3)
    xrange = range  # more back-compat (xrange() is range() in python3)

NUMA_METRICS = [
    "mem.numa.alloc.hit",
    "mem.numa.alloc.miss",
    "mem.numa.alloc.foreign",
    "mem.numa.alloc.interleave_hit",
    "mem.numa.alloc.local_node",
    "mem.numa.alloc.other_node",
]

MEM_METRICS = [
    "mem.numa.util.total",
    "mem.numa.util.free",
    "mem.numa.util.used",
    "mem.numa.util.active",
    "mem.numa.util.inactive",
    "mem.numa.util.active_anon",
    "mem.numa.util.inactive_anon",
    "mem.numa.util.active_file",
    "mem.numa.util.inactive_file",
    "mem.numa.util.unevictable",
    "mem.numa.util.mlocked",
    "mem.numa.util.dirty",
    "mem.numa.util.writeback",
    "mem.numa.util.filePages",
    "mem.numa.util.mapped",
    "mem.numa.util.anonpages",
    "mem.numa.util.shmem",
    "mem.numa.util.kernelStack",
    "mem.numa.util.pageTables",
    "mem.numa.util.NFS_Unstable",
    "mem.numa.util.bounce",
    "mem.numa.util.writebackTmp",
    "mem.numa.util.filehugepages",
    "mem.numa.util.filepmdmapped",
    "mem.numa.util.slab",
    "mem.numa.util.slabReclaimable",
    "mem.numa.util.slabUnreclaimable",
    "mem.numa.util.anonhugepages",
    "mem.numa.util.shmemhugepages",
    "mem.numa.util.shmempmdmapped",
    "mem.numa.util.hugepagesTotal",
    "mem.numa.util.hugepagesFree",
    "mem.numa.util.hugepagesSurp",
    "mem.numa.util.swapCached",
    "mem.numa.util.kreclaimable",
]

SYS_METRICS = [
    'kernel.uname.nodename',
    'kernel.uname.release',
    'kernel.uname.sysname',
    'kernel.uname.machine',
    'hinv.ncpu',
]

ALL_METRICS = NUMA_METRICS + MEM_METRICS

PROCESS_METRICS = [
    "proc.psinfo.pid",
    "proc.psinfo.cmd",
    "proc.psinfo.psargs",
    "proc.numa_maps.huge",
    "proc.numa_maps.heap",
    "proc.numa_maps.stack",
    "proc.numa_maps.private",
]

PROCESS_NUMA_METRICS = [
    ("Huge", "proc.numa_maps.huge"),
    ("Heap", "proc.numa_maps.heap"),
    ("Stack", "proc.numa_maps.stack"),
    ("Private", "proc.numa_maps.private"),
]

def prefix(metric):
    last_part = metric.split('.')[-1]
    result = last_part[0].upper() + last_part[1:]
    return result

class MetricRepository:
    def __init__(self, group):
        self.group = group
        self.current_cached_values = {}
        self.previous_cached_values = {}
        self.current_cached_instance_names = {}

    def _fetch_current_values(self, metric, instance):
        if instance is not None:
            return dict(
                map(lambda x: (x[0].inst, x[2]), self.group[metric].netValues)
            )
        else:
            if self.group[metric].netValues == []:
                return None
            else:
                return self.group[metric].netValues[0][2]
    def current_values(self, metric_name):
        if self.group.get(metric_name, None) is None:
            return None
        if self.current_cached_values.get(metric_name, None) is None:
            self.current_cached_values[
                metric_name
            ] = self._fetch_current_values(metric_name, True)
        return self.current_cached_values.get(metric_name, None)

class NUMAStat:

    def __init__(self, group):
        self.group = group
        self.repo = MetricRepository(group)

    def resize(self, width):
        """ Find a suitable display width limit """
        if width == 0:
            if not sys.stdout.isatty():
                width = 1000000000        # mimic numastat(1) here
            else:
                # popen() is SAFE, command is a literal string
                (_, width) = os.popen('stty size', 'r').read().split()
                width = int(width)
            width = int(os.getenv('NUMASTAT_WIDTH', str(width)))
        return max(width, 32)

    def __format_table(self, width, nodes, data):
        null_output = False
        if not nodes:
            null_output = True
            nodes = [(0, 'Node ')]

        if "numastat" in data:
            metrics = NUMA_METRICS
            title = "NUMA memory allocation statistics (pages)"
        else:
            metrics = MEM_METRICS
            title = "Per-node system memory usage (KB)"

        total_w = max(42, int(width))
        print(title[:total_w])

        width = self.resize(width)
        maxnodes = int((width - 16) / 16)
        if maxnodes > len(nodes):        # just an initial header suffices
            header = '%30s' % ''
            for _, node in nodes:
                header += '%-12s' % node
            print(header)

        for m in metrics:
            if not null_output:
                vals = self.repo.current_values(m)
            done = 0  # reset for each metric

            # Loop through nodes in chunks of 'maxnodes'
            while done < len(nodes):
                header = '%-30s' % ''
                window = '%-20s : ' % prefix(m)

                # Slice the range we'll print in this batch
                chunk = nodes[done:done + maxnodes]

                for i, ( _, name) in enumerate(chunk):
                    header += '%-12s' % name
                    if not null_output:
                        window += '%12s' % vals[done + i]
                    else:
                        window += '%12s' % "NA"

                # Print header once per row group (not every metric)
                if done > maxnodes or maxnodes <= len(nodes):
                    print('%s\n%s' % (header, window))
                else:
                    print('%s' % window)
                done += maxnodes
        print()

    def print_mem(self, width, nodes, data):
        self.__format_table(width, nodes, data)

    def print_numa(self, width, nodes, data):
        self.__format_table(width, nodes, data)

class ProcessNUMAStat:
    def __init__(self, group, ignore_pid=None):
        self.group = group
        self.repo = MetricRepository(group)
        self.ignore_pid = ignore_pid

    def __resize(self, width):
        """ Find a suitable display width limit (matches NUMAStat.resize) """
        if width == 0:
            if not sys.stdout.isatty():
                width = 1000000000        # mimic numastat(1) here
            else:
                try:
                    # popen() is SAFE, command is a literal string
                    (_, width) = os.popen('stty size', 'r').read().split()
                    width = int(width)
                except Exception:
                    width = 80
            width = int(os.getenv('NUMASTAT_WIDTH', str(width)))
        return max(int(width), 32)

    def __normalize_value(self, value):
        if value is None:
            return ""
        if hasattr(value, "decode"):
            try:
                return value.decode("utf-8")
            except Exception:
                return value.decode("utf-8", "ignore")
        return str(value)

    def __normalize_cmdline(self, value):
        if value is None:
            return ""

        # Convert bytes → string
        if isinstance(value, bytes):
            value = value.decode("utf-8", "ignore")

        # /proc/<pid>/cmdline uses NULL separators
        if "\0" in value:
            parts = value.split("\0")
            value = " ".join(p for p in parts if p)

        return value.strip()

    def __is_missing_command(self, command):
        normalized = self.__normalize_value(command).strip()
        return normalized == "" or normalized.lower() == "(null)"

    def __parse_nodes(self, value):
        node_values = {}
        text = self.__normalize_value(value)
        if not text:
            return node_values
        for token in text.split(','):
            if ':' not in token:
                continue
            name, raw_value = token.split(':', 1)
            name = name.strip()
            if not name.startswith("node"):
                continue
            try:
                node_id = int(name[4:])
                node_values[node_id] = float(raw_value)
            except (TypeError, ValueError):
                continue
        return node_values

    def __matches_process(self, pid, selectors, haystack):
        if not selectors:
            return True
        haystack = self.__normalize_value(haystack)
        for selector in selectors:
            selector_text = self.__normalize_value(selector)
            if re.fullmatch(r"\d+", selector_text):
                try:
                    if int(selector_text) == int(pid):
                        return True
                except (TypeError, ValueError):
                    continue
                continue
            if selector_text in haystack:
                return True
        return False

    def __collect_process_categories(self, inst_id, metric_maps):
        category_values = {}
        nodes = set()
        has_data = False
        for label, metric in PROCESS_NUMA_METRICS:
            parsed = self.__parse_nodes(metric_maps[metric].get(inst_id, ""))
            if parsed:
                has_data = True
            category_values[label] = parsed
            nodes.update(parsed.keys())
        return category_values, nodes, has_data

    def __sum_categories(self, category_values):
        node_totals = {}
        for values in category_values.values():
            for node_id, value in values.items():
                node_totals[node_id] = node_totals.get(node_id, 0.0) + value
        return node_totals

    def __process_rows(self, selectors, system_nodes=None):
        rows = []
        nodes = set(system_nodes or [])
        selectors_provided = bool(selectors)
        requested_pids = set()
        for selector in selectors or []:
            selector_text = self.__normalize_value(selector)
            if re.fullmatch(r"\d+", selector_text):
                try:
                    requested_pids.add(int(selector_text))
                except (TypeError, ValueError):
                    continue
        pid_map = self.repo.current_values("proc.psinfo.pid") or {}
        command_map = self.repo.current_values("proc.psinfo.cmd") or {}
        psargs_map = self.repo.current_values("proc.psinfo.psargs") or {}
        metric_maps = {}
        for _, metric in PROCESS_NUMA_METRICS:
            metric_maps[metric] = self.repo.current_values(metric) or {}

        # Restrict scanning to processes that are actually present in proc.numa_maps.*.
        numa_inst_ids = set()
        for values in metric_maps.values():
            try:
                numa_inst_ids.update(values.keys())
            except Exception:
                continue

        for inst_id, pid in sorted(pid_map.items(), key=lambda item: item[1]):
            if inst_id not in numa_inst_ids and int(pid) not in requested_pids:
                continue
            if (
                self.ignore_pid is not None
                and int(pid) == int(self.ignore_pid)
                and int(pid) not in requested_pids
            ):
                continue
            command = command_map.get(inst_id, "")
            full_command = psargs_map.get(inst_id, command)
            if self.__is_missing_command(command):
                if int(pid) in requested_pids:
                    command = "unknown"
                    full_command = "unknown"
                else:
                    continue
            # Constrain matching to the same process set and labels as the
            # proc.numa_maps.* instance domains (e.g. `pminfo -f proc.numa_maps.heap`).
            match_text = "%s %s" % (
                self.__normalize_value(command),
                self.__normalize_cmdline(full_command),
                )
            if not self.__matches_process(pid, selectors, match_text):
                continue

            category_values, category_nodes, has_data = self.__collect_process_categories(
                inst_id,
                metric_maps,
            )
            if not has_data and not selectors_provided:
                continue
            if not has_data and self.__is_missing_command(command):
                continue

            if category_nodes:
                nodes.update(category_nodes)
            node_totals = self.__sum_categories(category_values)
            rows.append((pid, command, node_totals, category_values))
        return rows, sorted(nodes)

    def __node_blocks_for_table(self, nodes, width, pid_col_width):
        num_col_width = 15
        sep = "  "
        col_width = len(sep) + num_col_width

        width = self.__resize(width)
        max_cols = int((width - pid_col_width) / col_width)
        max_cols = max(1, max_cols)
        max_nodes_no_total = max_cols

        # If we can only fit one numeric column, there is no room to display
        # any node column together with a Total column. Emit the nodes first
        # (one per block), then a final Total-only block.
        if max_cols <= 1:
            for node_id in nodes:
                yield [node_id], False
            yield [], True
            return

        max_nodes_with_total = max_cols - 1

        if len(nodes) <= max_nodes_with_total:
            yield nodes, True
            return

        done = 0
        while len(nodes) - done > max_nodes_with_total:
            remaining = len(nodes) - done
            chunk_size = min(max_nodes_no_total, remaining)

            # Avoid consuming all remaining nodes in a non-total block (which
            # would otherwise suppress the Total column entirely when nodes
            # exactly fill the display width).
            if remaining <= max_nodes_no_total and remaining - chunk_size == 0:
                chunk_size = max(1, remaining - 1)

            chunk = nodes[done:done + chunk_size]
            if not chunk:
                break
            yield chunk, False
            done += chunk_size

        yield nodes[done:], True

    def __print_process_table(self, rows, nodes, width):
        print("Per-node process memory usage (in MBs)")
        pid_col_width = max(
            16,
            max(
                len("%s (%s)" % (pid, self.__normalize_value(command)))
                for pid, command, _, _ in rows
            ),
        )
        num_col_width = 15
        sep = "  "

        node_totals_all = dict((node_id, 0.0) for node_id in nodes)
        row_totals = {}
        grand_total = 0.0
        for pid, _, per_node, _ in rows:
            total = 0.0
            for node_id in nodes:
                value = per_node.get(node_id, 0.0)
                node_totals_all[node_id] += value
                total += value
            row_totals[pid] = total
            grand_total += total

        for chunk, include_total in self.__node_blocks_for_table(nodes, width, pid_col_width):
            header = "%-*s" % (pid_col_width, "PID")
            for node_id in chunk:
                header += "%s%*s" % (sep, num_col_width, "Node %d" % node_id)
            if include_total:
                header += "%s%*s" % (sep, num_col_width, "Total")
            print(header)

            line = "-" * pid_col_width
            for _ in range(len(chunk) + (1 if include_total else 0)):
                line += "%s%s" % (sep, "-" * num_col_width)
            print(line)

            for pid, command, per_node, _ in rows:
                label = "%s (%s)" % (pid, self.__normalize_value(command))
                row = "%-*s" % (pid_col_width, label)
                for node_id in chunk:
                    row += "%s%*.2f" % (sep, num_col_width, per_node.get(node_id, 0.0))
                if include_total:
                    row += "%s%*.2f" % (sep, num_col_width, row_totals.get(pid, 0.0))
                print(row)

            print(line)
            total_row = "%-*s" % (pid_col_width, "Total")
            for node_id in chunk:
                total_row += "%s%*.2f" % (sep, num_col_width, node_totals_all.get(node_id, 0.0))
            if include_total:
                total_row += "%s%*.2f" % (sep, num_col_width, grand_total)
            print(total_row)
            print()

    def __node_blocks(self, nodes, width):
        label_width = 16
        num_col_width = 15
        sep = "  "
        col_width = len(sep) + num_col_width

        width = self.__resize(width)
        max_cols = int((width - label_width) / col_width)
        max_cols = max(1, max_cols)
        max_nodes_no_total = max_cols

        # If we can only fit one numeric column, there is no room to display
        # any node column together with a Total column. Emit the nodes first
        # (one per block), then a final Total-only block.
        if max_cols <= 1:
            for node_id in nodes:
                yield [node_id], False
            yield [], True
            return

        max_nodes_with_total = max_cols - 1

        if len(nodes) <= max_nodes_with_total:
            yield nodes, True
            return

        done = 0
        while len(nodes) - done > max_nodes_with_total:
            remaining = len(nodes) - done
            chunk_size = min(max_nodes_no_total, remaining)

            # Avoid consuming all remaining nodes in a non-total block (which
            # would otherwise suppress the Total column entirely when nodes
            # exactly fill the display width).
            if remaining <= max_nodes_no_total and remaining - chunk_size == 0:
                chunk_size = max(1, remaining - 1)

            chunk = nodes[done:done + chunk_size]
            if not chunk:
                break
            yield chunk, False
            done += chunk_size

        yield nodes[done:], True

    def __print_process_detail(self, row, nodes, width):
        pid, command, _, categories = row
        print("Per-node process memory usage (in MBs) for PID %s (%s)" %
              (pid, self.__normalize_value(command)))
        label_width = 16
        num_col_width = 15
        sep = "  "

        if not nodes:
            nodes = [0]

        category_totals = {}
        node_totals = dict((node_id, 0.0) for node_id in nodes)
        all_total = 0.0
        for label, _ in PROCESS_NUMA_METRICS:
            values = categories.get(label, {})
            total = 0.0
            for node_id in nodes:
                value = values.get(node_id, 0.0)
                total += value
                node_totals[node_id] += value
            category_totals[label] = total
            all_total += total

        for chunk, include_total in self.__node_blocks(nodes, width):
            header = "%-*s" % (label_width, "")
            for node_id in chunk:
                header += "%s%*s" % (sep, num_col_width, "Node %d" % node_id)
            if include_total:
                header += "%s%*s" % (sep, num_col_width, "Total")
            print(header)

            header_line = " " * label_width
            for _ in range(len(chunk) + (1 if include_total else 0)):
                header_line += "%s%s" % (sep, "-" * num_col_width)
            print(header_line)

            for label, _ in PROCESS_NUMA_METRICS:
                row = "%-*s" % (label_width, label)
                values = categories.get(label, {})
                for node_id in chunk:
                    row += "%s%*.2f" % (sep, num_col_width, values.get(node_id, 0.0))
                if include_total:
                    row += "%s%*.2f" % (sep, num_col_width, category_totals.get(label, 0.0))
                print(row)

            line = "-" * label_width
            for _ in range(len(chunk) + (1 if include_total else 0)):
                line += "%s%s" % (sep, "-" * num_col_width)
            print(line)

            total_row = "%-*s" % (label_width, "Total")
            for node_id in chunk:
                total_row += "%s%*.2f" % (sep, num_col_width, node_totals.get(node_id, 0.0))
            if include_total:
                total_row += "%s%*.2f" % (sep, num_col_width, all_total)
            print(total_row)
            print()

    def print_processes(self, selectors, system_nodes=None, width=0):
        rows, nodes = self.__process_rows(selectors, system_nodes=system_nodes)
        if not rows:
            if selectors:
                print("No matching processes with NUMA maps data.")
            else:
                print("No processes with NUMA maps data.")
            print()
            return
        if not nodes:
            nodes = [0]
        selector_texts = [self.__normalize_value(s).strip() for s in (selectors or [])]
        selectors_are_pids = bool(selector_texts) and all(
            re.fullmatch(r"\d+", text) for text in selector_texts
        )
        if selectors_are_pids:
            requested = []
            for text in selector_texts:
                try:
                    requested.append(int(text))
                except (TypeError, ValueError):
                    continue

            rows_by_pid = {}
            for pid, command, per_node, categories in rows:
                try:
                    rows_by_pid[int(pid)] = (pid, command, per_node, categories)
                except (TypeError, ValueError):
                    continue

            if len(requested) == 1:
                pid = requested[0]
                found = rows_by_pid.get(pid)
                if found is None:
                    print("No matching process with NUMA maps data for PID %s." % pid)
                    print()
                    return
                self.__print_process_detail(found, nodes, width)
                return

            selected_rows = []
            for pid in requested:
                found = rows_by_pid.get(pid)
                if found is None:
                    print("No matching process with NUMA maps data for PID %s." % pid)
                    print()
                    continue
                row_pid, command, per_node, categories = found
                selected_rows.append((row_pid, command, per_node, categories))
            if selected_rows:
                self.__print_process_table(selected_rows, nodes, width)
            return

        single_selector = len(selectors or []) == 1
        if single_selector and len(rows) == 1:
            self.__print_process_detail(rows[0], nodes, width)
            return

        self.__print_process_table(rows, nodes, width)

class NumaStatOption(pmapi.pmOptions):
    context = None
    timefmt = "%m/%d/%Y %H:%M:%S"
    width = 0
    mem_out = False
    numa_out = False
    process_out = False
    process_filters = []

    def override(self,opt):
        """ Override standard PCP options to match numastat(1) """
        if opt in ('n', 'p'):
            return True
        return False

    def __init__(self):
        pmapi.pmOptions.__init__(self)
        self.width = 0
        self.mem_out = False
        self.numa_out = False
        self.process_out = False
        self.process_filters = []
        self.pmSetShortOptions("w:mV?np")
        self.pmSetOptionCallback(self.extraOptions)
        self.pmSetOverrideCallback(self.override)
        self.pmSetLongOptionHeader("Numastat options")
        self.pmSetLongOption("width", 1, 'w', "n", "limit the display width")
        # Map long options to our non-conflicting short letters
        self.pmSetLongOption("meminfo", 0, 'm', "", "show meminfo-like system-wide memory usage")
        self.pmSetLongOption("numastat", 0, 'n', "", "show the numastat statistics info")
        self.pmSetLongOption("process", 0, 'p', "", "show per-process NUMA memory usage")
        self.pmSetLongOptionVersion()
        self.pmSetLongOptionHelp()

    def extraOptions(self, opt, optarg, index):
        if opt == 'w':
            self.width = int(optarg)
        elif opt == "m":
            self.mem_out = True
        elif opt == "n":
            self.numa_out = True
        elif opt == "p":
            self.process_out = True
        elif opt == "V":
            pass
        else:
            raise pmapi.pmUsageErr()
        return True

    def checkoptions(self):
        if (not self.mem_out) and (not self.numa_out) and (not self.process_out):
            self.numa_out = True
        if self.width < 0:
            return False
        return True

class NumaStatReport(pmcc.MetricGroupPrinter):
    machine_info_count = 0

    def __init__(self, options):
        self.options = options
        self.timestamp = None

    def __get_timestamp(self, group):
        ts = group.contextCache.pmLocaltime(int(group.timestamp))
        self.timestamp = time.strftime(NumaStatOption.timefmt, ts.struct_time())
        return self.timestamp

    def __get_ncpu(self, group):
        return group['hinv.ncpu'].netValues[0][2]

    def print_machine_info(self,group, context):
        timestamp = context.pmLocaltime(group.timestamp.tv_sec)
        # Please check strftime(3) for different formatting options.
        # Also check TZ and LC_TIME environment variables for more
        # information on how to override the default formatting of
        # the date display in the header
        time_string = time.strftime("%x", timestamp.struct_time())
        header_string = ''
        header_string += group['kernel.uname.sysname'].netValues[0][2] + '  '
        header_string += group['kernel.uname.release'].netValues[0][2] + '  '
        header_string += '(' + group['kernel.uname.nodename'].netValues[0][2] + ')  '
        header_string += time_string + '  '
        header_string += group['kernel.uname.machine'].netValues[0][2] + '  '
        print("%s  (%s CPU)" % (header_string, self.__get_ncpu(group)))

    def __discover_nodes(self, group, name):
        # Build list of online nodes (instance id, instance name)
        nodes = []
        try:
            for ent in group[name].netValues:
                inst_id = ent[0].inst
                inst_name = ent[1]           # usually "node0", "node1", ...
                online = int(ent[2]) != 0
                if online:
                    nodes.append((inst_id, inst_name))
        except Exception:
            pass
        # Sort by instance id (node number)
        nodes.sort(key=lambda t: t[0])
        return nodes

    def __discover_nodes_all(self, group, name):
        # Build list of nodes from instances (instance id, instance name)
        nodes = []
        try:
            for ent in group[name].netValues:
                inst_id = ent[0].inst
                inst_name = ent[1]           # usually "node0", "node1", ...
                nodes.append((inst_id, inst_name))
        except Exception:
            pass
        nodes.sort(key=lambda t: t[0])
        return nodes

    def __node_ids(self, nodes):
        node_ids = []
        for inst_id, inst_name in nodes or []:
            match = re.match(r"node(\d+)$", str(inst_name))
            if match:
                node_ids.append(int(match.group(1)))
                continue
            try:
                node_ids.append(int(inst_id))
            except (TypeError, ValueError):
                continue
        node_ids = sorted(set(node_ids))
        return node_ids or None

    def report(self, manager):
        # Print in a stable order
        group = manager["sys_info"]
        try:
            if not self.machine_info_count:
                self.print_machine_info(group, manager)
                self.machine_info_count = 1
        except IndexError:
            return

        output_numa = (
            self.options.numa_out
        )
        output_mem = self.options.mem_out
        output_process = self.options.process_out

        if output_mem or output_numa:
            group = manager["numastat"]
            timestamp_group = group
        elif output_process:
            group = manager["process_numa"]
            timestamp_group = group
        else:
            return

        timestamp = self.__get_timestamp(timestamp_group)
        print("%-20s : %s"%("Timestamp", timestamp))
        if output_mem:
            nodes = self.__discover_nodes(group, "mem.numa.util.total")
            NUMAStat(group).print_mem(self.options.width, nodes, "meminfo")
        if output_numa:
            nodes = self.__discover_nodes(group, "mem.numa.util.total")
            NUMAStat(group).print_numa(self.options.width, nodes, "numastat")
        if output_process:
            ignore_pid = None
            if (
                NumaStatOption.context is not PM_CONTEXT_ARCHIVE
                and not self.options.pmGetOptionHosts()
            ):
                ignore_pid = os.getpid()
            system_nodes = None
            try:
                system_nodes = self.__node_ids(
                    self.__discover_nodes(manager["sys_info"], "hinv.node.online")
                )
            except Exception:
                system_nodes = None
            if system_nodes is None:
                try:
                    system_nodes = self.__node_ids(
                        self.__discover_nodes_all(manager["sys_info"], "mem.numa.alloc.hit")
                    )
                except Exception:
                    system_nodes = None

            ProcessNUMAStat(manager["process_numa"], ignore_pid).print_processes(
                self.options.process_filters,
                system_nodes=system_nodes,
                width=self.options.width,
            )

        if (
            NumaStatOption.context is not PM_CONTEXT_ARCHIVE
            and self.options.pmGetOptionSamples() is None
        ):
            sys.exit(0)

if __name__ == '__main__':
    try:
        opts = NumaStatOption()
        mngr = pmcc.MetricGroupManager.builder(opts, sys.argv)
        if not opts.checkoptions():
            print("Invalid options from command line")
            raise pmapi.pmUsageErr()
        NumaStatOption.context = mngr.type

        opts.process_filters = opts.pmGetOperands()
        if opts.process_filters and not opts.process_out:
            print("Process selectors require -p/--process option")
            raise pmapi.pmUsageErr()
        if not opts.process_filters and opts.process_out:
            print("Provide pid or process name for -p/--process option")
            raise pmapi.pmUsageErr()

        required_metrics = list(SYS_METRICS)
        if opts.mem_out or opts.numa_out:
            required_metrics.extend(ALL_METRICS)
        if opts.process_out:
            required_metrics.extend(PROCESS_METRICS)
        required_metrics = list(dict.fromkeys(required_metrics))

        missing = mngr.checkMissingMetrics(required_metrics)
        if missing is not None:
            sys.stderr.write('Error: not all required metrics are available\nMissing: %s\n' % (missing))
            sys.exit(1)

        sys_info_metrics = list(SYS_METRICS)
        if mngr.checkMissingMetrics(["hinv.node.online"]) is None:
            sys_info_metrics.append("hinv.node.online")
        if mngr.checkMissingMetrics(["mem.numa.alloc.hit"]) is None:
            sys_info_metrics.append("mem.numa.alloc.hit")

        if opts.mem_out or opts.numa_out:
            mngr["numastat"] = ALL_METRICS
        if opts.process_out:
            mngr["process_numa"] = PROCESS_METRICS
        mngr["sys_info"] = sys_info_metrics
        mngr.printer = NumaStatReport(opts)
        sts = mngr.run()
        sys.exit(sts)
    except IOError:
        signal.signal(signal.SIGPIPE, signal.SIG_DFL)
    except pmapi.pmErr as error:
        sys.stderr.write("%s %s\n" % (error.progname(), error.message()))
    except pmapi.pmUsageErr as usage:
        usage.message()
        sys.exit(1)
    except KeyboardInterrupt:
        pass
