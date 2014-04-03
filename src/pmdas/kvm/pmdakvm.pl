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

use vars qw( $pmda );
my $kvm_path = '/sys/kernel/debug/kvm';

sub kvm_fetch_callback
{
    my ($cluster, $item, $inst) = @_;
    my $metric_name = pmda_pmid_name($cluster, $item);
    my ($path, $value, $fh);

    # $pmda->log("kvm_fetch_callback $metric_name $cluster:$item ($inst)\n");

    if ($inst != PM_IN_NULL)	{ return (PM_ERR_INST, 0); }
    if (!defined($metric_name))	{ return (PM_ERR_PMID, 0); }
    $path = $metric_name;
    $path =~ s/^kvm\./$kvm_path\//;
    open($fh, $path) || 	return (PM_ERR_APPVERSION, 0);
    $value = <$fh>;
    close $fh;

    if (!defined($value))	{ return (PM_ERR_APPVERSION, 0); }
    return ($value, 1);
}

$pmda = PCP::PMDA->new('kvm', 95);
$pmda->connect_pmcd;

# May need to be root to read the directory $kvm_path (/sys/kernel/debug/kvm)
# and so
# (a) do not use $pmda->set_user('pcp') below, and
# (b) need forced_restart=true in the Install script so pmcd is restarted
#     and we're running as root at this point (SIGHUP pmcd once it has
#     changed to user "pcp" is not going to work for PMDA installation)
#
my $pmid = 0;
opendir(DIR, $kvm_path) || $pmda->err("pmdakvm failed to open $kvm_path: $!");
my @metrics = grep {
    unless (/^\./) {
	$pmda->add_metric(pmda_pmid(0,$pmid++), PM_TYPE_U64, PM_INDOM_NULL,
		PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		"kvm.$_", '', '');
	# $pmda->log("pmdakvm added metric kvm.$_\n");
    }
} readdir(DIR);
closedir DIR;

$pmda->set_fetch_callback(\&kvm_fetch_callback);
# Careful with permissions - may need to be root to read /sys/kernel/debug/kvm
# see note above.
#$pmda->set_user('pcp') if -r $kvm_path;
$pmda->run;

=pod

=head1 NAME

pmdakvm - Linux virtualisation performance metrics domain agent (PMDA)

=head1 DESCRIPTION

B<pmdakvm> is a Performance Metrics Domain Agent (PMDA) which exports
metric values from the Linux KVM virtualisation subsystem.

Unlike many PMDAs it dynamically enumerates its metric hierarchy,
based entirely on the contents of /sys/kernel/debug/kvm.

=head1 INSTALLATION

If you want access to the names and values for the kvm performance
metrics, do the following as root:

	# cd $PCP_PMDAS_DIR/kvm
	# ./Install

If you want to undo the installation, do the following as root:

	# cd $PCP_PMDAS_DIR/kvm
	# ./Remove

B<pmdakvm> is launched by pmcd(1) and should never be executed
directly.  The Install and Remove scripts notify pmcd(1) when
the agent is installed or removed.

=head1 FILES

=over

=item $PCP_PMDAS_DIR/kvm/Install

installation script for the B<pmdakvm> agent

=item $PCP_PMDAS_DIR/kvm/Remove

undo installation script for the B<pmdakvm> agent

=item $PCP_LOG_DIR/pmcd/kvm.log

default log file for error messages from B<pmdakvm>

=back

=head1 SEE ALSO

pmcd(1) and kvm(1).
