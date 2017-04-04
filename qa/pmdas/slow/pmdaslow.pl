#
# Copyright (c) 2014 Ken McDonell.  All Rights Reserved.
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

use vars qw( $pmda $start_delay $fetch_delay );

sub slow_fetch_callback
{
    my ($cluster, $item, $inst) = @_;
    my $metric_name = pmda_pmid_name($cluster, $item);

    if ($fetch_delay > 0) {
	sleep($fetch_delay);
    }

    $pmda->log("slow_fetch_callback $metric_name $cluster:$item ($inst)\n");

    if ($inst != PM_IN_NULL)	{ return (PM_ERR_INST, 0); }
    if (!defined($metric_name))	{ return (PM_ERR_PMID, 0); }

    return (17, 1);
}

# Usage: pmdaslow [startdelay [fetchdelay]]
#
$start_delay = 0;
$start_delay = $ARGV[0] unless !defined($ARGV[0]);
$fetch_delay = 0;
$fetch_delay = $ARGV[1] unless !defined($ARGV[1]);

#debug# print 'start delay: ',$start_delay,' sec\n';
#debug# print 'fetch delay: ',$fetch_delay,' sec\n';

$pmda = PCP::PMDA->new('slow', 243);
$pmda->connect_pmcd unless $start_delay < 0;

if ($start_delay < 0) {
    sleep(-$start_delay);
}
if ($start_delay > 0) {
    sleep($start_delay);
}

$pmda->add_metric(pmda_pmid(0,0), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'slow.seventeen', '', '');

$pmda->set_fetch_callback(\&slow_fetch_callback);
$pmda->set_user('pcp');
$pmda->run;

=pod

=head1 NAME

pmdaslow - QA PMDA

=head1 DESCRIPTION

DO NOT INSTALL THIS PMDA.
