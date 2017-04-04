#
# Copyright (c) 2012-2013 Red Hat.
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

use vars qw( $pmda %metrics );

#
# This is the main workhorse routine, both value extraction and
# namespace population is under-pinned by this.  The approach we
# use here is to extract profile output, construct a hash (keyed
# by metric ID), containing name and value pairs (array refs).
#
sub samba_fetch
{
    my $item = 0;
    my $cluster = 0;
    my $prefix = '';
    my $generated_cluster = 20; # start well above hard-coded ones

    # work around smbstatus / libpopt adverse reaction to these variables 
    delete $ENV{'POSIXLY_CORRECT'};
    delete $ENV{'POSIX_ME_HARDER'};

    my $smbstats = "smbstatus --profile";
    open(STATS, "$smbstats |") ||
	$pmda->err("pmdasamba failed to open $smbstats pipe: $!");

    while (<STATS>) {
	if (m/^\*\*\*\*\s+(\w+[^*]*)\**$/) {
	    my $heading = $1;
	    $heading =~ s/ +$//g;
	    $item = 0;
	    if ($heading eq 'System Calls') {
		$cluster = 1; $prefix = 'syscalls';
	    } elsif ($heading eq 'Stat Cache') {
		$cluster = 2; $prefix = 'statcache';
	    } elsif ($heading eq 'Write Cache') {
		$cluster = 3; $prefix = 'writecache';
	    } elsif ($heading eq 'SMB Calls') {
		$cluster = 4; $prefix = 'smb';
	    } elsif ($heading eq 'Pathworks Calls') {
		$cluster = 5; $prefix = 'pathworks';
	    } elsif ($heading eq 'Trans2 Calls') {
		$cluster = 6; $prefix = 'trans2';
	    } elsif ($heading eq 'NT Transact Calls') {
		$cluster = 7; $prefix = 'NTtransact';
	    } elsif ($heading eq 'ACL Calls') {
		$cluster = 8; $prefix = 'acl';
	    } elsif ($heading eq 'NMBD Calls') {
		$cluster = 9; $prefix = 'nmb';
	    } else {
                # samba 4.1 renames several clusters of statistics.
                # Let's generate cluster names instead of hard-coding them.
                $cluster = $generated_cluster++;
                $prefix = $heading;
                $prefix =~ s/ /_/g;
                $prefix =~ tr/A-Z/a-z/;
	    }
	    # $pmda->log("metric cluster: $cluster = $prefix");
	}
	# we've found a real name/value pair, work out PMID and hash it
	elsif (m/^([\[\]\w]+):\s+(\d+)$/) {
	    my @metric = ( $1, $2 );
	    my $pmid;

            $metric[0] =~ tr/\[\]/_/d;

	    if ($cluster == 0) {
		$metric[0] = "samba.$metric[0]";
	    } else {
	        $metric[0] = "samba.$prefix.$metric[0]";
	    }
	    $pmid = pmda_pmid($cluster,$item++);
	    $metrics{$pmid} = \@metric;
	    # $pmda->log("metric: $metric[0], ID = $pmid, value = $metric[1]");
	}
        else {
	    $pmda->log("pmdasamba failed to parse line $_");
        }
    }
    close STATS;
}

sub samba_fetch_callback
{
    my ($cluster, $item, $inst) = @_;
    my $pmid = pmda_pmid($cluster, $item);
    my $value;

#    $pmda->log("samba_fetch_callback $metric_name $cluster:$item ($inst)\n");

    if ($inst != PM_IN_NULL)	{ return (PM_ERR_INST, 0); }

    # hash lookup based on PMID, value is $metrics{$pmid}[1]
    $value = $metrics{$pmid};
    if (!defined($value))	{ return (PM_ERR_APPVERSION, 0); }
    return ($value->[1], 1);
}

$pmda = PCP::PMDA->new('samba', 76);

samba_fetch();	# extract names and values into %metrics, keyed on PMIDs

# hash iterate, keys are PMIDs, names and values are in @metrics{$pmid}.
foreach my $pmid (sort(keys %metrics)) {
    my $name = $metrics{$pmid}[0];
    if ($name eq 'samba.writecache.num_write_caches' ||
	$name eq 'samba.writecache.allocated_caches') {
	$pmda->add_metric($pmid, PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT,
			pmda_units(0,0,1,0,0,PM_COUNT_ONE), $name, '', '');
    } elsif ($name =~ /_time$/) {
	$pmda->add_metric($pmid, PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
			pmda_units(0,1,0,0,PM_TIME_USEC,0), $name, '', '');
    } else {
	$pmda->add_metric($pmid, PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
			pmda_units(0,0,1,0,0,PM_COUNT_ONE), $name, '', '');
    }
    # $pmda->log("pmdasamba added metric $name\n");
}
# close STATS;

$pmda->set_fetch(\&samba_fetch);
$pmda->set_fetch_callback(\&samba_fetch_callback);
# NB: needs to run as root, as smb usually does
$pmda->run;

=pod

=head1 NAME

pmdasamba - Samba performance metrics domain agent (PMDA)

=head1 DESCRIPTION

B<pmdasamba> is a Performance Metrics Domain Agent (PMDA) which exports
metric values from Samba, a Windows SMB/CIFS server for UNIX.

In order for values to be made available by this PMDA, Samba must have
been built with profiling support (WITH_PROFILE in "smbd -b" output).
This PMDA dynamically enumerates much of its metric hierarchy, based on
the contents of "smbstatus --profile".

When the agent is installed (see below), the Install script will attempt
to enable Samba statistics gathering, using "smbcontrol --profile".

=head1 INSTALLATION

If you want access to the names and values for the samba performance
metrics, do the following as root:

	# cd $PCP_PMDAS_DIR/samba
	# ./Install

If you want to undo the installation, do the following as root:

	# cd $PCP_PMDAS_DIR/samba
	# ./Remove

B<pmdasamba> is launched by pmcd(1) and should never be executed
directly.  The Install and Remove scripts notify pmcd(1) when
the agent is installed or removed.

=head1 FILES

=over

=item $PCP_PMDAS_DIR/samba/Install

installation script for the B<pmdasamba> agent

=item $PCP_PMDAS_DIR/samba/Remove

undo installation script for the B<pmdasamba> agent

=item $PCP_LOG_DIR/pmcd/samba.log

default log file for error messages from B<pmdasamba>

=back

=head1 SEE ALSO

pmcd(1), samba(7) and smbd(8).
