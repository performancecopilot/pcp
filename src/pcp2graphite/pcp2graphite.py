#!/usr/bin/pcp python
#
# Copyright (C) 2014-2015 Red Hat.
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
""" Relay PCP metrics to a graphite server """

import re
import sys
import time

from pcp import pmapi
import cpmapi as c_api

class GraphiteRelay(object):
    """ Sends a periodic report to graphite about all instances of named
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
            if hosts is None: # pmapi.py idiosyncracy; it has already defaulted to this
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
        self.socket = None
        self.sample_count = 0
        self.opts = pmapi.pmOptions()
        self.opts.pmSetShortOptions("a:O:s:T:g:p:P:u:m:t:h:t:D:LV?")
        self.opts.pmSetShortUsage("[options] metricname ...")
        self.opts.pmSetOptionCallback(self.option)
        self.opts.pmSetOverrideCallback(self.option_override)
        self.opts.pmSetLongOptionText("""
Description: Periodically, relay raw values of all instances of a
given hierarchies of PCP metrics to a graphite/carbon server on the
network.""")
        self.opts.pmSetLongOptionHeader("Options")
        self.opts.pmSetLongOptionVersion() # -V
        self.opts.pmSetLongOptionArchive() # -a FILE
        self.opts.pmSetLongOptionOrigin() # -O TIME
        self.opts.pmSetLongOptionSamples() # -s NUMBER
        self.opts.pmSetLongOptionFinish() # -T NUMBER
        self.opts.pmSetLongOptionDebug() # -D stuff
        self.opts.pmSetLongOptionHost() # -h HOST
        self.opts.pmSetLongOptionLocalPMDA() # -L
        self.opts.pmSetLongOptionInterval() # -t NUMBER
        self.opts.pmSetLongOption("graphite-host", 1, 'g', '',
                                  "graphite server host " +
                                  "(default \"localhost\")")
        self.opts.pmSetLongOption("pickled-port", 1, 'p', '',
                                  "graphite pickled port (default 2004)")
        self.opts.pmSetLongOption("text-port", 1, 'P', '',
                                  "graphite plaintext port (usually 2003)")
        self.opts.pmSetLongOption("units", 1, 'u', '',
                                  "rescale units " +
                                  "(e.g. \"MB\", will omit incompatible units)")
        self.opts.pmSetLongOption("prefix", 1, 'm', '',
                                  "prefix for metric names (default \"pcp.\")")
        self.opts.pmSetLongOptionHelp()
        self.graphite_host = "localhost"
        self.graphite_port = 2004
        self.pickle = True
        self.prefix = "pcp."
        self.unitsstr = None
        self.units = None # pass verbatim by default
        self.units_mult = None # pass verbatim by default

        # now actually parse
        self.context = pmapi.pmContext.fromOptions(self.opts, sys.argv)
        self.interval = self.opts.pmGetOptionInterval() or pmapi.timeval(60, 0)
        if self.unitsstr is not None:
            units = self.context.pmParseUnitsStr(self.unitsstr)
            (self.units, self.units_mult) = units
        self.metrics = []
        self.pmids = []
        self.descs = []
        metrics = self.opts.pmNonOptionsFromList(sys.argv)
        if metrics:
            for m in metrics:
                try:
                    self.context.pmTraversePMNS(m, self.handle_candidate_metric)
                except pmapi.pmErr as error:
                    sys.stderr.write("Excluding metric %s (%s)\n" %
                                     (m, str(error)))
            sys.stderr.flush()

        if len(self.metrics) == 0:
            sys.stderr.write("No acceptable metrics specified.\n")
            raise pmapi.pmUsageErr()

        # Report what we're about to do
        print("Relaying %d %smetric(s) with prefix %s from %s "
              "in %s mode to %s:%d every %f s" %
              (len(self.metrics),
               "rescaled " if self.units else "",
               self.prefix,
               self.describe_source(),
               "pickled" if self.pickle else "text",
               self.graphite_host, self.graphite_port,
               self.interval))
        sys.stdout.flush()

    def option_override(self, opt):
        if (opt == 'p') or (opt == 'g'):
            return 1
        return 0

    def option(self, opt, optarg, index):
        # need only handle the non-common options
        if opt == 'g':
            self.graphite_host = optarg
        elif opt == 'p':
            self.graphite_port = int(optarg if optarg else "2004")
            self.pickle = True
        elif opt == 'P':
            self.graphite_port = int(optarg if optarg else "2003")
            self.pickle = False
        elif opt == 'u':
            self.unitsstr = optarg
        elif opt == 'm':
            self.prefix = optarg
        else:
            raise pmapi.pmUsageErr()


    # Check the given metric name (a leaf in the PMNS) for
    # acceptability for graphite: it needs to be numeric, and
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

    def send(self, timestamp, miv_tuples):
        import socket
        try:
            # reuse socket
            if self.socket is None:
                self.socket = socket.create_connection((self.graphite_host,
                                                        self.graphite_port))
            if self.pickle:
                import pickle
                import struct
                pickled_input = []
                for (metric, value) in miv_tuples:
                    pickled_input.append((metric, (timestamp, value)))
                    # protocol=0 in case carbon is running under an
                    # older python version than we are
                    pickled_output = pickle.dumps(pickled_input, protocol=0)
                    header = struct.pack("!L", len(pickled_output))
                    msg = header + pickled_output
                    if self.context.pmDebug(c_api.PM_DEBUG_APPL0):
                        print ("Sending %s #tuples %d" %
                               (time.ctime(timestamp), len(pickled_input)))
                    self.socket.send(msg)
            else:
                for (metric, value) in miv_tuples:
                    message = ("%s %s %s\n" % (metric, value, timestamp))
                    msg = str.encode(message)
                    if self.context.pmDebug(c_api.PM_DEBUG_APPL0):
                        print("Sending %s: %s" %
                              (time.ctime(timestamp), msg.rstrip().decode()))
                    self.socket.send(msg)
        except socket.error as err:
            sys.stderr.write("cannot send message to %s:%d, %s, continuing\n" %
                             (self.graphite_host, self.graphite_port,
                              err.strerror))
            self.socket = None
            return

    def sanitize_nameindom(self, string):
        """ Quote the given instance-domain string for proper digestion
        by carbon/graphite. """
        return "_" + re.sub('[^a-zA-Z_0-9-]', '_', string)

    def execute(self):
        """ Using a PMAPI context (could be either host or archive),
            fetch and report a fixed set of values related to graphite.
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

        miv_tuples = []

        for i, name in enumerate(self.metrics):
            for j in range(0, result.contents.get_numval(i)):
                # a fetch or other error will just omit that data value
                # from the graphite-bound set
                try:
                    atom = self.context.pmExtractValue(
                        result.contents.get_valfmt(i),
                        result.contents.get_vlist(i, j),
                        self.descs[i].contents.type, c_api.PM_TYPE_FLOAT)

                    inst = result.contents.get_vlist(i, j).inst
                    if inst is None or inst < 0:
                        suffix = ""
                    else:
                        indom = self.context.pmNameInDom(self.descs[i], inst)
                        suffix = "." + self.sanitize_nameindom(indom)

                    # Rescale if desired
                    if self.units is not None:
                        atom = self.context.pmConvScale(c_api.PM_TYPE_FLOAT,
                                                        atom,
                                                        self.descs, i,
                                                        self.units)

                    if self.units_mult is not None:
                        atom.f = atom.f * self.units_mult

                    miv_tuples.append((self.prefix+name+suffix, atom.f))

                except pmapi.pmErr as error:
                    sys.stderr.write("%s[%d]: %s, continuing\n" %
                                     (name, inst, str(error)))

        self.send(sample_time, miv_tuples)
        self.context.pmFreeResult(result)

        self.sample_count += 1
        max_samples = self.opts.pmGetOptionSamples()
        if max_samples is not None and self.sample_count >= max_samples:
            raise SystemExit


if __name__ == '__main__':
    try:
        G = GraphiteRelay()
        while True:
            G.execute()

    except pmapi.pmErr as error:
        if error.args[0] == c_api.PM_ERR_EOL:
            pass
        sys.stderr.write('%s: %s\n' % (error.progname(), error.message()))
    except pmapi.pmUsageErr as usage:
        usage.message()
    except KeyboardInterrupt:
        pass
