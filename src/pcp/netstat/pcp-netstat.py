#!/usr/bin/env pmpython
#
# Copyright (c) 2023 Oracle and/or its affiliates.
# DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
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
# pylint: disable=bad-whitespace,too-many-arguments,too-many-lines, bad-continuation, line-too-long
# pylint: disable=redefined-outer-name,unnecessary-lambda, wildcard-import, unused-wildcard-import
#

import sys
import time
from pcp import pmapi
from pcp import pmcc
from collections import OrderedDict
from cpmapi import PM_CONTEXT_ARCHIVE

IFF_UP           = 1<<0
IFF_BROADCAST    = 1<<1
IFF_DEBUG        = 1<<2
IFF_LOOPBACK     = 1<<3
IFF_POINTOPOINT  = 1<<4
IFF_NOTRAILERS   = 1<<5
IFF_RUNNING      = 1<<6
IFF_NOARP        = 1<<7
IFF_PROMISC      = 1<<8
IFF_ALLMULTI     = 1<<9
IFF_MASTER       = 1<<10
IFF_SLAVE        = 1<<11
IFF_MULTICAST    = 1<<12
IFF_PORTSEL      = 1<<13
IFF_AUTOMEDIA    = 1<<14
IFF_DYNAMIC      = 1<<15
IFF_LOWER_UP     = 1<<16
IFF_DORMANT      = 1<<17
IFF_ECHO         = 1<<18

TCP_METRICS = ["network.tcp.activeopens",
            "network.tcp.passiveopens",
            "network.tcp.attemptfails",
            "network.tcp.estabresets",
            "network.tcpconn.established",
            "network.tcp.insegs",
            "network.tcp.outsegs",
            "network.tcp.retranssegs",
            "network.tcp.inerrs",
            "network.tcp.outrsts"]

TCP_METRICS_DESC = [
            "__ active connections openings",
            "__ passive connection openings",
            "__ failed connection attempts",
            "__ connection resets received",
            "__ connections established",
            "__ segments received",
            "__ segments send out",
            "__ segments retransmited",
            "__ bad segments received.",
            "__ resets sent"]

TCP_EXT_METRICS =  [
                    # tcpsock.finishedtimewait.fast_timer,
                    "network.tcp.delayedacks",
                    "network.tcp.delayedacklocked",
                    # Quick ack mode was activated __ times,
                    "network.tcp.hphits",
                    # acknowledgments not containing data payload received,
                    "network.tcp.hpacks",
                    "network.tcp.sackrecovery",
                    "network.tcp.sackreorder",
                    "network.tcp.dsackundo",
                    # congestion windows recovered without slow start after partial ack,
                    "network.tcp.lostretransmit",
                    "network.tcp.sackfailures",
                    "network.tcp.fastretrans",
                    "network.tcp.timeouts",
                    "network.tcp.lossprobes",
                    "network.tcp.lossproberecovery",
                    # "TCPDSACKRecv",
                    "network.tcp.tcpbacklogcoalesce",
                    "network.tcp.dsackoldsent",
                    "network.tcp.dsackofosent",
                    "network.tcp.dsackrecv",
                    "network.tcp.dsackoforecv",
                    "network.tcp.abortondata",
                    "network.tcp.abortonclose",
                    "network.tcp.abortontimeout",
                    "network.tcp.dsackignorednoundo",
                    # "TCPSpuriousRTOs",
                    "network.tcp.sackshifted",
                    "network.tcp.sackmerged",
                    "network.tcp.sackshiftfallback",
                    "network.tcp.iprpfilter",
                    "network.tcp.rcvcoalesce",
                    "network.tcp.ofoqueue",
                    "network.tcp.ofomerge",
                    "network.tcp.challengeack",
                    "network.tcp.synchallenge",
                    "network.tcp.spuriousrtxhostqueues",
                    "network.tcp.autocorking",
                    # "TCPFromZeroWindowAdv: __",
                    # "TCPToZeroWindowAdv: __",
                    # "TCPWantZeroWindowAdv: __",
                    "network.tcp.synretrans",
                    "network.tcp.origdatasent",
                    "network.tcp.tcphystarttraindetect",
                    "network.tcp.tcphystarttraincwnd",
                    "network.tcp.tcphystartdelaydetect",
                    "network.tcp.tcphystartdelaycwnd",
                    "network.tcp.tcpackskippedseq",
                    "network.tcp.tcpkeepalive",
                    "network.tcp.tcpdelivered",
                    "network.tcp.tcpackcompressed"]

