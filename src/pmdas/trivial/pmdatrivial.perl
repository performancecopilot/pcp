#!/usr/bin/perl
#
# Copyright (c) 2012 Red Hat.
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
my ( $now_indom ) = ( 1 );

sub simple_fetch_callback	# must return array of value,status
{
	my ($cluster, $item, $inst) = @_;

	if ($cluster == 0 && $item == 0) { return (time(), 1); }
	return (PM_ERR_PMID, 0);
}


$pmda = PCP::PMDA->new('trivial', 250);
$pmda->connect_pmcd;

$pmda->add_metric(pmda_pmid(0,0), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'trivial.time', '', '');

$now_indom = $pmda->add_indom($now_indom, {}, '', ''); # initialized on-the-fly
$pmda->set_fetch_callback( \&simple_fetch_callback );

$pmda->set_user('pcp');
$pmda->run;
