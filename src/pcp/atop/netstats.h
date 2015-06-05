/*
** structures defined from the output of /proc/net/snmp and /proc/net/snmp6
*/
struct ipv4_stats {
	count_t Forwarding;
	count_t DefaultTTL;
	count_t InReceives;
	count_t InHdrErrors;
	count_t InAddrErrors;
	count_t ForwDatagrams;
	count_t InUnknownProtos;
	count_t InDiscards;
	count_t InDelivers;
	count_t OutRequests;
	count_t OutDiscards;
	count_t OutNoRoutes;
	count_t ReasmTimeout;
	count_t ReasmReqds;
	count_t ReasmOKs;
	count_t ReasmFails;
	count_t FragOKs;
	count_t FragFails;
	count_t FragCreates;
};

struct icmpv4_stats {
	count_t InMsgs;
	count_t InErrors;
	count_t InDestUnreachs;
	count_t InTimeExcds;
	count_t InParmProbs;
	count_t InSrcQuenchs;
	count_t InRedirects;
	count_t InEchos;
	count_t InEchoReps;
	count_t InTimestamps;
	count_t InTimestampReps;
	count_t InAddrMasks;
	count_t InAddrMaskReps;
	count_t OutMsgs;
	count_t OutErrors;
	count_t OutDestUnreachs;
	count_t OutTimeExcds;
	count_t OutParmProbs;
	count_t OutSrcQuenchs;
	count_t OutRedirects;
	count_t OutEchos;
	count_t OutEchoReps;
	count_t OutTimestamps;
	count_t OutTimestampReps;
	count_t OutAddrMasks;
	count_t OutAddrMaskReps;
};

struct udpv4_stats {
	count_t InDatagrams;
	count_t NoPorts;
	count_t InErrors;
	count_t OutDatagrams;
};

struct tcp_stats {
	count_t RtoAlgorithm;
	count_t RtoMin;
	count_t RtoMax;
	count_t MaxConn;
	count_t ActiveOpens;
	count_t PassiveOpens;
	count_t AttemptFails;
	count_t EstabResets;
	count_t CurrEstab;
	count_t InSegs;
	count_t OutSegs;
	count_t RetransSegs;
	count_t InErrs;
	count_t OutRsts;
};

struct ipv6_stats {
	count_t Ip6InReceives;
	count_t Ip6InHdrErrors;
	count_t Ip6InTooBigErrors;
	count_t Ip6InNoRoutes;
	count_t Ip6InAddrErrors;
	count_t Ip6InUnknownProtos;
	count_t Ip6InTruncatedPkts;
	count_t Ip6InDiscards;
	count_t Ip6InDelivers;
	count_t Ip6OutForwDatagrams;
	count_t Ip6OutRequests;
	count_t Ip6OutDiscards;
	count_t Ip6OutNoRoutes;
	count_t Ip6ReasmTimeout;
	count_t Ip6ReasmReqds;
	count_t Ip6ReasmOKs;
	count_t Ip6ReasmFails;
	count_t Ip6FragOKs;
	count_t Ip6FragFails;
	count_t Ip6FragCreates;
	count_t Ip6InMcastPkts;
	count_t Ip6OutMcastPkts;
};

struct icmpv6_stats {
	count_t Icmp6InMsgs;
	count_t Icmp6InErrors;
	count_t Icmp6InDestUnreachs;
	count_t Icmp6InPktTooBigs;
	count_t Icmp6InTimeExcds;
	count_t Icmp6InParmProblems;
	count_t Icmp6InEchos;
	count_t Icmp6InEchoReplies;
	count_t Icmp6InGroupMembQueries;
	count_t Icmp6InGroupMembResponses;
	count_t Icmp6InGroupMembReductions;
	count_t Icmp6InRouterSolicits;
	count_t Icmp6InRouterAdvertisements;
	count_t Icmp6InNeighborSolicits;
	count_t Icmp6InNeighborAdvertisements;
	count_t Icmp6InRedirects;
	count_t Icmp6OutMsgs;
	count_t Icmp6OutDestUnreachs;
	count_t Icmp6OutPktTooBigs;
	count_t Icmp6OutTimeExcds;
	count_t Icmp6OutParmProblems;
	count_t Icmp6OutEchoReplies;
	count_t Icmp6OutRouterSolicits;
	count_t Icmp6OutNeighborSolicits;
	count_t Icmp6OutNeighborAdvertisements;
	count_t Icmp6OutRedirects;
	count_t Icmp6OutGroupMembResponses;
	count_t Icmp6OutGroupMembReductions;
};

struct udpv6_stats {
	count_t Udp6InDatagrams;
	count_t Udp6NoPorts;
	count_t Udp6InErrors;
	count_t Udp6OutDatagrams;
};
