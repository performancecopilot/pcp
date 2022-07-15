/*
 * Copyright (c) 2014,2016,2020 Red Hat.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 */
#include "linux.h"
#include "proc_net_netstat.h"

/*
 * used to control once per execution checking for buffer truncation and
 * unknown fields in the "stat" file
 * == 1 initially
 * == 0 if checks done and nothing bad was found
 * < 0 if parsing is botched in a way that makes refreshing non-sensical
 */
static int	onetrip = 1;

extern proc_net_netstat_t	_pm_proc_net_netstat;

typedef struct {
    const char		*field;
    __uint64_t		*offset;
} netstat_fields_t;

__uint64_t	not_exported;

netstat_fields_t netstat_ip_fields[] = {
    { .field = "InNoRoutes",
     .offset = &_pm_proc_net_netstat.ip[_PM_NETSTAT_IPEXT_INNOROUTES] },
    { .field = "InTruncatedPkts",
     .offset = &_pm_proc_net_netstat.ip[_PM_NETSTAT_IPEXT_INTRUNCATEDPKTS] },
    { .field = "InMcastPkts",
     .offset = &_pm_proc_net_netstat.ip[_PM_NETSTAT_IPEXT_INMCASTPKTS] },
    { .field = "OutMcastPkts",
     .offset = &_pm_proc_net_netstat.ip[_PM_NETSTAT_IPEXT_OUTMCASTPKTS] },
    { .field = "InBcastPkts",
     .offset = &_pm_proc_net_netstat.ip[_PM_NETSTAT_IPEXT_INBCASTPKTS] },
    { .field = "OutBcastPkts",
     .offset = &_pm_proc_net_netstat.ip[_PM_NETSTAT_IPEXT_OUTBCASTPKTS] },
    { .field = "InOctets",
     .offset = &_pm_proc_net_netstat.ip[_PM_NETSTAT_IPEXT_INOCTETS] },
    { .field = "OutOctets",
     .offset = &_pm_proc_net_netstat.ip[_PM_NETSTAT_IPEXT_OUTOCTETS] },
    { .field = "InMcastOctets",
     .offset = &_pm_proc_net_netstat.ip[_PM_NETSTAT_IPEXT_INMCASTOCTETS] },
    { .field = "OutMcastOctets",
     .offset = &_pm_proc_net_netstat.ip[_PM_NETSTAT_IPEXT_OUTMCASTOCTETS] },
    { .field = "InBcastOctets",
     .offset = &_pm_proc_net_netstat.ip[_PM_NETSTAT_IPEXT_INBCASTOCTETS] },
    { .field = "OutBcastOctets",
     .offset = &_pm_proc_net_netstat.ip[_PM_NETSTAT_IPEXT_OUTBCASTOCTETS] },
    { .field = "InCsumErrors",
     .offset = &_pm_proc_net_netstat.ip[_PM_NETSTAT_IPEXT_CSUMERRORS] },
    { .field = "InNoECTPkts",
     .offset = &_pm_proc_net_netstat.ip[_PM_NETSTAT_IPEXT_NOECTPKTS] },
    { .field = "InECT1Pkts",
     .offset = &_pm_proc_net_netstat.ip[_PM_NETSTAT_IPEXT_ECT1PKTS] },
    { .field = "InECT0Pkts",
     .offset = &_pm_proc_net_netstat.ip[_PM_NETSTAT_IPEXT_ECT0PKTS] },
    { .field = "InCEPkts",
     .offset = &_pm_proc_net_netstat.ip[_PM_NETSTAT_IPEXT_CEPKTS] },
    { .field = "ReasmOverlaps",
      .offset = &_pm_proc_net_netstat.ip[_PM_NETSTAT_IPEXT_REASMOVERLAPS] },

    { .field = NULL, .offset = NULL }
};

