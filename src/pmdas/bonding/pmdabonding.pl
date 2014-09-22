#
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

my $pmda = PCP::PMDA->new('bonding', 96);
my $sysfs = '/sys/class/net/';

sub bonding_interface_check
{
    my @interfaces = ();
    my $instanceid = 0;

    if (open(BONDS, $sysfs . 'bonding_masters')) {
	my @bonds = split / /, <BONDS>;
	foreach my $bond (@bonds) {
	    chomp $bond;
	    push @interfaces, $instanceid++, $bond;
	}
	close BONDS;
    }
    $pmda->replace_indom(0, \@interfaces);
}

sub bonding_fetch_callback
{
    my ($cluster, $item, $inst) = @_;
    my $metric_name = pmda_pmid_name($cluster, $item);
    my ($path, $name, $value, $fh, @vals);

    #$pmda->log("bonding_fetch_callback $metric_name $cluster:$item ($inst)\n");

    if ($inst == PM_IN_NULL)	{ return (PM_ERR_INST, 0); }
    if (!defined($metric_name))	{ return (PM_ERR_PMID, 0); }

    # special case: failures count from /proc (no sysfs equivalent)
    if ($item == 7) {
	$value = 0;
	$name = '/proc/net/bonding/' . 'bond' . $inst;
	open($fh, $name) || 	return (PM_ERR_APPVERSION, 0);
	while (<$fh>) {
	    if (m/^Link Failure Count: (\d+)$/) { $value += $1; }
	}
	close $fh;
    } else {
	$name = $metric_name;
	$path = $sysfs . 'bond' . $inst . '/bonding/';
	$name =~ s/^bonding\./$path/;

	# special case: mode contains two values (name and type)
	if ($item == 5) { $name =~ s/\.type$//; }
	if ($item == 6) { $name =~ s/\.name$//; }

	open($fh, $name) || 	return (PM_ERR_APPVERSION, 0);
	$value = <$fh>;
	close $fh;

	if (!defined($value))	{ return (PM_ERR_APPVERSION, 0); }
	if ($item == 5) { @vals = split / /, $value; $value = $vals[1]; }
	if ($item == 6) { @vals = split / /, $value; $value = $vals[0]; }
	chomp $value;
    }

    return ($value, 1);
}

$pmda->add_metric(pmda_pmid(0,0), PM_TYPE_STRING, 0, PM_SEM_INSTANT,
		pmda_units(0,0,0,0,0,0), "bonding.slaves", '', '');
$pmda->add_metric(pmda_pmid(0,1), PM_TYPE_STRING, 0, PM_SEM_INSTANT,
		pmda_units(0,0,0,0,0,0), "bonding.active_slave", '', '');
$pmda->add_metric(pmda_pmid(0,2), PM_TYPE_U32, 0, PM_SEM_INSTANT,
		pmda_units(0,0,0,0,0,0), "bonding.use_carrier", '', '');
$pmda->add_metric(pmda_pmid(0,3), PM_TYPE_U32, 0, PM_SEM_INSTANT,
		pmda_units(0,1,0,0,PM_TIME_MSEC,0), "bonding.updelay", '', '');
$pmda->add_metric(pmda_pmid(0,4), PM_TYPE_U32, 0, PM_SEM_INSTANT,
		pmda_units(0,1,0,0,PM_TIME_MSEC,0), "bonding.downdelay", '', '');
$pmda->add_metric(pmda_pmid(0,5), PM_TYPE_U32, 0, PM_SEM_INSTANT,
		pmda_units(0,0,0,0,0,0), "bonding.mode.type", '', '');
$pmda->add_metric(pmda_pmid(0,6), PM_TYPE_STRING, 0, PM_SEM_INSTANT,
		pmda_units(0,0,0,0,0,0), "bonding.mode.name", '', '');
$pmda->add_metric(pmda_pmid(0,7), PM_TYPE_U32, 0, PM_SEM_COUNTER,
		pmda_units(0,0,1,0,0,PM_COUNT_ONE), "bonding.failures", '', '');

$pmda->add_indom(0, [], '', '');

$pmda->set_fetch(\&bonding_interface_check);
$pmda->set_instance(\&bonding_interface_check);
$pmda->set_fetch_callback(\&bonding_fetch_callback);
$pmda->set_user('pcp');
$pmda->run;
