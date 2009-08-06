#
# Copyright (c) 2009 Josef 'Jeff' Sipek <jeffpc@josefsipek.net>
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

my $cached = 0;
my %vals = ();

sub pdns_fetch_callback
{
    my ($cluster, $item, $inst) = @_;
    my $metric_name = pmda_pmid_name($cluster, $item);
    my ($path, $value, $fh);

    my $now = time;

    # $pmda->log("pdns_fetch_callback $metric_name $cluster:$item ($inst)\n");

    if ($inst != PM_IN_NULL)    { return (PM_ERR_INST, 0); }
    if (!defined($metric_name))    { return (PM_ERR_PMID, 0); }

    $metric_name =~ s/^pdns\.//;

    if ($now - $cached > 1.0) {
        # $pmda->log("pdns_fetch_callback update now:$now cached:$cached\n");

        open(PIPE, "$pdns_control list |") || return (PM_ERR_APPVERSION, 0);
        $_ = <PIPE>;
        close PIPE;

        $_ =~ s/-/_/g;
        $_ =~ s/,$//;
        for my $kv (split(/,/, $_)) {
            if ("$kv" eq "") {
                last;
            }

            my ($k, $v) = split(/=/, $kv);
            $vals{$k} = $v;
        }

        $cached = $now;
    }

    if (!defined($vals{$metric_name}))        { return (PM_ERR_APPVERSION, 0); }
    return ($vals{$metric_name}, 1);
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

$pmda->set_fetch_callback(\&pdns_fetch_callback);
$pmda->run;

=pod

=head1 NAME

pmdapdns - PowerDNS performance metrics domain agent (PMDA)

=head1 DESCRIPTION

B<pmdapdns> is a Performance Metrics Domain Agent (PMDA) which exports
metric values from the PowerDNS daemon.

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

pmcd(1) and pdns_control(8).