netstat_fields_t netstat_tcp_fields[] = {
    { .field = "SyncookiesSent",
     .offset = &_pm_proc_net_netstat.tcp[_PM_NETSTAT_TCPEXT_SYNCOOKIESSENT] },
    { .field = "SyncookiesRecv",
     .offset = &_pm_proc_net_netstat.tcp[_PM_NETSTAT_TCPEXT_SYNCOOKIESRECV] },
    { .field = "SyncookiesFailed",
     .offset = &_pm_proc_net_netstat.tcp[_PM_NETSTAT_TCPEXT_SYNCOOKIESFAILED] },
    { .field = "EmbryonicRsts",
     .offset = &_pm_proc_net_netstat.tcp[_PM_NETSTAT_TCPEXT_EMBRYONICRSTS] },
    { .field = "PruneCalled",
     .offset = &_pm_proc_net_netstat.tcp[_PM_NETSTAT_TCPEXT_PRUNECALLED] },
    { .field = "RcvPruned",
     .offset = &_pm_proc_net_netstat.tcp[_PM_NETSTAT_TCPEXT_RCVPRUNED] },
    { .field = "OfoPruned",
     .offset = &_pm_proc_net_netstat.tcp[_PM_NETSTAT_TCPEXT_OFOPRUNED] },
    { .field = "OutOfWindowIcmps",
     .offset = &_pm_proc_net_netstat.tcp[_PM_NETSTAT_TCPEXT_OUTOFWINDOWICMPS] },
    { .field = "LockDroppedIcmps",
     .offset = &_pm_proc_net_netstat.tcp[_PM_NETSTAT_TCPEXT_LOCKDROPPEDICMPS] },
    { .field = "ArpFilter",
     .offset = &_pm_proc_net_netstat.tcp[_PM_NETSTAT_TCPEXT_ARPFILTER] },
    { .field = "TW",
     .offset = &_pm_proc_net_netstat.tcp[_PM_NETSTAT_TCPEXT_TIMEWAITED] },
    { .field = "TWRecycled",
     .offset = &_pm_proc_net_netstat.tcp[_PM_NETSTAT_TCPEXT_TIMEWAITRECYCLED] },
    { .field = "TWKilled",
     .offset = &_pm_proc_net_netstat.tcp[_PM_NETSTAT_TCPEXT_TIMEWAITKILLED] },
    { .field = "PAWSPassive",
     .offset = &_pm_proc_net_netstat.tcp[_PM_NETSTAT_TCPEXT_PAWSPASSIVEREJECTED] },
    { .field = "PAWSActive",
     .offset = &_pm_proc_net_netstat.tcp[_PM_NETSTAT_TCPEXT_PAWSACTIVEREJECTED] },
    { .field = "PAWSEstab",
     .offset = &_pm_proc_net_netstat.tcp[_PM_NETSTAT_TCPEXT_PAWSESTABREJECTED] },
    { .field = "DelayedACKs",
     .offset = &_pm_proc_net_netstat.tcp[_PM_NETSTAT_TCPEXT_DELAYEDACKS] },
    { .field = "DelayedACKLocked",
     .offset = &_pm_proc_net_netstat.tcp[_PM_NETSTAT_TCPEXT_DELAYEDACKLOCKED] },
    { .field = "DelayedACKLost",
     .offset = &_pm_proc_net_netstat.tcp[_PM_NETSTAT_TCPEXT_DELAYEDACKLOST] },
    { .field = "ListenOverflows",
     .offset = &_pm_proc_net_netstat.tcp[_PM_NETSTAT_TCPEXT_LISTENOVERFLOWS] },
    { .field = "ListenDrops",
     .offset = &_pm_proc_net_netstat.tcp[_PM_NETSTAT_TCPEXT_LISTENDROPS] },
    { .field = "TCPPrequeued",
     .offset = &_pm_proc_net_netstat.tcp[_PM_NETSTAT_TCPEXT_TCPPREQUEUED] },
    { .field = "TCPDirectCopyFromBacklog",
     .offset = &_pm_proc_net_netstat.tcp[_PM_NETSTAT_TCPEXT_TCPDIRECTCOPYFROMBACKLOG] },
    { .field = "TCPDirectCopyFromPrequeue",
     .offset = &_pm_proc_net_netstat.tcp[_PM_NETSTAT_TCPEXT_TCPDIRECTCOPYFROMPREQUEUE] },
    { .field = "TCPPrequeueDropped",
     .offset = &_pm_proc_net_netstat.tcp[_PM_NETSTAT_TCPEXT_TCPPREQUEUEDROPPED] },
    { .field = "TCPHPHits",
     .offset = &_pm_proc_net_netstat.tcp[_PM_NETSTAT_TCPEXT_TCPHPHITS] },
    { .field = "TCPHPHitsToUser",
     .offset = &_pm_proc_net_netstat.tcp[_PM_NETSTAT_TCPEXT_TCPHPHITSTOUSER] },
    { .field = "TCPPureAcks",
     .offset = &_pm_proc_net_netstat.tcp[_PM_NETSTAT_TCPEXT_TCPPUREACKS] },
    { .field = "TCPHPAcks",
     .offset = &_pm_proc_net_netstat.tcp[_PM_NETSTAT_TCPEXT_TCPHPACKS] },
    { .field = "TCPRenoRecovery",
     .offset = &_pm_proc_net_netstat.tcp[_PM_NETSTAT_TCPEXT_TCPRENORECOVERY] },
    { .field = "TCPSackRecovery",
     .offset = &_pm_proc_net_netstat.tcp[_PM_NETSTAT_TCPEXT_TCPSACKRECOVERY] },
    { .field = "TCPSACKReneging",
     .offset = &_pm_proc_net_netstat.tcp[_PM_NETSTAT_TCPEXT_TCPSACKRENEGING] },
    { .field = "TCPFACKReorder",
     .offset = &_pm_proc_net_netstat.tcp[_PM_NETSTAT_TCPEXT_TCPFACKREORDER] },
    { .field = "TCPSACKReorder",
     .offset = &_pm_proc_net_netstat.tcp[_PM_NETSTAT_TCPEXT_TCPSACKREORDER] },
    { .field = "TCPRenoReorder",
     .offset = &_pm_proc_net_netstat.tcp[_PM_NETSTAT_TCPEXT_TCPRENOREORDER] },
    { .field = "TCPTSReorder",
     .offset = &_pm_proc_net_netstat.tcp[_PM_NETSTAT_TCPEXT_TCPTSREORDER] },
    { .field = "TCPFullUndo",
     .offset = &_pm_proc_net_netstat.tcp[_PM_NETSTAT_TCPEXT_TCPFULLUNDO] },
    { .field = "TCPPartialUndo",
     .offset = &_pm_proc_net_netstat.tcp[_PM_NETSTAT_TCPEXT_TCPPARTIALUNDO] },
    { .field = "TCPDSACKUndo",
     .offset = &_pm_proc_net_netstat.tcp[_PM_NETSTAT_TCPEXT_TCPDSACKUNDO] },
    { .field = "TCPLossUndo",
     .offset = &_pm_proc_net_netstat.tcp[_PM_NETSTAT_TCPEXT_TCPLOSSUNDO] },
    { .field = "TCPLostRetransmit",
     .offset = &_pm_proc_net_netstat.tcp[_PM_NETSTAT_TCPEXT_TCPLOSTRETRANSMIT] },
    { .field = "TCPRenoFailures",
     .offset = &_pm_proc_net_netstat.tcp[_PM_NETSTAT_TCPEXT_TCPRENOFAILURES] },
    { .field = "TCPSackFailures",
     .offset = &_pm_proc_net_netstat.tcp[_PM_NETSTAT_TCPEXT_TCPSACKFAILURES] },
    { .field = "TCPLossFailures",
     .offset = &_pm_proc_net_netstat.tcp[_PM_NETSTAT_TCPEXT_TCPLOSSFAILURES] },
    { .field = "TCPFastRetrans",
     .offset = &_pm_proc_net_netstat.tcp[_PM_NETSTAT_TCPEXT_TCPFASTRETRANS] },
    { .field = "TCPForwardRetrans",
     .offset = &_pm_proc_net_netstat.tcp[_PM_NETSTAT_TCPEXT_TCPFORWARDRETRANS] },
    { .field = "TCPSlowStartRetrans",
     .offset = &_pm_proc_net_netstat.tcp[_PM_NETSTAT_TCPEXT_TCPSLOWSTARTRETRANS] },
    { .field = "TCPTimeouts",
     .offset = &_pm_proc_net_netstat.tcp[_PM_NETSTAT_TCPEXT_TCPTIMEOUTS] },
    { .field = "TCPLossProbes",
     .offset = &_pm_proc_net_netstat.tcp[_PM_NETSTAT_TCPEXT_TCPLOSSPROBES] },
    { .field = "TCPLossProbeRecovery",
     .offset = &_pm_proc_net_netstat.tcp[_PM_NETSTAT_TCPEXT_TCPLOSSPROBERECOVERY] },
    { .field = "TCPRenoRecoveryFail",
     .offset = &_pm_proc_net_netstat.tcp[_PM_NETSTAT_TCPEXT_TCPRENORECOVERYFAIL] },
    { .field = "TCPSackRecoveryFail",
     .offset = &_pm_proc_net_netstat.tcp[_PM_NETSTAT_TCPEXT_TCPSACKRECOVERYFAIL] },
    { .field = "TCPSchedulerFailed",
     .offset = &_pm_proc_net_netstat.tcp[_PM_NETSTAT_TCPEXT_TCPSCHEDULERFAILED] },
    { .field = "TCPRcvCollapsed",
     .offset = &_pm_proc_net_netstat.tcp[_PM_NETSTAT_TCPEXT_TCPRCVCOLLAPSED] },
    { .field = "TCPDSACKOldSent",
     .offset = &_pm_proc_net_netstat.tcp[_PM_NETSTAT_TCPEXT_TCPDSACKOLDSENT] },
    { .field = "TCPDSACKOfoSent",
     .offset = &_pm_proc_net_netstat.tcp[_PM_NETSTAT_TCPEXT_TCPDSACKOFOSENT] },
    { .field = "TCPDSACKRecv",
     .offset = &_pm_proc_net_netstat.tcp[_PM_NETSTAT_TCPEXT_TCPDSACKRECV] },
    { .field = "TCPDSACKOfoRecv",
     .offset = &_pm_proc_net_netstat.tcp[_PM_NETSTAT_TCPEXT_TCPDSACKOFORECV] },
    { .field = "TCPAbortOnData",
     .offset = &_pm_proc_net_netstat.tcp[_PM_NETSTAT_TCPEXT_TCPABORTONDATA] },
    { .field = "TCPAbortOnClose",
     .offset = &_pm_proc_net_netstat.tcp[_PM_NETSTAT_TCPEXT_TCPABORTONCLOSE] },
    { .field = "TCPAbortOnMemory",
     .offset = &_pm_proc_net_netstat.tcp[_PM_NETSTAT_TCPEXT_TCPABORTONMEMORY] },
    { .field = "TCPAbortOnTimeout",
     .offset = &_pm_proc_net_netstat.tcp[_PM_NETSTAT_TCPEXT_TCPABORTONTIMEOUT] },
    { .field = "TCPAbortOnLinger",
     .offset = &_pm_proc_net_netstat.tcp[_PM_NETSTAT_TCPEXT_TCPABORTONLINGER] },
    { .field = "TCPAbortFailed",
     .offset = &_pm_proc_net_netstat.tcp[_PM_NETSTAT_TCPEXT_TCPABORTFAILED] },
    { .field = "TCPMemoryPressures",
     .offset = &_pm_proc_net_netstat.tcp[_PM_NETSTAT_TCPEXT_TCPMEMORYPRESSURES] },
    { .field = "TCPSACKDiscard",
     .offset = &_pm_proc_net_netstat.tcp[_PM_NETSTAT_TCPEXT_TCPSACKDISCARD] },
    { .field = "TCPDSACKIgnoredOld",
     .offset = &_pm_proc_net_netstat.tcp[_PM_NETSTAT_TCPEXT_TCPDSACKIGNOREDOLD] },
    { .field = "TCPDSACKIgnoredNoUndo",
     .offset = &_pm_proc_net_netstat.tcp[_PM_NETSTAT_TCPEXT_TCPDSACKIGNOREDNOUNDO] },
    { .field = "TCPSpuriousRTOs",
     .offset = &_pm_proc_net_netstat.tcp[_PM_NETSTAT_TCPEXT_TCPSPURIOUSRTOS] },
    { .field = "TCPMD5NotFound",
     .offset = &_pm_proc_net_netstat.tcp[_PM_NETSTAT_TCPEXT_TCPMD5NOTFOUND] },
    { .field = "TCPMD5Unexpected",
     .offset = &_pm_proc_net_netstat.tcp[_PM_NETSTAT_TCPEXT_TCPMD5UNEXPECTED] },
    { .field = "TCPSackShifted",
     .offset = &_pm_proc_net_netstat.tcp[_PM_NETSTAT_TCPEXT_SACKSHIFTED] },
    { .field = "TCPSackMerged",
     .offset = &_pm_proc_net_netstat.tcp[_PM_NETSTAT_TCPEXT_SACKMERGED] },
    { .field = "TCPSackShiftFallback",
     .offset = &_pm_proc_net_netstat.tcp[_PM_NETSTAT_TCPEXT_SACKSHIFTFALLBACK] },
    { .field = "TCPBacklogDrop",
     .offset = &_pm_proc_net_netstat.tcp[_PM_NETSTAT_TCPEXT_TCPBACKLOGDROP] },
    { .field = "TCPMinTTLDrop",
     .offset = &_pm_proc_net_netstat.tcp[_PM_NETSTAT_TCPEXT_TCPMINTTLDROP] },
    { .field = "TCPDeferAcceptDrop",
     .offset = &_pm_proc_net_netstat.tcp[_PM_NETSTAT_TCPEXT_TCPDEFERACCEPTDROP] },
    { .field = "IPReversePathFilter",
     .offset = &_pm_proc_net_netstat.tcp[_PM_NETSTAT_TCPEXT_IPRPFILTER] },
    { .field = "TCPTimeWaitOverflow",
     .offset = &_pm_proc_net_netstat.tcp[_PM_NETSTAT_TCPEXT_TCPTIMEWAITOVERFLOW] },
    { .field = "TCPReqQFullDoCookies",
     .offset = &_pm_proc_net_netstat.tcp[_PM_NETSTAT_TCPEXT_TCPREQQFULLDOCOOKIES] },
    { .field = "TCPReqQFullDrop",
     .offset = &_pm_proc_net_netstat.tcp[_PM_NETSTAT_TCPEXT_TCPREQQFULLDROP] },
    { .field = "TCPRetransFail",
     .offset = &_pm_proc_net_netstat.tcp[_PM_NETSTAT_TCPEXT_TCPRETRANSFAIL] },
    { .field = "TCPRcvCoalesce",
     .offset = &_pm_proc_net_netstat.tcp[_PM_NETSTAT_TCPEXT_TCPRCVCOALESCE] },
    { .field = "TCPOFOQueue",
     .offset = &_pm_proc_net_netstat.tcp[_PM_NETSTAT_TCPEXT_TCPOFOQUEUE] },
    { .field = "TCPOFODrop",
     .offset = &_pm_proc_net_netstat.tcp[_PM_NETSTAT_TCPEXT_TCPOFODROP] },
    { .field = "TCPOFOMerge",
     .offset = &_pm_proc_net_netstat.tcp[_PM_NETSTAT_TCPEXT_TCPOFOMERGE] },
    { .field = "TCPChallengeACK",
     .offset = &_pm_proc_net_netstat.tcp[_PM_NETSTAT_TCPEXT_TCPCHALLENGEACK] },
    { .field = "TCPSYNChallenge",
     .offset = &_pm_proc_net_netstat.tcp[_PM_NETSTAT_TCPEXT_TCPSYNCHALLENGE] },
    { .field = "TCPFastOpenActive",
     .offset = &_pm_proc_net_netstat.tcp[_PM_NETSTAT_TCPEXT_TCPFASTOPENACTIVE] },
    { .field = "TCPFastOpenPassive",
     .offset = &_pm_proc_net_netstat.tcp[_PM_NETSTAT_TCPEXT_TCPFASTOPENPASSIVE] },
    { .field = "TCPFastOpenPassiveFail",
     .offset = &_pm_proc_net_netstat.tcp[_PM_NETSTAT_TCPEXT_TCPFASTOPENPASSIVEFAIL] },
    { .field = "TCPFastOpenListenOverflow",
     .offset = &_pm_proc_net_netstat.tcp[_PM_NETSTAT_TCPEXT_TCPFASTOPENLISTENOVERFLOW] },
    { .field = "TCPFastOpenCookieReqd",
     .offset = &_pm_proc_net_netstat.tcp[_PM_NETSTAT_TCPEXT_TCPFASTOPENCOOKIEREQD] },
    { .field = "TCPSpuriousRtxHostQueues",
     .offset = &_pm_proc_net_netstat.tcp[_PM_NETSTAT_TCPEXT_TCPSPURIOUS_RTX_HOSTQUEUES] },
    { .field = "BusyPollRxPackets",
     .offset = &_pm_proc_net_netstat.tcp[_PM_NETSTAT_TCPEXT_BUSYPOLLRXPACKETS] },
    { .field = "TCPAutoCorking",
     .offset = &_pm_proc_net_netstat.tcp[_PM_NETSTAT_TCPEXT_TCPAUTOCORKING] },
    { .field = "TCPFromZeroWindowAdv",
     .offset = &_pm_proc_net_netstat.tcp[_PM_NETSTAT_TCPEXT_TCPFROMZEROWINDOWADV] },
    { .field = "TCPToZeroWindowAdv",
     .offset = &_pm_proc_net_netstat.tcp[_PM_NETSTAT_TCPEXT_TCPTOZEROWINDOWADV] },
    { .field = "TCPWantZeroWindowAdv",
     .offset = &_pm_proc_net_netstat.tcp[_PM_NETSTAT_TCPEXT_TCPWANTZEROWINDOWADV] },
    { .field = "TCPSynRetrans",
     .offset = &_pm_proc_net_netstat.tcp[_PM_NETSTAT_TCPEXT_TCPSYNRETRANS] },
    { .field = "TCPOrigDataSent",
     .offset = &_pm_proc_net_netstat.tcp[_PM_NETSTAT_TCPEXT_TCPORIGDATASENT] },
    { .field = "TCPBacklogCoalesce",
      .offset = &_pm_proc_net_netstat.tcp[_PM_NETSTAT_TCPEXT_TCPBACKLOGCOALESCE] },
    { .field = "TCPMemoryPressuresChrono",
      .offset = &_pm_proc_net_netstat.tcp[_PM_NETSTAT_TCPEXT_TCPMEMORYPRESSURESCHRONO] },
    { .field = "TCPMD5Failure",
      .offset = &_pm_proc_net_netstat.tcp[_PM_NETSTAT_TCPEXT_TCPMD5FAILURE] },
    { .field = "PFMemallocDrop",
      .offset = &_pm_proc_net_netstat.tcp[_PM_NETSTAT_TCPEXT_PFMEMALLOCDROP] },
    { .field = "TCPFastOpenActiveFail",
      .offset = &_pm_proc_net_netstat.tcp[_PM_NETSTAT_TCPEXT_TCPFASTOPENACTIVEFAIL] },
    { .field = "TCPFastOpenBlackhole",
      .offset = &_pm_proc_net_netstat.tcp[_PM_NETSTAT_TCPEXT_TCPFASTOPENBLACKHOLE] },
    { .field = "TCPHystartTrainDetect",
      .offset = &_pm_proc_net_netstat.tcp[_PM_NETSTAT_TCPEXT_TCPHYSTARTTRAINDETECT] },
    { .field = "TCPHystartTrainCwnd",
      .offset = &_pm_proc_net_netstat.tcp[_PM_NETSTAT_TCPEXT_TCPHYSTARTTRAINCWND] },
    { .field = "TCPHystartDelayDetect",
      .offset = &_pm_proc_net_netstat.tcp[_PM_NETSTAT_TCPEXT_TCPHYSTARTDELAYDETECT] },
    { .field = "TCPHystartDelayCwnd",
      .offset = &_pm_proc_net_netstat.tcp[_PM_NETSTAT_TCPEXT_TCPHYSTARTDELAYCWND] },
    { .field = "TCPACKSkippedSynRecv",
      .offset = &_pm_proc_net_netstat.tcp[_PM_NETSTAT_TCPEXT_TCPACKSKIPPEDSYNRECV] },
    { .field = "TCPACKSkippedPAWS",
      .offset = &_pm_proc_net_netstat.tcp[_PM_NETSTAT_TCPEXT_TCPACKSKIPPEDPAWS] },
    { .field = "TCPACKSkippedSeq",
      .offset = &_pm_proc_net_netstat.tcp[_PM_NETSTAT_TCPEXT_TCPACKSKIPPEDSEQ] },
    { .field = "TCPACKSkippedFinWait2",
      .offset = &_pm_proc_net_netstat.tcp[_PM_NETSTAT_TCPEXT_TCPACKSKIPPEDFINWAIT2] },
    { .field = "TCPACKSkippedTimeWait",
      .offset = &_pm_proc_net_netstat.tcp[_PM_NETSTAT_TCPEXT_TCPACKSKIPPEDTIMEWAIT] },
    { .field = "TCPACKSkippedChallenge",
      .offset = &_pm_proc_net_netstat.tcp[_PM_NETSTAT_TCPEXT_TCPACKSKIPPEDCHALLENGE] },
    { .field = "TCPWinProbe",
      .offset = &_pm_proc_net_netstat.tcp[_PM_NETSTAT_TCPEXT_TCPWINPROBE] },
    { .field = "TCPKeepAlive",
      .offset = &_pm_proc_net_netstat.tcp[_PM_NETSTAT_TCPEXT_TCPKEEPALIVE] },
    { .field = "TCPMTUPFail",
      .offset = &_pm_proc_net_netstat.tcp[_PM_NETSTAT_TCPEXT_TCPMTUPFAIL] },
    { .field = "TCPMTUPSuccess",
      .offset = &_pm_proc_net_netstat.tcp[_PM_NETSTAT_TCPEXT_TCPMTUPSUCCESS] },
    { .field = "TCPDelivered",
      .offset = &_pm_proc_net_netstat.tcp[_PM_NETSTAT_TCPEXT_TCPDELIVERED] },
    { .field = "TCPDeliveredCE",
      .offset = &_pm_proc_net_netstat.tcp[_PM_NETSTAT_TCPEXT_TCPDELIVEREDCE] },
    { .field = "TCPAckCompressed",
      .offset = &_pm_proc_net_netstat.tcp[_PM_NETSTAT_TCPEXT_TCPACKCOMPRESSED] },
    { .field = "TCPZeroWindowDrop",
      .offset = &_pm_proc_net_netstat.tcp[_PM_NETSTAT_TCPEXT_TCPZEROWINDOWDROP] },
    { .field = "TCPRcvQDrop",
      .offset = &_pm_proc_net_netstat.tcp[_PM_NETSTAT_TCPEXT_TCPRCVQDROP] },
    { .field = "TCPWqueueTooBig",
      .offset = &_pm_proc_net_netstat.tcp[_PM_NETSTAT_TCPEXT_TCPWQUEUETOOBIG] },
    { .field = "TCPFastOpenPassiveAltKey",
      .offset = &_pm_proc_net_netstat.tcp[_PM_NETSTAT_TCPEXT_TCPFASTOPENPASSIVEALTKEY] },
    { .field = "TcpTimeoutRehash",
      .offset = &_pm_proc_net_netstat.tcp[_PM_NETSTAT_TCPEXT_TCPTIMEOUTREHASH] },
    { .field = "TcpDuplicateDataRehash",
      .offset = &_pm_proc_net_netstat.tcp[_PM_NETSTAT_TCPEXT_TCPDUPLICATEDATAREHASH] },
    { .field = "TCPDSACKRecvSegs",
      .offset = &_pm_proc_net_netstat.tcp[_PM_NETSTAT_TCPEXT_TCPDSACKRECVSEGS] },
    { .field = "TCPDSACKIgnoredDubious",
      .offset = &_pm_proc_net_netstat.tcp[_PM_NETSTAT_TCPEXT_TCPDSACKIGNOREDDUBIOUS] },
    { .field = "TCPMigrateReqSuccess",
      .offset = &_pm_proc_net_netstat.tcp[_PM_NETSTAT_TCPEXT_TCPMIGRATEREQSUCCESS] },
    { .field = "TCPMigrateReqFailure",
      .offset = &_pm_proc_net_netstat.tcp[_PM_NETSTAT_TCPEXT_TCPMIGRATEREQFAILURE] },
    { .field = "TCPLoss",
      .offset = &_pm_proc_net_netstat.tcp[_PM_NETSTAT_TCPEXT_TCPLOSS] },

    { .field = NULL, .offset = NULL }
};

