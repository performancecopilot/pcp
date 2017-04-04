#
# Copyright (c) 2009-2011 Josef 'Jeff' Sipek <jeffpc@josefsipek.net>
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

use strict;
use warnings;
use PCP::PMDA;
use Time::HiRes qw ( time );

use vars qw( $pmda );
my $pdns_control = 'pdns_control';
my $rec_control  = 'rec_control';

my $pdns_disable = 0;
my $rec_disable = 0;

my $cached = 0;
my %vals = ();
my %vals_rec_answers = ();

sub pdns_fetch
{
    my $now = time;

    if ($now - $cached > 1.0) {
        # $pmda->log("pdns_fetch_callback update now:$now cached:$cached\n");

        %vals = ();
        %vals_rec_answers = ();

        # get the authoritative server stats
        if ($pdns_disable == 0 && open(PIPE, "$pdns_control list |")) {
            $_ = <PIPE>;
            close PIPE;

            $_ =~ s/-/_/g;
            $_ =~ s/,$//;
            for my $kv (split(/,/, $_)) {
                if ("$kv" eq '') {
                    last;
                }
    
                my ($k, $v) = split(/=/, $kv);
                $vals{$k} = $v;
            }
        } else {
            $pdns_disable = 1;
        }

        # get the recursive server stats
        if ($rec_disable == 0 && open(PIPE, "$rec_control get-all |")) {
            while($_ = <PIPE>) {
                $_ =~ s/-/_/g;
                my ($k, $v) = split(/\t/, $_);

                if ($k eq 'answers0_1') {
			$vals_rec_answers{0} = $v;
		} elsif ($k eq 'answers1_10') {
			$vals_rec_answers{1} = $v;
		} elsif ($k eq 'answers10_100') {
			$vals_rec_answers{2} = $v;
		} elsif ($k eq 'answers100_1000') {
			$vals_rec_answers{3} = $v;
		} elsif ($k eq 'answers_slow') {
			$vals_rec_answers{4} = $v;
		} else {
	                $vals{"recursor.$k"} = $v;
		}
            }
            close PIPE;
        } else {
            $rec_disable = 1;
        }

        $cached = $now;
    }
}

sub pdns_fetch_callback
{
    my ($cluster, $item, $inst) = @_;
    my $metric_name = pmda_pmid_name($cluster, $item);

    # $pmda->log("pdns_fetch_callback $metric_name $cluster:$item ($inst)\n");

    if (!defined($metric_name))    { return (PM_ERR_PMID, 0); }

    $metric_name =~ s/^pdns\.//;

    if ($metric_name eq 'recursor.answers') {
        if ($inst == PM_IN_NULL)    { return (PM_ERR_INST, 0); }
        return ($vals_rec_answers{$inst}, 1) if (defined($vals_rec_answers{$inst}));
    } else {
        if ($inst != PM_IN_NULL)    { return (PM_ERR_INST, 0); }
        return ($vals{$metric_name}, 1) if (defined($vals{$metric_name}));
    }
    return (PM_ERR_APPVERSION, 0);
}

$pmda = PCP::PMDA->new('pdns', 101);

$pmda->add_metric(pmda_pmid(0,0), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
                pmda_units(0,0,1,0,0,PM_COUNT_ONE),
                "pdns.corrupt_packets", '', '');
$pmda->add_metric(pmda_pmid(0,1), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
                pmda_units(0,0,1,0,0,PM_COUNT_ONE),
                "pdns.deferred_cache_inserts", '', '');
$pmda->add_metric(pmda_pmid(0,2), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
                pmda_units(0,0,1,0,0,PM_COUNT_ONE),
                "pdns.deferred_cache_lookup", '', '');
$pmda->add_metric(pmda_pmid(0,3), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT,
                pmda_units(0,1,0,0,PM_TIME_USEC,0),
                "pdns.latency", '', '');
$pmda->add_metric(pmda_pmid(0,4), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
                pmda_units(0,0,1,0,0,PM_COUNT_ONE),
                "pdns.packetcache_hit", '', '');
$pmda->add_metric(pmda_pmid(0,5), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
                pmda_units(0,0,1,0,0,PM_COUNT_ONE),
                "pdns.packetcache_miss", '', '');
$pmda->add_metric(pmda_pmid(0,6), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT,
                pmda_units(0,0,0,0,0,0),
                "pdns.packetcache_size", '', '');
