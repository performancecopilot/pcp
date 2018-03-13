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

# May need to be root to read directory $kvm_path (/sys/kernel/debug/kvm)
# - hence we cannot default to using $pmda->set_user('pcp') below.
#
my $pmid = 0;
opendir(DIR, $kvm_path) || $pmda->err("pmdakvm failed to open $kvm_path: $!");
my @metrics = grep {
    unless (/^(\.|[0-9])/) {
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
