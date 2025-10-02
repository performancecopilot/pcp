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
# Create a PCP archive from vLLM benchmark results JSON file.
#

import argparse
import calendar
import json
import sys

from datetime import datetime, timedelta, timezone
from pcp import pmapi, pmi
from cpmapi import (PM_COUNT_ONE, PM_LABEL_CONTEXT,
                    PM_TIME_SEC, PM_TIME_MSEC,
                    PM_TYPE_DOUBLE, PM_TYPE_U64, PM_SEM_DISCRETE,
                    PM_TEXT_PMID, PM_TEXT_ONELINE, PM_TEXT_HELP)

def register_metrics(pcp) -> pmapi.pmInDom:
    domain = 510 # PCP domain number (vllm import)
    indom = pcp.pmiInDom(domain, 1) # benchmark iterations
    cluster = 0 # incremented for each metric group added
    item = 0 # incremented for each metric within a group

    nounit = pcp.pmiUnits(0, 0, 0, 0, 0, 0)
    unitone = pcp.pmiUnits(0, 0, 1, 0, 0, PM_COUNT_ONE)
    secunits = pcp.pmiUnits(0, 1, 0, 0, PM_TIME_SEC, 0)
    msecunits = pcp.pmiUnits(0, 1, 0, 0, PM_TIME_MSEC, 0)
    psecunits = pcp.pmiUnits(0, 1, 1, 0, PM_TIME_SEC, PM_COUNT_ONE)

    def add_metric(name, datatype, units, oneline=None, text=None) -> None:
        nonlocal item
        pmid = pcp.pmiID(domain, cluster, item)
        pcp.pmiAddMetric(name, pmid, datatype,
                     indom, PM_SEM_DISCRETE, units)
        if oneline:
            pcp.pmiPutText(PM_TEXT_PMID, PM_TEXT_ONELINE, pmid, oneline)
        if text:
            pcp.pmiPutText(PM_TEXT_PMID, PM_TEXT_HELP, pmid, text)
        item += 1

    pmns = 'vllm.bench.'
    add_metric(pmns + 'duration', PM_TYPE_DOUBLE, secunits,
        'Elapsed time in seconds for each vllm bench run completed')
    add_metric(pmns + 'request_rate', PM_TYPE_DOUBLE, psecunits,
        'Request processing rate goal, a benchmark input parameter',
        "Number of requests per second. If this is inf, then all requests "
        "are sent at time 0.  Otherwise vllm uses Poisson process or gamma "
        "distribution to synthesize the request arrival times.")
    add_metric(pmns + 'burstiness', PM_TYPE_DOUBLE, nounit,
        'Burstiness factor of request generation, a benchmark input parameter',
        "Burstiness only takes effect when request_rate is set. "
        "Default value is 1, which follows Poisson process. "
        "Otherwise, the request intervals follow a gamma distribution. "
        "A lower burstiness value (0 < burstiness < 1) results in more "
        "bursty requests. A higher burstiness value (burstiness > 1) "
        "results in a more uniform arrival of requests.")
    add_metric(pmns + 'max_concurrency', PM_TYPE_DOUBLE, unitone,
        'Maximum number of concurrent requests, a benchmark input parameter',
        "Can be used to help simulate an environment where a higher level "
        "component is enforcing a maximum number of concurrent requests.  "
        "While the --request-rate argument controls the rate requests are "
        "initiated, this argument will control how many are actually allowed "
        "to execute at a time.  This means that when used in combination, the "
        "actual request rate may be lower than specified with --request-rate, "
        "if the server is not processing requests fast enough to keep up.")
    add_metric(pmns + 'completed', PM_TYPE_U64, unitone,
               'Count of requests processed (requests-per-second as a rate)')
    add_metric(pmns + 'num_prompts', PM_TYPE_U64, unitone,
               'Count of all requests started irrespective of how they finish')

    add_metric(pmns + 'total_input_tokens', PM_TYPE_U64, unitone,
        'Total number of input tokens')
    add_metric(pmns + 'total_output_tokens', PM_TYPE_U64, unitone,
        'Total number of output tokens')
    add_metric(pmns + 'request_throughput', PM_TYPE_DOUBLE, psecunits,
        'Total number of requests handled per second')
    add_metric(pmns + 'request_goodput', PM_TYPE_DOUBLE, psecunits,
        'Total number of requests meeting service level objectives per second',
        "")
    add_metric(pmns + 'output_throughput', PM_TYPE_DOUBLE, psecunits,
        'Total number of output tokens per second')
    add_metric(pmns + 'total_token_throughput', PM_TYPE_DOUBLE, psecunits,
        'Total number of tokens per second')
    add_metric(pmns + 'max_output_tokens_per_s', PM_TYPE_DOUBLE, secunits,
        'Maximum rate of output tokens')
    add_metric(pmns + 'max_concurrent_requests', PM_TYPE_U64, nounit,
        'Maximum number of concurrent requests')
    add_metric(pmns + 'mean_ttft_ms', PM_TYPE_DOUBLE, msecunits,
        'Average time taken to generate the first token of the output',
        'Indicates initial response time of the model, crucial for user-\n' +
        'facing applications.')
    add_metric(pmns + 'median_ttft_ms', PM_TYPE_DOUBLE, msecunits,
        'Median time taken to generate the first token of the output',
        'Indicates initial response time of the model, crucial for user-\n' +
        'facing applications.')
    add_metric(pmns + 'std_ttft_ms', PM_TYPE_DOUBLE, msecunits,
        'Standard deviation of time taken to generate first output token')
    add_metric(pmns + 'p99_ttft_ms', PM_TYPE_DOUBLE, msecunits,
        '99th percentile time taken to generate first output token')
    add_metric(pmns + 'mean_tpot_ms', PM_TYPE_DOUBLE, msecunits,
        'Average time taken to generate each output token',
        'Provides a detailed view of the models token generation efficiency.')
    add_metric(pmns + 'median_tpot_ms', PM_TYPE_DOUBLE, msecunits,
        'Median time taken to generate each output token',
        'Provides a detailed view of the models token generation efficiency.')
    add_metric(pmns + 'std_tpot_ms', PM_TYPE_DOUBLE, msecunits,
        'Standard deviation of time taken to generate each output token')
    add_metric(pmns + 'p99_tpot_ms', PM_TYPE_DOUBLE, msecunits,
        '99th percentile of time taken to generate each output token')
    add_metric(pmns + 'mean_itl_ms', PM_TYPE_DOUBLE, msecunits,
        'Average time between generating consecutive tokens in the output',
        'Helps assess the smoothness and speed of token generation.')
    add_metric(pmns + 'median_itl_ms', PM_TYPE_DOUBLE, msecunits,
        'Median time between generating consecutive tokens in the output',
        'Helps assess the smoothness and speed of token generation.')
    add_metric(pmns + 'std_itl_ms', PM_TYPE_DOUBLE, msecunits,
        'Standard deviation of time between consecutive output tokens')
    add_metric(pmns + 'p99_itl_ms', PM_TYPE_DOUBLE, msecunits,
        '99th percentile of time between consecutive output tokens')
    add_metric(pmns + 'mean_e2el_ms', PM_TYPE_DOUBLE, msecunits,
        'Average end-to-end request latency per request',
        "End-to-end latency is the time taken on the client side from "
        "sending a request to receiving the complete response.  This "
        "measure helps to assess the end-user perceived performance.")
    add_metric(pmns + 'median_e2el_ms', PM_TYPE_DOUBLE, msecunits,
        'Median end-to-end request latency per request',
        "End-to-end latency is the time taken on the client side from "
        "sending a request to receiving the complete response.  This "
        "measure helps to assess the end-user perceived performance.")
    add_metric(pmns + 'std_e2el_ms', PM_TYPE_DOUBLE, msecunits,
        'Standard deviation of end-to-end request latency')
    add_metric(pmns + 'p99_e2el_ms', PM_TYPE_DOUBLE, msecunits,
        '99th percentile of end-to-end request latency')

    return indom

