#!/usr/bin/pmpython
#
# Copyright (C) 2025 Red Hat.
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
# Create a PCP archive from Prometheus node_exporter JSON data.
#
# Reads a directory of JSON files produced by the Prometheus HTTP API
# query_range endpoint (e.g. cpu.json, memory.json, disk.json) and
# converts them into a PCP archive for analysis with pmrep, pcp2csv,
# pmchart and other PCP tools.
#

import argparse
import json
import os
import sys

from collections import defaultdict
from pcp import pmapi, pmi
from cpmapi import (PM_COUNT_ONE, PM_ID_NULL, PM_INDOM_NULL,
                    PM_SEM_COUNTER, PM_SEM_INSTANT,
                    PM_SPACE_BYTE, PM_SPACE_KBYTE,
                    PM_TIME_MSEC, PM_TIME_USEC,
                    PM_TYPE_FLOAT, PM_TYPE_U32, PM_TYPE_U64,
                    PM_TEXT_PMID, PM_TEXT_ONELINE)

DOMAIN = 510
CPU_MODES = ['user', 'nice', 'system', 'idle', 'iowait',
             'irq', 'softirq', 'steal']

# Source JSON files and the Prometheus metrics they contain
SOURCE_FILES = {
    'memory.json': [
        ('node_memory_MemTotal_bytes', 'mem.physmem',
         PM_SEM_INSTANT, (1, 0, 0, PM_SPACE_KBYTE, 0, 0),
         PM_TYPE_U64, 1024,
         'Total installed physical memory'),
        ('node_memory_MemFree_bytes', 'mem.freemem',
         PM_SEM_INSTANT, (1, 0, 0, PM_SPACE_KBYTE, 0, 0),
         PM_TYPE_U64, 1024,
         'Free physical memory'),
        ('node_memory_MemAvailable_bytes', 'mem.util.available',
         PM_SEM_INSTANT, (1, 0, 0, PM_SPACE_KBYTE, 0, 0),
         PM_TYPE_U64, 1024,
         'Available memory for starting new applications'),
        ('node_memory_Buffers_bytes', 'mem.util.bufmem',
         PM_SEM_INSTANT, (1, 0, 0, PM_SPACE_KBYTE, 0, 0),
         PM_TYPE_U64, 1024,
         'Memory used for kernel buffers'),
        ('node_memory_Cached_bytes', 'mem.util.cached',
         PM_SEM_INSTANT, (1, 0, 0, PM_SPACE_KBYTE, 0, 0),
         PM_TYPE_U64, 1024,
         'Memory used for page cache'),
        ('node_memory_Active_bytes', 'mem.util.active',
         PM_SEM_INSTANT, (1, 0, 0, PM_SPACE_KBYTE, 0, 0),
         PM_TYPE_U64, 1024,
         'Memory recently used and not reclaimed'),
        ('node_memory_Inactive_bytes', 'mem.util.inactive',
         PM_SEM_INSTANT, (1, 0, 0, PM_SPACE_KBYTE, 0, 0),
         PM_TYPE_U64, 1024,
         'Memory not recently used, eligible for reclaim'),
        ('node_memory_Dirty_bytes', 'mem.util.dirty',
         PM_SEM_INSTANT, (1, 0, 0, PM_SPACE_KBYTE, 0, 0),
         PM_TYPE_U64, 1024,
         'Memory waiting to be written back to disk'),
        ('node_memory_Slab_bytes', 'mem.util.slab',
         PM_SEM_INSTANT, (1, 0, 0, PM_SPACE_KBYTE, 0, 0),
         PM_TYPE_U64, 1024,
         'Kernel slab allocator memory'),
        ('node_memory_SwapTotal_bytes', 'mem.util.swapTotal',
         PM_SEM_INSTANT, (1, 0, 0, PM_SPACE_KBYTE, 0, 0),
         PM_TYPE_U64, 1024,
         'Total swap space'),
        ('node_memory_SwapFree_bytes', 'mem.util.swapFree',
         PM_SEM_INSTANT, (1, 0, 0, PM_SPACE_KBYTE, 0, 0),
         PM_TYPE_U64, 1024,
         'Free swap space'),
        ('node_memory_AnonPages_bytes', 'mem.util.anonpages',
         PM_SEM_INSTANT, (1, 0, 0, PM_SPACE_KBYTE, 0, 0),
         PM_TYPE_U64, 1024,
         'Memory in anonymous pages'),
        ('node_memory_Mapped_bytes', 'mem.util.mapped',
         PM_SEM_INSTANT, (1, 0, 0, PM_SPACE_KBYTE, 0, 0),
         PM_TYPE_U64, 1024,
         'Memory mapped with mmap'),
        ('node_memory_Shmem_bytes', 'mem.util.shmem',
         PM_SEM_INSTANT, (1, 0, 0, PM_SPACE_KBYTE, 0, 0),
         PM_TYPE_U64, 1024,
         'Shared memory (shmem and tmpfs)'),
        ('node_memory_Committed_AS_bytes', 'mem.util.committed_AS',
         PM_SEM_INSTANT, (1, 0, 0, PM_SPACE_KBYTE, 0, 0),
         PM_TYPE_U64, 1024,
         'Total committed address space'),
    ],
    'loadavg.json': [
        ('node_load1', 'kernel.all.load',
         PM_SEM_INSTANT, (0, 0, 0, 0, 0, 0),
         PM_TYPE_FLOAT, 1,
         'One-minute load average'),
        ('node_load5', 'kernel.all.load5',
         PM_SEM_INSTANT, (0, 0, 0, 0, 0, 0),
         PM_TYPE_FLOAT, 1,
         'Five-minute load average'),
        ('node_load15', 'kernel.all.load15',
         PM_SEM_INSTANT, (0, 0, 0, 0, 0, 0),
         PM_TYPE_FLOAT, 1,
         'Fifteen-minute load average'),
        ('node_procs_running', 'kernel.all.running',
         PM_SEM_INSTANT, (0, 0, 0, 0, 0, 0),
         PM_TYPE_U32, 1,
         'Number of currently running processes'),
        ('node_procs_blocked', 'kernel.all.blocked',
         PM_SEM_INSTANT, (0, 0, 0, 0, 0, 0),
         PM_TYPE_U32, 1,
         'Number of processes blocked on I/O'),
    ],
    'scheduler.json': [
        ('node_context_switches_total', 'kernel.all.pswitch',
         PM_SEM_COUNTER, (0, 0, 1, 0, 0, PM_COUNT_ONE),
         PM_TYPE_U64, 1,
         'Context switches since boot'),
        ('node_intr_total', 'kernel.all.intr',
         PM_SEM_COUNTER, (0, 0, 1, 0, 0, PM_COUNT_ONE),
         PM_TYPE_U64, 1,
         'Interrupt count since boot'),
        ('node_forks_total', 'kernel.all.sysfork',
         PM_SEM_COUNTER, (0, 0, 1, 0, 0, PM_COUNT_ONE),
         PM_TYPE_U64, 1,
         'Number of fork() calls since boot'),
    ],
    'vmstat.json': [
        ('node_vmstat_pgfault', 'mem.vmstat.pgfault',
         PM_SEM_COUNTER, (0, 0, 1, 0, 0, PM_COUNT_ONE),
         PM_TYPE_U64, 1,
         'Page faults since boot'),
        ('node_vmstat_pgmajfault', 'mem.vmstat.pgmajfault',
         PM_SEM_COUNTER, (0, 0, 1, 0, 0, PM_COUNT_ONE),
         PM_TYPE_U64, 1,
         'Major page faults since boot'),
        ('node_vmstat_pgpgin', 'mem.vmstat.pgpgin',
         PM_SEM_COUNTER, (0, 0, 1, 0, 0, PM_COUNT_ONE),
         PM_TYPE_U64, 1,
         'Pages paged in from disk'),
        ('node_vmstat_pgpgout', 'mem.vmstat.pgpgout',
         PM_SEM_COUNTER, (0, 0, 1, 0, 0, PM_COUNT_ONE),
         PM_TYPE_U64, 1,
         'Pages paged out to disk'),
        ('node_vmstat_pswpin', 'mem.vmstat.pswpin',
         PM_SEM_COUNTER, (0, 0, 1, 0, 0, PM_COUNT_ONE),
         PM_TYPE_U64, 1,
         'Pages swapped in'),
        ('node_vmstat_pswpout', 'mem.vmstat.pswpout',
         PM_SEM_COUNTER, (0, 0, 1, 0, 0, PM_COUNT_ONE),
         PM_TYPE_U64, 1,
         'Pages swapped out'),
        ('node_vmstat_oom_kill', 'mem.vmstat.oom_kill',
         PM_SEM_COUNTER, (0, 0, 1, 0, 0, PM_COUNT_ONE),
         PM_TYPE_U64, 1,
         'OOM killer invocations'),
    ],
    'pressure.json': [
        ('node_pressure_cpu_waiting_seconds_total',
         'kernel.all.pressure.cpu.some.total',
         PM_SEM_COUNTER, (0, 1, 0, 0, PM_TIME_USEC, 0),
         PM_TYPE_U64, 1e-6,
         'CPU pressure stall time (some)'),
        ('node_pressure_memory_waiting_seconds_total',
         'kernel.all.pressure.memory.some.total',
         PM_SEM_COUNTER, (0, 1, 0, 0, PM_TIME_USEC, 0),
         PM_TYPE_U64, 1e-6,
         'Memory pressure stall time (some)'),
        ('node_pressure_memory_stalled_seconds_total',
         'kernel.all.pressure.memory.full.total',
         PM_SEM_COUNTER, (0, 1, 0, 0, PM_TIME_USEC, 0),
         PM_TYPE_U64, 1e-6,
         'Memory pressure stall time (full)'),
        ('node_pressure_io_waiting_seconds_total',
         'kernel.all.pressure.io.some.total',
         PM_SEM_COUNTER, (0, 1, 0, 0, PM_TIME_USEC, 0),
         PM_TYPE_U64, 1e-6,
         'I/O pressure stall time (some)'),
        ('node_pressure_io_stalled_seconds_total',
         'kernel.all.pressure.io.full.total',
         PM_SEM_COUNTER, (0, 1, 0, 0, PM_TIME_USEC, 0),
         PM_TYPE_U64, 1e-6,
         'I/O pressure stall time (full)'),
    ],
}