$pmda->add_metric(pmda_pmid(0,7), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT,
                pmda_units(0,0,0,0,0,0),
                "pdns.qsize_q", '', '');
$pmda->add_metric(pmda_pmid(0,8), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
                pmda_units(0,0,1,0,0,PM_COUNT_ONE),
                "pdns.query_cache_hit", '', '');
$pmda->add_metric(pmda_pmid(0,9), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
                pmda_units(0,0,1,0,0,PM_COUNT_ONE),
                "pdns.query_cache_miss", '', '');
$pmda->add_metric(pmda_pmid(0,10), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
                pmda_units(0,0,1,0,0,PM_COUNT_ONE),
                "pdns.recursing_answers", '', '');
$pmda->add_metric(pmda_pmid(0,11), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
                pmda_units(0,0,1,0,0,PM_COUNT_ONE),
                "pdns.recursing_questions", '', '');
$pmda->add_metric(pmda_pmid(0,12), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
                pmda_units(0,0,1,0,0,PM_COUNT_ONE),
                "pdns.servfail_packets", '', '');
$pmda->add_metric(pmda_pmid(0,13), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
                pmda_units(0,0,1,0,0,PM_COUNT_ONE),
                "pdns.tcp_answers", '', '');
$pmda->add_metric(pmda_pmid(0,14), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
                pmda_units(0,0,1,0,0,PM_COUNT_ONE),
                "pdns.tcp_queries", '', '');
$pmda->add_metric(pmda_pmid(0,15), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
                pmda_units(0,0,1,0,0,PM_COUNT_ONE),
                "pdns.timedout_packets", '', '');
$pmda->add_metric(pmda_pmid(0,16), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
                pmda_units(0,0,1,0,0,PM_COUNT_ONE),
                "pdns.udp_answers", '', '');
$pmda->add_metric(pmda_pmid(0,17), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
                pmda_units(0,0,1,0,0,PM_COUNT_ONE),
                "pdns.udp_queries", '', '');
$pmda->add_metric(pmda_pmid(0,18), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
                pmda_units(0,0,1,0,0,PM_COUNT_ONE),
                "pdns.udp4_answers", '', '');
$pmda->add_metric(pmda_pmid(0,19), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
                pmda_units(0,0,1,0,0,PM_COUNT_ONE),
                "pdns.udp4_queries", '', '');
$pmda->add_metric(pmda_pmid(0,20), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
                pmda_units(0,0,1,0,0,PM_COUNT_ONE),
                "pdns.udp6_answers", '', '');
$pmda->add_metric(pmda_pmid(0,21), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
                pmda_units(0,0,1,0,0,PM_COUNT_ONE),
                "pdns.udp6_queries", '', '');

my $recursor_answers_indom = 1;
my @recursor_answers_dom = (
                0 => '0-1 ms',
                1 => '1-10 ms',
                2 => '10-100 ms',
                3 => '100-1000 ms',
                4 => '1000+ ms',
             );

$pmda->add_metric(pmda_pmid(1,0),PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
                pmda_units(0,0,1,0,0,PM_COUNT_ONE),
                "pdns.recursor.all_outqueries",
                'counts the number of outgoing UDP queries since starting', '');
$pmda->add_metric(pmda_pmid(1,1),PM_TYPE_U64, $recursor_answers_indom,
                PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
                "pdns.recursor.answers",
                'counts the number of queries answered within X miliseconds', '');
$pmda->add_metric(pmda_pmid(1,2),PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT,
                pmda_units(0,0,0,0,0,0),
                "pdns.recursor.cache_entries",
                'the number of entries in the cache', '');
$pmda->add_metric(pmda_pmid(1,3),PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
                pmda_units(0,0,1,0,0,PM_COUNT_ONE),
                "pdns.recursor.cache_hits",
                'counts the number of cache hits since starting', '');
$pmda->add_metric(pmda_pmid(1,4),PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
                pmda_units(0,0,1,0,0,PM_COUNT_ONE),
                "pdns.recursor.cache_misses",
                'counts the number of cache misses since starting', '');
$pmda->add_metric(pmda_pmid(1,5),PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT,
                pmda_units(0,0,0,0,0,0),
                "pdns.recursor.chain_resends",
                'number of queries chained to existing outstanding query', '');
$pmda->add_metric(pmda_pmid(1,6),PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
                pmda_units(0,0,1,0,0,PM_COUNT_ONE),
                "pdns.recursor.client_parse_errors",
                'counts number of client packets that could not be parsed', '');