def put_metric_values(pcp, values, instance) -> None:
    """ Scan through JSON data and extract values for each PCP metric;
        pmiPutValue inserts one value for one metric:inst at one time.
    """
    pmns = 'vllm.bench.'
    for metric in ['request_rate', 'burstiness', 'max_concurrency',
                   'duration', 'completed', 'num_prompts',
                   'total_input_tokens', 'total_output_tokens',
                   'request_throughput', 'request_goodput',
                   'output_throughput', 'total_token_throughput',
                   'max_output_tokens_per_s', 'max_concurrent_requests',
                   'mean_ttft_ms', 'median_ttft_ms',
                   'std_ttft_ms', 'p99_ttft_ms',
                   'mean_tpot_ms', 'median_tpot_ms',
                   'std_tpot_ms', 'p99_tpot_ms',
                   'mean_itl_ms', 'median_itl_ms',
                   'std_itl_ms', 'p99_itl_ms',
                   'mean_e2el_ms', 'median_e2el_ms',
                   'std_e2el_ms', 'p99_e2el_ms']:
        if metric in values and values[metric] is not None:
            if args.verbose:
                print('adding', metric, ':', str(values[metric]))
            pcp.pmiPutValue(pmns + metric, instance, str(values[metric]))


def timestamp(timestring) -> datetime:
    # extract timestamp from string like "20250929-080417"
    (dates, times) = timestring.split('-')
    # (year, month, day, hour, minute, second, weekday, day_of_year, is_dst)
    (yy, mm, dd) = (int(dates[0:4]), int(dates[4:6]), int(dates[6:8]))
    (HH, MM, SS) = (int(times[0:2]), int(times[2:4]), int(times[4:6]))
    local_time_tuple = (yy, mm, dd, HH, MM, SS, 0, 0, 0)
    seconds_since_epoch = calendar.timegm(local_time_tuple)
    return datetime.fromtimestamp(seconds_since_epoch, timezone.utc)