netstat_fields_t netstat_mptcp_fields[] = {
    { .field = "MPCapableSYNRX",
     .offset = &_pm_proc_net_netstat.mptcp[_PM_NETSTAT_MPTCPEXT_MPCAPABLESYNRX] },
    { .field = "MPCapableACKRX",
     .offset = &_pm_proc_net_netstat.mptcp[_PM_NETSTAT_MPTCPEXT_MPCAPABLEACKRX] },
    { .field = "MPCapableFallbackACK",
     .offset = &_pm_proc_net_netstat.mptcp[_PM_NETSTAT_MPTCPEXT_MPCAPABLEFALLBACKACK] },
    { .field = "MPCapableFallbackSYNACK",
     .offset = &_pm_proc_net_netstat.mptcp[_PM_NETSTAT_MPTCPEXT_MPCAPABLEFALLBACKSYNACK] },
    { .field = "MPTCPRetrans",
     .offset = &_pm_proc_net_netstat.mptcp[_PM_NETSTAT_MPTCPEXT_MPTCPRETRANS] },
    { .field = "MPJoinNoTokenFound",
     .offset = &_pm_proc_net_netstat.mptcp[_PM_NETSTAT_MPTCPEXT_MPJOINNOTOKENFOUND] },
    { .field = "MPJoinSynRx",
     .offset = &_pm_proc_net_netstat.mptcp[_PM_NETSTAT_MPTCPEXT_MPJOINSYNRX] },
    { .field = "MPJoinSynAckRx",
     .offset = &_pm_proc_net_netstat.mptcp[_PM_NETSTAT_MPTCPEXT_MPJOINSYNACKRX] },
    { .field = "MPJoinSynAckHMacFailure",
     .offset = &_pm_proc_net_netstat.mptcp[_PM_NETSTAT_MPTCPEXT_MPJOINSYNACKHMACFAILURE] },
    { .field = "MPJoinAckRx",
     .offset = &_pm_proc_net_netstat.mptcp[_PM_NETSTAT_MPTCPEXT_MPJOINACKRX] },
    { .field = "MPJoinAckHMacFailure",
     .offset = &_pm_proc_net_netstat.mptcp[_PM_NETSTAT_MPTCPEXT_MPJOINACKHMACFAILURE] },
    { .field = "DSSNotMatching",
     .offset = &_pm_proc_net_netstat.mptcp[_PM_NETSTAT_MPTCPEXT_DSSNOTMATCHING] },
    { .field = "InfiniteMapRx",
     .offset = &_pm_proc_net_netstat.mptcp[_PM_NETSTAT_MPTCPEXT_INFINITEMAPRX] },
    { .field = "MPCapableSYNTX",
      .offset = &_pm_proc_net_netstat.mptcp[_PM_NETSTAT_MPTCPEXT_MPCAPABLESYNTX] },
    { .field = "MPCapableSYNACKRX",
      .offset = &_pm_proc_net_netstat.mptcp[_PM_NETSTAT_MPTCPEXT_MPCAPABLESYNACKRX] },
    { .field = "MPFallbackTokenInit",
      .offset = &_pm_proc_net_netstat.mptcp[_PM_NETSTAT_MPTCPEXT_MPFALLBACKTOKENINIT] },
    { .field = "DSSNoMatchTCP",
      .offset = &_pm_proc_net_netstat.mptcp[_PM_NETSTAT_MPTCPEXT_DSSNOMATCHTCP] },
    { .field = "DataCsumErr",
      .offset = &_pm_proc_net_netstat.mptcp[_PM_NETSTAT_MPTCPEXT_DATACSUMERR] },
    { .field = "OFOQueueTail",
      .offset = &_pm_proc_net_netstat.mptcp[_PM_NETSTAT_MPTCPEXT_OFOQUEUETAIL] },
    { .field = "OFOQueue",
      .offset = &_pm_proc_net_netstat.mptcp[_PM_NETSTAT_MPTCPEXT_OFOQUEUE] },
    { .field = "OFOMerge",
      .offset = &_pm_proc_net_netstat.mptcp[_PM_NETSTAT_MPTCPEXT_OFOMERGE] },
    { .field = "NoDSSInWindow",
      .offset = &_pm_proc_net_netstat.mptcp[_PM_NETSTAT_MPTCPEXT_NODSSINWINDOW] },
    { .field = "DuplicateData",
      .offset = &_pm_proc_net_netstat.mptcp[_PM_NETSTAT_MPTCPEXT_DUPLICATEDATA] },
    { .field = "AddAddr",
      .offset = &_pm_proc_net_netstat.mptcp[_PM_NETSTAT_MPTCPEXT_ADDADDR] },
    { .field = "AddAddrDrop",
      .offset = &_pm_proc_net_netstat.mptcp[_PM_NETSTAT_MPTCPEXT_ADDADDRDROPS] },
    { .field = "EchoAdd",
      .offset = &_pm_proc_net_netstat.mptcp[_PM_NETSTAT_MPTCPEXT_ECHOADD] },
    { .field = "PortAdd",
      .offset = &_pm_proc_net_netstat.mptcp[_PM_NETSTAT_MPTCPEXT_PORTADD] },
    { .field = "MPJoinPortSynRx",
      .offset = &_pm_proc_net_netstat.mptcp[_PM_NETSTAT_MPTCPEXT_MPJOINPORTSYNRX] },
    { .field = "MPJoinPortSynAckRx",
      .offset = &_pm_proc_net_netstat.mptcp[_PM_NETSTAT_MPTCPEXT_MPJOINPORTSYNACKRX] },
    { .field = "MPJoinPortAckRx",
      .offset = &_pm_proc_net_netstat.mptcp[_PM_NETSTAT_MPTCPEXT_MPJOINPORTACKRX] },
    { .field = "MismatchPortSynRx",
      .offset = &_pm_proc_net_netstat.mptcp[_PM_NETSTAT_MPTCPEXT_MISMATCHPORTSYNRX] },
    { .field = "MismatchPortAckRx",
      .offset = &_pm_proc_net_netstat.mptcp[_PM_NETSTAT_MPTCPEXT_MISMATCHPORTACKRX] },
    { .field = "RmAddr",
      .offset = &_pm_proc_net_netstat.mptcp[_PM_NETSTAT_MPTCPEXT_RMADDR] },
    { .field = "RmAddrDrop",
      .offset = &_pm_proc_net_netstat.mptcp[_PM_NETSTAT_MPTCPEXT_RMADDRDROPS] },
    { .field = "RmSubflow",
      .offset = &_pm_proc_net_netstat.mptcp[_PM_NETSTAT_MPTCPEXT_RMSUBFLOW] },
    { .field = "MPPrioTx",
      .offset = &_pm_proc_net_netstat.mptcp[_PM_NETSTAT_MPTCPEXT_MPPRIOTX] },
    { .field = "MPPrioRx",
      .offset = &_pm_proc_net_netstat.mptcp[_PM_NETSTAT_MPTCPEXT_MPPRIORX] },
    { .field = "RcvPruned",
      .offset = &_pm_proc_net_netstat.mptcp[_PM_NETSTAT_MPTCPEXT_RCVPRUNED] },
    { .field = "MPFailTx",
      .offset = &_pm_proc_net_netstat.mptcp[_PM_NETSTAT_MPTCPEXT_MPFAILTX] },
    { .field = "MPFailRx",
      .offset = &_pm_proc_net_netstat.mptcp[_PM_NETSTAT_MPTCPEXT_MPFAILRX] },
    { .field = "SubflowStale",
      .offset = &_pm_proc_net_netstat.mptcp[_PM_NETSTAT_MPTCPEXT_SUBFLOWSTALE] },
    { .field = "SubflowRecover",
      .offset = &_pm_proc_net_netstat.mptcp[_PM_NETSTAT_MPTCPEXT_SUBFLOWRECOVER] },
    { .field = "MPFastcloseTx",
      .offset = &_pm_proc_net_netstat.mptcp[_PM_NETSTAT_MPTCPEXT_MPFASTCLOSETX] },
    { .field = "MPFastcloseRx",
      .offset = &_pm_proc_net_netstat.mptcp[_PM_NETSTAT_MPTCPEXT_MPFASTCLOSERX] },
    { .field = "MPRstTx",
      .offset = &_pm_proc_net_netstat.mptcp[_PM_NETSTAT_MPTCPEXT_MPRSTTX] },
    { .field = "MPRstRx",
      .offset = &_pm_proc_net_netstat.mptcp[_PM_NETSTAT_MPTCPEXT_MPRSTRX] },

    { .field = NULL, .offset = NULL }
};


