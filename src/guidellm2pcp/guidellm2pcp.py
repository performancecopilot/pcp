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
# Create a PCP archive from GuideLLM benchmark results JSON file.
#

import argparse
import json
import sys

from datetime import datetime, timezone
from pcp import pmapi, pmi
from cpmapi import (PM_COUNT_ONE, PM_LABEL_CONTEXT, PM_TIME_SEC,
                    PM_TIME_MSEC, PM_TYPE_DOUBLE, PM_TYPE_U64,
                    PM_SEM_DISCRETE, PM_TEXT_PMID, PM_TEXT_ONELINE)

def register_metrics(pcp) -> pmapi.pmInDom:
    domain = 510 # PCP domain number (guidellm import)
    indom = pcp.pmiInDom(domain, 1) # benchmark iterations
    cluster = 0 # incremented for each metric group added
    item = 0 # incremented for each metric within a group

    nounit = pcp.pmiUnits(0, 0, 0, 0, 0, 0)
    unitone = pcp.pmiUnits(0, 0, 1, 0, 0, PM_COUNT_ONE)
    secunits = pcp.pmiUnits(0, 1, 0, 0, PM_TIME_SEC, 0)
    msecunits = pcp.pmiUnits(0, 1, 0, 0, PM_TIME_MSEC, 0)
    psecunits = pcp.pmiUnits(0, 1, 1, 0, PM_TIME_SEC, PM_COUNT_ONE)

    def add_metric(name, datatype, units, text=None):
        nonlocal item
        pmid = pcp.pmiID(domain, cluster, item)
        pcp.pmiAddMetric(name, pmid, datatype,
                     indom, PM_SEM_DISCRETE, units)
        if text:
            pcp.pmiPutText(PM_TEXT_PMID, PM_TEXT_ONELINE, pmid, text)
        item += 1

    pmns = 'guidellm.'
    add_metric(pmns + 'duration', PM_TYPE_DOUBLE, secunits)
    pmns = 'guidellm.run_stats.requests_made.'
    add_metric(pmns + 'successful', PM_TYPE_U64, unitone)
    add_metric(pmns + 'errored', PM_TYPE_U64, unitone)
    add_metric(pmns + 'incomplete', PM_TYPE_U64, unitone)
    add_metric(pmns + 'total', PM_TYPE_U64, unitone)
    pmns = 'guidellm.run_stats.'
    add_metric(pmns + 'queued_time_avg', PM_TYPE_DOUBLE, secunits)
    add_metric(pmns + 'scheduled_time_delay_avg', PM_TYPE_DOUBLE, secunits)
    add_metric(pmns + 'scheduled_time_sleep_avg', PM_TYPE_DOUBLE, secunits)
    add_metric(pmns + 'worker_start_delay_avg', PM_TYPE_DOUBLE, secunits)
    add_metric(pmns + 'worker_time_avg', PM_TYPE_DOUBLE, secunits)
    add_metric(pmns + 'worker_start_time_targeted_delay_avg', PM_TYPE_DOUBLE, secunits)
    add_metric(pmns + 'request_start_time_delay_avg', PM_TYPE_DOUBLE, secunits)
    add_metric(pmns + 'request_start_time_targeted_delay_avg', PM_TYPE_DOUBLE, secunits)
    add_metric(pmns + 'request_time_delay_avg', PM_TYPE_DOUBLE, secunits)
    add_metric(pmns + 'request_time_avg', PM_TYPE_DOUBLE, secunits)
    pmns = 'guidellm.requests_per_second.total.'
    add_metric(pmns + 'mean', PM_TYPE_DOUBLE, psecunits)
    add_metric(pmns + 'median', PM_TYPE_DOUBLE, psecunits)
    add_metric(pmns + 'mode', PM_TYPE_DOUBLE, psecunits)
    add_metric(pmns + 'count', PM_TYPE_U64, unitone)
    add_metric(pmns + 'min', PM_TYPE_DOUBLE, psecunits)
    add_metric(pmns + 'max', PM_TYPE_DOUBLE, psecunits)
    add_metric(pmns + 'std_dev', PM_TYPE_DOUBLE, nounit)
    add_metric(pmns + 'variance', PM_TYPE_DOUBLE, nounit)
    pmns = 'guidellm.request_latency.total.'
    add_metric(pmns + 'mean', PM_TYPE_DOUBLE, msecunits)
    add_metric(pmns + 'median', PM_TYPE_DOUBLE, msecunits)
    add_metric(pmns + 'mode', PM_TYPE_DOUBLE, msecunits)
    add_metric(pmns + 'count', PM_TYPE_U64, unitone)
    add_metric(pmns + 'min', PM_TYPE_DOUBLE, msecunits)
    add_metric(pmns + 'max', PM_TYPE_DOUBLE, msecunits)
    add_metric(pmns + 'std_dev', PM_TYPE_DOUBLE, nounit)
    add_metric(pmns + 'variance', PM_TYPE_DOUBLE, nounit)
    pmns = 'guidellm.time_to_first_token_ms.total.'
    add_metric(pmns + 'mean', PM_TYPE_DOUBLE, msecunits)
    add_metric(pmns + 'median', PM_TYPE_DOUBLE, msecunits)
    add_metric(pmns + 'mode', PM_TYPE_DOUBLE, msecunits)
    add_metric(pmns + 'count', PM_TYPE_U64, unitone)
    add_metric(pmns + 'min', PM_TYPE_DOUBLE, msecunits)
    add_metric(pmns + 'max', PM_TYPE_DOUBLE, msecunits)
    add_metric(pmns + 'std_dev', PM_TYPE_DOUBLE, nounit)
    add_metric(pmns + 'variance', PM_TYPE_DOUBLE, nounit)
    pmns = 'guidellm.time_per_output_token_ms.total.'
    add_metric(pmns + 'mean', PM_TYPE_DOUBLE, msecunits)
    add_metric(pmns + 'median', PM_TYPE_DOUBLE, msecunits)
    add_metric(pmns + 'mode', PM_TYPE_DOUBLE, msecunits)
    add_metric(pmns + 'count', PM_TYPE_U64, unitone)
    add_metric(pmns + 'min', PM_TYPE_DOUBLE, msecunits)
    add_metric(pmns + 'max', PM_TYPE_DOUBLE, msecunits)
    add_metric(pmns + 'std_dev', PM_TYPE_DOUBLE, nounit)
    add_metric(pmns + 'variance', PM_TYPE_DOUBLE, nounit)
    pmns = 'guidellm.inter_token_latency_ms.total.'
    add_metric(pmns + 'mean', PM_TYPE_DOUBLE, msecunits)
    add_metric(pmns + 'median', PM_TYPE_DOUBLE, msecunits)
    add_metric(pmns + 'mode', PM_TYPE_DOUBLE, msecunits)
    add_metric(pmns + 'count', PM_TYPE_U64, unitone)
    add_metric(pmns + 'min', PM_TYPE_DOUBLE, msecunits)
    add_metric(pmns + 'max', PM_TYPE_DOUBLE, msecunits)
    add_metric(pmns + 'std_dev', PM_TYPE_DOUBLE, nounit)
    add_metric(pmns + 'variance', PM_TYPE_DOUBLE, nounit)
    pmns = 'guidellm.output_tokens_per_second.total.'
    add_metric(pmns + 'mean', PM_TYPE_DOUBLE, psecunits)
    add_metric(pmns + 'median', PM_TYPE_DOUBLE, psecunits)
    add_metric(pmns + 'mode', PM_TYPE_DOUBLE, psecunits)
    add_metric(pmns + 'count', PM_TYPE_U64, unitone)
    add_metric(pmns + 'min', PM_TYPE_DOUBLE, psecunits)
    add_metric(pmns + 'max', PM_TYPE_DOUBLE, psecunits)
    add_metric(pmns + 'std_dev', PM_TYPE_DOUBLE, nounit)
    add_metric(pmns + 'variance', PM_TYPE_DOUBLE, nounit)
    pmns = 'guidellm.tokens_per_second.total.'
    add_metric(pmns + 'mean', PM_TYPE_DOUBLE, psecunits)
    add_metric(pmns + 'median', PM_TYPE_DOUBLE, psecunits)
    add_metric(pmns + 'mode', PM_TYPE_DOUBLE, psecunits)
    add_metric(pmns + 'count', PM_TYPE_U64, unitone)
    add_metric(pmns + 'min', PM_TYPE_DOUBLE, psecunits)
    add_metric(pmns + 'max', PM_TYPE_DOUBLE, psecunits)
    add_metric(pmns + 'std_dev', PM_TYPE_DOUBLE, nounit)
    add_metric(pmns + 'variance', PM_TYPE_DOUBLE, nounit)

    return indom

