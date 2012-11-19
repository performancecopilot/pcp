#
# Copyright (c) 2012 Red Hat.
# Copyright (c) 2008 Aconex.  All Rights Reserved.
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

use vars qw( $pmda $id );
my $probe_indom = 0;
my $probe_script = pmda_config('PCP_PMDAS_DIR') . '/systemtap/probes.stp';
my $probe_command = "/usr/bin/stap -m pmdasystemtap $probe_script";
my @probe_instances = ( 0 => 'sync', 1 => 'readdir' );
my ( $sync_count, $sync_pid, $sync_cmd ) = ( 0, 0, "(none)" );
my ( $readdir_count, $readdir_pid, $readdir_cmd ) = ( 0, 0, "(none)" );

sub systemtap_input_callback
{
    ( $id, $_ ) = @_;
    # $pmda->log($_);

    if (/^readdir: \((\d+)\) (.*)$/) {
	( $readdir_pid, $readdir_cmd ) = ( $1, $2 );
	$readdir_count++;
    }
    elsif (/^sync: \((\d+)\) (.*)$/) {
	( $sync_pid, $sync_cmd ) = ( $1, $2 );
	$sync_count++;
    }
}

sub systemtap_fetch_callback
{
    my ($cluster, $item, $inst) = @_;

    if ($inst < 0 || $inst > 1)	{ return (PM_ERR_INST, 0); }
    if ($cluster == 0) {
	if ($item == 0)	{
	    if ($inst == 0)	{ return ($sync_count, 1); }
	    else		{ return ($readdir_count, 1); }
	}
	elsif ($item == 1) {
	    if ($inst == 0)	{ return ($sync_pid, 1); }
	    else		{ return ($readdir_pid, 1); }
	}
	elsif ($item == 2) {
	    if ($inst == 0)	{ return ($sync_cmd, 1); }
	    else		{ return ($readdir_cmd, 1); }
	}
    }
    return (PM_ERR_PMID, 0);
}

$pmda = PCP::PMDA->new('systemtap', 88);

$pmda->add_metric(pmda_pmid(0,0), PM_TYPE_U32, $probe_indom,
		  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  'systemtap.probes.count',
		  'Number of times the probe has been observed', '');
$pmda->add_metric(pmda_pmid(0,1), PM_TYPE_32, $probe_indom,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'systemtap.probes.pid',
		  'The PID of the last process to pass the probe point', '');
$pmda->add_metric(pmda_pmid(0,2), PM_TYPE_STRING, $probe_indom,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'systemtap.probes.cmd',
		  'The name of the last process to pass the probe point', '');

$pmda->add_indom($probe_indom, \@probe_instances,
		 'Instance domain exporting each SystemTap probe', '');

$pmda->set_fetch_callback(\&systemtap_fetch_callback);
$pmda->add_pipe($probe_command, \&systemtap_input_callback, 0);
$pmda->set_user('pcp');
$pmda->run;

=pod

=head1 NAME

pmdasystemtap - Systemtap performance metrics domain agent (PMDA)

=head1 DESCRIPTION

B<pmdasystemtap> is a Performance Metrics Domain Agent (PMDA) which exports
metric values from the Linux Systemtap dynamic tracing toolkit.

This implementation uses the stap(1) tool, which is a front-end to
the Systemtap toolkit.

=head1 INSTALLATION

In order to access performance data exported by Systemtap from
with PCP, it is necessary to perform two configuration steps:

=over

=item 1.

Configure Systemtap probes, and verify them with stap(1).
These should be produced in a format that is easily parsed,
and then stored in the $PCP_PMDAS_DIR/systemtap/probes.stp
file.

=item 2.

Configure B<pmdasystemtap> to extract the values from the text
produced by stap.  Two example probes are implemented in the
default systemtap PMDA script - readdir and sync traces (see
$PCP_PMDAS_DIR/systemtap/pmdasystemtap.pl for details).

=back

	# cd $PCP_PMDAS_DIR/systemtap
	# [ edit probes.stp, test /usr/bin/stap probes.stp ]
	# [ edit pmdasystemtap.pl ]

Once this is setup, you can access the names and values for the
systemtap performance metrics by doing the following as root:

	# cd $PCP_PMDAS_DIR/systemtap
	# ./Install

If you want to undo the installation, do the following as root:

	# cd $PCP_PMDAS_DIR/systemtap
	# ./Remove

B<pmdasystemtap> is launched by pmcd(1) and should never be executed
directly.  The Install and Remove scripts notify pmcd(1) when
the agent is installed or removed.

=head1 FILES

=over

=item $PCP_PMDAS_DIR/systemtap/probes.stp

probe configuration file for stap(1), run by B<pmdasystemtap>

=item $PCP_PMDAS_DIR/systemtap/Install

installation script for the B<pmdasystemtap> agent

=item $PCP_PMDAS_DIR/systemtap/Remove

undo installation script for the B<pmdasystemtap> agent

=item $PCP_LOG_DIR/pmcd/systemtap.log

default log file for error messages from B<pmdasystemtap>

=back

=head1 SEE ALSO

pmcd(1) and stap(1).