static void
get_fields(netstat_fields_t *fields, char *header, char *buffer)
{
    int i, j, count;
    char *p, *indices[NETSTAT_MAX_COLUMNS];

    /* first get pointers to each of the column headings */
    strtok(header, " ");
    for (i = 0; i < NETSTAT_MAX_COLUMNS; i++) {
	if ((p = strtok(NULL, " \n")) == NULL)
	    break;
	indices[i] = p;
    }
    count = i;
    while (p != NULL) {
	if (onetrip == 1)
	    pmNotifyErr(LOG_WARNING, "proc_net_netstat: %s extra field \"%s\" (increase NETSTAT_MAX_COLUMNS)\n", header, p);
	p = strtok(NULL, " \n");
    }

    /*
     * Extract values via back-referencing column headings.
     * "i" is the last found index, which we use for a bit
     * of optimisation for the (common) in-order maps case
     * (where "in order" means in the order defined by the
     * passed in "fields" table which typically matches the
     * kernel - but may be out-of-order for older kernels).
     */
    strtok(buffer, " ");
    for (i = j = 0; j <= count; j++) {
        if ((p = strtok(NULL, " \n")) == NULL)
            break;
	if (fields[i].field == NULL)
	    /* wrap search in fields table */
	    i = 0;
        if (strcmp(fields[i].field, indices[j]) == 0) {
	    if (fields[i].offset != &not_exported)
		*fields[i].offset = strtoull(p, NULL, 10);
	    else if (onetrip)
		pmNotifyErr(LOG_INFO, "proc_net_netstat: %s \"%s\" parsed but not exported\n", header, indices[j]);
	    i++;
	}
        else {
            for (i = 0; fields[i].field; i++) {
                if (strcmp(fields[i].field, indices[j]) != 0)
                    continue;
		if (fields[i].offset != &not_exported)
		    *fields[i].offset = strtoull(p, NULL, 10);
		else if (onetrip)
		    pmNotifyErr(LOG_INFO, "proc_net_netstat: %s \"%s\" parsed but not exported\n", header, indices[j]);
                break;
            }
	    if (fields[i].field == NULL) {
		/* not found, warn */
		if (onetrip == 1)
		    pmNotifyErr(LOG_WARNING, "proc_net_netstat: %s unknown field[#%d] \"%s\"\n", header, j, indices[j]);
	    }
	    else
		i++;
	}
    }
}