DISK_METRICS = [
    ('node_disk_read_bytes_total', 'disk.dev.read_bytes',
     PM_SEM_COUNTER, (1, 0, 0, PM_SPACE_KBYTE, 0, 0),
     PM_TYPE_U64, 1024,
     'Bytes read from each disk device'),
    ('node_disk_written_bytes_total', 'disk.dev.write_bytes',
     PM_SEM_COUNTER, (1, 0, 0, PM_SPACE_KBYTE, 0, 0),
     PM_TYPE_U64, 1024,
     'Bytes written to each disk device'),
    ('node_disk_reads_completed_total', 'disk.dev.read',
     PM_SEM_COUNTER, (0, 0, 1, 0, 0, PM_COUNT_ONE),
     PM_TYPE_U64, 1,
     'Read operations completed per disk device'),
    ('node_disk_writes_completed_total', 'disk.dev.write',
     PM_SEM_COUNTER, (0, 0, 1, 0, 0, PM_COUNT_ONE),
     PM_TYPE_U64, 1,
     'Write operations completed per disk device'),
    ('node_disk_io_time_seconds_total', 'disk.dev.avactive',
     PM_SEM_COUNTER, (0, 1, 0, 0, PM_TIME_MSEC, 0),
     PM_TYPE_U64, 0.001,
     'Time spent doing I/O per disk device'),
]