TCP_EXT_METRICS_DESC = [
            # "__ TCP sockets finished time wait in fast timer",
            "__ delayed acks sent",
            "__ delayed acks further delayed because of locked socket",
            # "Quick ack mode was activated __ times" ,
            "__ packet headers predicted",
            # "__ acknowledgments not containing data payload received",
            "__ predicted acknowledgments",
            "TCPSackRecovery: __",
            "Detected reordering __ times using SACK",
            "TCPDSACKUndo: __",
            # "__ congestion windows recovered without slow start after partial ack",
            "TCPLostRetransmitt: __",
            "TCPSackFailures: __",
            "__ fast retransmits",
            "TCPTimeouts: __",
            "TCPLossProbes: __",
            "TCPLossProbeRecovery: __",
            # "TCPSackRecoveryFail: __"
            "TCPBacklogCoalesce: __",
            "TCPDSACKOldSent: __",
            "TCPDSACKOfoSent: __",
            "TCPDSACKRecv: __",
            "TCPDSACKOfoRecv: __",
            "__ connections aborted due to data",
            "__ connections reset due to early user close",
            "__ connections aborted due to timeout",
            "TCPDSACKIgnoredNoUndo: __",
            # "TCPSpuriousRTOs: __",
            "TCPSackShifted: __",
            "TCPSackMerged: __",
            "TCPSackShiftFallback: __",
            "IPReversePathFilter: __",
            "TCPRcvCoalesce: __",
            "TCPOFOQueue: __",
            "TCPOFOMerge: __",
            "TCPChallengeAck: __",
            "TCPSynChallenge: __",
            "TCPSpuriousRtxHostQueues: __",
            "TCPAutoCorking: __",
            # "TCPFromZeroWindowAdv: __",
            # "TCPToZeroWindowAdv: __",
            # "TCPWantZeroWindowAdv: __",
            "TCPSynRetrans: __",
            "TCPOrigDataSent: __",
            "TCPHystartTrainDetect: __",
            "TCPHystartTrainCwnd: __",
            "TCPHystartDelayDetect: __",
            "TCPHystartDelayCwnd: __",
            "TCPACKSkippedSeq: __",
            "TCPKeepAlive: __",
            "TCPDelivered: __",
            "TCPAckCompressed: __"]

IP_METRICS = [
            "network.ip.forwarding",
            "network.ip.inreceives",
            "network.ip.inaddrerrors",
            "network.ip.forwdatagrams",
            "network.ip.indiscards",
            "network.ip.indelivers",
            "network.ip.outrequests",
            "network.ip.outnoroutes"]

IP_METRICS_DESC = [
            "Forwarding: __",
            "__ total packets received",
            "__ with invalid addresses",
            "__ forwarded",
            "__ incoming packets discarded",
            "__ incoming packets delivered",
            "__ requests sent out",
            "__ dropped because of missing route"]

IP_EXT_METRICS = [
            "network.ip.inmcastpkts",
            "network.ip.inbcastpkts",
            "network.ip.inoctets",
            "network.ip.outoctets",
            "network.ip.inmcastoctets",
            "network.ip.inbcastoctets",
            "network.ip.noectpkts"]

IP_EXT_METRICS_DESC = [
            "InMcastPkts: __",
            "InBcastPkts: __",
            "InOctets: __",
            "OutOctets: __",
            "InMcastOctets: __",
            "InBcastOctets: __",
            "InNoECTpkts: __"]

ICMP_METRICS = [
            "network.icmp.inmsgs",
            "network.icmp.inerrors",
            "network.icmp.indestunreachs",
            # "echo requests",
            "network.icmp.outmsgs",
            "network.icmp.outerrors",
            "network.icmp.outdestunreachs"]
            # "echo replies"]

ICMP_METRICS_DESC = [
            "__ ICMP messages received",
            "__ Input ICMP message failed",
            "ICMP input histogram:\n\t\tdestination unreachable: __",
            # "\techo requests",
            "__ ICMP messages sent",
            "__ ICMP messages failed",
            "ICMP input histogram:\n\t\tOutput destination unreachable: __"]
            # "\techo replies"]

ICMP_MSG_METRICS = [
            "network.icmpmsg.intype",
            # "network.icmpmsg.intype8",
            "network.icmpmsg.outtype"]
            # "network.icmpmsg.outtype3"]

ICMP_MSG_METRICS_DESC = [
            "\tInType3: __",
            # "\tInType8: __",
            "\tOutType0: __"]
            # "\tOutType3: __"]