#define NETSTAT_IP_OFFSET(ii, pp) (int64_t *)((char *)pp + \
    (__psint_t)netstat_ip_fields[ii].offset - (__psint_t)&_pm_proc_net_netstat.ip)
#define NETSTAT_TCP_OFFSET(ii, pp) (int64_t *)((char *)pp + \
    (__psint_t)netstat_tcp_fields[ii].offset - (__psint_t)&_pm_proc_net_netstat.tcp)
#define NETSTAT_MPTCP_OFFSET(ii, pp) (int64_t *)((char *)pp + \
    (__psint_t)netstat_mptcp_fields[ii].offset - (__psint_t)&_pm_proc_net_netstat.mptcp)

static void
init_refresh_proc_net_netstat(proc_net_netstat_t *netstat)
{
    int		i;

    /* initially, all marked as "no value available" */
    for (i = 0; netstat_ip_fields[i].field != NULL; i++)
	*(NETSTAT_IP_OFFSET(i, netstat->ip)) = -1;
    for (i = 0; netstat_tcp_fields[i].field != NULL; i++)
	*(NETSTAT_TCP_OFFSET(i, netstat->tcp)) = -1;
    for (i = 0; netstat_mptcp_fields[i].field != NULL; i++)
	*(NETSTAT_MPTCP_OFFSET(i, netstat->mptcp)) = -1;
}