NET_METRICS = [
    ('node_network_receive_bytes_total', 'network.interface.in.bytes',
     PM_SEM_COUNTER, (1, 0, 0, PM_SPACE_BYTE, 0, 0),
     PM_TYPE_U64, 1,
     'Bytes received per network interface'),
    ('node_network_transmit_bytes_total', 'network.interface.out.bytes',
     PM_SEM_COUNTER, (1, 0, 0, PM_SPACE_BYTE, 0, 0),
     PM_TYPE_U64, 1,
     'Bytes transmitted per network interface'),
    ('node_network_receive_packets_total', 'network.interface.in.packets',
     PM_SEM_COUNTER, (0, 0, 1, 0, 0, PM_COUNT_ONE),
     PM_TYPE_U64, 1,
     'Packets received per network interface'),
    ('node_network_transmit_packets_total', 'network.interface.out.packets',
     PM_SEM_COUNTER, (0, 0, 1, 0, 0, PM_COUNT_ONE),
     PM_TYPE_U64, 1,
     'Packets transmitted per network interface'),
    ('node_network_receive_errs_total', 'network.interface.in.errors',
     PM_SEM_COUNTER, (0, 0, 1, 0, 0, PM_COUNT_ONE),
     PM_TYPE_U64, 1,
     'Receive errors per network interface'),
    ('node_network_transmit_errs_total', 'network.interface.out.errors',
     PM_SEM_COUNTER, (0, 0, 1, 0, 0, PM_COUNT_ONE),
     PM_TYPE_U64, 1,
     'Transmit errors per network interface'),
    ('node_network_receive_drop_total', 'network.interface.in.drops',
     PM_SEM_COUNTER, (0, 0, 1, 0, 0, PM_COUNT_ONE),
     PM_TYPE_U64, 1,
     'Packets dropped on receive per network interface'),
    ('node_network_transmit_drop_total', 'network.interface.out.drops',
     PM_SEM_COUNTER, (0, 0, 1, 0, 0, PM_COUNT_ONE),
     PM_TYPE_U64, 1,
     'Packets dropped on transmit per network interface'),
]


