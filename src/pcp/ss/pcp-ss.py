#!/usr/bin/env pmpython
#
# Copyright (C) 2021 Red Hat.
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
# pylint: disable=line-too-long,bad-continuation,broad-except,bare-except
# pylint: disable=missing-docstring,multiple-imports,unused-import
# pylint: disable=too-many-boolean-expressions,consider-using-dict-items

""" Display socket statistics """

import os, sys, argparse, errno

# PCP Python PMAPI
from pcp import pmapi
from cpmapi import PM_CONTEXT_ARCHIVE, PM_CONTEXT_HOST
from cpmapi import PM_ERR_EOL, PM_MODE_INTERP

# pmns prefix for pmdasockets(1) metrics
pmns = "network.persocket"

def remove_prefix(text, prefix):
    """ remove prefix from text (for python < 3.9) """
    return text[text.startswith(prefix) and len(prefix):]

class SS(object):
    """ PCP client implementation of the ss(1) tool, using pmdasockets(1) """

    def __init__(self):
        """ Construct object - prepare for command line handling """
        self.args = self.options()
        self.source = "local:"
        self.speclocal = None
        self.metrics = []
        self.instD = {} # { instid: instname }
        self.metricsD = {} # { name: fetchgroup }
        self.descsD = {} # { name: pmDesc }
        self.valuesD = {} # { name: {instid: value} }
        self.context = None
        self.pmfg = None

    def options(self):
        """ define command line arguments """
        p = argparse.ArgumentParser()
        p.add_argument('-V', '--version', action='store_true', help='output version information')
        p.add_argument('-n', '--numeric', action='store_true', help='don\'t resolve service names or port names (currently always set)')
        #p.add_argument('-r', '--resolve', action='store_true', help='resolve host names (currently never set)')
        p.add_argument('-a', '--all', action='store_true', help='display both listening and non-listening states')
        p.add_argument('-l', '--listening', action='store_true', default=False, help='display only listening sockets')
        p.add_argument('-o', '--options', action='store_true', help='show timer information')
        p.add_argument('-e', '--extended', action='store_true', help='show detailed socket information')
        p.add_argument('-m', '--memory', action='store_true', help='show socket memory usage')
        #p.add_argument('-p', '--processes', action='store_true', help='show process using socket (not yet implemented)')
        p.add_argument('-i', '--info', action='store_true', help='show internal TCP information')
        #p.add_argument('-s', '--summary', action='store_true', help='show socket usage summary (not yet implemented)')
        #p.add_argument('-b', '--bpf', action='store_true', help='show bpf filter socket information (not yet implemented)')
        #p.add_argument('-E', '--events', action='store_true', help='continually display sockets as they are destroyed (not implemented)')
        #p.add_argument('-Z', '--context', action='store_true', help='display process SELinux security contexts (not implemented)')
        #p.add_argument('-z', '--contexts', action='store_true', help='display process and socket SELinux security contexts (not implemented)')
        #p.add_argument('-N', '--net', action='store_true', help='switch to the specified network namespace name (not implemented)')
        p.add_argument('-4', '--ipv4', action='store_true', help='display only IP version 4 sockets')
        p.add_argument('-6', '--ipv6', action='store_true', help='display only IP version 6 sockets')
        #p.add_argument('-0', '--packet', action='store_true', help='display PACKET sockets (not implemented)')
        p.add_argument('-t', '--tcp', action='store_true', help='display only TCP sockets')
        #p.add_argument('-M', '--mptcp', action='store_true', help='display only MPTCP sockets (not implemented)')
        #p.add_argument('-S', '--sctp', action='store_true', help='display only SCTP sockets (not implemented)')
        p.add_argument('-u', '--udp', action='store_true', help='display only UDP sockets')
        #p.add_argument('-d', '--dccp', action='store_true', help='display only DCCP sockets (not implemented)')
        #p.add_argument('-w', '--raw', action='store_true', help='display only RAW sockets (not implemented)')
        #p.add_argument('-x', '--unix', action='store_true', help='display only Unix domain sockets (not implemented)')
        p.add_argument('-H', '--noheader', action='store_true', help='Suppress header line')
        p.add_argument('-O', '--oneline', action='store_true', help='print each socket\'s data on a single line')
        args = p.parse_args()

        # special cases
        if not (args.tcp or args.udp): # or args.mptcp or args.sctp or args.packet or args.dccp or args.raw or args.unix):
            args.tcp = args.udp = True # args.mptcp = args.sctp = args.packet = args.dccp = args.raw = args.unix = True

        if not (args.ipv4 or args.ipv6):
            # default to both ipv4 and ipv6, subject to the prevailing filter
            args.ipv4 = args.ipv6 = True

        return args

    def connect(self):
        """ Establish a fetchgroup PMAPI context to archive, host or local,
            via environment passed in by pcp(1). The ss(1) command has many
            clashes with standard PCP arguments so this is the only supported
            invocation. Debug options (if any) are set for us by pcp(1).
            Return True or False if we fail to connect.
        """
        # source
        pcp_host = os.getenv("PCP_HOST")
        pcp_archive = os.getenv("PCP_ARCHIVE")

        # time window - only via environment: too many clashes with ss args
        pcp_origin = os.getenv("PCP_ORIGIN_TIME")
        pcp_start_time = os.getenv("PCP_START_TIME")
        pcp_align_time = os.getenv("PCP_ALIGN_TIME")
        pcp_timezone = os.getenv("PCP_TIMEZONE")
        pcp_hostzone = os.getenv("PCP_HOSTZONE")
        pcp_debug = os.getenv("PCP_DEBUG")

        if pcp_archive is not None:
            self.context_type = PM_CONTEXT_ARCHIVE
            if pcp_origin is None and pcp_start_time is None:
                pcp_origin = "-0" # end of archive
            self.source = pcp_archive
        else:
            self.context_type = PM_CONTEXT_HOST
            if pcp_host is not None:
                self.source = pcp_host
            else:
                self.source = "localhost"

        try:
            self.pmfg = pmapi.fetchgroup(self.context_type, self.source)
            self.context = self.pmfg.get_context()
            if pcp_archive:
                options = pmapi.pmOptions("a:A:O:S:D:zZ:")
                optargv = ["pcp-ss", pcp_archive]
                if pcp_debug:
                    optargv.append("-D%s" % pcp_debug)
                if pcp_align_time:
                    optargv.append("-A%s" % pcp_align_time)
                if pcp_timezone:
                    optargv.append("-Z%s" % pcp_timezone)
                if pcp_hostzone:
                    optargv.append("-z")
                if pcp_origin:
                    optargv.append("-O%s" % pcp_origin)
                    pmapi.pmContext.fromOptions(options, optargv)
                    origin = options.pmGetOptionOrigin()
                    self.context.pmSetModeHighRes(PM_MODE_INTERP, origin, None)
                elif pcp_start_time:
                    optargv.append("-S%s" % pcp_start_time)
                    pmapi.pmContext.fromOptions(options, optargv)
                    start = options.pmGetOptionStart()
                    self.context.pmSetModeHighRes(PM_MODE_INTERP, start, None)

        except pmapi.pmErr as pmerr:
            sys.stderr.write("%s: %s '%s'\n" % (pmerr.progname(), pmerr.message(), self.source))
            return False

        # check network.persocket metrics are available
        try:
            self.context.pmLookupName((pmns + ".filter"))
        except Exception:
            if self.context_type == PM_CONTEXT_HOST:
                msg = "on host %s.\nIs the 'sockets' PMDA installed and enabled? See pmdasockets(1)." % self.source
            else:
                msg = "in archive %s" % self.source
            print("Error: metrics for '%s' not found %s" % (pmns, msg))
            return False

        return True

    def traverseCB(self, name):
        if not name.endswith(".filter"):
            self.metrics.append(name)

    def fetch(self):
        """ fetch metrics and report as per given options """

        # filter and timestamp
        self.filter = self.pmfg.extend_item(pmns + ".filter")
        self.timestamp = self.pmfg.extend_timestamp()

        self.context.pmTraversePMNS(pmns, self.traverseCB)
        self.pmids = self.context.pmLookupName(self.metrics)
        descs = self.context.pmLookupDescs(self.pmids)

        # Create metrics dict keyed by metric name (without pmns prefix).
        nmetrics = len(self.metrics)
        for i in range (0, nmetrics):
            pmnsname = self.metrics[i]
            name = remove_prefix(pmnsname, pmns + ".")
            try:
                # do not want rate conversion, so use "instant" scale and only one fetch
                self.descsD[name] = descs[i]
                self.metricsD[name] = self.pmfg.extend_indom(pmnsname,
                    descs[i].contents.type, scale="instant", maxnum=10000)
            except Exception as e:
                print("Warning: Failed to add %s to fetch group: %s" % (name, e))

        # fetch the lot
        try:
            self.pmfg.fetch()
        except Exception as e:
            print("Error: fetch failed: %s" % e)
            sys.exit(1)

        # extract instances and values
        for name in self.metricsD:
            try:
                instvalsD = {}
                # walk the instances in the fetch group for this metric
                for inst, iname, value in self.metricsD[name]():
                    self.instD[inst] = iname
                    try:
                        instvalsD[inst] = value()
                    except Exception as e:
                        print("Error: value() failed for metric %s, inst %d, iname %s: %s" % (name, inst, iname, e))
                self.valuesD[name] = instvalsD
            except Exception as e:
                pass # instance went away, socket probably closed

    def strfield(self, fmt, metric, inst, default=""):
        """ return formatted field, if metric and inst are available else default string """
        try:
            s = self.valuesD[metric][inst]
            if s is not None:
                return fmt % s
        except Exception:
            pass
        return fmt % default

    def intfield(self, fmt, metric, inst, default=0):
        """ return formatted field, if metric and inst are available else default """
        try:
            s = self.valuesD[metric][inst]
            if s is not None:
                return fmt % s
        except Exception:
            pass
        return fmt % default

    def boolfield(self, field, metric, inst, default=""):
        """ return field if metric and inst are available and non-zero """
        try:
            s = self.valuesD[metric][inst]
            if s is not None and s != 0:
                return field
        except Exception:
            pass
        return default

    def filter_netid(self, inst):
        """ filter on netid and -t, -u and -x cmdline options """
        ret = False
        netid = self.valuesD["netid"][inst]
        if self.args.tcp and netid == "tcp":
            ret = True
        elif self.args.udp and netid == "udp":
            state = self.valuesD["state"][inst]
            ret = bool(state != "UNCONN" or self.args.listening)
        #elif self.args.unix and netid == "unix":
        #    ret = True
        #elif self.args.raw and netid == "raw":
        #    ret = True
        return ret

    def filter_ipv46(self, inst):
        """ filter on ip v4 or v6 """
        ret = False
        if self.valuesD["src"][inst]:
            v6 = self.valuesD["src"][inst][0] == '['
        elif self.valuesD["dst"][inst]:
            v6 = self.valuesD["dst"][inst][0] == '['
        else:
            v6 = False
        if self.args.ipv6 and v6:
            ret = True
        if self.args.ipv4 and not v6:
            ret = True
        return ret

    def filter_listening(self, inst):
        """ filter on tcp socket listening state """
        if self.args.all:
            return True
        state = self.valuesD["state"][inst]
        if self.args.listening:
            netid = self.valuesD["netid"][inst]
            if state == "UNCONN" and netid == 'udp':
                return True
            if state != "LISTEN":
                return False
        else:
            if state == "LISTEN":
                return False
        return True

    def report(self):
        """ output report based on cmdline options """
        if not self.args.noheader: # -H flag
            print("# Time: %s Filter: %s" % (self.timestamp(), self.filter()))
            print("Netid  State  Recv-Q Send-Q %25s %-25s  Process" % ("Local Address:Port", "Peer Address:Port"))
        for inst in self.instD:
            if not self.filter_netid(inst) or not self.filter_listening(inst):
                continue
            if not self.filter_ipv46(inst):
                continue
            out = ""
            out += self.strfield("%-6s", "netid", inst)
            out += self.strfield("%-9s", "state", inst, "-")
            out += self.strfield("%6u", "recvq", inst, 0)
            out += self.strfield("%6u", "sendq", inst, 0)
            out += self.strfield(" %25s", "src", inst)
            out += self.strfield(" %-25s", "dst", inst)

            if self.args.options: # -o --options flag
                m = self.valuesD["timer.str"][inst]
                if m is not None and len(m) > 0:
                    out += " timer(%s)" % m

            if self.args.extended: # -e --extended flag
                out += self.strfield(" uid:%d", "uid", inst, 0)
                out += self.strfield(" inode:%lu", "inode", inst, 0)
                out += self.strfield(" sk:%x", "sk", inst, 0)
                out += self.strfield(" cgroup:%s", "cgroup", inst)
                if self.valuesD["v6only"][inst] != 0:
                    out += " v6only:%d" % self.valuesD["v6only"][inst]
                out += " <->"

            if not self.args.oneline and (self.args.memory or self.args.info):
                out += "\n"

            if self.args.memory: # -m --memory flag
                m = self.valuesD["skmem.str"][inst]
                if m is not None and len(m) > 0:
                    out += " skmem(%s)" % m

            if self.args.info: # -i --info flag
                out += self.boolfield(" ts", "ts", inst)
                out += self.boolfield(" sack", "sack", inst)
                # TODO ecn, ecnseen, fastopen
                out += self.boolfield(" cubic", "cubic", inst)
                out += self.strfield(" wscale:%s", "wscale.str", inst)
                # TODO rto, backoff
                out += self.strfield(" rtt:%s", "round_trip.str", inst)
                out += self.strfield(" ato:%.0lf", "ato", inst, 0.0)
                out += self.strfield(" mss:%d", "mss", inst, 0)
                out += self.strfield(" cwnd:%d", "cwnd", inst, 0)
                out += self.strfield(" pmtu:%d", "pmtu", inst, 0)
                out += self.strfield(" ssthresh:%d", "ssthresh", inst, 0)
                out += self.strfield(" bytes_sent:%lu", "bytes_sent", inst, 0)
                out += self.strfield(" bytes_acked:%lu", "bytes_acked", inst, 0)
                out += self.strfield(" bytes_received:%lu", "bytes_received", inst, 0)
                out += self.strfield(" segs_out:%lu", "segs_out", inst, 0)
                out += self.strfield(" segs_in:%lu", "segs_in", inst, 0)
                out += self.strfield(" send %.0lfbps", "send", inst, 0)
                out += self.strfield(" lastsnd:%.0lf", "lastsnd", inst, 0.0)
                out += self.strfield(" lastrcv:%.0lf", "lastrcv", inst, 0.0)
                out += self.strfield(" lastack:%.0lf", "lastack", inst, 0.0)
                out += self.strfield(" pacing_rate %.0lfbps", "pacing_rate", inst, 0.0)
                out += self.strfield(" delivery_rate %.0lfbps", "delivery_rate", inst, 0.0)
                # TODO max_pacing_rate
                out += self.strfield(" rcv_space:%lu", "rcv_space", inst, 0)
                # TODO tcp-ulp-mptcp
                # TODO token:<rem_token(rem_id)/loc_token(loc_id)>
                # TODO seq:<sn>
                # TODO sfseq:<ssn>
                # TODO ssnoff

            print(out)

if __name__ == '__main__':
    try:
        ss = SS()
        if not ss.connect():
            # failed to connect or metrics not found - error already reported.
            sys.exit(1)
        ss.fetch()
        ss.report()
    except pmapi.pmErr as error:
        if error.args[0] == PM_ERR_EOL:
            sys.exit(0)
        sys.stderr.write('%s: %s\n' % (error.progname(), error.message()))
        sys.exit(1)
    except pmapi.pmUsageErr as usage:
        usage.message()
        sys.exit(1)
    except IOError as error:
        if error.errno != errno.EPIPE:
            sys.stderr.write("Error: %s\n" % str(error))
            sys.exit(1)
    except KeyboardInterrupt:
        sys.stdout.write("\n")