size_t
check_read_trunc(char *buf, FILE *fp)
{
    char	*p;
    size_t	lost;
    int		c;

    for (p = buf; *p; p++)
	;
    if (p > buf)
	p--;
    if (*p == '\n') {
	return 0;
    }

    lost = 1;
    while ((c = fgetc(fp)) != EOF) {
	if (c == '\n')
	    break;
	lost++;
    }

    return lost;
}

#define MAXLINELEN 4192

int
refresh_proc_net_netstat(proc_net_netstat_t *netstat)
{
    /* Need a sufficiently large value to hold a full line */
    char	buf[MAXLINELEN];
    char	header[MAXLINELEN];
    FILE	*fp;

    if (onetrip < 0)
	return onetrip;

    init_refresh_proc_net_netstat(netstat);
    if ((fp = linux_statsfile("/proc/net/netstat", buf, sizeof(buf))) == NULL)
	return -oserror();
    while (fgets(header, sizeof(header), fp) != NULL) {
	if (onetrip == 1) {
	    size_t	lost;
	    if ((lost = check_read_trunc(header, fp)) != 0) {
		pmNotifyErr(LOG_ERR, "refresh_proc_net_netstat: header[] too small, need at least %zd more bytes\n", lost);
		onetrip = PM_ERR_BOTCH;
		fclose(fp);
		return onetrip;
	    }
	}
	if (fgets(buf, sizeof(buf), fp) != NULL) {
	    if (onetrip == 1) {
		size_t	lost;
		if ((lost = check_read_trunc(buf, fp)) != 0) {
		    pmNotifyErr(LOG_ERR, "refresh_proc_net_netstat: buf[] too small, need at least %zd more bytes\n", lost);
		    onetrip = PM_ERR_BOTCH;
		    fclose(fp);
		    return onetrip;
		}
	    }
	    if (strncmp(buf, "IpExt:", 6) == 0)
		get_fields(netstat_ip_fields, header, buf);
	    else if (strncmp(buf, "TcpExt:", 7) == 0)
		get_fields(netstat_tcp_fields, header, buf);
	    else if (strncmp(buf, "MPTcpExt:", 9) == 0)
		get_fields(netstat_mptcp_fields, header, buf);
	    else
		pmNotifyErr(LOG_ERR, "Unrecognised netstat row: %s\n", buf);
	}
    }
    onetrip = 0;	/* been thru the whole file, assume OK next time */
    fclose(fp);
    return 0;
}
