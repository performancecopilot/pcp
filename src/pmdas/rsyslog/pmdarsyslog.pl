#
# Copyright (c) 2011 Aconex.  All Rights Reserved.
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

my $pmda = PCP::PMDA->new('rsyslog', 107);
my $statsfile = '/var/log/pcp/rsyslog/stats';
my ($es_connfail, $es_submits, $es_failed, $es_success) = (0,0,0);
my ($ux_submitted, $ux_discarded, $ux_ratelimiters) = (0,0,0);
my ($interval, $lasttime) = (0,0);

my $queue_indom = 0;
my @queue_insts = ();
use vars qw(%queue_ids %queue_values);

# .* rsyslogd-pstats:
# imuxsock: submitted=37 ratelimit.discarded=0 ratelimit.numratelimiters=22 
# elasticsearch: connfail=0 submits=0 failed=0 success=0 
# [main Q]: size=1 enqueued=1436 full=0 maxqsize=3 

sub rsyslog_parser
{
    ( undef, $_ ) = @_;

    #$pmda->log("rsyslog_parser got line: $_");
    if (m|rsyslogd-pstats:|) {
	my $timenow = time;
	if ($lasttime != 0) {
	    if ($timenow > $lasttime) {
		$interval = $timenow - $lasttime;
		$lasttime = $timenow;
	    }
	} else {
	    $lasttime = $timenow;
	}
    }
    if (m|imuxsock: submitted=(\d+) ratelimit.discarded=(\d+) ratelimit.numratelimiters=(\d+)|) {
	($ux_submitted, $ux_discarded, $ux_ratelimiters) = ($1,$2,$3);
    }
    elsif (m|elasticsearch: connfail=(\d+) submits=(\d+) failed=(\d+) success=(\d+)|) {
	($es_connfail, $es_submits, $es_failed, $es_success) = ($1,$2,$3,$4);
    }
    elsif (m|stats: (.+): size=(\d+) enqueued=(\d+) full=(\d+) maxqsize=(\d+)|) {
	my ($qname, $qid) = ($1, undef);

	if (!defined($queue_ids{$qname})) {
	    $qid = @queue_insts / 2;
	    $queue_ids{$qname} = $qid;
	    push @queue_insts, ($qid, $qname);
	    $pmda->replace_indom($queue_indom, \@queue_insts);
	}
	$queue_values{$qname} = [ $2, $3, $4, $5 ];
    }
}

sub rsyslog_fetch_callback
{
    my ($cluster, $item, $inst) = @_;

    #$pmda->log("rsyslog_fetch_callback for PMID: $cluster.$item ($inst)");

    return (PM_ERR_AGAIN,0) unless ($interval != 0);

    if ($cluster == 0) {
	return (PM_ERR_INST, 0) unless ($inst == PM_IN_NULL);
	if ($item == 0) { return ($interval, 1); }
	if ($item == 1) { return ($ux_submitted, 1); }
	if ($item == 2)	{ return ($ux_discarded, 1); }
	if ($item == 3)	{ return ($ux_ratelimiters, 1); }
	if ($item == 8)	{ return ($es_connfail, 1); }
	if ($item == 9)	{ return ($es_submits, 1); }
	if ($item == 10){ return ($es_failed, 1); }
	if ($item == 11){ return ($es_success, 1); }
    }
    elsif ($cluster == 1) {	# queues
	return (PM_ERR_INST, 0) unless ($inst != PM_IN_NULL);
	return (PM_ERR_INST, 0) unless ($inst <= @queue_insts);
	my $qname = $queue_insts[$inst * 2 + 1];
	my $qvref = $queue_values{$qname};
	my @qvals;

	return (PM_ERR_INST, 0) unless defined ($qvref);
	@qvals = @$qvref;

	if ($item == 0) { return ($qvals[0], 1); }
	if ($item == 1)	{ return ($qvals[1], 1); }
	if ($item == 2)	{ return ($qvals[2], 1); }
	if ($item == 3) { return ($qvals[3], 1); }
    }
    return (PM_ERR_PMID, 0);
}

die "Cannot find a valid rsyslog statistics named pipe\n" unless -p $statsfile;

$pmda->add_metric(pmda_pmid(0,0), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT,
	pmda_units(0,1,0,0,PM_TIME_SEC,0), 'rsyslog.interval',
	'Time interval observed between samples', '');
$pmda->add_metric(pmda_pmid(0,1), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	pmda_units(0,0,1,0,0,PM_COUNT_ONE), 'rsyslog.imuxsock.submitted',
	'', '');
$pmda->add_metric(pmda_pmid(0,2), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	pmda_units(0,0,1,0,0,PM_COUNT_ONE), 'rsyslog.imuxsock.discarded',
	'', '');
