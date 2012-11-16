#
# Copyright (c) 2012 Red Hat.
# Copyright (c) 2009 Aconex.  All Rights Reserved.
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
# TODO: BIND 9.5 has more, different metrics.  We should probably
# change this PMDA to be more like the KVM PMDA which configures
# its namespace and metrics on-the-fly at Install time.
#

use strict;
use warnings;
use PCP::PMDA;

my $pmda = PCP::PMDA->new('named', 100);
my @files = ( '/var/named/data', '/var/named/chroot/var/named/data' );

my ($success, $referral, $nxrrset, $nxdomain, $recursion, $failure) = (0,0,0,0,0,0);
my ($timestamp_now, $timestamp_then) = (0,0);

sub named_parser
{
    ( undef, $_ ) = @_;

    #$pmda->log("named_parser got line: $_");
    if (m|^\+\+\+ Statistics Dump \+\+\+ \((\d+)\)$|) {
	$timestamp_then = $timestamp_now;
	$timestamp_now = $1;
    }
    elsif (m|^success (\d+)$|)	{ $success = $1; }
    elsif (m|^referral (\d+)$|)	{ $referral = $1; }
    elsif (m|^nxrrset (\d+)$|)	{ $nxrrset = $1; }
    elsif (m|^nxdomain (\d+)$|)	{ $nxdomain = $1; }
    elsif (m|^recursion (\d+)$|){ $recursion = $1; }
    elsif (m|^failure (\d+)$|)	{ $failure = $1; }
}

sub named_fetch_callback
{
    my ($cluster, $item, $inst) = @_;

    #$pmda->log("named_fetch_callback for PMID: $cluster.$item ($inst)");
    if ($inst != PM_IN_NULL)	{ return (PM_ERR_INST, 0); }
    if ($cluster != 0)		{ return (PM_ERR_PMID, 0); }

    if ($item == 0)	{ return ($success, 1); }
    elsif ($item == 1)	{ return ($referral, 1); }
    elsif ($item == 2)	{ return ($nxrrset, 1); }
    elsif ($item == 3)	{ return ($nxdomain, 1); }
    elsif ($item == 4)	{ return ($recursion, 1); }
    elsif ($item == 5)	{ return ($failure, 1); }
    elsif ($item == 6)	{
	return (PM_ERR_AGAIN,0) unless ($timestamp_then != 0);
	return ($timestamp_now - $timestamp_then, 1);
    }
    return (PM_ERR_PMID, 0);
}

$pmda->add_metric(pmda_pmid(0,0), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
	pmda_units(0,0,1,0,0,PM_COUNT_ONE), 'named.success', '', '');
$pmda->add_metric(pmda_pmid(0,1), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
	pmda_units(0,0,1,0,0,PM_COUNT_ONE), 'named.referral', '', '');
$pmda->add_metric(pmda_pmid(0,2), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
	pmda_units(0,0,1,0,0,PM_COUNT_ONE), 'named.nxrrset', '', '');
$pmda->add_metric(pmda_pmid(0,3), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
	pmda_units(0,0,1,0,0,PM_COUNT_ONE), 'named.nxdomain', '', '');
$pmda->add_metric(pmda_pmid(0,4), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
	pmda_units(0,0,1,0,0,PM_COUNT_ONE), 'named.recursion', '', '');
$pmda->add_metric(pmda_pmid(0,5), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
	pmda_units(0,0,1,0,0,PM_COUNT_ONE), 'named.failure', '', '');
$pmda->add_metric(pmda_pmid(0,6), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT,
	pmda_units(0,0,0,0,0,0), 'named.interval', 'Time between samples', '');

my ( $statsdir, $statsfile );
foreach $statsdir ( @files ) {
    $statsfile = $statsdir . "/named_stats.txt";
    last if ( -f $statsfile );
}
die "Cannot find a valid named statistics file\n" unless -f $statsfile;

$pmda->add_tail($statsfile, \&named_parser, 0);
$pmda->set_fetch_callback(\&named_fetch_callback);
$pmda->set_user('pcp');
$pmda->run;

=pod

=head1 NAME

pmdanamed - BIND (named) PMDA

=head1 DESCRIPTION

B<pmdanamed> is a Performance Metrics Domain Agent (PMDA) which
exports metric values from the BIND DNS server.
Further details on BIND can be found at http://isc.org/.
Currently, only BIND version 9.4 is supported.

=head1 INSTALLATION

If you want access to the names and values for the named performance
metrics, do the following as root:

	# cd $PCP_PMDAS_DIR/named
	# ./Install

If you want to undo the installation, do the following as root:

	# cd $PCP_PMDAS_DIR/named
	# ./Remove

B<pmdanamed> is launched by pmcd(1) and should never be executed
directly.  The Install and Remove scripts notify pmcd(1) when
the agent is installed or removed.

=head1 FILES

=over

=item /var/named/data

statistics file showing values exported from named

=item /var/named/chroot/var/named/data

chroot variant of statistics file showing values exported from named

=item $PCP_PMDAS_DIR/named/Install

installation script for the B<pmdanamed> agent

=item $PCP_PMDAS_DIR/named/Remove

undo installation script for the B<pmdanamed> agent

=item $PCP_LOG_DIR/pmcd/named.log

default log file for error messages from B<pmdanamed>

=back

=head1 SEE ALSO

pmcd(1).
