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

my $pmda = PCP::PMDA->new('netfilter', 97);
my $procfs = '/proc/sys/net/ipv4/';

sub netfilter_fetch_callback
{
    my ($cluster, $item, $inst) = @_;
    my $metric_name = pmda_pmid_name($cluster, $item);
    my ($path, $name, $value, $fh, @vals);

    if ($inst != PM_IN_NULL)	{ return (PM_ERR_INST, 0); }
    if (!defined($metric_name))	{ return (PM_ERR_PMID, 0); }

    $metric_name =~ s/\./\//;
    $name = $procfs . $metric_name;
    open($fh, $name) || 	return (PM_ERR_APPVERSION, 0);
    $value = <$fh>;
    close $fh;
    chomp $value;

    return ($value, 1);
}

$pmda->add_metric(pmda_pmid(0,0), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT,
		pmda_units(0,0,0,0,0,0), 'netfilter.ip_conntrack_max', '', '');
$pmda->add_metric(pmda_pmid(0,1), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT,
		pmda_units(0,0,0,0,0,0), 'netfilter.ip_conntrack_count', '', '');

$pmda->set_fetch_callback(\&netfilter_fetch_callback);
$pmda->set_user('pcp');
$pmda->run;

=pod

=head1 NAME

pmdanetfilter - Linux netfilter IP connection tracking performance metrics domain agent (PMDA)

=head1 DESCRIPTION

B<pmdanetfilter> is a Performance Metrics Domain Agent (PMDA) which
exports metric values from IP connection tracking module in the Linux
kernel.

=head1 INSTALLATION

If you want access to the names and values for the netfilter performance
metrics, do the following as root:

	# cd $PCP_PMDAS_DIR/netfilter
	# ./Install

If you want to undo the installation, do the following as root:

	# cd $PCP_PMDAS_DIR/netfilter
	# ./Remove

B<pmdanetfilter> is launched by pmcd(1) and should never be executed
directly.  The Install and Remove scripts notify pmcd(1) when
the agent is installed or removed.

=head1 FILES

=over

=item $PCP_PMDAS_DIR/netfilter/Install

installation script for the B<pmdanetfilter> agent

=item $PCP_PMDAS_DIR/netfilter/Remove

undo installation script for the B<pmdanetfilter> agent

=item $PCP_LOG_DIR/pmcd/netfilter.log

default log file for error messages from B<pmdanetfilter>

=back

=head1 SEE ALSO

pmcd(1).