def put_metric_values(pcp, values, instance) -> None:
    """ Scan through JSON data and extract values for each PCP metric;
        pmiPutValue inserts one value for one metric:inst at one time.
    """
    pmns = 'guidellm.'
    pcp.pmiPutValue(pmns + 'duration', instance, str(values['duration']))

    pmns = 'guidellm.run_stats.requests_made.'
    stats = values['run_stats']['requests_made']
    for metric in ['successful', 'errored', 'incomplete', 'total']:
        pcp.pmiPutValue(pmns + metric, instance, str(stats[metric]))

    pmns = 'guidellm.run_stats.'
    stats = values['run_stats']
    for metric in ['queued_time_avg', 'scheduled_time_delay_avg',
                   'worker_start_delay_avg', 'worker_time_avg',
                   'worker_start_time_targeted_delay_avg',
                   'request_start_time_delay_avg', 'request_time_delay_avg',
                   'request_start_time_targeted_delay_avg', 'request_time_avg']:
        pcp.pmiPutValue(pmns + metric, instance, str(stats[metric]))

    for group in ['requests_per_second', 'request_latency',
                  'time_to_first_token_ms', 'time_per_output_token_ms',
                  'inter_token_latency_ms', 'output_tokens_per_second',
                  'tokens_per_second']:
        pmns = 'guidellm.' + group + '.total.'
        stats = values['metrics'][group]['total']
        pcp.pmiPutValue(pmns + 'mean', instance, str(stats['mean']))
        pcp.pmiPutValue(pmns + 'mode', instance, str(stats['mode']))
        pcp.pmiPutValue(pmns + 'count', instance, str(stats['count']))
        pcp.pmiPutValue(pmns + 'median', instance, str(stats['median']))
        pcp.pmiPutValue(pmns + 'min', instance, str(stats['min']))
        pcp.pmiPutValue(pmns + 'max', instance, str(stats['max']))
        pcp.pmiPutValue(pmns + 'std_dev', instance, str(stats['std_dev']))
        pcp.pmiPutValue(pmns + 'variance', instance, str(stats['variance']))


