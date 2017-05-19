#!/usr/bin/env pmpython
#
# Copyright (C) 2014-2017 Red Hat.
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
""" Relay PCP metrics to an InfluxDB server """

import re
import sys
import time

from pcp import pmapi
import cpmapi as c_api


class Metric:
    """ A wrapper around metrics, due to InfluxDB's non-heirachical way of
    organizing metrics.

    For example, take disk.partitions.read, which (on my machine) has 4
    instances (sda1, sda2, sda3, and sr0). For graphite, appending the instance
    name after the metric name with a dot (i.e. disk.partitions.read.sda1) is
    both easy and the correct solution, since graphite stores metrics
    in an heirarchy.

    For InfluxDB, the proper solution is to submit a "measurement" with 4
    "fields". The request body could look like:

        disk_partitions_read,host=myhost.example.com sda1=5,sda2=4,sda3=10,sr0=0 1147483647000000000

    If there is only a single value, like for proc.nprocs, then the field key
    will be "value". For example:

        proc_nprocs,host=myhost.example.com value=200 1147483647000000000

    This class deals with this format. 
    """

    def __init__(self, name):
        self.name = self.sanitize_name(name)
        self.fields = dict()
        self.tags = None

    def add_field(self, key="value", value=None):
        if value:
            self.fields[key] = value

    def set_tag_string(self, tag_str):
        self.tags = tag_str

    def sanitize_name(self, name):
        tmp = name

        for c in ['.', '-']:
            tmp = tmp.replace(c, '_')

        while '__' in tmp:
            tmp = tmp.replace('__', '_')

        return tmp

    def set_timestamp(self, ts):
        self.ts = ts

    def __str__(self):
        tmp = self.name

        if self.tags:
            tmp += ',' + self.tags

        tmp += ' '

        fields = []
        for k in self.fields:
            fields.append(k + '=' + str(self.fields[k]))

        tmp += ','.join(fields)
        tmp += ' '

        # converting to nanoseconds for influxdb
        tmp += str(self.ts) + '000000000'

        return tmp


class WriteBody(object):
    """ Create a request to POST to /write on an InfluxDB server

    name will be used for the measurement name after it has been
    transformed to be allowable. Characters like '-' and '.' will be replaced
    with '_', and multiple underscores in a row will be replaced with a single
    underscore.

    value will be put into the measurement with a field key of 'value'.
    It should be a numeric type, but it will _not_ be checked, just cast
    directly to a string.

    timestamp should be an integer that is unix time from epoch in seconds.

    tags should be a dictionary of tags to add, with keys being tag
    keys and values being tag values.
    """

    def __init__(self):
        self.metrics = []

    def add(self, metric):
        if metric.fields:
            self.metrics.append(metric)

    def __str__(self):
        if self.metrics:
            return "\n".join(map(lambda t: str(t), self.metrics))

        raise ValueError("Invalid request - no metrics")


