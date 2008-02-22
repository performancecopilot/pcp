#!/usr/bin/perl -w
# Simple, configurable PMDA
# $Id: pmdasimple.pl,v 1.2 2004/08/24 01:01:58 kenmcd Exp $
#
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
# You should have received a copy of the GNU General Public License along
# with this program; if not, write to the Free Software Foundation, Inc.,
# 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
# 
# Contact information: Silicon Graphics, Inc., 1500 Crittenden Lane,
# Mountain View, CA 94043, USA, or: http://www.sgi.com
# 

use strict;
use PCP::PMDA;

use vars qw( $pmda $now $red $green $blue $user $system );
my ( $numfetch, $oldfetch ) = ( 0, -1 );
my ( $color_indom, $now_indom ) = ( 0, 1 );
my ( $red, $green, $blue ) = ( 0, 100, 200 );

# simple.now instance domain stuff...
my $simple_config = '/var/lib/pcp/pmdas/simple/simple.conf';
# timeslice array format: value, instance id, instance name
my @timeslices = ( [0, 1, 'sec'], [0, 60, 'min'], [0, 3600, 'hour'] );
my $file_change = 0;
my $file_error = 0;


# ---custom callbacks follow---

sub simple_instance {	# called once per ``instance request'' pdu
    &simple_timenow_check;
}

sub simple_fetch {	# called once per ``fetch'' pdu, before callbacks
    $numfetch++;
    &simple_timenow_check;
    ($timeslices[0]->[0], $timeslices[1]->[0], $timeslices[2]->[0],
	    undef,undef,undef,undef,undef) = localtime;
}

sub simple_fetch_callback {	# must return array of value,status
    my ($cluster, $item, $inst) = @_;

    return (PM_ERR_INST, 0) unless ( $inst == -1
				    || ($cluster == 0 && $item == 1)
				    || ($cluster == 2 && $item == 4) );
    if ($cluster == 0) {
	if ($item == 0)	{ return ($numfetch, 1); }
	elsif ($item == 1) {
	    if ($inst == 0)	{ return ($red = ($red+1) % 255, 1); }
	    elsif ($inst == 1)	{ return ($green = ($green+1) % 255, 1); }
	    elsif ($inst == 2)	{ return ($blue = ($blue+1) % 255, 1); }
	    else		{ return (PM_ERR_INST, 0); }
	} else		{ return (PM_ERR_PMID, 0); }
    }
    elsif ($cluster == 1) {
	if ($oldfetch < $numfetch) {	# get current values, if needed
	    ($user, $system, undef, undef) = times;
	    $oldfetch = $numfetch;
	}
	if ($item == 2)		{ return ($user, 1); }
	elsif ($item == 3)	{ return ($system, 1); }
	else			{ return (PM_ERR_PMID, 0); }
    }
    elsif ($cluster == 2 && $item == 4) {
	foreach (0..2) {
	    ($inst == $timeslices[$_]->[1]) && return ($timeslices[$_]->[0], 1);
	}
	return (PM_ERR_INST, 0);
    }
    return (PM_ERR_PMID, 0);
}

sub simple_store_callback {	# must return a single value (scalar context)
    my ($cluster, $item, $inst, $val) = @_;
    my $sts = 0;

    if ($cluster == 0) {
	if ($item == 0) {
	    if ($val < 0)	{ $val = 0; $sts = PM_ERR_SIGN; }
	    $numfetch = $val;
	}
	elsif ($item == 1) {
	    if ($val < 0)	{ $sts = PM_ERR_SIGN; $val = 0; }
	    elsif ($val > 255)	{ $sts = PM_ERR_CONV; $val = 255; }

	    if ($inst == 0)	{ $red = $val; }
	    elsif ($inst == 1)	{ $green = $val; }
	    elsif ($inst == 2)	{ $blue = $val; }
	    else		{ $sts = PM_ERR_INST; }
	}
	else	{ $sts = PM_ERR_PMID; }
	return $sts;
    }
    elsif ( ($cluster == 1 && ($item == 2 || $item == 3))
	|| ($cluster == 2 && $item == 4) ) {
	return PM_ERR_PERMISSION;
    }
    return PM_ERR_PMID;
}

# ---end of custom routines---

# ---local routines follow---

sub simple_timenow_check {
    my @statbuf;

    if ((@statbuf) = stat($simple_config)) {
	$file_error = 0;
	if ($file_change != $statbuf[9]) {
	    &simple_timenow_init;
	    $file_change = $statbuf[9];		# mtime field
	}
    }
    else {
	unless ($file_error == $!) {
	    print STDERR "stat failed on $simple_config: $!\n";
	    $file_error = $!;
	}
	$pmda->replace_indom( $now_indom, [] );
    }
}

sub simple_timenow_init {
    my ( $spec, $i );
    my @inst;

    if (open(CONFIG, $simple_config)) {
	$_ = <CONFIG>;
	chomp;		# avoid possible \n on last field
	foreach $spec (split(/,/)) {
	    foreach $i (0..2) {
		($spec eq $timeslices[$i]->[2]) && push @inst,
			$timeslices[$i]->[1], $timeslices[$i]->[2];
	    }
	}
	close CONFIG;
	$pmda->replace_indom( $now_indom, \@inst );
    }
    else {
	print STDERR "read failed on $simple_config: $!\n";
	$pmda->replace_indom( $now_indom, [] );
    }
}

# ---end of local routines---


$pmda = PCP::PMDA->new('pmdasimple', 253, 'simple.log', 'help');
$pmda->openlog;	        # send messages to ^^^^^^^^^^ from now on

# simple.numfetch
$pmda->add_metric( pmda_pmid(0,0), PM_TYPE_U32, PM_INDOM_NULL,
		PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0) );
# simple.color
$pmda->add_metric( pmda_pmid(0,1), PM_TYPE_32, $color_indom,
		PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0) );
# simple.time.user
$pmda->add_metric( pmda_pmid(1,2), PM_TYPE_DOUBLE, PM_INDOM_NULL,
		PM_SEM_COUNTER, pmda_units(0,1,0,0,PM_TIME_SEC,0) );
# simple.time.sys
$pmda->add_metric( pmda_pmid(1,3), PM_TYPE_DOUBLE, PM_INDOM_NULL,
		PM_SEM_COUNTER, pmda_units(0,1,0,0,PM_TIME_SEC,0) );
# simple.time.now
$pmda->add_metric( pmda_pmid(2,4), PM_TYPE_U32, $now_indom,
		PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0) );

$pmda->add_indom( $color_indom, [0 => 'red', 1 => 'green', 2 => 'blue'] );
$now = $pmda->add_indom( $now_indom, [] );	# initialized on-the-fly
&simple_timenow_check;

$pmda->set_fetch( \&simple_fetch );
$pmda->set_instance( \&simple_instance );
$pmda->set_fetch_callback( \&simple_fetch_callback );
$pmda->set_store_callback( \&simple_store_callback );

$pmda->run;
