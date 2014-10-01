#
# Copyright (c) 2004 Silicon Graphics, Inc.  All Rights Reserved.
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
use vars qw( $pmda $stamp $response $status $timestamp );

my $delay = $ARGV[0];	# delay in seconds between database ping's
$delay = 60 unless defined($delay);
my $dbprobe = pmda_config('PCP_PMDAS_DIR') . '/dbping/dbprobe.pl';
$dbprobe = "perl " . $dbprobe . " $delay";
my ( $stamp, $response, $status, $timestamp ) = ( 0, 0, 1, 0 );

sub dbping_probe_callback
{
    ( $_ ) = @_;
    ($stamp, $response) = split(/\t/);

    # $pmda->log("dbping_probe_callback: time=$stamp resp=$response\n");

    if (defined($stamp) && defined($response)) {
	$timestamp = $stamp;
	$status = 0;
    } else {
	$response = -1;
	$status = 1;	# bad result, keep old $timestamp
    }
}

sub dbping_fetch_callback	# must return array of value,status
{
    my ($cluster, $item, $inst) = @_;

    # $pmda->log("dbping_fetch_callback $cluster:$item ($inst)\n");

    return (PM_ERR_INST, 0) unless ($inst == PM_IN_NULL);

    if ($cluster == 0) {
	if ($item == 0)		{ return ($response, 1); }
	elsif ($item == 1)	{ return ($status, 1); }
    }
    elsif ($cluster == 1) {
	if ($item == 0)		{ return ($timestamp, 1); }
	elsif ($item == 1)	{ return ($delay, 1); }
    }
    return (PM_ERR_PMID, 0);
}

sub dbping_store_callback	# must return a single value (scalar context)
{
    my ($cluster, $item, $inst, $val) = @_;
    my $sts = 0;

    # $pmda->log("dbping_store_callback $cluster:$item ($inst) $val\n");

    if ($cluster == 1 && $item == 1) {
	$delay = $val;
	return 0;
    }
    elsif ( ($cluster == 0 && ($item == 0 || $item == 1))
	|| ($cluster == 1 && $item == 0) ) {
	return PM_ERR_PERMISSION;
    }
    return PM_ERR_PMID;
}

$pmda = PCP::PMDA->new('dbping', 244);

$pmda->add_metric(pmda_pmid(0,0), PM_TYPE_DOUBLE, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,1,0,0,PM_TIME_SEC,0),
		  'dbping.response_time',
		  'Length of time taken to access the database', '');
$pmda->add_metric(pmda_pmid(0,1), PM_TYPE_32, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'dbping.status',
		  'Success state of last attempt to ping the database', '');
$pmda->add_metric(pmda_pmid(1,0), PM_TYPE_STRING, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'dbping.control.timestamp',
		  'Time of last successful database ping', '');
$pmda->add_metric(pmda_pmid(1,1), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,1,0,0,PM_TIME_SEC,0),
		  'dbping.control.delay',
		  'Time to sleep between database ping attempts', '');
$pmda->set_fetch_callback( \&dbping_fetch_callback );
$pmda->set_store_callback( \&dbping_store_callback );
$pmda->add_pipe( $dbprobe, \&dbping_probe_callback, 0 );
$pmda->set_user('pcp');
$pmda->run;