$pmda->add_metric(pmda_pmid(0,3), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	pmda_units(0,0,0,0,0,0), 'rsyslog.imuxsock.numratelimiters',
	'', '');
$pmda->add_metric(pmda_pmid(0,8), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	pmda_units(0,0,1,0,0,PM_COUNT_ONE), 'rsyslog.elasticsearch.connfail',
	'Count of failed connections while attempting to send events', '');
$pmda->add_metric(pmda_pmid(0,9), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	pmda_units(0,0,1,0,0,PM_COUNT_ONE), 'rsyslog.elasticsearch.submits',
	'Count of valid submissions of events to elasticsearch indexer', '');
$pmda->add_metric(pmda_pmid(0,10), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	pmda_units(0,0,1,0,0,PM_COUNT_ONE), 'rsyslog.elasticsearch.failed',
	'Count of failed attempts to send events to elasticsearch',
	'This count is often a good indicator of malformed JSON messages');
$pmda->add_metric(pmda_pmid(0,11), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	pmda_units(0,0,1,0,0,PM_COUNT_ONE), 'rsyslog.elasticsearch.success',
	'Count of successfully acknowledged events from elasticsearch', '');

$pmda->add_metric(pmda_pmid(1,0), PM_TYPE_U64, $queue_indom, PM_SEM_INSTANT,
	pmda_units(0,0,0,0,0,0), 'rsyslog.queues.size',
	'Current queue depth for each rsyslog queue', '');
$pmda->add_metric(pmda_pmid(1,1), PM_TYPE_U64, $queue_indom, PM_SEM_COUNTER,
	pmda_units(0,0,1,0,0,PM_COUNT_ONE), 'rsyslog.queues.enqueued',
	'Cumumlative count of entries enqueued to individual queues', '');
$pmda->add_metric(pmda_pmid(1,2), PM_TYPE_U64, $queue_indom, PM_SEM_INSTANT,
	pmda_units(0,0,1,0,0,PM_COUNT_ONE), 'rsyslog.queues.full',
	'Cumulative count of occassions where a queue has been full', '');
$pmda->add_metric(pmda_pmid(1,3), PM_TYPE_U64, $queue_indom, PM_SEM_INSTANT,
	pmda_units(0,0,1,0,0,PM_COUNT_ONE), 'rsyslog.queues.maxsize',
	'Maximum depth reached by an individual queue', '');

$pmda->add_indom($queue_indom, \@queue_insts,
	'Instance domain exporting each rsyslog queue', '');

$pmda->add_tail($statsfile, \&rsyslog_parser, 0);
$pmda->set_fetch_callback(\&rsyslog_fetch_callback);
$pmda->run;

=pod

=head1 NAME

pmdarsyslog - rsyslog (reliable and extended syslog) PMDA

=head1 DESCRIPTION

B<pmdarsyslog> is a Performance Metrics Domain Agent (PMDA) which
exports metric values from the rsyslogd(8) server.
Further details about rsyslog can be found at http://www.rsyslog.com/.

=head1 INSTALLATION

If you want access to the names and values for the rsyslog performance
metrics, do the following as root:

	# cd $PCP_PMDAS_DIR/rsyslog
	# ./Install

If you want to undo the installation, do the following as root:

	# cd $PCP_PMDAS_DIR/rsyslog
	# ./Remove

B<pmdarsyslog> is launched by pmcd(1) and should never be executed
directly.  The Install and Remove scripts notify pmcd(1) when
the agent is installed or removed.

In order to use this agent, rsyslog stats gathering must be enabled.
This is done by adding the lines:

	$ModLoad impstats
	$PStatsInterval 5	# log every 5 seconds
	syslog.info		|/var/log/pcp/rsyslog/stats

to your rsyslog.conf(5) configuration file after installing the PMDA.
Take care to ensure the syslog.info messages do not get logged in any
other file, as this could unexpectedly fill your filesystem.  Syntax
useful for this is syslog.!=info for explicitly excluding these.

=head1 FILES

=over

=item /var/log/pcp/rsyslog/stats

named pipe containing statistics exported from rsyslog,
usually created by the PMDA Install script.

=item $PCP_PMDAS_DIR/rsyslog/Install

installation script for the B<pmdarsyslog> agent

=item $PCP_PMDAS_DIR/rsyslog/Remove

undo installation script for the B<pmdarsyslog> agent

=item $PCP_LOG_DIR/pmcd/rsyslog.log

default log file for error messages from B<pmdarsyslog>

=back

=head1 SEE ALSO

pmcd(1), rsyslog.conf(5), rsyslogd(8).
