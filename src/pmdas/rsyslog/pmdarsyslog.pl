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
my ($submitted, $discarded, $ratelimiters, $qsize, $enqueued, $qfull, $maxqsize)
	= (0,0,0,0,0,0,0);
my ($interval, $lasttime) = (0,0);

# .* rsyslogd-pstats:
# imuxsock: submitted=37 ratelimit.discarded=0 ratelimit.numratelimiters=22 
# rsyslogd-pstats: main Q: size=1 enqueued=1436 full=0 maxqsize=3 

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
	( $submitted, $discarded, $ratelimiters ) = ( $1, $2, $3 );
    }
    elsif (m|main Q: size=(\d+) enqueued=(\d+) full=(\d+) maxqsize=(\d+)|) {
	( $qsize, $enqueued, $qfull, $maxqsize ) = ( $1, $2, $3, $4 );
    }
}

sub rsyslog_fetch_callback
{
    my ($cluster, $item, $inst) = @_;

    #$pmda->log("rsyslog_fetch_callback for PMID: $cluster.$item ($inst)");
    if ($inst != PM_IN_NULL)	{ return (PM_ERR_INST, 0); }
    if ($cluster != 0)		{ return (PM_ERR_PMID, 0); }

    return (PM_ERR_AGAIN,0) unless ($interval != 0);

    if ($item == 0)	{ return ($interval, 1); }
    elsif ($item == 1)	{ return ($submitted, 1); }
    elsif ($item == 2)	{ return ($discarded, 1); }
    elsif ($item == 3)	{ return ($ratelimiters, 1); }
    elsif ($item == 4)	{ return ($qsize, 1); }
    elsif ($item == 5)	{ return ($enqueued, 1); }
    elsif ($item == 6)	{ return ($qfull, 1); }
    elsif ($item == 7)	{ return ($maxqsize, 1); }
    return (PM_ERR_PMID, 0);
}

die "Cannot find a valid rsyslog statistics named pipe\n" unless -p $statsfile;

$pmda->add_metric(pmda_pmid(0,0), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT,
	pmda_units(0,1,0,0,PM_TIME_SEC,0), 'rsyslog.interval',
	'Time interval observed between samples', '');
$pmda->add_metric(pmda_pmid(0,1), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
	pmda_units(0,0,1,0,0,PM_COUNT_ONE), 'rsyslog.imuxsock.submitted',
	'', '');
$pmda->add_metric(pmda_pmid(0,2), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
	pmda_units(0,0,1,0,0,PM_COUNT_ONE), 'rsyslog.imuxsock.discarded',
	'', '');
$pmda->add_metric(pmda_pmid(0,3), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT,
	pmda_units(0,0,0,0,0,0), 'rsyslog.imuxsock.numratelimiters',
	'', '');
$pmda->add_metric(pmda_pmid(0,4), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT,
	pmda_units(0,0,0,0,0,0), 'rsyslog.mainqueue.size',
	'', '');
$pmda->add_metric(pmda_pmid(0,5), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
	pmda_units(0,0,1,0,0,PM_COUNT_ONE), 'rsyslog.mainqueue.enqueued',
	'', '');
$pmda->add_metric(pmda_pmid(0,6), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT,
	pmda_units(0,0,1,0,0,PM_COUNT_ONE), 'rsyslog.mainqueue.full',
	'', '');
$pmda->add_metric(pmda_pmid(0,7), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT,
	pmda_units(0,0,1,0,0,PM_COUNT_ONE), 'rsyslog.mainqueue.maxsize',
	'', '');

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