def load_json(filepath):
    """ Load a Prometheus query_range API JSON response file. """
    if not os.path.exists(filepath):
        return None
    with open(filepath) as f:
        data = json.load(f)
    if data.get('status') != 'success':
        return None
    return data.get('data', {}).get('result', [])


def discover_instances(results, label_key):
    """ Find all unique instance labels (device names, cpu numbers). """
    instances = set()
    for r in results:
        val = r['metric'].get(label_key, '')
        if val:
            instances.add(val)
    return sorted(instances)


def build_indexed_data(results, label_key):
    """ Build {prom_metric: {instance_label: [(ts, value), ...]}}. """
    data = defaultdict(lambda: defaultdict(list))
    for r in results:
        name = r['metric'].get('__name__', '')
        inst = r['metric'].get(label_key, '')
        for ts, val in r.get('values', []):
            data[name][inst].append((float(ts), float(val)))
    return data


def register_simple_metrics(log, data_dir, verbose):
    """ Register and load non-instanced metrics from source JSON files. """
    cluster = 0
    item = 0
    all_timestamps = set()
    metric_values = {}

    for src_file, entries in SOURCE_FILES.items():
        results = load_json(os.path.join(data_dir, src_file))
        if not results:
            continue

        for entry in entries:
            (prom_name, pcp_name, sem, units_args,
             pcp_type, divisor, helptext) = entry

            matching = [r for r in results
                        if r['metric'].get('__name__') == prom_name]
            if not matching:
                continue

            r = matching[0]
            units = pmapi.pmUnits(*units_args)
            pmid = log.pmiID(DOMAIN, cluster, item)

            try:
                log.pmiAddMetric(pcp_name, pmid, pcp_type,
                                 PM_INDOM_NULL, sem, units)
                log.pmiPutText(PM_TEXT_PMID, PM_TEXT_ONELINE,
                               pmid, helptext)
            except pmi.pmiErr:
                pass

            item += 1
            values = {}
            for ts, val in r.get('values', []):
                fts = float(ts)
                fval = float(val)
                if divisor != 1:
                    fval = int(fval / divisor)
                elif pcp_type in (PM_TYPE_U32, PM_TYPE_U64):
                    fval = int(fval)
                values[fts] = fval
                all_timestamps.add(fts)

            metric_values[pcp_name] = {
                'instances': {'': values}, 'type': pcp_type
            }
            if verbose:
                print('%s (%d samples)' % (pcp_name, len(values)))

        cluster += 1

    return all_timestamps, metric_values, cluster, item