UDP_METRICS = [
            "network.udp.indatagrams",
            "network.udp.noports",
            "network.udp.inerrors",
            "network.udp.outdatagrams",
            "network.udp.recvbuferrors",
            "network.udp.sndbuferrors"]
            # "network.udp.ignoredmulti"]

UDP_METRICS_DESC = [
            "__ packets received",
            "__ packets to unknown port received",
            "__ packet receive errors",
            "__ packets sent",
            "__ receive buffer errors",
            "__ send buffer errors"]
            #"IgnoredMulti: __"]

UDP_LITE_METRICS = [
            "network.udplite.indatagrams",
            "network.udplite.noports",
            "network.udplite.inerrors",
            "network.udplite.outdatagrams",
            "network.udplite.recvbuferrors",
            "network.udplite.sndbuferrors",
            "network.udplite.incsumerrors"]

UDP_LITE_METRICS_DESC = [
            "__ packets received",
            "__ packets to unknown port received",
            "__ packet receive errors",
            "__ packets sent",
            "__ receive buffer errors",
            "__ send buffer errors",
            "__ checksum errors"]

IFACE_METRICS = [
            "network.interface.mtu",
            "network.interface.in.packets",
            "network.interface.in.errors",
            "network.interface.in.drops",
            # "ovr __ (override)",
            "network.interface.out.packets",
            "network.interface.out.errors",
            "network.interface.out.drops"]
            # "ovr __ (override)",
            # "network.interface.type"]
            # "network.interface.running",
            # "network.interface.up"]

SYS_METRICS = ["kernel.uname.sysname",
                "kernel.uname.release",
                "kernel.uname.nodename",
                "kernel.uname.machine",
                "hinv.ncpu"]

METRICS = IP_METRICS + ICMP_METRICS + ICMP_MSG_METRICS + TCP_METRICS + UDP_METRICS + UDP_LITE_METRICS + TCP_EXT_METRICS + IP_EXT_METRICS
METRICS_DESC = IP_METRICS_DESC + ICMP_METRICS_DESC + ICMP_MSG_METRICS_DESC + TCP_METRICS_DESC + UDP_METRICS_DESC + UDP_LITE_METRICS_DESC + TCP_EXT_METRICS_DESC + IP_EXT_METRICS_DESC

METRICS_DICT = OrderedDict()

METRICS_DICT["Ip_METRICS"] = [IP_METRICS, IP_METRICS_DESC]
METRICS_DICT["Icmp_METRICS"] = [ICMP_METRICS, ICMP_METRICS_DESC]
METRICS_DICT["IcmpMsg_METRICS"] = [ICMP_MSG_METRICS, ICMP_MSG_METRICS_DESC]
METRICS_DICT["Tcp_METRICS"] = [TCP_METRICS, TCP_METRICS_DESC]
METRICS_DICT["Udp_METRICS"] = [UDP_METRICS, UDP_METRICS_DESC]
METRICS_DICT["UdpLite_METRICS"] = [UDP_LITE_METRICS, UDP_LITE_METRICS_DESC]
METRICS_DICT["TcpExt_METRICS"] = [TCP_EXT_METRICS, TCP_EXT_METRICS_DESC]
METRICS_DICT["IpExt_METRICS"] = [IP_EXT_METRICS, IP_EXT_METRICS_DESC]

MISSING_METRICS = []

