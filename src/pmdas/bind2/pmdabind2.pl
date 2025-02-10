#!/usr/bin/perl
#!/usr/bin/perl -d:Trace
# Copyright (C) 2016 by Lukas Oliva (olivalukas@gmail.com)
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
# Author:  <olivalukas@gmail.com>
# Created: 09 Sep 2016
# Version: 0.01

use warnings  FATAL => qw{uninitialized};
use strict;
use autodie;
use English;

use PCP::PMDA;
use Time::HiRes            qw{gettimeofday alarm};
use LWP::UserAgent;
use XML::LibXML;
use File::Spec::Functions  qw{catfile};
use File::Slurp            qw{read_file};
use Data::Dumper;

# Note: Enable this together with -d:Trace in the hashbang at the top of the file for per line traces
#$Devel::Trace::TRACE = 0;

# Note: Enable this for performance profiling if you find a reason to do so
# use Devel::NYTProf;
# $ENV{use_db_sub} = 1;

# Note: Enable this to get stacks in the log files
#$SIG{ __DIE__ } = sub { Carp::confess( @_ ) };

# Note: Read the README file for short description and notes about necessary Bind server configuration

$Data::Dumper::Sortkeys = 1;
$Data::Dumper::Pad = "   <dump> ";

#
# Pre-release TODOs:
#
#TODO: Review all the metrics for type/semantics/units/help/longhelp
#TODO: follow PMNS naming convensions (i.e. no hyphens)

#
# TODOs and ideas for improvements:
#
#TODO: Make the autoconfiguration optional or take it out to a script
#TODO: Make the instance handling adaptive
#TODO: Separate the Bind stats reading data into a module, so that it can be otherwise used and better tested
#TODO: Add warnings/unknown_stats counter
#TODO: Add .pmda metrics for config, errors and reasons (bind2.pmda.errors.http_fetch, bind2.pmda.errors.timeout, bind2.pmda.delays.{data_fetch,fetch,fetch_callback}[actual,1min,5min,10min])
#Idea: Make zone-statistics possible to be disabled (e.g. for servers with huge amount of zones)
#Idea: Better handle bind2.memory.contexts.blocksize set to '-'
#Idea: Better handle undef in bind2.sockets.peer-address
#TODO: Add .total metrics for memory and sockstat metrics
#TODO: Make the intentionally deleted data available once they are considered useful. Note that some of these may grow large, so test under load at first. Once enabling these, make their ignoring configurable.
#Idea: Support more than one server (multiple host parameters -> $0, per-daemon process)
#Idea: Make the optional items (sockets, memory contexts) possible to be disabled by file configuration
#Idea: bind2.sockets.{local_address,name,peer_address,bound/unbound} -> bind2.sockets.total
#Idea: Make the bind2.sockets.{stats.{listener,connected},type}, bind2.sockets.type a scalar

# Testing
#TODO: Test both bind server response time and parsing time for huge (tens of thousands) sockets and use only stats if needed (the same with memory contexts)
#TODO: Test Install/start with Bind server down, then bring it up


our %cfg = (
    config_fname => "bind2.conf",
    pmda_prefix  => "bind2",
    xml_prefix   => undef,

    metrics => {
        types => {
            # Give a list of metrics with non-default type here

            PM_TYPE_STRING() => [qw{bind2.memory.contexts.name
                                    bind2.boot_time
                                    bind2.config_time
                                    bind2.current_time

                                    bind2.sockets.local_address
                                    bind2.sockets.name
                                    bind2.sockets.peer_address
                                    bind2.sockets.type
                                    bind2.tasks.ADB.state
                                    bind2.tasks.client.state
                                    bind2.tasks.other.state
                               }],
        },

        semantics => {
            # Give a list of metrics with non-default semantics here

            PM_SEM_DISCRETE() => [qw{bind2.memory.total.BlockSize
                                     bind2.memory.contexts.name
                                     bind2.sockets.local_address
                                     bind2.sockets.name
                                     bind2.sockets.peer_address
                                     bind2.tasks.ADB
                                     bind2.tasks.client
                                     bind2.tasks.other
                              }],
            PM_SEM_INSTANT()  => [qw{bind2.boot_time
                                     bind2.config_time
                                     bind2.current_time

                                     bind2.memory.total.InUse
                                     bind2.memory.total.Lost
                                     bind2.memory.total.ContextSize
                                     bind2.memory.total.TotalUse
                                     bind2.memory.total.Malloced
                                     bind2.memory.contexts.blocksize
                                     bind2.memory.contexts.hiwater
                                     bind2.memory.contexts.inuse
                                     bind2.memory.contexts.lowater
                                     bind2.memory.contexts.maxinuse
                                     bind2.memory.contexts.pools
                                     bind2.memory.contexts.references
                                     bind2.memory.contexts.total

                                     bind2.sockets.references
                                     bind2.sockets.states.bound
                                     bind2.sockets.states.connected
                                     bind2.sockets.states.listener

                                     bind2.sockets.total
                                }],
        },

        defaults => {
            # Default metric properties here

            type      => PM_TYPE_U32,
            semantics => PM_SEM_COUNTER,
        },
    },

    # Maximum time in seconds (may also be a fraction) to keep the data for responses
    max_delta_sec      => 0.9,
    max_get_delay_sec  => 0.7,

    # Note: Debug level should be on 0 for normal use or for running some load tests
    debug              => 0,
    debug_pcp          => 0,
);

our (%current_data,%metrics,%indoms,%id2metrics,%id2instances,$lwp_user_agent);

$0 = "pmda$cfg{pmda_prefix}";

# Enable PCP debugging if required
$ENV{PCP_DEBUG} = 65535
    if $cfg{debug_pcp};

# Construct the PCP::PMDA object
my $pmda = PCP::PMDA->new($cfg{pmda_prefix}, 25);

die "Failed to construct the PMDA object"
    unless $pmda;

# Construct the LWP User Agent object
$lwp_user_agent = LWP::UserAgent->new
    or die "Failed to create the LWP::UserAgent";

mytrace(Data::Dumper->Dump([\$lwp_user_agent],[qw{lwp_user_agent}]))
    if $cfg{debug};

# Load the configuration
die "Failed to load config file"
    unless $cfg{loaded} = load_config(catfile(pmda_config('PCP_PMDAS_DIR'),
                                              $cfg{pmda_prefix},
                                              $cfg{config_fname}));

$pmda->connect_pmcd;

#$Devel::Trace::TRACE = 1;
mydebug("Connected to PMCD");

# Initialize the list of metrics
init_metrics($cfg{loaded}{uri})
    or die "Failed to init metrics from Bind server at '$cfg{loaded}{uri}'";

mydebug("Metrics initialized");

# Add the instance domains
foreach my $indom_id (sort {$a <=> $b} keys %indoms) {
    mydebug("$cfg{pmda_prefix} adding instance domain '$indom_id' ...")
        if $cfg{debug};

    my $res = $pmda->add_indom($indom_id,
                               $indoms{$indom_id},
                               "TODO",
                               "long TODO");
    mydebug("add_indom returned: " . Dumper($res))
        if $cfg{debug};

    $id2instances{$indom_id}{$indoms{$indom_id}{$_}} = $_
        foreach sort keys %{$indoms{$indom_id}};
}

