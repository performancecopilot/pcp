#!/usr/bin/env pmpython
#
# Copyright (C) 2025 Lauren Chilton <lchilton@redhat.com>
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


""" PCP to OPENTELEMETRY Bridge """

# Common imports
from collections import OrderedDict
import errno
import time
import sys
import platform

# Our imports
import requests
import os
import cpmapi
import json

# PCP Python PMAPI
from pcp import pmapi, pmconfig
from cpmapi import PM_CONTEXT_ARCHIVE, PM_INDOM_NULL, PM_DEBUG_APPL1, PM_TIME_SEC
# Default config
DEFAULT_CONFIG = ["./pcp2opentelemetry.conf", "$HOME/.pcp2opentelemetry.conf",
                  "$HOME/.pcp/pcp2opentelemetry.conf", "$PCP_SYSCONF_DIR/pcp2opentelemetry.conf"]

# Defaults
CONFVER = 1
INDENT = 2
TIMEFMT = "%Y-%m-%d %H:%M:%S"
TIMEOUT = 2.5 # seconds

class PCP2OPENTELEMETRY(object):
    """ PCP to OPENTELEMETRY """
    def __init__(self):
        """ Construct object, prepare for command line handling """
        self.context = None
        self.daemonize = 0
        self.pmconfig = pmconfig.pmConfig(self)
        self.opts = self.options()

        # Configuration directives
        self.keys = ('source', 'output', 'derived', 'header', 'globals',
                     'samples', 'interval', 'type', 'precision', 'daemonize',
                     'timefmt', 'everything',
                     'count_scale', 'space_scale', 'time_scale', 'version',
                     'count_scale_force', 'space_scale_force', 'time_scale_force',
                     'type_prefer', 'precision_force', 'limit_filter', 'limit_filter_force',
                     'live_filter', 'rank', 'invert_filter', 'predicate', 'names_change',
                     'speclocal', 'instances', 'ignore_incompat', 'ignore_unknown',
                     'omit_flat', 'include_labels', 'url', 'http_user', 'http_pass',
                     'http_timeout')

        # Ignored for pmrep(1) compatibility
        self.keys_ignore = (
                     'timestamp', 'unitinfo', 'colxrow', 'separate_header', 'fixed_header',
                     'delay', 'width', 'delimiter', 'extcsv', 'width_force',
                     'extheader', 'repeat_header', 'interpol',
                     'dynamic_header', 'overall_rank', 'overall_rank_alt', 'sort_metric',
                     'instinfo', 'include_texts')

        # The order of preference for options (as present):
        # 1 - command line options
        # 2 - options from configuration file(s)
        # 3 - built-in defaults defined below
        self.check = 0
        self.version = CONFVER
        self.source = "local:"
        self.output = None # For pmrep conf file compat only
        self.speclocal = None
        self.derived = None
        self.header = 1
        self.globals = 1
        self.samples = None # forever
        self.interval = pmapi.timeval(10)      # 10 sec
        self.opts.pmSetOptionInterval(str(10)) # 10 sec
        self.delay = 0
        self.type = 0
        self.type_prefer = self.type
        self.ignore_incompat = 0
        self.ignore_unknown = 0
        self.names_change = 0 # ignore
        self.instances = []
        self.live_filter = 0
        self.rank = 0
        self.limit_filter = 0
        self.limit_filter_force = 0
        self.invert_filter = 0
        self.predicate = None
        self.omit_flat = 0
        self.include_labels = 0
        self.precision = 3 # .3f
        self.precision_force = None
        self.timefmt = TIMEFMT
        self.interpol = 0
        self.count_scale = None
        self.count_scale_force = None
        self.space_scale = None
        self.space_scale_force = None
        self.time_scale = None
        self.time_scale_force = None

        # Not in pcp2openmetrics.conf, won't overwrite
        self.outfile = None

        self.everything = 0
        self.url = None
        self.http_user = None
        self.http_pass = None
        self.http_timeout = TIMEOUT

        # Internal
        self.runtime = -1

        self.data = None
        self.prev_ts = None
        self.writer = None

        # Performance metrics store
        # key - metric name
        # values - 0:txt label, 1:instance(s), 2:unit/scale, 3:type,
        #          4:width, 5:pmfg item, 6:precision, 7:limit
        self.metrics = OrderedDict()
        self.pmfg = None
        self.pmfg_ts = None

        # Read configuration and prepare to connect
        self.config = self.pmconfig.set_config_path(DEFAULT_CONFIG)
        self.pmconfig.read_options()
        self.pmconfig.read_cmd_line()
        self.pmconfig.prepare_metrics()
        self.pmconfig.set_signal_handler()

    def options(self):
        """ Setup default command line argument option handling """
        opts = pmapi.pmOptions()
        opts.pmSetOptionCallback(self.option)
        opts.pmSetOverrideCallback(self.option_override)
        opts.pmSetShortOptions("a:h:LK:c:Ce:D:V?HGA:S:T:O:s:t:rRIi:jJ:4:58:9:nN:vmP:0:q:b:y:Q:B:Y:F:f:Z:zo:p:U:u:")
        opts.pmSetShortUsage("[option...] metricspec [...]")

        opts.pmSetLongOptionHeader("General options")
        opts.pmSetLongOptionArchive()      # -a/--archive
        opts.pmSetLongOptionArchiveFolio() # --archive-folio
        opts.pmSetLongOptionContainer()    # --container
        opts.pmSetLongOptionHost()         # -h/--host
        opts.pmSetLongOptionLocalPMDA()    # -L/--local-PMDA
        opts.pmSetLongOptionSpecLocal()    # -K/--spec-local
        opts.pmSetLongOption("config", 1, "c", "FILE", "config file path")
        opts.pmSetLongOption("check", 0, "C", "", "check config and metrics and exit")
        opts.pmSetLongOption("output-file", 1, "F", "OUTFILE", "output file")
        opts.pmSetLongOption("derived", 1, "e", "FILE|DFNT", "derived metrics definitions")
        opts.pmSetLongOption("daemonize", 0, "", "", "daemonize on startup")
        opts.pmSetLongOptionDebug()        # -D/--debug
        opts.pmSetLongOptionVersion()      # -V/--version
        opts.pmSetLongOptionHelp()         # -?/--help

        opts.pmSetLongOptionHeader("Reporting options")
        opts.pmSetLongOption("no-header", 0, "H", "", "omit headers")
        opts.pmSetLongOption("no-globals", 0, "G", "", "omit global metrics")
        opts.pmSetLongOptionAlign()        # -A/--align
        opts.pmSetLongOptionStart()        # -S/--start
        opts.pmSetLongOptionFinish()       # -T/--finish
        opts.pmSetLongOptionOrigin()       # -O/--origin
        opts.pmSetLongOptionSamples()      # -s/--samples
        opts.pmSetLongOptionInterval()     # -t/--interval
        opts.pmSetLongOptionTimeZone()     # -Z/--timezone
        opts.pmSetLongOptionHostZone()     # -z/--hostzone
        opts.pmSetLongOption("raw", 0, "r", "", "output raw counter values (no rate conversion)")
        opts.pmSetLongOption("raw-prefer", 0, "R", "", "prefer output raw counter values (no rate conversion)")
        opts.pmSetLongOption("ignore-incompat", 0, "I", "", "ignore incompatible instances (default: abort)")
        opts.pmSetLongOption("ignore-unknown", 0, "5", "", "ignore unknown metrics (default: abort)")
        opts.pmSetLongOption("names-change", 1, "4", "ACTION", "update/ignore/abort on PMNS change (default: ignore)")
        opts.pmSetLongOption("instances", 1, "i", "STR", "instances to report (default: all current)")
        opts.pmSetLongOption("live-filter", 0, "j", "", "perform instance live filtering")
        opts.pmSetLongOption("rank", 1, "J", "COUNT", "limit results to COUNT highest/lowest valued instances")
        opts.pmSetLongOption("limit-filter", 1, "8", "LIMIT", "default limit for value filtering")
        opts.pmSetLongOption("limit-filter-force", 1, "9", "LIMIT", "forced limit for value filtering")
        opts.pmSetLongOption("invert-filter", 0, "n", "", "perform ranking before live filtering")
        opts.pmSetLongOption("predicate", 1, "N", "METRIC", "set predicate filter reference metric")
        opts.pmSetLongOption("omit-flat", 0, "v", "", "omit single-valued metrics")
        opts.pmSetLongOption("include-labels", 0, "m", "", "include metric label info")
        opts.pmSetLongOption("timestamp-format", 1, "f", "STR", "strftime string for timestamp format")
        opts.pmSetLongOption("precision", 1, "P", "N", "prefer N digits after decimal separator (default: 3)")
        opts.pmSetLongOption("precision-force", 1, "0", "N", "force N digits after decimal separator")
        opts.pmSetLongOption("count-scale", 1, "q", "SCALE", "default count unit")
        opts.pmSetLongOption("count-scale-force", 1, "Q", "SCALE", "forced count unit")
        opts.pmSetLongOption("space-scale", 1, "b", "SCALE", "default space unit")
        opts.pmSetLongOption("space-scale-force", 1, "B", "SCALE", "forced space unit")
        opts.pmSetLongOption("time-scale", 1, "y", "SCALE", "default time unit")
        opts.pmSetLongOption("time-scale-force", 1, "Y", "SCALE", "forced time unit")

        opts.pmSetLongOption("url", 1, "u", "URL", "URL of endpoint to receive HTTP POST")
        opts.pmSetLongOption("http-timeout", 1, "o", "SECONDS", "timeout when sending HTTP POST")
        opts.pmSetLongOption("http-pass", 1, "p", "PASSWORD", "password for endpoint")
        opts.pmSetLongOption("http-user", 1, "U", "USERNAME", "username for endpoint")

        return opts

    def option_override(self, opt):
        """ Override standard PCP options """
        if opt in ('g', 'H', 'K', 'n', 'N', 'p'):
            return 1
        return 0

    def option(self, opt, optarg, _index):
        """ Perform setup for individual command line option """
        if opt == 'daemonize':
            self.daemonize = 1
        elif opt == 'K':
            if not self.speclocal or not self.speclocal.startswith(";"):
                self.speclocal = ";" + optarg
            else:
                self.speclocal = self.speclocal + ";" + optarg
        elif opt == 'c':
            self.config = optarg
        elif opt == 'C':
            self.check = 1
        elif opt == 'F':
            if os.path.exists(optarg):
                sys.stderr.write("File %s already exists.\n" % optarg)
                sys.exit(1)
            self.outfile = optarg
        elif opt == 'e':
            if not self.derived or not self.derived.startswith(";"):
                self.derived = ";" + optarg
            else:
                self.derived = self.derived + ";" + optarg
        elif opt == 'H':
            self.header = 0
        elif opt == 'G':
            self.globals = 0
        elif opt == 'r':
            self.type = 1
        elif opt == 'R':
            self.type_prefer = 1
        elif opt == 'I':
            self.ignore_incompat = 1
        elif opt == '5':
            self.ignore_unknown = 1
        elif opt == '4':
            if optarg == 'ignore':
                self.names_change = 0
            elif optarg == 'abort':
                self.names_change = 1
            elif optarg == 'update':
                self.names_change = 2
            else:
                sys.stderr.write("Unknown names-change action '%s' specified.\n" % optarg)
                sys.exit(1)
        elif opt == 'i':
            self.instances = self.instances + self.pmconfig.parse_instances(optarg)
        elif opt == 'j':
            self.live_filter = 1
        elif opt == 'J':
            self.rank = optarg
        elif opt == '8':
            self.limit_filter = optarg
        elif opt == '9':
            self.limit_filter_force = optarg
        elif opt == 'n':
            self.invert_filter = 1
        elif opt == 'N':
            self.predicate = optarg
        elif opt == 'v':
            self.omit_flat = 1
        elif opt == 'm':
            self.include_labels = 1
        elif opt == 'P':
            self.precision = optarg
        elif opt == '0':
            self.precision_force = optarg
        elif opt == 'f':
            self.timefmt = optarg
        elif opt == 'q':
            self.count_scale = optarg
        elif opt == 'Q':
            self.count_scale_force = optarg
        elif opt == 'b':
            self.space_scale = optarg
        elif opt == 'B':
            self.space_scale_force = optarg
        elif opt == 'y':
            self.time_scale = optarg
        elif opt == 'Y':
            self.time_scale_force = optarg
        elif opt == 'u':
            self.url = optarg
        elif opt == 'o':
            self.http_timeout = float(optarg)
        elif opt == 'U':
            self.http_user = optarg
        elif opt == 'P':
            self.http_pass = optarg
        else:
            raise pmapi.pmUsageErr()

    def connect(self):
        """ Establish PMAPI context """
        context, self.source = pmapi.pmContext.set_connect_options(self.opts, self.source, self.speclocal)

        self.pmfg = pmapi.fetchgroup(context, self.source)
        self.pmfg_ts = self.pmfg.extend_timestamp()
        self.context = self.pmfg.get_context()

        if pmapi.c_api.pmSetContextOptions(self.context.ctx, self.opts.mode, self.opts.delta):
            raise pmapi.pmUsageErr()

    def validate_config(self):
        """ Validate configuration options """
        if self.version != CONFVER:
            sys.stderr.write("Incompatible configuration file version (read v%s, need v%d).\n" %
                             (self.version, CONFVER))
            sys.exit(1)

        self.pmconfig.validate_common_options()

        self.pmconfig.validate_metrics(curr_insts=not self.live_filter)
        self.pmconfig.finalize_options()

    def execute(self):
        """ Fetch and report """
        # Debug
        if self.context.pmDebug(PM_DEBUG_APPL1):
            sys.stdout.write("Known config file keywords: " + str(self.keys) + "\n")
            sys.stdout.write("Known metric spec keywords: " + str(self.pmconfig.metricspec) + "\n")

        # Set delay mode, interpolation
        if self.context.type != PM_CONTEXT_ARCHIVE:
            self.delay = 1
            self.interpol = 1

        # Common preparations
        self.context.prepare_execute(self.opts, False, self.interpol, self.interval)

        # Headers
        if self.header == 1:
            self.header = 0
            self.write_header()

        # Just checking
        if self.check == 1:
            return

        # Daemonize when requested
        if self.daemonize == 1:
            self.opts.daemonize()

        # Align poll interval to host clock
        if self.context.type != PM_CONTEXT_ARCHIVE and self.opts.pmGetOptionAlignment():
            align = float(self.opts.pmGetOptionAlignment()) - (time.time() % float(self.opts.pmGetOptionAlignment()))
            time.sleep(align)

        # Main loop
        refresh_metrics = 0
        while self.samples != 0:
            # Refresh metrics as needed
            if refresh_metrics:
                refresh_metrics = 0
                self.pmconfig.update_metrics(curr_insts=not self.live_filter)

            # Fetch values
            refresh_metrics = self.pmconfig.fetch()
            if refresh_metrics < 0:
                break

            # Report and prepare for the next round
            self.report(self.pmfg_ts())
            if self.samples and self.samples > 0:
                self.samples -= 1
            if self.delay and self.interpol and self.samples != 0:
                self.pmconfig.pause()

        # Allow to flush buffered values / say goodbye
        self.report(None)

    def report(self, tstamp):
        """ Report metric values """
        if tstamp is not None:
            tstamp = tstamp.strftime(self.timefmt)

        self.write_opentelemetry(tstamp)

    def write_header(self):
        """ Write info header """
        output = self.outfile if self.outfile else "stdout"
        if self.context.type == PM_CONTEXT_ARCHIVE:
            sys.stdout.write('{ "//": "Writing %d archived metrics to %s..." }\n{ "//": "(Ctrl-C to stop)" }\n'
                             % (len(self.metrics), output))
            return

        sys.stdout.write('{ "//": "Waiting for %d metrics to be written to %s' % (len(self.metrics), output))
        if self.runtime != -1:
            sys.stdout.write(':" }\n{ "//": "%s samples(s) with %.1f sec interval ~ %d sec runtime." }\n' %
                             (self.samples, float(self.interval), self.runtime))
        elif self.samples:
            duration = (self.samples - 1) * float(self.interval)
            sys.stdout.write(':" }\n{ "//": "%s samples(s) with %.1f sec interval ~ %d sec runtime." }\n' %
                             (self.samples, float(self.interval), duration))
        else:
            sys.stdout.write('..." }\n{ "//": "(Ctrl-C to stop)" }\n')

    def write_opentelemetry(self, timestamp):
        """ Write results in opentelemetry format """
        if timestamp is None:
            # Silent goodbye, close in finalize()
            return

        ts = self.context.datetime_to_secs(self.pmfg_ts(), PM_TIME_SEC)

        if self.prev_ts is None:
            self.prev_ts = ts

        if not self.writer and not self.url:
            if self.outfile is None:
                self.writer = sys.stdout
            else:
                self.writer = open(self.outfile, 'wt')

        results = self.pmconfig.get_ranked_results(valid_only=True)
        def get_type_string(desc):
            """ Get metric type as string """
            if desc.contents.type == pmapi.c_api.PM_TYPE_32:
                mtype = "32"
            elif desc.contents.type == pmapi.c_api.PM_TYPE_U32:
                mtype = "u32"
            elif desc.contents.type == pmapi.c_api.PM_TYPE_64:
                mtype = "64"
            elif desc.contents.type == pmapi.c_api.PM_TYPE_U64:
                mtype = "u64"
            elif desc.contents.type == pmapi.c_api.PM_TYPE_FLOAT:
                mtype = "float"
            elif desc.contents.type == pmapi.c_api.PM_TYPE_DOUBLE:
                mtype = "float"
            elif desc.contents.type == pmapi.c_api.PM_TYPE_STRING:
                mtype = "string"
            else:
                mtype = "unknown"
            return mtype

        def attribute_converter(a_dictionary):
            entry_list = []
            for key, value in a_dictionary.items():
                new_dictionary = {"key": key, "value": {"stringValue": value}}
                entry_list.append(new_dictionary)

            return entry_list

        def data_attribute_function(labels, context, desc, inst, name):
            attribute_list = []
            new_dict = {}
            new_dict["semantics"] = self.context.pmSemStr(desc.contents.sem)
            new_dict["type"] = get_type_string(desc)
            if desc.indom != PM_INDOM_NULL:
                new_dict["instname"] = name
                new_dict["instid"] = inst
            for key in labels:
                new_dict.update(labels[key])
            tmp_list = attribute_converter(new_dict)
            for item in tmp_list:
                attribute_list.append(item)
            return attribute_list

        def data_points_function(metric, results, labels, context, desc):
            numdatapoints_list = []
            for inst, name, value in results[metric]:
                datapoint_dict = {}
                tmp_type = get_type_string(desc)
                if '32' or '64' in tmp_type:
                    datapoint_dict["asInt"] = value
                else:
                    datapoint_dict["asDouble"] = value
                if inst != PM_INDOM_NULL:
                    datapoint_dict["instance"] = inst
                datapoint_dict["startTimeUnixNano"] = ts
                datapoint_dict["attributes"] = data_attribute_function(labels, context, desc, inst, name)
                numdatapoints_list.append(datapoint_dict)
            return numdatapoints_list

        def attribute_function():
            attribute_list = []
            body = {}
            body["os.type"] = platform.system().lower()
            body["telemetry.sdk.language"] = "python"
            body["telemetry.sdk.name"] = "opentelemetry"
            body["telemetry.sdk.version"] = "1.24.0"
            body["service.name"] = "pcp"
            if self.url:
                body["server.address"] = self.url
            tmp_list = attribute_converter(body)
            for item in tmp_list:
                attribute_list.append(item)
            return attribute_list

        def sum_function(metric, results, labels, context, desc):
            sum_body = {}
            sum_body["aggregationTemporality"] = 1
            sum_body["isMonotonic"] = 'true'
            sum_body["dataPoints"] = data_points_function(metric, results, labels, context, desc)
            return sum_body

        def gauge_function(metric, results, labels, context, desc):
            gauge_body = {}
            gauge_body["dataPoints"] = data_points_function(metric, results, labels, context, desc)
            return gauge_body

        def scope_function(metric):
            split_metric = metric.split(".")
            scope_body = {}
            scope_body["name"] = "pcp.%s" % split_metric[0]
            scope_body["version"] = self.version
            return scope_body

        def unit_function(unit):
            def dimTime_units(num):
                if num == 0:
                    return "ns"
                elif num == 1:
                    return "us"
                elif num == 2:
                    return "ms"
                elif num == 3:
                    return "s"
                elif num == 4:
                    return "min"
                elif num == 5:
                    return "h"
                else:
                    return "none"

            def dimSpace_units(num):
                if num == 0:
                    return "By"
                elif num == 1:
                    return "KiBy"
                elif num == 2:
                    return "MiBy"
                elif num == 3:
                    return "GiBy"
                elif num == 4:
                    return "TiBy"
                elif num == 5:
                    return "PiBy"
                elif num == 6:
                    return "EiBy"
                elif num == 7:
                    return "ZiBy"
                elif num == 8:
                    return "YiBy"
                else:
                    return "none"

            def dimCount_units(num):
                if num == 0:
                    return "count"
                elif num == 1:
                    return "count x 10"
                else:
                    return "count x 10%d" % num

            if unit.dimTime < 0:
                if unit.dimSpace:
                    return "%s / %s" % (dimSpace_units(unit.scaleSpace), dimTime_units(unit.scaleTime))
                elif unit.dimCount:
                    return "%s / %s" % (dimCount_units(unit.scaleSpace), dimTime_units(unit.scaleTime))
            elif unit.dimCount < 0:
                if unit.dimTime:
                    return "%s / %s" % (dimTime_units(unit.scaleSpace), dimCount_units(unit.scaleTime))
                elif unit.dimSpace:
                    return "%s / %s" % (dimSpace_units(unit.scaleSpace), dimCount_units(unit.scaleTime))
            elif unit.dimSpace:
                return dimSpace_units(unit.scaleSpace)
            elif unit.dimTime:
                return dimTime_units(unit.scaleTime)
            elif unit.dimCount:
                return dimCount_units(unit.scaleCount)
            else:
                return "none"

        def scopeMetric_function(results):
            body = {}
            body["metrics"] = []
            for metric in results:
                # variable declaration
                context = self.pmfg.get_context()
                pmid = context.pmLookupName(metric)
                i = list(self.metrics.keys()).index(metric)
                labels = context.pmLookupLabels(pmid[0])
                desc = self.pmconfig.descs[i]
                unit = desc.contents.units

                body["scope"] = scope_function(metric)
                metric_dict = {}
                metric_dict["name"] = metric
                metric_dict["unit"] = unit_function(unit)
                metric_dict["description"] = context.pmLookupText(pmid[0])
                if desc.sem == cpmapi.PM_SEM_COUNTER:
                    metric_dict["sum"] = sum_function(metric, results, labels, context, desc)
                else:
                    metric_dict["gauge"] = gauge_function(metric, results, labels, context, desc)
                body["metrics"].append(metric_dict)
            return body

        self.data = {"resourceMetrics": [{"resource":  {"attributes": attribute_function()},
                                          "scopeMetrics":[scopeMetric_function(results)]}]}
        data = json.dumps(self.data, indent=INDENT, sort_keys=True, ensure_ascii=False, separators=(',', ': '))

        if self.url:
            auth = None
            if self.http_user and self.http_pass:
                auth = requests.auth.HTTPBasicAuth(self.http_user, self.http_pass)
            try:
                timeout = self.http_timeout
                headers = {'Content-Type': 'application/json'}
                res = requests.post(self.url, data=data, auth=auth, headers=headers, timeout=timeout)
                if res.status_code > 299:
                    msg = "Cannot send metrics: HTTP code %s\n" % str(res.status_code)
                    sys.stderr.write(msg)
            except requests.exceptions.ConnectionError as post_error:
                msg = "Cannot connect to server at %s: %s\n" % (self.url, str(post_error))
                sys.stderr.write(msg)
        elif self.outfile:
            self.writer.write(data)
        else:
            print(data)

    def finalize(self):
        """ Finalize and clean up """
        if self.writer:
            try:
                self.writer.write("\n")
                self.writer.flush()
            except IOError as write_error:
                if write_error.errno != errno.EPIPE:
                    raise
            try:
                self.writer.close()
            except Exception:
                pass
            self.writer = None

if __name__ == '__main__':
    try:
        P = PCP2OPENTELEMETRY()
        P.connect()
        P.validate_config()
        P.execute()
        P.finalize()
    except pmapi.pmErr as error:
        sys.stderr.write("%s: %s" % (error.progname(), error.message()))
        if error.message() == "Connection refused":
            sys.stderr.write("; is pmcd running?")
        sys.stderr.write("\n")
        sys.exit(1)
    except pmapi.pmUsageErr as usage:
        usage.message()
        sys.exit(1)
    except IOError as error:
        if error.errno != errno.EPIPE:
            sys.stderr.write("%s\n" % str(error))
            sys.exit(1)
    except KeyboardInterrupt:
        sys.stdout.write("\n")
        P.finalize()
