# Before `make install' is performed this script should be runnable with
# `make test'. After `make install' it should work as `perl test.pl'
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

######################### We start with some black magic to print on failure.

BEGIN { $| = 1; print "1..4\n"; }
END {print "not ok 1\n" unless $loaded;}
use PCP::PMDA;
$loaded = 1;
print "ok 1\n";

######################### End of black magic.

use vars qw( $cvalue $perlvalue $failed $cases );

`make -f Makefile cvalue`;

# verify constants are defined and match their C counterparts
# - assuming here that the header file matches our PMDA.pm
# (d==define)
# 
$failed = 0;
$cases = 0;
open(TEST, './cvalue d |') || die "cannot run test program 'cvalue'";
while (<TEST>) {
    /^(\w+)=(.*)$/;
    $cvalue = $2;
    $perlvalue = &$1;
    unless ($perlvalue == $cvalue) {
	print "$1: $perlvalue != $cvalue\n";
	$failed++;
    }
    $cases++;
}
close TEST;
if ($failed != 0) { print "not ok 2 (failed $failed of $cases cases)\n"; }
else { print "ok 2\n"; }

######################### 

# test data initialisation via the pmda_pmid macro (i==id)
# 
$failed = 0;
$cases = 0;
open(TEST, './cvalue i |') || die "cannot run test program 'cvalue'";
while (<TEST>) {
    /^PMDA_PMID: (\d+),(\d+) = (.*)$/;
    $cvalue = $3;
    $perlvalue = pmda_pmid($1, $2);
    unless ($perlvalue == $cvalue) {
	print "$1,$2: $perlvalue != $cvalue\n";
	$failed++;
    }
    $cases++;
}
close TEST;
if ($failed != 0) { print "not ok 3 (failed $failed of $cases cases)\n"; }
else { print "ok 3\n"; }

# test data initialisation via the pmda_units macro (u==units)
# 
$failed=0;
$cases = 0;
open(TEST, './cvalue u |') || die "cannot run test program 'cvalue'";
while (<TEST>) {
    /^pmUnits: {(\d+),}5(\d) = (.*)$/;
    $cvalue = $7;
    $perlvalue = pmda_units($1, $2, $3, $4, $5, $6);
    unless ($perlvalue == $cvalue) {
	print "$1,$2,$3,$4,$5,$6: $perlvalue != $cvalue\n";
	$failed++;
    }
    $cases++;
}
close TEST;
if ($failed != 0) { print "not ok 4 (failed $failed of $cases cases)\n"; }
else { print "ok 4\n"; }


######################### 