$pmda->add_metric(pmda_pmid(1,7),PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT,
                pmda_units(0,0,0,0,0,0),
                "pdns.recursor.concurrent_queries",
                'shows the number of MThreads currently running', '');
$pmda->add_metric(pmda_pmid(1,8),PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
                pmda_units(0,0,1,0,0,PM_COUNT_ONE),
                "pdns.recursor.dlg_only_drops",
                'number of records dropped because of delegation only setting', '');
$pmda->add_metric(pmda_pmid(1,9),PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
                pmda_units(0,0,1,0,0,PM_COUNT_ONE),
                "pdns.recursor.dont_outqueries",
                'number of outgoing queries dropped because of "dont-query" setting', '');
$pmda->add_metric(pmda_pmid(1,10),PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
                pmda_units(0,0,1,0,0,PM_COUNT_ONE),
                "pdns.recursor.ipv6_outqueries",
                'number of outgoing queries over IPv6', '');
$pmda->add_metric(pmda_pmid(1,11),PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT,
                pmda_units(0,0,0,0,0,0),
                "pdns.recursor.negcache_entries",
                'shows the number of entries in the Negative answer cache', '');
$pmda->add_metric(pmda_pmid(1,12),PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
                pmda_units(0,0,1,0,0,PM_COUNT_ONE),
                "pdns.recursor.noerror_answers",
                'counts the number of times it answered NOERROR since starting', '');
$pmda->add_metric(pmda_pmid(1,13),PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT,
                pmda_units(0,0,0,0,0,0),
                "pdns.recursor.nsspeeds_entries",
                'shows the number of entries in the NS speeds map', '');
$pmda->add_metric(pmda_pmid(1,14),PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
                pmda_units(0,0,1,0,0,PM_COUNT_ONE),
                "pdns.recursor.nsset_invalidations",
                'number of times an nsset was dropped because it no longer worked', '');
$pmda->add_metric(pmda_pmid(1,15),PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
                pmda_units(0,0,1,0,0,PM_COUNT_ONE),
                "pdns.recursor.nxdomain_answers",
                'counts the number of times it answered NXDOMAIN since starting', '');
$pmda->add_metric(pmda_pmid(1,16),PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
                pmda_units(0,0,1,0,0,PM_COUNT_ONE),
                "pdns.recursor.outgoing_timeouts",
                'counts the number of timeouts on outgoing UDP queries since starting', '');
$pmda->add_metric(pmda_pmid(1,17),PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
                pmda_units(0,0,1,0,0,PM_COUNT_ONE),
                "pdns.recursor.over_capacity_drops",
                'Questions dropped because over maximum concurrent query limit', '');
$pmda->add_metric(pmda_pmid(1,18),PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT,
                pmda_units(0,0,0,0,0,0),
                "pdns.recursor.packetcache_entries",
                'Size of packet cache', '');
$pmda->add_metric(pmda_pmid(1,19),PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
                pmda_units(0,0,1,0,0,PM_COUNT_ONE),
                "pdns.recursor.packetcache_hits",
                'Packet cache hits', '');
$pmda->add_metric(pmda_pmid(1,20),PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
                pmda_units(0,0,1,0,0,PM_COUNT_ONE),
                "pdns.recursor.packetcache_misses",
                'Packet cache misses', '');
$pmda->add_metric(pmda_pmid(1,21),PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT,
                pmda_units(0,1,0,0,PM_TIME_USEC,0),
                "pdns.recursor.qa_latency",
                'shows the current latency average, in microseconds', '');
$pmda->add_metric(pmda_pmid(1,22),PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
                pmda_units(0,0,1,0,0,PM_COUNT_ONE),
                "pdns.recursor.questions",
                'counts all End-user initiated queries with the RD bit set', '');
$pmda->add_metric(pmda_pmid(1,23),PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
                pmda_units(0,0,1,0,0,PM_COUNT_ONE),
                "pdns.recursor.resource_limits",
                'counts number of queries that could not be performed because of resource limits', '');
$pmda->add_metric(pmda_pmid(1,24),PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
                pmda_units(0,0,1,0,0,PM_COUNT_ONE),
                "pdns.recursor.server_parse_errors",
                'counts number of server replied packets that could not be parsed', '');
$pmda->add_metric(pmda_pmid(1,25),PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
                pmda_units(0,0,1,0,0,PM_COUNT_ONE),
                "pdns.recursor.servfail_answers",
                'counts the number of times it answered SERVFAIL since starting', '');