def timestamp(timestring) -> datetime:
    return datetime.fromtimestamp(timestring, timezone.utc)


parser = argparse.ArgumentParser()
parser.add_argument("-v", "--verbose", help="Enable verbose progress diagnostics",
                    action="store_true")
parser.add_argument("-a", "--archive", help="Performance Co-Pilot archive output (timestamp if not specified)")
parser.add_argument("-H", "--hostname", help="Performance Co-Pilot archive hostname (run_id if not specified)")
parser.add_argument("results", help="JSON results from GuideLLM or Model Furnace")
args = parser.parse_args()

with open(args.results) as json_data:
    try:
        payload = json.load(json_data)
    except: # pylint: disable=bare-except
        print('Failed to load JSON document from:', args.results)
        sys.exit(1)
    instid = 0
    run_id = None

    # check if the results are wrapped with model furnace metadata
    try:
        result = payload['report'] # may generate a KeyError
    except KeyError:
        result = payload # default GuideLLM format

    for benchmark in result['benchmarks']:
        if not run_id:
            run_id = benchmark['run_id']
            if args.verbose:
                print('Run', run_id)
            # Create a new PCP archive
            if not args.hostname:
                args.hostname = run_id
            if not args.archive:
                args.archive = timestamp(benchmark['run_stats']['start_time'])
                args.archive = args.archive.strftime('%Y%m%d.%H.%M')
            log = pmi.pmiLogImport(args.archive)
            log.pmiSetHostname(args.hostname)
            log.pmiSetTimezone('UTC')
            log.pmiPutLabel(PM_LABEL_CONTEXT, 0, 0, 'guidellm_run_id', run_id)
            # extract some Model Furnace metadata as labels, when present
            for label in ['model', 'inference_server', 'accelerator_type']:
                value = payload.get(label)
                if value:
                    log.pmiPutLabel(PM_LABEL_CONTEXT, 0, 0, label, value)
            ids = register_metrics(log)

        instid = instid + 1
        instname = benchmark['id_']
        if args.verbose:
            print('[%d] %s' % (instid, instname))

        log.pmiAddInstance(ids, instname, instid)
        log.pmiPutLabel(PM_LABEL_CONTEXT, 0, 0, 'guidellm_id', instname)

        put_metric_values(log, benchmark, instname)
        log.pmiWrite(timestamp(benchmark['run_stats']['start_time']))

        put_metric_values(log, benchmark, instname)
        log.pmiWrite(timestamp(benchmark['run_stats']['end_time']))

        log.pmiPutMark() # end of benchmark iteration

    del log # flush the archive to persistent storage