def register_cpu_metrics(log, data_dir, cluster, verbose):
    """ Register per-cpu per-mode metrics from cpu.json. """
    all_timestamps = set()
    metric_values = {}
    serial = 0

    cpu_results = load_json(os.path.join(data_dir, 'cpu.json'))
    if not cpu_results:
        return all_timestamps, metric_values, cluster, serial

    cpu_numbers = set()
    cpu_data = defaultdict(list)

    for r in cpu_results:
        if r['metric'].get('__name__') != 'node_cpu_seconds_total':
            continue
        cpu_num = r['metric'].get('cpu', '0')
        mode = r['metric'].get('mode', 'unknown')
        if mode not in CPU_MODES:
            continue
        cpu_numbers.add(cpu_num)
        for ts, val in r.get('values', []):
            cpu_data[(cpu_num, mode)].append((float(ts), float(val)))

    cpu_numbers = sorted(cpu_numbers,
                         key=lambda x: int(x) if x.isdigit() else 999)
    item = 0
    helptext = {
        'user': 'User mode CPU time per processor',
        'nice': 'Nice priority CPU time per processor',
        'system': 'System mode CPU time per processor',
        'idle': 'Idle CPU time per processor',
        'iowait': 'I/O wait CPU time per processor',
        'irq': 'Hardware interrupt CPU time per processor',
        'softirq': 'Software interrupt CPU time per processor',
        'steal': 'Stolen CPU time per processor (virtualisation)',
    }

    for mode in CPU_MODES:
        pcp_name = 'kernel.percpu.cpu.' + mode
        units = pmapi.pmUnits(0, 1, 0, 0, PM_TIME_MSEC, 0)
        indom = log.pmiInDom(DOMAIN, serial)
        serial += 1
        pmid = log.pmiID(DOMAIN, cluster, item)
        item += 1

        try:
            log.pmiAddMetric(pcp_name, pmid, PM_TYPE_U64,
                             indom, PM_SEM_COUNTER, units)
            log.pmiPutText(PM_TEXT_PMID, PM_TEXT_ONELINE,
                           pmid, helptext.get(mode, ''))
        except pmi.pmiErr:
            pass

        for cpu_num in cpu_numbers:
            try:
                log.pmiAddInstance(indom, 'cpu' + cpu_num, int(cpu_num))
            except pmi.pmiErr:
                pass

        instances = {}
        for cpu_num in cpu_numbers:
            values = {}
            for ts, val in cpu_data.get((cpu_num, mode), []):
                values[ts] = int(float(val) * 1000)
                all_timestamps.add(ts)
            instances['cpu' + cpu_num] = values

        metric_values[pcp_name] = {
            'instances': instances, 'type': PM_TYPE_U64
        }
        if verbose:
            print('%s (%d CPUs)' % (pcp_name, len(cpu_numbers)))

    return all_timestamps, metric_values, cluster + 1, serial


def register_device_metrics(log, data_dir, metric_list, src_file,
                            label_key, cluster, serial, verbose):
    """ Register per-device instanced metrics (disk, network). """
    all_timestamps = set()
    metric_values = {}

    results = load_json(os.path.join(data_dir, src_file))
    if not results:
        return all_timestamps, metric_values, cluster, serial

    indexed = build_indexed_data(results, label_key)
    devices = discover_instances(results, label_key)
    item = 0

    for entry in metric_list:
        (prom_name, pcp_name, sem, units_args,
         pcp_type, divisor, helptext) = entry

        if prom_name not in indexed:
            continue

        units = pmapi.pmUnits(*units_args)
        indom = log.pmiInDom(DOMAIN, serial)
        serial += 1
        pmid = log.pmiID(DOMAIN, cluster, item)
        item += 1

        try:
            log.pmiAddMetric(pcp_name, pmid, pcp_type,
                             indom, sem, units)
            log.pmiPutText(PM_TEXT_PMID, PM_TEXT_ONELINE,
                           pmid, helptext)
        except pmi.pmiErr:
            pass

        for i, dev in enumerate(devices):
            try:
                log.pmiAddInstance(indom, dev, i)
            except pmi.pmiErr:
                pass

        instances = {}
        for dev in devices:
            values = {}
            for ts, val in indexed[prom_name].get(dev, []):
                if divisor != 1:
                    values[ts] = int(val / divisor)
                else:
                    values[ts] = int(val)
                all_timestamps.add(ts)
            instances[dev] = values

        metric_values[pcp_name] = {
            'instances': instances, 'type': pcp_type
        }
        if verbose:
            print('%s (%d %ss)' % (pcp_name, len(devices), label_key))

    return all_timestamps, metric_values, cluster + 1, serial