# known metrics and the item part of their PMID ... this list
# may need to be updated over time, any metric that ends up with
# a PMID in cluster 1, so PMID = 25.1.*, is a metrics that should
# be in %known_pmids, so they end up in cluster 0 with PMID 25.0.*
#
my %known_pmids = (
    # bind version 9.10 (or possibly earlier) metrics
    'bind2.boot_time',			0,
    'bind2.config_time',		1,
    'bind2.current_time',		2,
    'bind2.memory.total.BlockSize',	3,
    'bind2.memory.total.ContextSize',	4,
    'bind2.memory.total.InUse',		5,
    'bind2.memory.total.Lost',		6,
    'bind2.memory.total.TotalUse',	7,
    'bind2.nsstat.AuthQryRej',		8,
    'bind2.nsstat.DNS64',		9,
    'bind2.nsstat.ExpireOpt',		10,
    'bind2.nsstat.NSIDOpt',		11,
    'bind2.nsstat.OtherOpt',		12,
    'bind2.nsstat.QryAuthAns',		13,
    'bind2.nsstat.QryDropped',		14,
    'bind2.nsstat.QryDuplicate',	15,
    'bind2.nsstat.QryFORMERR',		16,
    'bind2.nsstat.QryFailure',		17,
    'bind2.nsstat.QryNXDOMAIN',		18,
    'bind2.nsstat.QryNoauthAns',	19,
    'bind2.nsstat.QryNxrrset',		20,
    'bind2.nsstat.QryRecursion',	21,
    'bind2.nsstat.QryReferral',		22,
    'bind2.nsstat.QrySERVFAIL',		23,
    'bind2.nsstat.QrySuccess',		24,
    'bind2.nsstat.QryTCP',		25,
    'bind2.nsstat.QryUDP',		26,
    'bind2.nsstat.RPZRewrites',		27,
    'bind2.nsstat.RateDropped',		28,
    'bind2.nsstat.RateSlipped',		29,
    'bind2.nsstat.RecQryRej',		30,
    'bind2.nsstat.RecursClients',	31,
    'bind2.nsstat.ReqBadEDNSVer',	32,
    'bind2.nsstat.ReqBadSIG',		33,
    'bind2.nsstat.ReqEdns0',		34,
    'bind2.nsstat.ReqSIG0',		35,
    'bind2.nsstat.ReqTCP',		36,
    'bind2.nsstat.ReqTSIG',		37,
    'bind2.nsstat.Requestv4',		38,
    'bind2.nsstat.Requestv6',		39,
    'bind2.nsstat.RespEDNS0',		40,
    'bind2.nsstat.RespSIG0',		41,
    'bind2.nsstat.RespTSIG',		42,
    'bind2.nsstat.Response',		43,
    'bind2.nsstat.SitBadSize',		44,
    'bind2.nsstat.SitBadTime',		45,
    'bind2.nsstat.SitMatch',		46,
    'bind2.nsstat.SitNew',		47,
    'bind2.nsstat.SitNoMatch',		48,
    'bind2.nsstat.SitOpt',		49,
    'bind2.nsstat.TruncatedResp',	50,
    'bind2.nsstat.UpdateBadPrereq',	51,
    'bind2.nsstat.UpdateDone',		52,
    'bind2.nsstat.UpdateFail',		53,
    'bind2.nsstat.UpdateFwdFail',	54,
    'bind2.nsstat.UpdateRej',		55,
    'bind2.nsstat.UpdateReqFwd',	56,
    'bind2.nsstat.UpdateRespFwd',	57,
    'bind2.nsstat.XfrRej',		58,
    'bind2.nsstat.XfrReqDone',		59,
    'bind2.sockstat.FDWatchClose',	60,
    'bind2.sockstat.FDwatchConn',	61,
    'bind2.sockstat.FDwatchConnFail',	62,
    'bind2.sockstat.FDwatchRecvErr',	63,
    'bind2.sockstat.FDwatchSendErr',	64,
    'bind2.sockstat.FdwatchBindFail',	65,
    'bind2.sockstat.RawActive',		66,
    'bind2.sockstat.RawClose',		67,
    'bind2.sockstat.RawOpen',		68,
    'bind2.sockstat.RawOpenFail',	69,
    'bind2.sockstat.RawRecvErr',	70,
    'bind2.sockstat.TCP4Accept',	71,
    'bind2.sockstat.TCP4AcceptFail',	72,
    'bind2.sockstat.TCP4Active',	73,
    'bind2.sockstat.TCP4BindFail',	74,
    'bind2.sockstat.TCP4Close',		75,
    'bind2.sockstat.TCP4Conn',		76,
    'bind2.sockstat.TCP4ConnFail',	77,
    'bind2.sockstat.TCP4Open',		78,
    'bind2.sockstat.TCP4OpenFail',	79,
    'bind2.sockstat.TCP4RecvErr',	80,
    'bind2.sockstat.TCP4SendErr',	81,
    'bind2.sockstat.TCP6Accept',	82,
    'bind2.sockstat.TCP6AcceptFail',	83,
    'bind2.sockstat.TCP6Active',	84,
    'bind2.sockstat.TCP6BindFail',	85,
    'bind2.sockstat.TCP6Close',		86,
    'bind2.sockstat.TCP6Conn',		87,
    'bind2.sockstat.TCP6ConnFail',	88,
    'bind2.sockstat.TCP6Open',		89,
    'bind2.sockstat.TCP6OpenFail',	90,
    'bind2.sockstat.TCP6RecvErr',	91,
    'bind2.sockstat.TCP6SendErr',	92,
    'bind2.sockstat.UDP4Active',	93,
    'bind2.sockstat.UDP4BindFail',	94,
    'bind2.sockstat.UDP4Close',		95,
    'bind2.sockstat.UDP4Conn',		96,
    'bind2.sockstat.UDP4ConnFail',	97,
    'bind2.sockstat.UDP4Open',		98,
    'bind2.sockstat.UDP4OpenFail',	99,
    'bind2.sockstat.UDP4RecvErr',	100,
    'bind2.sockstat.UDP4SendErr',	101,
    'bind2.sockstat.UDP6Active',	102,
    'bind2.sockstat.UDP6BindFail',	103,
    'bind2.sockstat.UDP6Close',		104,
    'bind2.sockstat.UDP6Conn',		105,
    'bind2.sockstat.UDP6ConnFail',	106,
    'bind2.sockstat.UDP6Open',		107,
    'bind2.sockstat.UDP6OpenFail',	108,
    'bind2.sockstat.UDP6RecvErr',	109,
    'bind2.sockstat.UDP6SendErr',	110,
    'bind2.sockstat.UnixAccept',	111,
    'bind2.sockstat.UnixAcceptFail',	112,
    'bind2.sockstat.UnixActive',	113,
    'bind2.sockstat.UnixBindFail',	114,
    'bind2.sockstat.UnixClose',		115,
    'bind2.sockstat.UnixConn',		116,
    'bind2.sockstat.UnixConnFail',	117,
    'bind2.sockstat.UnixOpen',		118,
    'bind2.sockstat.UnixOpenFail',	119,
    'bind2.sockstat.UnixRecvErr',	120,
    'bind2.sockstat.UnixSendErr',	121,
    'bind2.total.queries.out.IQUERY',	122,
    'bind2.total.queries.out.NOTIFY',	123,
    'bind2.total.queries.out.QUERY',	124,
    'bind2.total.queries.out.STATUS',	125,
    'bind2.total.queries.out.UPDATE',	126,
    'bind2.zonestat.AXFRReqv4',		127,
    'bind2.zonestat.AXFRReqv6',		128,
    'bind2.zonestat.IXFRReqv4',		129,
    'bind2.zonestat.IXFRReqv6',		130,
    'bind2.zonestat.NotifyInv4',	131,
    'bind2.zonestat.NotifyInv6',	132,
    'bind2.zonestat.NotifyOutv4',	133,
    'bind2.zonestat.NotifyOutv6',	134,
    'bind2.zonestat.NotifyRej',		135,
    'bind2.zonestat.SOAOutv4',		136,
    'bind2.zonestat.SOAOutv6',		137,
    'bind2.zonestat.XfrFail',		138,
    'bind2.zonestat.XfrSuccess',	139,
    # bind version 9.11
    'bind2.nsstat.CookieBadSize',	140,
    'bind2.nsstat.CookieBadTime',	141,
    'bind2.nsstat.CookieIn',		142,
    'bind2.nsstat.CookieMatch',		143,
    'bind2.nsstat.CookieNew',		144,
    'bind2.nsstat.CookieNoMatch',	145,
    'bind2.nsstat.ECSOpt',		146,
    'bind2.nsstat.KeyTagOpt',		147,
    'bind2.nsstat.QryBADCOOKIE',	148,
    'bind2.nsstat.QryNXRedir',		149,
    'bind2.nsstat.QryNXRedirRLookup',	150,
    'bind2.nsstat.QryTryStale',		151,
    'bind2.nsstat.QryUsedStale',	152,
    'bind2.nsstat.TCPConnHighWater',	153,
    # bind version somwhere between 9.12 and 9.18
    'bind2.memory.total.Malloced',	154,
    'bind2.nsstat.KeepAliveOpt',	155,
    'bind2.nsstat.PadOpt',		156,
    'bind2.nsstat.Prefetch',		157,
    'bind2.nsstat.RecLimitDropped',	158,
    'bind2.nsstat.SynthNODATA',		159,
    'bind2.nsstat.SynthNXDOMAIN',	160,
    'bind2.nsstat.SynthWILDCARD',	161,
    'bind2.nsstat.UpdateQuota',		162,
    'bind2.sockstat.TCP4Clients',	163,
    'bind2.sockstat.TCP6Clients',	164,
    'bind2.queries.in.A',		165,
    'bind2.queries.in.IXFR',		166,
    'bind2.queries.in.SOA',		167,
    # enabled by resolver metrics patch (for bind version 9.10 or later)
    'bind2.resolver.total.resqtype.DNSKEY',		168,
    'bind2.resolver.total.resqtype.NS',			169,
    'bind2.resolver.total.resstats.BadEDNSVersion',	170,
    'bind2.resolver.total.resstats.BucketSize',		171,
    'bind2.resolver.total.resstats.EDNS0Fail',		172,
    'bind2.resolver.total.resstats.FORMERR',		173,
    'bind2.resolver.total.resstats.GlueFetchv4',	174,
    'bind2.resolver.total.resstats.GlueFetchv4Fail',	175,
    'bind2.resolver.total.resstats.GlueFetchv6',	176,
    'bind2.resolver.total.resstats.GlueFetchv6Fail',	177,
    'bind2.resolver.total.resstats.Lame',		178,
    'bind2.resolver.total.resstats.Mismatch',		179,
    'bind2.resolver.total.resstats.NXDOMAIN',		180,
    'bind2.resolver.total.resstats.NumFetch',		181,
    'bind2.resolver.total.resstats.OtherError',		182,
    'bind2.resolver.total.resstats.QryRTT10',		183,
    'bind2.resolver.total.resstats.QryRTT100',		184,
    'bind2.resolver.total.resstats.QryRTT1800',		185,
    'bind2.resolver.total.resstats.QryRTT1800p',	186,
    'bind2.resolver.total.resstats.QryRTT500',		187,
    'bind2.resolver.total.resstats.QryRTT800',		188,
    'bind2.resolver.total.resstats.QueryAbort',		189,
    'bind2.resolver.total.resstats.QueryCurTCP',	190,
    'bind2.resolver.total.resstats.QueryCurUDP',	191,
    'bind2.resolver.total.resstats.QuerySockFail',	192,
    'bind2.resolver.total.resstats.QueryTimeout',	193,
    'bind2.resolver.total.resstats.Queryv4',		194,
    'bind2.resolver.total.resstats.Queryv6',		195,
    'bind2.resolver.total.resstats.REFUSED',		196,
    'bind2.resolver.total.resstats.Responsev4',		197,
    'bind2.resolver.total.resstats.Responsev6',		198,
    'bind2.resolver.total.resstats.Retry',		199,
    'bind2.resolver.total.resstats.SERVFAIL',		200,
    'bind2.resolver.total.resstats.ServerQuota',	201,
    'bind2.resolver.total.resstats.SitClientOk',	202,
    'bind2.resolver.total.resstats.SitClientOut',	203,
    'bind2.resolver.total.resstats.SitIn',		204,
    'bind2.resolver.total.resstats.SitOut',		205,
    'bind2.resolver.total.resstats.Truncated',		206,
    'bind2.resolver.total.resstats.ValAttempt',		207,
    'bind2.resolver.total.resstats.ValFail',		208,
    'bind2.resolver.total.resstats.ValNegOk',		209,
    'bind2.resolver.total.resstats.ValOk',		210,
    'bind2.resolver.total.resstats.ZoneQuota',		211,
    'bind2.resolver.total.resstats.QryRTT1600',		212,
    'bind2.resolver.total.resstats.QryRTT1600p',	213,
    'bind2.resolver.total.resstats.BadCookieRcode',	214,
    'bind2.resolver.total.resstats.ClientCookieOut',	215,
    'bind2.resolver.total.resstats.CookieClientOk',	216,
    'bind2.resolver.total.resstats.CookieIn',		217,
    'bind2.resolver.total.resstats.NextItem',		218,
    # duplicates from earlier commit botch ... these are available
    'UNASSIGNED.219',					219,
    'UNASSIGNED.220',					220,
    'UNASSIGNED.221',					221,
    'bind2.resolver.total.resstats.ServerCookieOut',	222,
    # enabled by resolver meteric patch (for bind version somwhere between 9.12 and 9.18)
    'bind2.resolver.total.resqtype.AAAA',		223,
    'bind2.resolver.total.resqtype.CNAME',		224,
    'bind2.resolver.total.resqtype.DS',			225,
    'bind2.resolver.total.resqtype.HTTPS',		226,
    'bind2.resolver.total.resqtype.MX',			227,
    'bind2.resolver.total.resqtype.NULL',		228,
    'bind2.resolver.total.resqtype.PTR',		229,
    'bind2.resolver.total.resqtype.SOA',		230,
    'bind2.resolver.total.resqtype.SRV',		231,
    'bind2.resolver.total.resstats.ClientQuota',	232,
    'bind2.resolver.total.resstats.Priming',		233,
    'bind2.resolver.total.resqtype.A',			234,
    # enabled by rcode & qtype metrics patch (for bind version 9.11 or later)
    'bind2.total.queries.out.BADCOOKIE',		235,
    'bind2.total.queries.out.BADVERS',			236,
    'bind2.total.queries.out.FORMERR',			237,
    'bind2.total.queries.out.NOERROR',			238,
    'bind2.total.queries.out.NOTAUTH',			239,
    'bind2.total.queries.out.NOTIMP',			240,
    'bind2.total.queries.out.NOTZONE',			241,
    'bind2.total.queries.out.NXDOMAIN',			242,
    'bind2.total.queries.out.NXRRSET',			243,
    'bind2.total.queries.out.REFUSED',			244,
    'bind2.total.queries.out.SERVFAIL',			245,
    'bind2.total.queries.out.YXDOMAIN',			246,
    'bind2.total.queries.out.YXRRSET',			247,
    # enabled by rcode & qtype meteric patch (for bind version somwhere between 9.12 and 9.18)
    'bind2.qtype.A',					248,
    'bind2.qtype.AAAA',					249,
    'bind2.qtype.DNSKEY',				250,
    'bind2.qtype.HTTPS',				251,
    'bind2.qtype.MX',					252,
    'bind2.qtype.NS',					253,
    'bind2.qtype.PTR',					254,
    'bind2.qtype.SOA',					255,
    'bind2.qtype.SRV',					256,
);


