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

use strict;
use warnings;
use PCP::PMDA;

my $pmda = PCP::PMDA->new('named', 100);
my @paths = ( '/var/named/chroot/var/named/data', '/var/named/data' );
my $filename = 'named_stats.txt';
my $statscmd = 'rdnc stats';	# writes to $paths/$filename
my ( $statsdir, $statsfile );
my $interval = 10;		# time (in seconds) between runs of $statscmd
my %values;			# hash with all values mapped to metric names

sub named_update
{
    #$pmda->log('Updating values in named statistics file');
    system 'rndc', 'stats';
}

# Parser formats (PMDA internal numbering scheme):
#  0: not yet known what ondisk format is
#  1: bind 8,9.[0-4]   (<=rhel5.5)
#  2: bind 9.5+        (>=rhel5.6)
my $version = 0;

sub named_parser
{
    ( undef, $_ ) = @_;

    #$pmda->log("named_parser got line: $_");

    if (m|^\+\+\+ Statistics Dump \+\+\+ \(\d+\)$|) {
	$version = 1;	# all observed formats have this
    } elsif (m|^\+\+ Incoming Requests \+\+$|) {
	$version = 2;	# but, new format has this also.
    }

    if ($version == 2) {
	if      (m|^\s+(\d+) responses sent$|) {
	    $values{'named.nameserver.responses.sent'} = $1;
	} elsif (m|^\s+(\d+) IPv4 requests received$|) {
	    $values{'named.nameserver.requests.IPv4'} = $1;
	} elsif (m|^\s+(\d+) queries resulted in successful answer$|) {
	    $values{'named.nameserver.queries.successful'} = $1;
	} elsif (m|^\s+(\d+) queries resulted in authoritative answer$|) {
	    $values{'named.nameserver.queries.authoritative'} = $1;
	} elsif (m|^\s+(\d+) queries resulted in non authoritative answer$|) {
	    $values{'named.nameserver.queries.non_authoritative'} = $1;
	} elsif (m|^\s+(\d+) queries resulted in nxrrset$|) {
	    $values{'named.nameserver.queries.nxrrset'} = $1;
	} elsif (m|^\s+(\d+) queries resulted in NXDOMAIN$|) {
	    $values{'named.nameserver.queries.nxdomain'} = $1;
	} elsif (m|^\s+(\d+) queries caused recursion$|) {
	    $values{'named.nameserver.queries.recursion'} = $1;
	}
    }
    elsif ($version == 1) {
	if    (m|^success (\d+)$|)	{ $values{'named.success'} = $1; }
	elsif (m|^referral (\d+)$|)	{ $values{'named.referral'} = $1; }
	elsif (m|^nxrrset (\d+)$|)	{ $values{'named.nxrrset'} = $1; }
	elsif (m|^nxdomain (\d+)$|)	{ $values{'named.nxdomain'} = $1; }
	elsif (m|^recursion (\d+)$|)	{ $values{'named.recursion'} = $1; }
	elsif (m|^failure (\d+)$|)	{ $values{'named.failure'} = $1; }
    }
}

sub named_metrics
{
    my $id = 0;

    open STATS, $statsfile || die "Cannot open statistics file: $statsfile\n";
    while (<STATS>) { named_parser(undef, $_); }
    close STATS;

    $pmda->add_metric(pmda_pmid(0,$id++), PM_TYPE_U32, PM_INDOM_NULL,
		PM_SEM_INSTANT, pmda_units(0,1,0,0,PM_TIME_SEC,0),
		'named.interval',
		'Time interval between invocations of rndc stats command', '');
    foreach my $key (sort keys %values) {
	$pmda->add_metric(pmda_pmid(0,$id++), PM_TYPE_U32, PM_INDOM_NULL,
		PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		$key, '', '');
	#$pmda->log("named_metrics adding $key metric");
    }
}

sub named_fetch_callback
{
    my ($cluster, $item, $inst) = @_;

    if ($inst != PM_IN_NULL)	{ return (PM_ERR_INST, 0); }
    if ($cluster != 0)		{ return (PM_ERR_PMID, 0); }
    if ($item == 0)		{ return ($interval, 1); }

    my $metric_name = pmda_pmid_name($cluster, $item);
    my $value = $values{$metric_name};
    #$pmda->log("named_fetch_callback for $metric_name: $cluster.$item");

    return ($value, 1) if (defined($value));
    return (PM_ERR_PMID, 0);
}

# PMDA starts here
foreach $statsdir ( @paths ) {
    $statsfile = $statsdir . '/' . $filename;
    last if ( -f $statsfile );
}
die "Cannot find a valid named statistics file\n" unless -f $statsfile;
named_update();		# push some values into the statistics file

$pmda->set_fetch_callback(\&named_fetch_callback);
$pmda->add_tail($statsfile, \&named_parser, 0);
$pmda->add_timer($interval, \&named_update, 0);
$pmda->set_user('named');

named_metrics();	# fetch/parse the stats file, create metrics
$pmda->run;

=pod

=head1 NAME

pmdanamed - BIND (named) PMDA

=head1 DESCRIPTION

B<pmdanamed> is a Performance Metrics Domain Agent (PMDA) which
exports metric values from the BIND DNS server.
Further details on BIND can be found at http://isc.org/.

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

=item /var/named/data/named_stats.txt

statistics file showing values exported from named

=item /var/named/chroot/var/named/data/named_stats.txt

chroot variant of statistics file showing values exported from named

=item $PCP_PMDAS_DIR/named/Install

installation script for the B<pmdanamed> agent

=item $PCP_PMDAS_DIR/named/Remove

undo installation script for the B<pmdanamed> agent

=item $PCP_LOG_DIR/pmcd/named.log

default log file for error messages from B<pmdanamed>

=back

=head1 SEE ALSO

pmcd(1), named.conf(5), named(8).
