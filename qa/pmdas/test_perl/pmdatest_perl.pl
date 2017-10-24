#
# Copyright (c) 2014 Aconex
# Copyright (c) 2014-2015,2017 Red Hat.
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

my $refreshes = 0;
my ($some_indom, $other_indom, $loaded_indom) = (0, 1, 2);
my @some_indom_instances;
my $other_indom_instances;	# hashref
my $loaded_indom_instances;	# hashref
my $pmda = PCP::PMDA->new('test_perl', 242);

sub test_perl_update_status
{
    my ($cluster) = @_;

    $refreshes++;

    if ($cluster == 1) {
        my @empty_instances = ();
        my @one_instances = (0, 'instance0');
        my @two_instances = (0, 'instance0', 1, 'instance1');
        my @never_seen_instances = (99, 'instance99', 100, 'instance100', 101, 'instance101', 102, 'instance102');

        if ($refreshes < 1) {
            $pmda->replace_indom($some_indom, \@empty_instances);
        }
        elsif ($refreshes < 3) {
            $pmda->replace_indom($some_indom, \@one_instances);
        }
        elsif ($refreshes < 5) {
            $pmda->replace_indom($some_indom, \@two_instances);
        }
        elsif ($refreshes < 7) {
            $pmda->replace_indom($some_indom, \@never_seen_instances);
        }
        else {
            $pmda->replace_indom($some_indom, \@empty_instances);
	    $refreshes = 0;
        }
    }
    elsif ($cluster == 2) {
	if ($refreshes % 2 == 0) {
	    $other_indom_instances = {};
	} else {
	    $other_indom_instances = {'one' => 'ONE', 'two' => 'TWO'};
	}
	$pmda->replace_indom($other_indom, $other_indom_instances);
    }
    elsif ($cluster == 3) {
	$loaded_indom_instances = {'five' => 'FIVE', 'nine' => 'NINE'};
	$pmda->replace_indom($loaded_indom, $loaded_indom_instances);
    }
}

sub test_perl_store_callback
{
    my ($cluster, $item, $inst, $val) = @_;

    if ($cluster == 0 && $item == 0) {
	$refreshes = $val;
	return 0;
    }
    elsif ($cluster == 3 && $item == 0) {
	$pmda->load_indom($loaded_indom);
	return 0;
    }
    return PM_ERR_PERMISSION;
}

sub test_perl_fetch_callback
{
    my ($cluster, $item, $inst) = @_;

    if ($cluster == 0) {
        if ($item == 0) {
            return ($refreshes, 1);
        }
    }
    elsif ($cluster == 1) {
        if ($item == 0) {
            return (123, 1);
        }
    }
    elsif ($cluster == 2) {
        if ($item == 0) {
            my $value = pmda_inst_name($other_indom, $inst);
            return (PM_ERR_INST, 0) unless defined($value);
            return ($value, 1);
        }
    }
    elsif ($cluster == 3) {
        if ($item == 0) {
            my $value = pmda_inst_name($loaded_indom, $inst);
            return (PM_ERR_INST, 0) unless defined($value);
            return ($value, 1);
        }
    }
    return (PM_ERR_PMID, 0);
}


$pmda->add_metric(pmda_pmid(0,0), PM_TYPE_U32, PM_INDOM_NULL,
	PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
	'test_perl.some_value',	'', '');

$pmda->add_metric(pmda_pmid(1,0), PM_TYPE_U32, $some_indom,
	PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
	'test_perl.some_indom.some_value', '', '');

$pmda->add_metric(pmda_pmid(2,0), PM_TYPE_STRING, $other_indom,
	PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
	'test_perl.other_indom.some_value', '', '');

$pmda->add_metric(pmda_pmid(3,0), PM_TYPE_STRING, $loaded_indom,
	PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
	'test_perl.loaded_indom.some_value', '', '');

$some_indom = $pmda->add_indom($some_indom, \@some_indom_instances,
		'Instance domain exporting some instances', '');

$other_indom = $pmda->add_indom($other_indom, {},
		'Instance domain exporting other instances', '');

$loaded_indom = $pmda->add_indom($loaded_indom, {},
		'Instance domain exporting loaded instances', '');

$pmda->set_fetch_callback(\&test_perl_fetch_callback);
$pmda->set_store_callback(\&test_perl_store_callback);
$pmda->set_refresh(\&test_perl_update_status);
$pmda->set_user('pcp');
$pmda->run;