# metrics with non-default pmunits
my %known_units = (
    'bind2.memory.total.BlockSize',	pmda_units(1,0,0,PM_SPACE_BYTE,0,0),
    'bind2.memory.total.ContextSize',	pmda_units(1,0,0,PM_SPACE_BYTE,0,0),
    'bind2.memory.total.InUse',		pmda_units(1,0,0,PM_SPACE_BYTE,0,0),
    'bind2.memory.total.TotalUse',	pmda_units(1,0,0,PM_SPACE_BYTE,0,0),
    'bind2.memory.total.Lost',		pmda_units(1,0,0,PM_SPACE_BYTE,0,0),
    'bind2.memory.total.Malloced',	pmda_units(1,0,0,PM_SPACE_BYTE,0,0),
);

# Add all the metrics
my $serial = 0;		# maximum item field in PMID, used for metrics
			# not in %known_pmids, and counts UP
foreach my $metric (sort keys %metrics) {
    my $item;			# actual item field for each PMID
    my $cluster;		# actual cluster field for each PMID

    if (exists $known_pmids{$metric}) {
	$item = $known_pmids{$metric};
	$cluster = 0;
    }
    else {
	$item = $serial++;
	$cluster = 1;
	myinfo("Warning: metric $metric: no fixed PMID, using item $cluster.$item");
    }

    my $refh_metric = $metrics{$metric};
    my ($pmid,$type,$indom,$semantics,$units,$shorthelp,$longhelp) = ( pmda_pmid($cluster,$item),
                                                                       $refh_metric->{type},
                                                                       exists $refh_metric->{indom_id} ? $refh_metric->{indom_id} : PM_INDOM_NULL,
                                                                       $refh_metric->{semantics},
                                                                       (exists $known_units{$metric} ? $known_units{$metric} : pmda_units(0,0,1,0,0,PM_COUNT_ONE)),
                                                                       (exists $refh_metric->{help}     ? $refh_metric->{help} : ""),
                                                                       (exists $refh_metric->{longhelp} ? $refh_metric->{longhelp} : ""));

    mydebug("Current metric parameters: " . Dumper(\$refh_metric));
    mydebug("$cfg{pmda_prefix} adding metric $metric using:\n" . Data::Dumper->Dump([\$pmid,
                                                                                     \$type,
                                                                                     \$indom,
                                                                                     \$semantics,
                                                                                     \$units,
                                                                                     \$shorthelp,
                                                                                     \$longhelp],
                                                                                    [qw{pmid type indom semantics units shorthelp longhelp}]));

    my $res = $pmda->add_metric(
        $pmid,                   # PMID
        $type,                   # data type
        $indom,                  # indom
        $semantics,              # semantics
        $units,                  # units
        $metric,                 # key name
        $shorthelp,              # short help
        $longhelp);              # long help
    $id2metrics{1024*$cluster + $item} = $metric;

    mydebug("... returned '" . ($res // "<undef>") . "'");
}

# Set all the necessary callbacks and the account to run under
$pmda->set_refresh(\&refresh);
$pmda->set_fetch_callback(\&fetch_callback);
$pmda->set_user('pcp');

mydebug("Dump of the most important variables:" . Data::Dumper->Dump([\%metrics,\%indoms,\%id2metrics],
                                                                     [qw{metrics indoms id2metrics}]));

# Start the PMDA
mydebug("Calling ->run (PMDA PID: $PID)");
$pmda->run;
# Not reached.


## Subroutines
#- Callback routines
sub refresh {
    my ($cluster) = @_;

    mydebug("Entering refresh() ...");

    # Clean deprecated data for all the instances
    my ($cur_date_sec,$cur_date_msec) = gettimeofday;
    my $cur_date = $cur_date_sec + $cur_date_msec/1e6;

    # Fetch Bind metrics info ...
    if (not keys %metrics) {
        mydebug("No metrics found, calling init_metrics ...");

        init_metrics()
    }

    if (($cur_date - $current_data{timestamp}) > $cfg{max_delta_sec}) {
        mydebug("Removing cache data as too old - cur_date: $cur_date, timestamp: $current_data{timestamp}");

        delete $current_data{values};

        my $refh_bind_stats = fetch_bind_stats($cfg{loaded}{uri});

        unless (defined $refh_bind_stats) {
            $pmda->err("HTTP GET from '$cfg{loaded}{uri}' failed");

            return (PM_ERR_INST, 0)
        }

        $current_data{timestamp} = $cur_date;
        $current_data{values} = $refh_bind_stats;

        mydebug("... current_data updated");
    }

    mydebug("... refresh() finished");
}

sub fetch_callback {
    my ($cluster, $item, $inst) = @_;

    mydebug("Entering fetch_callback(): "
                . Data::Dumper->Dump([\$cluster,\$item,\$inst],
                                     [qw(cluster item inst)]))
        if $cfg{debug};

    unless (defined $cluster and defined $item and defined $inst) {
        myerror("Either cluster, item or inst were undefined: " . Data::Dumper->Dump([$cluster,$item,$inst],
                                                                                     [qw{cluster item inst}]))
            if $cfg{debug};

        return
    }

    my $metric_name = pmda_pmid_name($cluster, $item);

    mydebug("Metric name: " . (defined $metric_name ? $metric_name : "<undef>"));

    if (not defined $metric_name) {
        # Return error if metric was not given
        mydebug("Given metric is not defined");

        return (PM_ERR_PMID, 0)
    }

    # Return the value if available
    mydebug("Finding the metric value in current_data ...");

    if (exists $current_data{values}{$metric_name} and defined $current_data{values}{$metric_name}) {
        # ... key exists

        mydebug("... it exists and is defined");

        my $indom_id = $metrics{$metric_name}{indom_id};

        if (defined $indom_id) {
            # ... and isn't a scalar

            mydebug("... it is not a scalar");

            if ($inst == PM_IN_NULL) {
                # Return error if instance number was not given
                mydebug("Given instance was PM_IN_NULL");

                return (PM_ERR_INST, 0)
            }

            mydebug("... and is not a PM_IN_NULL");

            my $inst_name = $id2instances{$indom_id}{$inst};

            unless (defined $inst_name) {
                myerror("Assertion error - unknown inst '$inst' should be defined for metric '$metric_name'");

                return (PM_ERR_INST, 0);
            }

            mydebug("For metric: '$metric_name', indom_id: '$indom_id', instance: '$inst_name' returning: $current_data{values}{$metric_name}{$inst_name} with success");

            return ($current_data{values}{$metric_name}{$inst_name},1)
        } else {
            # ... is a scalar - return (<value>,<success code>)
            mydebug("Returning success");

            return ($current_data{values}{$metric_name}, 1);
        }

        myerror("Assertion error - we should not get here");
    } else {
        # Return error if the atom value was not succesfully retrieved
        mydebug("Required metric '$metric_name' was not found in Bind stats or was undefined");

        return (PMDA_FETCH_NOVALUES, 0)
    }

    myerror("Assertion Error fetch_callback() finished - this line should never be reached");
}


#- Logging subroutines
sub mylog {
    my ($prefix,@args) = @_;

    chomp @args;
    #print STDERR join("",$prefix, " ", $_ , "\n")
    $pmda->log(join("",$prefix, " ", $_))
        foreach @args;
}


sub myinfo  { pmda_install() ? return : mylog("INFO",@_) }
sub mydebug { $cfg{debug} < 1 ? return : mylog("DEBUG",@_) }
sub mytrace { $cfg{debug} < 2 ? return : mylog("TRACE",@_) }

sub myerror {
    unless (pmda_install()) {
        $pmda->err("ERROR " . $_)
            foreach @_;
    }
}

#- Configuration-related subroutines
sub load_config {
    my ($in_fname) = @_;
    my $refh_res;

    open my $fh_in,"<",$in_fname;

    my ($host_id,$db_id,$lineno) = (0,0,0);
    my $err_count;

    while (my $aline = <$fh_in>) {
        $lineno++;
        chomp $aline;

        if ($aline =~ /\A\s*#/) {
            mytrace("#$lineno: Skipping line '$aline'");

            next
        } elsif ($aline =~ /\A\s*\Z/) {
            mytrace("#$lineno: Skipping line '$aline'");

            next
        } elsif ($aline =~ /\A\s*host\s*=\s*(?<uri>(?:(?<proto>\S+):\/\/)?(?<host>\S+):(?<port>\d+)?)\s*\Z/) {
            mytrace(Data::Dumper->Dump([\$lineno,$+{proto},$+{host},$+{port},$+{uri},$aline],
                                       [qw{lineno proto host port uri aline}]))
                if $cfg{debug};

            die "More than single host given"
                if exists $refh_res->{host} and defined $refh_res->{host};

            $refh_res = {
                proto => $+{proto},
                host  => $+{host},
                port  => $+{port},
                uri   => $+{uri}
            };

            # Autoconfigure if necessary
            $refh_res->{proto} //= "http";
            $refh_res->{port}  //= 80;

            $refh_res->{uri} = $refh_res->{proto} . "://" . $refh_res->{host} . ":" . $refh_res->{port};

            # Check that host names/addresses and port numbers are valid
            $err_count++,myerror("Failed to gethostbyname($$refh_res{host})")
                unless $refh_res->{host} and gethostbyname $refh_res->{host};
            $err_count++,myerror("Unexpected port number $$refh_res{port}")
                unless $refh_res->{port} and $refh_res->{port} >= 1 and $refh_res->{port} <= 65535;

            myinfo("Detected - host: '$$refh_res{host}', port: '$$refh_res{port}'");
        } elsif ($aline =~ /\A\s*test\s*=\s*(?<filename>\S+)\s*\Z/) {
            $refh_res = {
                test  => $+{filename},
            };

            myinfo("Detected - test data file: '$$refh_res{test}'");
        } else {
            warn "#$lineno: Unexpected line '$aline', skipping it";
        }
    }

    mytrace(Dumper($refh_res))
        if $cfg{debug};

    die "No host to be monitored found in '$in_fname'"
        unless exists $refh_res->{host} and defined $refh_res->{host} or defined $refh_res->{test};


    die "$err_count errors in config file detected, exiting"
        if $err_count;

    $refh_res
}

#- Data extraction subroutines (XML mongers)
#Note: I am an XML beginner. Anyone can find a better way to extract the information
#      and reimplement the subroutines but keep in mind to hold the minimum delay and memoization
#      features for minimum performance impact on the host where the PMDA is running.
sub get_server_stats {
    my ($xml) = @_;
    my $refh_res;

    my $pattern = $cfg{xml_prefix} . "/statistics/server/*";
    foreach my $node ($xml->findnodes($pattern)) {
        mydebug("node: ", Data::Dumper->Dump([$node->nodeName,$node->textContent],
                                             [qw{name content}]))
            if $cfg{debug};

        if ($node->nodeName eq "#text") {
            mytrace("skipping");

            next
        } elsif ($node->nodeName =~ /\A(boot-time|current-time|config-time)\Z/) {
            (my $name = $1) =~ s/-/_/g;
            mytrace("times - '"
                . $name
                . "': '"
                . $node->textContent);

            $refh_res->{join ".",$cfg{pmda_prefix},$name} = $node->textContent
        } elsif ($node->nodeName eq "requests") {
            mytrace("requests: ",
                    Data::Dumper->Dump([@{$node->childNodes}],
                                       [qw{child_nodes}]))
                if $cfg{debug};

            foreach my $opcode ($node->childNodes) {
                mytrace("opcode ",
                        Data::Dumper->Dump([$opcode->nodeName,$opcode->textContent],
                                           [qw{name value}]))
                    if $cfg{debug};

                next
                    if $opcode->nodeName eq "#text";

                my %data;

                foreach my $child_node ($opcode->childNodes) {
                    mytrace("child_node ",
                            Data::Dumper->Dump([$child_node->nodeName,$child_node->textContent],
                                               [qw{name content}]))
                        if $cfg{debug};

                    next
                        if $child_node->nodeName eq "#text";

                    $data{$child_node->nodeName} = $child_node->textContent;
                }

                mytrace("opcode ",
                        Data::Dumper->Dump([$data{name},$data{counter}],
                                           [qw{name counter}]))
                    if $cfg{debug};

                $refh_res->{join ".",
                            $cfg{pmda_prefix},
                            "total",
                            "queries",
                            "out",
                            $data{name}} = $data{counter};
            }
        } elsif ($node->nodeName eq "queries-in") {
            foreach my $rdtype ($node->childNodes) {
                next
                    if $rdtype->nodeName eq "#text";

                my %data;

                foreach my $child_node ($rdtype->childNodes) {
                    next
                        if $child_node->nodeName eq "#text";

                    $data{$child_node->nodeName} = $child_node->textContent;
                }

                $refh_res->{join ".",
                            $cfg{pmda_prefix},
                            "queries",
                            "in",
                            $data{name}} = $data{counter};
            }
        } elsif ($node->nodeName =~ /\A(nsstat|zonestat|sockstat)\Z/) {
            my %data;

            foreach my $child_node ($node->childNodes) {
                next
                    if $child_node->nodeName eq "#text";

                $data{$child_node->nodeName} = $child_node->textContent;
            }

            $refh_res->{join ".",$cfg{pmda_prefix},
                        $node->nodeName,
                        $data{name}} = $data{counter};
        } elsif ($node->nodeName eq "counters") {
            my $type = $node->getAttribute('type');

            next
                unless $type =~ /\A(opcode|nsstat|zonestat|sockstat|rcode|qtype)\Z/;

            foreach my $child_node ($node->childNodes) {
                next
                    unless $child_node->nodeName eq "counter";

                my $name = $child_node->getAttribute('name');
                my $metric = $type;

                if ($type eq "opcode" or $type eq "rcode") {
                    next
                        unless $name !~ /\ARESERVED.*\Z/;
                    next
                        unless $name !~ /\A[0-9][0-9]*\Z/;

                    $metric = "total.queries.out";
                }
                $refh_res->{join ".",
                            $cfg{pmda_prefix},
                            $metric,
                            $name} = $child_node->textContent;
            }
        } elsif ($node->nodeName eq "version") {
                # ignoring
        } else {
            warn "get_server_stats: unknown node name '" . $node->nodeName . "'";
        }
    }

    mytrace("get_server_stats: ", Dumper($refh_res))
        if $cfg{debug};

    $refh_res
}

sub get_task_stats {
    my ($xml) = @_;
    my $refh_res;
    my $pattern;

    # Get the thread model counters
    my %data;

    $pattern = $cfg{xml_prefix} . "/statistics/taskmgr/thread-model/*";
    foreach my $task ($xml->findnodes($pattern)) {
        next
            if $task->textContent eq "#text";

        $refh_res->{join '.',$cfg{pmda_prefix},"thread_model",$task->nodeName} = $task->textContent;
    }

    mytrace("get_task_stats - thread-model: ", Dumper(\$refh_res))
        if $cfg{debug};

    # Get the task counters
    $pattern = $cfg{xml_prefix} . "/statistics/taskmgr/tasks/*";
    foreach my $task ($xml->findnodes($pattern)) {
        next
            if $task->textContent eq "#text";

        my ($id,$name,%data);

        foreach my $node ($task->childNodes) {
            if ($node->nodeName eq "id") {
                $id = $node->textContent;
            } elsif ($node->nodeName eq "name") {
                $name = $node->textContent;
            } elsif ($node->nodeName eq "#text") {
                next
            } else {
                $data{$node->nodeName} = $node->textContent;
            }
        }

        $name //= "other";
        mytrace("task: ", Data::Dumper->Dump([$id,$name,\%data],
                                             [qw{id name data}]))
            if $cfg{debug};

        $refh_res->{join ".",
                    $cfg{pmda_prefix},
                    "tasks",
                    $name,
                    $_}{$id} = $data{$_}
            foreach keys %data;
    }

    mytrace("get_task_stats: ", Dumper(\$refh_res))
        if $cfg{debug};

    $refh_res
}

sub get_socket_stats {
    my ($xml) = @_;
    my ($refh_res,%stats);

    #Idea: Make the calculation of statistics independent to the node parsing

    my $pattern = $cfg{xml_prefix} . "/statistics/socketmgr/sockets/*";
    foreach my $node ($xml->findnodes($pattern)) {
        my %data = (id              => undef,
                    name            => undef,
                    references      => undef,
                    type            => undef,
                    "local-address" => undef,
                    "peer-address"  => undef);
        my $refh_states;

        foreach my $child_node ($node->childNodes) {
            mytrace("child_node: " . $child_node->nodeName);

            # foreach my $name (keys %data) {
            if ($child_node->nodeName eq "states") {
                mytrace("states: " . Dumper($child_node->childNodes->to_literal))
                    if $cfg{debug};

                foreach my $state ($child_node->childNodes) {
                    my $state_name = $state->textContent;
                    my $name = $state->nodeName;

                    mytrace(Data::Dumper->Dump([\$name,\$state_name],
                                               [qw{name state_name}]))
                        if $cfg{debug};

                    next
                        if $name eq "#text";

                    $refh_states->{$state_name} = 1;
                }
            } elsif (exists $data{$child_node->nodeName}) {
                $data{$child_node->nodeName} = $child_node->textContent;
            } elsif ($child_node->nodeName eq "#text") {
                next
            } else {
                die "Assertion error - unexpected node '" . $child_node->nodeName . "' in <socket>"
            # }
            }
        }

        # Calculate statistics
        $stats{"$cfg{pmda_prefix}.sockets.total.bound-remote"}++
            if defined $refh_res->{"peer-address"};
        $stats{"$cfg{pmda_prefix}.sockets.total.name"}{$data{name}}++
            if defined $refh_res->{name};
        $stats{"$cfg{pmda_prefix}.sockets.total.proto"}{$data{type}}++;

        foreach my $state (qw{bound connected listener}) {
            $stats{"$cfg{pmda_prefix}.sockets.total.state_$state"}++
                if exists $refh_states->{$state};
        }

        mytrace("id: '$data{id}'");

        foreach my $key (grep {not /\Aid\Z/} sort keys %data) {
            $refh_res->{join ".",
                        $cfg{pmda_prefix},
                        "sockets",
                        $key}{$data{id}} = $data{$key};
        }

        foreach my $state (keys %$refh_states) {
            $refh_res->{join ".",
                        $cfg{pmda_prefix},
                        "sockets",
                        "states",
                        $state}{$data{id}} = $refh_states->{$data{id}} // 1;
        }
    }

    $refh_res->{$_} = $stats{$_}
        foreach keys %stats;

    mytrace("get_socket_stats: ", Dumper(\$refh_res))
        if $cfg{debug};

    $refh_res;
}

sub get_resstat {
    my ($xml,$view) = @_;
    my $refh_res;

    my $pattern = $cfg{xml_prefix} . "/statistics/views/view[\@name='$view']/resstat|" . $cfg{xml_prefix} . "/statistics/views/view[\@name='$view']/counters[\@type='resstats' or \@type='resqtype']/counter";

    foreach my $node ($xml->findnodes($pattern)) {
        my ($name,$value);

	if ($node->nodeName eq 'counter' &&  $node->hasAttribute('name')) {
	  $name = $node->getAttribute('name');
	  $name =~ s/\+/p/g;
	  $value = $node->textContent;
	  my $type = $node->parentNode->getAttribute('type');

	  $refh_res->{join ".",
		      $cfg{pmda_prefix},
		      "resolver",
		      "total",
		      $type,
		    $name} = $value;
	  next;
	}

        foreach my $child_node ($node->childNodes) {
            my $node_name = $child_node->nodeName;

            mytrace("node_name: '$node_name'");

            if ($node_name eq "name") {
                $name = $child_node->textContent;

                mytrace("name: '$name'");
            } elsif ($node_name eq "counter") {
                $value = $child_node->textContent;

                mytrace("value: '$value'");
            } elsif ($node_name eq "#text") {
                next
            } else {
                die "Unexpected XML node in resstat: '$node_name'";
            }
        }

        $refh_res->{join ".",
                    $cfg{pmda_prefix},
                    "resolver",
                    "total",
                    $name} = $value;
    }

    mytrace("params: ", Dumper(\$refh_res))
        if $cfg{debug};

    $refh_res
}

sub get_zone_stats {
    my ($xml,$view) = @_;
    my $refh_res;

    mydebug("get_zone_stats (xml not dumped):" . Data::Dumper->Dump([\$view],[qw{view}]))
        if $cfg{debug};

    my $pattern = $cfg{xml_prefix} . "/statistics/views/view[name='$view']/zones/*";
    foreach my $zone ($xml->findnodes($pattern)) {
        my ($name) = map { $_->textContent } $zone->findnodes("./name");

        $name =~ s/\/\S+//;
        mytrace(Data::Dumper->Dump([\$zone,\$name],[qw{zone name}]))
            if $cfg{debug};

        foreach my $node ($zone->childNodes) {
            mytrace(Data::Dumper->Dump([$node->nodeName],[qw{name}]))
                if $cfg{debug};

            if ($node->nodeName =~ /\A(name|#text)\Z/) {
                next
            } elsif ($node->nodeName eq "counters") {
                foreach my $counter ($node->childNodes) {
                    next
                        if $counter->nodeName eq "#text";

                    $refh_res->{join ".",
                                $cfg{pmda_prefix},
                                "zones",
                                $counter->nodeName}->{$name} = $counter->textContent;
                }
            } else {
                $refh_res->{join ".",
                            $cfg{pmda_prefix},
                            "zones",
                            "serial"}{$name} = $node->textContent;
            }
        }
    }

    mydebug("get_zone_stats results: ", Data::Dumper->Dump([$refh_res],[qw{refh_res}]))
        if $cfg{debug};

    $refh_res
}

sub get_memory_stats {
    my ($xml) = @_;
    my $refh_res;

    my $pattern = $cfg{xml_prefix} . "/statistics/memory/summary";
    mytrace("get_memory_stats - from: ", $pattern);
    foreach my $node ($xml->findnodes($pattern)) {
        mytrace("get_memory_stats - node: ", Dumper($node));
        next
            if $node->nodeName eq "#text";

        foreach my $child_node ($node->childNodes) {
            next
                if $child_node->nodeName eq "#text";

            $refh_res->{join ".",
                        $cfg{pmda_prefix},
                        "memory",
                        "total",
                        $child_node->nodeName} = $child_node->textContent;
        }
    }

    mytrace("get_memory_stats - total: ", Dumper(\$refh_res))
        if $cfg{debug};

    #Note: Commented out as the values seem not to be useful now
    # $pattern = $cfg{xml_prefix} . "/statistics/memory/contexts/*";
    # foreach my $context ($xml->findnodes($pattern)) {
    #     next
    #         if $context->nodeName eq "#text";

    #     my ($id,%data);

    #     foreach my $child_node ($context->childNodes) {
    #         if ($child_node->nodeName eq "#text") {
    #             next
    #         } elsif ($child_node->nodeName eq "id") {
    #             $id = $child_node->textContent
    #         } else {
    #             $data{$child_node->nodeName} = $child_node->textContent;
    #         }
    #     }

    #     mytrace(Data::Dumper->Dump([$id,\%data],[qw{id data}]))
    #         if $cfg{debug};

    #     foreach (keys %data) {
    #         $refh_res->{join ".",
    #                     $cfg{pmda_prefix},
    #                     "memory",
    #                     "contexts",
    #                     $_}{$id} = $data{$_};
    #     }
    # }

    mytrace("get_memory_stats: ", Dumper(\$refh_res))
        if $cfg{debug};

    $refh_res
}

#- Detect properties of given XML, esp. global prefix (for older binds)
sub parse_version {
    my ($xml) = @_;

    unless (defined $cfg{xml_prefix}) {
        if ($xml->exists('/isc/bind')) {
            $cfg{xml_prefix} = '/isc/bind';
        } else {
            $cfg{xml_prefix} = '';
        }
    }
}

#- Extract XML data into internal data structures for serving pmcd requests
sub parse_bind_stats {
    my ($content) = @_;
    my $dom;

    # Parse the stats
    my $parser = XML::LibXML->new
        or die "Failed to create XML parser";

    eval {
        $dom = $parser->load_xml(
            # location => $uri,
            string => $content,
        );
    };

    if ($@) {
        myerror("Exception caught while parsing content: '$@'");

        return undef
    }

    parse_version($dom);

    my ($refh_res,$refh_foo);
    my %subs = (
        server           => sub { get_server_stats($_[0])          },
        # Note: Commented out as currently not being useful. If you enable it make a few tests.
        # task             => sub { get_task_stats($_[0])            },
        # Note: Commented out as currently not being useful and potentially problematic for Bind server being loaded with queries leading to the TCP responses, so having a lot of open sockets.
        # socket           => sub { get_socket_stats($_[0])          },
        resolver_default => sub { get_resstat($_[0],"_default")    },
        # resolver_bind    => sub { get_resstat($_[0],"_bind")       },
        # zone_default     => sub { get_zone_stats($_[0],"_default") },
        # zone_bind        => sub { get_zone_stats($_[0],"_bind")    },
        memory           => sub { get_memory_stats($_[0])          }
    );

    #Idea: Use socket information to give the amount of open sockets per source host

    mytrace(Data::Dumper->Dump([\%subs],[qw{subs}]))
        if $cfg{debug};

    foreach my $type (sort keys %subs) {
        mytrace(Data::Dumper->Dump([\$type],[qw{type}]))
            if $cfg{debug};

        mydebug("Running $type") if $cfg{debug};

        $refh_foo = $subs{$type}->($dom);
        next unless defined $refh_foo;
        mydebug("Good data from $type") if $cfg{debug};

        $refh_res = merge_hashrefs_to_first($refh_res, $refh_foo)
            or die "Failed to merge $type stats";
    }

    $refh_res
}

#- Overall data fetch subroutine to get the XML into the PMDA process
sub fetch_bind_stats {
    my ($uri) = @_;
    my $qaxml = $cfg{loaded}{test};
    
    return get_bind_stats($uri)
        unless defined $qaxml;
    return read_bind_stats($qaxml);
}

#- Inject test data, read XML directly from local filesystem
sub read_bind_stats {
    my ($fname) = @_;
    my $content = read_file($fname);

    mytrace(Data::Dumper->Dump([\$content],[qw{content}]))
        if $cfg{debug};

    my $refh_res = parse_bind_stats($content);

    {
        local $Data::Dumper::Maxdepth = 2;

        mydebug("read_bind_stats - result: " . Data::Dumper->Dump([\$refh_res],[qw{refh_res}]))
            if $cfg{debug};
    }

    $refh_res
}

#- Extract data in the normal way, using a HTTP GET request
sub get_bind_stats {
    my ($uri) = @_;
    my @time1 = gettimeofday;
    my $response;

    eval {
        local $SIG{ALRM} = sub {
            mydebug("alarm timeouted");

            die "Timeout alarm"
        };

        mydebug("Set alarm to $cfg{max_get_delay_sec} seconds ...");
        alarm $cfg{max_get_delay_sec};

        $response = $lwp_user_agent->get($cfg{loaded}{uri});
        mytrace(Data::Dumper->Dump([$response],[qw{response}]))
            if $cfg{debug};

        alarm 0;
        mydebug("Alarm disarmed");
    };

    if ($@) {
        if ($@ =~ /Timeout alarm/) {
            mydebug("LWP GET timeouted");

            return undef
        } else {
            myerror("Exception while waiting for LWP get: $@");

            return undef
        }
    }

    my @time2 = gettimeofday;
    my $fetch_delay_sec = (($time2[0] - $time1[0]) + ($time2[1] - $time1[1])/1e6);

    mydebug("Fetch delay: $fetch_delay_sec secs");

    # Check the response
    unless ($response->is_success) {
        myerror("Failed to retrieve stats from '$cfg{loaded}{uri}': " . $response->status_line);

        return undef
    }
    mydebug("Success retrieving stats from '$cfg{loaded}{uri}': " . $response->status_line);

    mytrace(Data::Dumper->Dump([\$response->content],[qw{content}]))
        if $cfg{debug};

    my $refh_res = parse_bind_stats($response->content);

    {
        local $Data::Dumper::Maxdepth = 2;

        mydebug("get_bind_stats - result: " . Data::Dumper->Dump([\$refh_res],[qw{refh_res}]))
            if $cfg{debug};
    }

    my @time3 = gettimeofday;
    my $processing_delay_sec = (($time3[0] - $time2[0]) + ($time3[1] - $time2[1])/1e6);

    mydebug("Processing delay: $processing_delay_sec secs ()");

    $refh_res
}

#- Initialization procedure
sub init_metrics {
    my ($uri) = @_;

    mydebug("Entering init_metrics() ...");

    my $refh_res = fetch_bind_stats($uri);
    mydebug("Failed to fetch Bind server stats")
        unless defined $refh_res;

    mydebug("init_metrics:");

    # Create list of current metrics, type, semantics and values
    foreach my $metric (sort keys %$refh_res) {
        $metrics{$metric}{value} = $refh_res->{$metric};

        # Get the metric type ...
        my (@types,@semantics);

        foreach my $data_type (keys %{$cfg{metrics}{types}}) {
            push @types,$data_type
                foreach grep { $metric =~ /\A$_/ } @{$cfg{metrics}{types}{$data_type}};
        }

        die "More than one data types found for metric '$metric': @types"
            if @types and @types > 1;

        $metrics{$metric}{type} = $types[0] // $cfg{metrics}{defaults}{type};

        # ...  and semantics
        foreach my $data_semantics (keys %{$cfg{metrics}{semantics}}) {
            push @semantics,$data_semantics
                foreach grep { $metric =~ /\A$_/ } @{$cfg{metrics}{semantics}{$data_semantics}};
        }

        die "More than one data semantics found for metric '$metric': @semantics"
            if @semantics and @semantics > 1;

        $metrics{$metric}{semantics} = $semantics[0] // $cfg{metrics}{defaults}{semantics};
        mytrace("Metric '$metric' - type: '$metrics{$metric}{type}', semantics: '$metrics{$metric}{semantics}', value: '$metrics{$metric}{value}'");
    }

    # Create a list of current instance domains
    foreach my $metric (sort keys %metrics) {
        # Skip all the scalar metrics
        next
            if ref $metrics{$metric}{value} ne ref {};

        mydebug("Deriving instance domain of metric: '$metric': " . Dumper($metrics{$metric}{value}))
            if $cfg{debug};

        # Check if there is an existing metric space - get the difference
        my $indom_id;

        foreach my $cur_indom_id (sort {$a <=> $b} keys %indoms) {
            # Check if instances in existing instance domain are the same (the domain is the same)
            # Note: This way may be prone to instance domains that would contain the same members
            #       at the time of the initialization but would be different in general. For the
            #       moment, I consider this to be unlikely case but in general this may lead to
            #       problems. General configuration generated on demand by script outside
            #       of the PMDA code could resolve this by enabling a check.

            my @difference = grep {
                not exists $indoms{$cur_indom_id}{$_}
            } sort keys %{$metrics{$metric}{value}};

            unless (@difference) {
                $indom_id = $cur_indom_id;

                last
            }
        }

        mydebug(Data::Dumper->Dump([$indom_id,\%indoms],[qw{indom_id indoms}]))
            if $cfg{debug};

        # This is a new indom
        unless (defined $indom_id) {
            $indom_id = scalar(keys(%indoms)) + 1;

            # Add the new indom
            my $instance_id = 0;

            $indoms{$indom_id} = { map {
                my $foo = $_;

                mydebug("foo: '$foo', instance_id: '$instance_id'");

                $foo => $instance_id++
            } sort keys %{$metrics{$metric}{value}} };
        }

        mydebug("... added indom: " . Data::Dumper->Dump([$indom_id,$indoms{$indom_id}],
                                                         [qw{indom_id indoms_of_indom_id}]))
            if $cfg{debug};

        $metrics{$metric}{indom_id} = $indom_id;
    }

    # Copy current values to current_data
    my @current_time = gettimeofday;

    $current_data{timestamp} = $current_time[0] + $current_time[1]/1e6;
    $current_data{values} = { map {
        $_ => $metrics{$_}{value};
    } keys %metrics };

    mydebug("init_metrics result: " . Data::Dumper->Dump([\%metrics,\%indoms,\%current_data],
                                                         [qw{metrics indoms current_data}]))
        if $cfg{debug};

    1
}

sub merge_hashrefs_to_first {
    my ($x,$y) = @_;

    mytrace("merge_hashrefs_to_first: " . Data::Dumper->Dump([\$x,\$y],[qw{x y}]))
        if $cfg{debug};

    return undef
        unless defined $y and ref $y eq ref {};

    foreach (keys %$y) {
        die "Assertion error - key $_ already exists"
            if exists $x->{$_};

        $x->{$_} = $y->{$_}
    }

    mytrace("merge_hashrefs_to_first - result: " . Data::Dumper->Dump([\$x],[qw{x}]))
        if $cfg{debug};

    $x
}