$pmda->add_metric(pmda_pmid(1,26),PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
                pmda_units(0,0,1,0,0,PM_COUNT_ONE),
                "pdns.recursor.spoof_prevents",
                'number of times PowerDNS considered itself spoofed, and dropped the data', '');
$pmda->add_metric(pmda_pmid(1,27),PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
                pmda_units(0,1,0,0,PM_TIME_MSEC,0),
                "pdns.recursor.sys_msec",
                'number of CPU milliseconds spent in "system" mode', '');
$pmda->add_metric(pmda_pmid(1,28),PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
                pmda_units(0,0,1,0,0,PM_COUNT_ONE),
                "pdns.recursor.tcp_client_overflow",
                'number of times an IP address was denied TCP access because it already had too many connections', '');
$pmda->add_metric(pmda_pmid(1,29),PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
                pmda_units(0,0,1,0,0,PM_COUNT_ONE),
                "pdns.recursor.tcp_outqueries",
                'counts the number of outgoing TCP queries since starting', '');
$pmda->add_metric(pmda_pmid(1,30),PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
                pmda_units(0,0,1,0,0,PM_COUNT_ONE),
                "pdns.recursor.tcp_questions",
                'counts all incoming TCP queries (since starting)', '');
$pmda->add_metric(pmda_pmid(1,31),PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
                pmda_units(0,0,1,0,0,PM_COUNT_ONE),
                "pdns.recursor.throttled_out",
                'counts the number of throttled outgoing UDP queries since starting', '');
$pmda->add_metric(pmda_pmid(1,32),PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT,
                pmda_units(0,0,0,0,0,0),
                "pdns.recursor.throttle_entries",
                'shows the number of entries in the throttle map', '');
$pmda->add_metric(pmda_pmid(1,33),PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
                pmda_units(0,0,1,0,0,PM_COUNT_ONE),
                "pdns.recursor.unauthorized_tcp",
                'number of TCP questions denied because of allow-from restrictions', '');
$pmda->add_metric(pmda_pmid(1,34),PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
                pmda_units(0,0,1,0,0,PM_COUNT_ONE),
                "pdns.recursor.unauthorized_udp",
                'number of UDP questions denied because of allow-from restrictions', '');
$pmda->add_metric(pmda_pmid(1,35),PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
                pmda_units(0,0,1,0,0,PM_COUNT_ONE),
                "pdns.recursor.unexpected_packets",
                'number of answers from remote servers that were unexpected (might point to spoofing)', '');
$pmda->add_metric(pmda_pmid(1,36),PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
                pmda_units(0,0,1,0,0,PM_COUNT_ONE),
                "pdns.recursor.uptime",
                'number of seconds process has been running', '');
$pmda->add_metric(pmda_pmid(1,37),PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
                pmda_units(0,1,0,0,PM_TIME_MSEC,0),
                "pdns.recursor.user_msec",
                'number of CPU milliseconds spent in "user" mode', '');

$pmda->add_indom($recursor_answers_indom, \@recursor_answers_dom, '', '');
$pmda->set_fetch_callback(\&pdns_fetch_callback);
$pmda->set_fetch(\&pdns_fetch);
$pmda->run;

=pod

=head1 NAME

pmdapdns - PowerDNS performance metrics domain agent (PMDA)

=head1 DESCRIPTION

B<pmdapdns> is a Performance Metrics Domain Agent (PMDA) which exports
metric values from the PowerDNS authoritative daemon as well as the recursive
resolver.

=head1 INSTALLATION

If you want access to the names and values for the PowerDNS performance
metrics, do the following as root:

        # cd $PCP_PMDAS_DIR/pdns
        # ./Install

If you want to undo the installation, do the following as root:

        # cd $PCP_PMDAS_DIR/pdns
        # ./Remove

B<pmdapdns> is launched by pmcd(1) and should never be executed
directly.  The Install and Remove scripts notify pmcd(1) when
the agent is installed or removed.

=head1 FILES

=over

=item $PCP_PMDAS_DIR/pdns/Install

installation script for the B<pmdapdns> agent

=item $PCP_PMDAS_DIR/pdns/Remove

undo installation script for the B<pmdapdns> agent

=item $PCP_LOG_DIR/pmcd/pdns.log

default log file for error messages from B<pmdapdns>

=back

=head1 SEE ALSO

pmcd(1), pdns_control(8), and rec_control(1).
