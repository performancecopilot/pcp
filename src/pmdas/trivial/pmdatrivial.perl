#!/usr/bin/perl
#
# Copyright (c) 2012,2018 Red Hat.
# Copyright (c) 2008,2012 Aconex.  All Rights Reserved.
# Copyright (c) 2004 Silicon Graphics, Inc.  All Rights Reserved.
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

sub trivial_fetch_callback	# must return array of value,status
{
	my ($cluster, $item, $inst) = @_;

	if ($cluster == 0 && $item == 0) { return (time(), 1); }
	return (PM_ERR_PMID, 0);
}

$pmda = PCP::PMDA->new('trivial', 250);	# domain name and number
$pmda->connect_pmcd;

$pmda->add_metric(pmda_pmid(0,0),	# metric ID (PMID)
		PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, # type, instances,
		pmda_units(0, 1, 0, 0, PM_TIME_SEC, 0),     # semantics, units
		'trivial.time',		# metric name
		'time in seconds since 1 Jan 1970',	# short and long help
		'The time in seconds since the epoch (1st of January, 1970).');

$pmda->set_fetch_callback( \&trivial_fetch_callback );
$pmda->set_user('pcp');
$pmda->run;
