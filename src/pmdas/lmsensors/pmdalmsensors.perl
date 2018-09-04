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
use File::Slurp;

use vars qw( $pmda );

my $basename="lmsensors";
my %sensorvalues;	# sensorpath   -> sensorvalue (temp/fan)
my %sensorname;		# sensorpath   -> sensorname
my $debug=0;

sub lmsensors_detect
{
	my $basedir = '/sys/class/hwmon';
	my $monfile;
	my $fh;

	my $devname0;		   # i.e. coretemp
	my $devname1;		   # i.e. temp1 or fan1
	my $sensorvalue;		# i.e. 42, actual sensor value

	opendir(DIR, $basedir) or die $!;
	while (my $hwmon = readdir(DIR)) {
		next if ($hwmon=~m/^\./);
		# We only want dirs
		next unless (-d "$basedir/$hwmon");

		opendir(DIR2, "$basedir/$hwmon") or die $!;

		if ( -f "$basedir/$hwmon/name" ) {
			open($fh,'<',"$basedir/$hwmon/name") or die "Could not open $basedir/$hwmon/name";
			$devname0=<$fh>;
			chomp($devname0);
			close($fh);
		}
		else {
			die "file $basedir/$hwmon/name not found.";
		}

		while ($monfile = readdir(DIR2)) {
			next if ($monfile=~m/^\./);
			next unless ($monfile=~m/(.*)_input$/);
			$devname1=$1;

			open($fh,'<',"$basedir/$hwmon/$monfile") or die "Could not open $basedir/$hwmon/$monfile";
			$sensorvalue=<$fh>;
			if ( $devname1 =~ m/temp/ ) {   # is this a fan, or temperature which needs fixing?
				$sensorvalue/=1000;
			}
			close($fh);
				 
			# print "debug, should register: ${devname0}.${devname1}\n";
			$sensorvalues{"$basedir/$hwmon/$monfile"}=$sensorvalue;
			$sensorname{"$basedir/$hwmon/$monfile"}="$devname0.$devname1";
		}
		closedir(DIR2);
	}
	closedir(DIR);
}

sub lmsensors_read
{
	my $fh;
	my $sensorvalue;		# i.e. 42, actual sensor value

	for my $monfile (sort keys %sensorvalues) {
		# print "debug monfile: $monfile\n";
		$sensorvalue = read_file($monfile);
		if ( $monfile =~ m/temp/ ) {   # is this a fan, or temperature which needs fixing?
			$sensorvalue/=1000;
		}
		$sensorvalues{"$monfile"}=$sensorvalue;
	}
}

sub lmsensors_register
{
	my $i=0;
	my $sens;
	my @devname;

	for my $monfile (sort keys %sensorvalues) {
		$sens=$sensorname{$monfile};
		@devname=split('\.',$sens);

		if ( $debug == 0 ) {
			$pmda->add_metric(pmda_pmid(0,$i),			# metric ID (PMID)
				PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT,	# type, instances,
			   	pmda_units(0, 0, 0, 0, 0, 0),			# semantics, units
			   	"$basename.$devname[0].$devname[1]",	# metric name
			   	"sensor values from 'sensors -u'",		# short help
			   	"sensor values from $sens");			# long help
		}
		else {
			print "registering metric: 0,$i $devname[0].$devname[1] $sens \n";
		};
		$sensorname{"$i"}=$monfile;
		$i++;
	}
}

sub lmsensors_fetch_callback	# must return array of value,status
{
	my ($cluster, $item, $inst) = @_;

	return (PM_ERR_INST, 0) unless ( $inst == PM_IN_NULL );

	&lmsensors_read;

	if ($cluster == 0) {
		return ($sensorvalues{$sensorname{$item}}, 1);
	}
	return (PM_ERR_PMID, 0);
}


if (( $ARGV[0] ) && ( $ARGV[0] eq 'debug' )) {
	print "Setting debugmode.\n";
	$debug=1;
}

&lmsensors_detect;
&lmsensors_read;

$pmda = PCP::PMDA->new('lmsensors', 74);
$pmda->connect_pmcd;

&lmsensors_register;

$pmda->set_fetch_callback( \&lmsensors_fetch_callback );

$pmda->set_user('pcp');
$pmda->run;