parser = argparse.ArgumentParser()
parser.add_argument("-v", "--verbose", help="Enable verbose progress diagnostics",
                    action="store_true")
parser.add_argument("-a", "--archive", help="Performance Co-Pilot archive output (timestamp if not specified)")
parser.add_argument("-H", "--hostname", help="Performance Co-Pilot archive hostname (backend if not specified)")
parser.add_argument("results", help="JSON results from vllm or Model Furnace")
args = parser.parse_args()

with open(args.results) as json_data:
    try:
        payload = json.load(json_data)
    except: # pylint: disable=bare-except
        print('Failed to load JSON document from:', args.results)
        sys.exit(1)
    instid = 0
    backend = None

    # check if the results are wrapped with model furnace metadata
    try:
        result = payload['report'] # may generate a KeyError
    except KeyError:
        result = payload # default vllm bench format

    benchmark = result
    backend = benchmark['backend']
    if args.verbose:
        print('Backend', backend)
    # Create a new PCP archive
    if not args.hostname:
        args.hostname = backend
    if not args.archive:
        args.archive = timestamp(benchmark['date'])
        args.archive = args.archive.strftime('%Y%m%d.%H.%M')
    log = pmi.pmiLogImport(args.archive)
    log.pmiSetHostname(args.hostname)
    log.pmiSetTimezone('UTC')
    # extract some Model Furnace metadata as labels, when present
    for label in ['model_id', 'tokenizer_id', 'endpoint_type', 'backend']:
        value = payload.get(label)
        if value:
            log.pmiPutLabel(PM_LABEL_CONTEXT, 0, 0, label, value)
    ids = register_metrics(log)

    instid = instid + 1
    instname = benchmark['backend'] + '-' + benchmark['model_id']
    if args.verbose:
        print('[%d] %s' % (instid, instname))

    log.pmiAddInstance(ids, instname, instid)

    start_time = timestamp(benchmark['date'])
    put_metric_values(log, benchmark, instname)
    log.pmiWrite(start_time)

    finish_time = start_time + timedelta(seconds=benchmark['duration'])
    put_metric_values(log, benchmark, instname)
    log.pmiWrite(finish_time)

    log.pmiPutMark() # end of benchmark iteration

    if args.verbose:
        print("Writing archive:", args.archive)
    del log # flush the archive to persistent storage