def convert(data_dir, archive_name, hostname, verbose):
    """ Convert Prometheus JSON data directory to a PCP archive. """
    log = pmi.pmiLogImport(archive_name)

    meta_file = os.path.join(data_dir, 'metadata.json')
    if os.path.exists(meta_file):
        with open(meta_file) as f:
            meta = json.load(f)
        host = meta.get('node', hostname)
        log.pmiSetHostname(host)
        tz_str = meta.get('timezone', 'UTC')
        try:
            log.pmiSetTimezone(tz_str)
        except Exception:
            log.pmiSetTimezone('UTC')
    else:
        log.pmiSetHostname(hostname)
        log.pmiSetTimezone('UTC')

    all_timestamps = set()
    metric_values = {}

    ts, mv, cluster, item = register_simple_metrics(
        log, data_dir, verbose)
    all_timestamps.update(ts)
    metric_values.update(mv)

    serial = 0
    ts, mv, cluster, serial = register_cpu_metrics(
        log, data_dir, cluster, verbose)
    all_timestamps.update(ts)
    metric_values.update(mv)

    ts, mv, cluster, serial = register_device_metrics(
        log, data_dir, DISK_METRICS, 'disk.json',
        'device', cluster, serial, verbose)
    all_timestamps.update(ts)
    metric_values.update(mv)

    ts, mv, cluster, serial = register_device_metrics(
        log, data_dir, NET_METRICS, 'network.json',
        'device', cluster, serial, verbose)
    all_timestamps.update(ts)
    metric_values.update(mv)

    if not all_timestamps:
        print('No metric data found in', data_dir, file=sys.stderr)
        sys.exit(1)

    sorted_timestamps = sorted(all_timestamps)
    if verbose:
        print('Writing %d timestamps (%d metrics)' %
              (len(sorted_timestamps), len(metric_values)))

    written = 0
    for ts in sorted_timestamps:
        sec = int(ts)
        usec = int((ts - sec) * 1000000)
        has_data = False

        for pcp_name, mdata in metric_values.items():
            pcp_type = mdata['type']
            for inst_name, inst_values in mdata['instances'].items():
                if ts not in inst_values:
                    continue
                has_data = True
                val = inst_values[ts]
                inst_arg = inst_name if inst_name else ''
                try:
                    if pcp_type == PM_TYPE_FLOAT:
                        log.pmiPutValue(pcp_name, inst_arg,
                                        str(float(val)))
                    else:
                        log.pmiPutValue(pcp_name, inst_arg,
                                        str(int(val)))
                except pmi.pmiErr:
                    pass

        if has_data:
            try:
                log.pmiWrite(sec, usec)
                written += 1
            except pmi.pmiErr as e:
                print('Write failed at ts=%d: %s' % (sec, e),
                      file=sys.stderr)

    del log
    if verbose:
        print('Wrote %d timestamp records to %s' %
              (written, archive_name))


parser = argparse.ArgumentParser(
    description='Import Prometheus node_exporter metrics into '
                'a PCP archive')
parser.add_argument('-v', '--verbose',
                    help='Enable verbose progress diagnostics',
                    action='store_true')
parser.add_argument('-a', '--archive',
                    help='PCP archive output name '
                         '(default: <datadir>/node_archive)')
parser.add_argument('-H', '--hostname', default='localhost',
                    help='PCP archive hostname '
                         '(default: from metadata.json or localhost)')
parser.add_argument('datadir',
                    help='Directory containing Prometheus JSON files')
args = parser.parse_args()

if not os.path.isdir(args.datadir):
    print('Not a directory:', args.datadir, file=sys.stderr)
    sys.exit(1)

archive = args.archive
if not archive:
    archive = os.path.join(args.datadir, 'node_archive')

convert(args.datadir, archive, args.hostname, args.verbose)