class Relay(object):
    """ Sends a periodic report to InfluxDB about all instances of named
        metrics.  Knows about some of the default PCP arguments.
    """

    def describe_source(self):
        """ Return a string describing our context; apprx. inverse of
            pmapi.fromOptions
        """

        ctxtype = self.context.type
        if ctxtype == c_api.PM_CONTEXT_ARCHIVE:
            return "archive " + ", ".join(self.opts.pmGetOptionArchives())
        elif ctxtype == c_api.PM_CONTEXT_HOST:
            hosts = self.opts.pmGetOptionHosts()

            # pmapi.py idiosyncracy; it has already defaulted to this
            if hosts is None:
                hosts = ["local:"]

            return "host " + ", ".join(hosts)
        elif ctxtype == c_api.PM_CONTEXT_LOCAL:
            hosts = ["local:"]

            return "host " + ", ".join(hosts)
        else:
            raise pmapi.pmUsageErr

    def __init__(self):
        """ Construct object, parse command line """
        self.context = None
        self.database = 'pcp'
        self.influxdb_tags = ''
        self.influxdb_address = 'http://127.0.0.1:8086'
        self.influxdb_user = None
        self.influxdb_pass = None
        self.sample_count = 0
        self.unitsstr = None
        self.units = None
        self.units_mult = None

        # option setup
        self.opts = pmapi.pmOptions()
        self.opts.pmSetShortOptions("a:O:s:T:g:p:P:r:m:t:u:h:t:D:LV?")
        self.opts.pmSetShortUsage("[options] metricname ...")
        self.opts.pmSetLongOptionText("""
Description: Periodically, relay raw values of all instances of a
given hierarchies of PCP metrics to an InfluxDB server on the network.""")
        self.opts.pmSetLongOptionHeader("Options")
        self.opts.pmSetOptionCallback(self.option)

        # common options
        self.opts.pmSetLongOptionVersion()
        self.opts.pmSetLongOptionArchive()
        self.opts.pmSetLongOptionOrigin()
        self.opts.pmSetLongOptionSamples()
        self.opts.pmSetLongOptionFinish()
        self.opts.pmSetLongOptionDebug()
        self.opts.pmSetLongOptionHost()
        self.opts.pmSetLongOptionLocalPMDA()
        self.opts.pmSetLongOptionInterval()

        # custom options
        self.opts.pmSetLongOption("influxdb-address", 1, 'i', '',
                                  "InfluxDB HTTP/HTTPS address " +
                                  "(default \"" + self.influxdb_address +
                                  "\")")
        self.opts.pmSetLongOption("units", 1, 'u', '',
                                  "rescale units " +
                                  "(e.g. \"MB\", will omit incompatible units)")
        self.opts.pmSetLongOption("database", 1, 'd', '',
                                  "database for metric (default \"pcp\")")
        self.opts.pmSetLongOption("db-user", 1, 'U', '',
                                  "username for InfluxDB database")
        self.opts.pmSetLongOption("db-password", 1, 'P', '',
                                  "password for InfluxDB database")
        self.opts.pmSetLongOption("tag-string", 1, 'I', '',
                                  "string of tags to add to the metrics")
        self.opts.pmSetLongOptionHelp()

        # parse options
        self.context = pmapi.pmContext.fromOptions(self.opts, sys.argv)
        self.interval = self.opts.pmGetOptionInterval() or pmapi.timeval(60, 0)
        if self.unitsstr is not None:
            units = self.context.pmParseUnitsStr(self.unitsstr)
            (self.units, self.units_mult) = units
        self.metrics = []
        self.pmids = []
        self.descs = []
        metrics = self.opts.pmGetOperands()
        if metrics:
            for m in metrics:
                try:
                    self.context.pmTraversePMNS(m,
                                                self.handle_candidate_metric)
                except pmapi.pmErr as error:
                    sys.stderr.write("Excluding metric %s (%s)\n" %
                                     (m, str(error)))

            sys.stderr.flush()

        if len(self.metrics) == 0:
            sys.stderr.write("No acceptable metrics specified.\n")
            raise pmapi.pmUsageErr()

        # Report what we're about to do
        print("Relaying %d %smetric(s) to database %s with tags %s from %s "
              "to %s every %f s" %
              (len(self.metrics),
               "rescaled " if self.units else "",
               self.database,
               self.influxdb_tags,
               self.describe_source(),
               self.influxdb_address,
               self.interval))

        sys.stdout.flush()

    def option(self, opt, optarg, index):
        # need only handle the non-common options
        if opt == 'i':
            self.influxdb_address = optarg
        elif opt == 'u':
            self.unitsstr = optarg
        elif opt == 'd':
            self.database = optarg
        elif opt == 'U':
            self.influxdb_user = optarg
        elif opt == 'P':
            self.influxdb_pass = optarg
        elif opt == 'I':
            self.influxdb_tags = optarg
        else:
            raise pmapi.pmUsageErr()

    # Check the given metric name (a leaf in the PMNS) for
    # acceptability for InfluxDB: it needs to be numeric, and
    # convertable to the given unit (if specified).
    #
    # Print an error message here if needed; can't throw an exception
    # through the pmapi pmTraversePMNS wrapper.
    def handle_candidate_metric(self, name):
        try:
            pmid = self.context.pmLookupName(name)[0]
            desc = self.context.pmLookupDescs(pmid)[0]
        except pmapi.pmErr as err:
            sys.stderr.write("Excluding metric %s (%s)\n" % (name, str(err)))
            return

        # reject non-numeric types (future pmExtractValue failure)
        types = desc.contents.type
        if not ((types == c_api.PM_TYPE_32) or
                (types == c_api.PM_TYPE_U32) or
                (types == c_api.PM_TYPE_64) or
                (types == c_api.PM_TYPE_U64) or
                (types == c_api.PM_TYPE_FLOAT) or
                (types == c_api.PM_TYPE_DOUBLE)):
            sys.stderr.write("Excluding metric %s (%s)\n" %
                             (name, "need numeric type"))
            return

        # reject dimensionally incompatible (future pmConvScale failure)
        if self.units is not None:
            units = desc.contents.units
            if (units.dimSpace != self.units.dimSpace or
                    units.dimTime != self.units.dimTime or
                    units.dimCount != self.units.dimCount):
                sys.stderr.write("Excluding metric %s (%s)\n" %
                                 (name, "incompatible dimensions"))
                return

        self.metrics.append(name)
        self.pmids.append(pmid)
        self.descs.append(desc)


    # Convert a python list of pmids (numbers) to a ctypes LP_c_uint
    # (a C array of uints).
    def convert_pmids_to_ctypes(self, pmids):
        from ctypes import c_uint
        pmidA = (c_uint * len(pmids))()
        for i, p in enumerate(pmids):
            pmidA[i] = c_uint(p)
        return pmidA

    def send(self, timestamp, metrics):
        try:
            import requests
        except ImportError:
            print("Please install the python 'requests' library")
            sys.exit(1)

        body = WriteBody()

        for metric in metrics:
            metric.set_tag_string(self.influxdb_tags)
            metric.set_timestamp(timestamp)
            body.add(metric)

        url = self.influxdb_address + '/write'
        params = {'db': self.database}
        auth = None

        if self.influxdb_user and self.influxdb_pass:
            auth = requests.auth.HTTPBasicAuth(self.influxdb_user,
                                               self.influxdb_pass)

        try:
            res = requests.post(url, params=params, data=str(body), auth=auth)

            if res.status_code != 204:
                msg = "could not send metrics: "

                if res.status_code == 200:
                    msg += "influx could not complete the request"
                elif res.status_code == 404:
                    msg += "Got an HTTP code 404. This most likely means "
                    msg += "that the requested database '"
                    msg += self.database
                    msg += "' does not exist"
                else:
                    msg += "request to "
                    msg += res.url
                    msg += " failed with code "
                    msg += str(res.status_code)
                    msg += "\n"
                    msg += "body of request is:\n"
                    msg += str(body)

                print(msg)
        except ValueError:
            print("Can't send a request that has no metrics")
        except requests.exceptions.ConnectionError:
            print("Can't connect to an InfluxDB server, running anyways...")

    def sanitize_nameindom(self, string):
        """ Quote the given instance-domain string for proper digestion
        by influxdb. """
        return "_" + re.sub('[^a-zA-Z_0-9-]', '_', string)

    def execute(self):
        """ Using a PMAPI context (could be either host or archive),
            fetch and report a fixed set of values related to influxdb.
        """

        # align poll interval to host clock
        ctype = self.context.type
        if ctype == c_api.PM_CONTEXT_HOST or ctype == c_api.PM_CONTEXT_LOCAL:
            align = float(self.interval) - (time.time() % float(self.interval))
            time.sleep(align)

        # We would like to do: result = self.context.pmFetch(self.pmids)
        # But pmFetch takes ctypes array-of-uint's and not a python list;
        # ideally, pmFetch would become polymorphic to improve this code.
        result = self.context.pmFetch(self.convert_pmids_to_ctypes(self.pmids))
        sample_time = result.contents.timestamp.tv_sec
             # + (result.contents.timestamp.tv_usec/1000000.0)

        if ctype == c_api.PM_CONTEXT_ARCHIVE:
            endtime = self.opts.pmGetOptionFinish()
            if endtime is not None:
                if float(sample_time) > float(endtime.tv_sec):
                    raise SystemExit

        metrics = []

        for i, name in enumerate(self.metrics):
            tmp = Metric(name)
            for j in range(0, result.contents.get_numval(i)):
                # a fetch or other error will just omit that data value
                # from the influxdb-bound set
                try:
                    atom = self.context.pmExtractValue(
                        result.contents.get_valfmt(i),
                        result.contents.get_vlist(i, j),
                        self.descs[i].contents.type, c_api.PM_TYPE_FLOAT)

                    inst = result.contents.get_vlist(i, j).inst
                    if inst is None or inst < 0:
                        suffix = "value"
                    else:
                        indom = self.context.pmNameInDom(self.descs[i], inst)
                        suffix = self.sanitize_nameindom(indom)

                    # Rescale if desired
                    if self.units is not None:
                        atom = self.context.pmConvScale(c_api.PM_TYPE_FLOAT,
                                                        atom,
                                                        self.descs, i,
                                                        self.units)

                    if self.units_mult is not None:
                        atom.f = atom.f * self.units_mult

                    tmp.add_field(key=suffix, value=atom.f)

                except pmapi.pmErr as error:
                    sys.stderr.write("%s[%d]: %s, continuing\n" %
                                     (name, inst, str(error)))

            metrics.append(tmp)

        self.send(sample_time, metrics)
        self.context.pmFreeResult(result)

        self.sample_count += 1
        max_samples = self.opts.pmGetOptionSamples()
        if max_samples is not None and self.sample_count >= max_samples:
            raise SystemExit


if __name__ == '__main__':
    try:
        relay = Relay()
        while True:
            relay.execute()

    except pmapi.pmErr as error:
        if error.args[0] == c_api.PM_ERR_EOL:
            pass
        sys.stderr.write('%s: %s\n' % (error.progname(), error.message()))
    except pmapi.pmUsageErr as usage:
        usage.message()
    except KeyboardInterrupt:
        pass