class NestatReport(pmcc.MetricGroupPrinter):
    Machine_info_count = 0

    def __get_ncpu(self, group):
        return group['hinv.ncpu'].netValues[0][2]

    def __print_machine_info(self, group, context):
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

    def print_metric(self, group, metrics,metrics_desc):
        idx = 0

        for metric in metrics:
            if metric in MISSING_METRICS:
                metric_val_str = metrics_desc[idx].replace("__", "NA")
                print("\t%s" % (metric_val_str))

                idx += 1
                continue

            try:
                val = group[metric].netValues[0][2]
            except IndexError:
                metric_val_str = metrics_desc[idx].replace("__", "NA")
                print("\t%s" % (metric_val_str))
                idx += 1
                continue
            metric_val_str = metrics_desc[idx].replace("__", str(val))
            # metric_val_str = metric_val_str
            print("\t%s" % (metric_val_str))

            idx += 1
        print("\n")

    def report(self, manager):
        group = manager["sysinfo"]
        try:
            if not self.Machine_info_count:
                self.__print_machine_info(group, manager)
                self.Machine_info_count = 1
        except IndexError:
            # missing some metrics
            return

        group = manager["netstat"]
        opts.pmGetOptionSamples()

        t_s = group.contextCache.pmLocaltime(int(group.timestamp))
        time_string = time.strftime(NetstatOptions.timefmt, t_s.struct_time())
        print("%s\n" % time_string)

        if NetstatOptions.all_stats_flag is False and NetstatOptions.filter_protocol_flag is False and NetstatOptions.filter_iface_flag is False:
            NetstatOptions.all_stats_flag = True
            NetstatOptions.filter_iface_flag = True

        if NetstatOptions.all_stats_flag:
            for metric_dict in METRICS_DICT.items():
                print("%s:" % metric_dict[0][:-8])
                self.print_metric(group, metric_dict[1][0], metric_dict[1][1])

        if NetstatOptions.filter_protocol_flag:
            protocol = NetstatOptions.filter_protocol

            for metric_dict in METRICS_DICT.items():
                if protocol.lower() in metric_dict[0].lower():
                    print("%s:" % metric_dict[0][:-8])
                    self.print_metric(group, metric_dict[1][0], metric_dict[1][1])

        if NetstatOptions.filter_iface_flag:
            ifstats = {}
            print("Kernel Interface table")
            for metric in IFACE_METRICS:
                idx = 0
                try:
                    val = group[metric].netValues
                except IndexError:
                    idx += 1
                    continue

                for elem in val:
                    if elem[1] not in ifstats:
                        ifstats[elem[1]] = []
                    ifstats[elem[1]].append(str(elem[2]))

            print("%10s %10s %10s %10s %10s %10s %10s %10s"%("Iface", "MTU", "RX-OK", "RX-ERR", "RX-DRP", "TX-OK", "TX-ERR", "TX-DRP"))

            ifstats_str = ""
            for intf in sorted(ifstats):
                if_row = ""
                if_row += "%10s " % intf

                # idx = 0
                for idx in range(len(ifstats[intf])):
                    ele = ifstats[intf][idx]

                    if_row += "%10s " % str(ele)

                ifstats_str += if_row + "\n"

            print(ifstats_str)

        if NetstatOptions.context is not PM_CONTEXT_ARCHIVE and opts.pmGetOptionSamples() is None:
            sys.exit(0)

class NetstatOptions(pmapi.pmOptions):
    context = None
    timefmt = "%H:%M:%S"
    samples = 0
    all_stats_flag = False
    filter_protocol_flag = False
    filter_protocol = None
    filter_iface_flag = False

    def override(self, opt):
        if opt == 'p':
            return 1
        return 0

    def extraOptions(self, opt, optarg, index):

        if opt == "statistics":
            NetstatOptions.all_stats_flag = True

        elif opt == 'p':
            NetstatOptions.filter_protocol_flag = True
            try:
                if optarg is not None:
                    if optarg in ["TCP", "IP", "UDP", "ICMP"]:
                        NetstatOptions.filter_protocol = str(optarg)
            except ValueError:
                print("Invalid command Id List: use comma separated pids without whitespaces")
                sys.exit(1)

        elif opt == 'i':
            NetstatOptions.filter_iface_flag = True

    def __init__(self):
        pmapi.pmOptions.__init__(self, "a:i?:s:S:T:p:z:")
        self.pmSetLongOptionStart()
        self.pmSetLongOptionFinish()
        self.pmSetLongOption("statistics", 0, "", "","shows output similar to netstat -s, which displays summary statistics for each protocol")
        self.pmSetLongOption("", 0, "i", "","shows output similar to netstat -i -a -n, which displays a table of all network interfaces")
        self.pmSetLongOption("", 1, "p", "[TCP|IP|UDP|ICMP]","filter stats specific to a protocol")
        self.pmSetOptionCallback(self.extraOptions)
        self.pmSetOverrideCallback(self.override)
        self.pmSetLongOptionHelp()

if __name__ == '__main__':
    try:
        opts = NetstatOptions()
        mngr = pmcc.MetricGroupManager.builder(opts,sys.argv)
        NetstatOptions.context = mngr.type

        missing = mngr.checkMissingMetrics(METRICS)

        if missing is not None:
            for missing_metric in missing:
                METRICS.remove(missing_metric)
                MISSING_METRICS.append(missing_metric)

        mngr["netstat"] = METRICS + IFACE_METRICS
        mngr["sysinfo"] = SYS_METRICS
        mngr.printer = NestatReport()
        sts = mngr.run()
        sys.exit(sts)

    except pmapi.pmErr as error:
        sys.stderr.write("%s %s\n"%(error.progname(),error.message()))
    except pmapi.pmUsageErr as usage:
        usage.message()
        sys.exit(1)
    except KeyboardInterrupt:
        pass
