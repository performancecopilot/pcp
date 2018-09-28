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

my $basename="lmsensors";
my %sensorvalues;	# sensorname -> sensorvalue (temp/fan)
my %sensorname;		# sensornumber -> sensorname
my $debug=0;

sub lmsensors_get
{
	my @result=qx( /usr/bin/sensors -u );

	my $devname0;	   # i.e. coretemp-isa-0000
	my $devname1;	   # i.e. Adapter: ISA adapter
	my $devname2;	   # i.e. Package id 0
	my $devname3;	   # i.e. temp1_input or fan1_input
	my $sensorvalue;	# i.e. 42, actual sensor value

	foreach (@result) {
		next if m/_crit/;   # we are not making these available
		next if m/_max/;	# we are not making these available
		next if m/^$/;
		chomp();	
		if ( $debug == 1 ) { 
			print "full line $_\n";
		};

		if ( m/^([a-z0-9-]*)/ && !m/:/ ) {
			$devname0=$1;
			next;
		}
		if ( m/^Adapter: (.*)/ ) {
			$devname1=$1;
			$devname1=~s/\s/-/g;
			next;
		}
		if ( m/^([a-zA-Z0-9-\s]*):/ ) {
			$devname2=$1;
			$devname2=~s/\s/-/g;
			next;
		}

		# now only lines with sensor name and value should be left
		m/([0-9a-zA-Z_]+)_input:\s([0-9]*)/;
		$devname3=$1;
		$sensorvalue=$2;

		$sensorvalues{"$devname0.$devname1.$devname2.$devname3"}=$sensorvalue;
	}
}

sub lmsensors_register
{
	my $i=0;
	my @devname;
	for my $sens (sort keys %sensorvalues) {
		@devname=split('\.',$sens);

		if ( $debug == 0 ) {
			$pmda->add_metric(pmda_pmid(0,$i),				# metric ID (PMID)
				PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT,	# type, instances,
			   	pmda_units(0, 0, 0, 0, 0, 0),				# semantics, units
			   	"$basename.$devname[0].$devname[3]",		# metric name
			 	"sensor values from 'sensors -u'",			# short help
				"sensor values from $sens");				# long help
		}
		else {
			print "registering metric: 0,$i $devname[0].$devname[3] $sens \n";
		};
		$sensorname{"$i"}=$sens;
		$i++;
	}
}

sub lmsensors_fetch_callback	# must return array of value,status
{
	my ($cluster, $item, $inst) = @_;

	return (PM_ERR_INST, 0) unless ( $inst == PM_IN_NULL );

	&lmsensors_get;

	if ($cluster == 0) {
		return ($sensorvalues{$sensorname{$item}}, 1);
	}
	return (PM_ERR_PMID, 0);
}


if (( $ARGV[0] ) && ( $ARGV[0] eq 'debug' )) {
	print "Setting debugmode.\n";
	$debug=1;
}

qx( /usr/bin/sensors -u ) ||
	die "'/usr/bin/sensors -u' failed. Either lm_sensors is not".
		"installed, or no sensor data is available.\n";
&lmsensors_get;

$pmda = PCP::PMDA->new('lmsensors', 74);
$pmda->connect_pmcd;

&lmsensors_register;

$pmda->set_fetch_callback( \&lmsensors_fetch_callback );

$pmda->set_user('pcp');
$pmda->run;
