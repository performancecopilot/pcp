#!/usr/bin/env perl
#
# Copyright (c) 2016 Red Hat.
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
use DBI;

my $os_user = 'oracle';
my $username = 'SYSTEM';
my $password = 'manager';
my $host = 'localhost';
my $port = '1521';
my @sids = ( 'master' );
my $disable_filestat = 0;
my $disable_object_cache = 0;

# Configuration files for overriding the above settings
for my $file (	'/etc/pcpdbi.conf',	# system defaults (lowest priority)
		pmda_config('PCP_PMDAS_DIR') . '/oracle/oracle.conf',
		pmda_config('PCP_VAR_DIR') . '/config/oracle/oracle.conf',
		'./oracle.conf' ) {	# current directory (high priority)
    if ( -f $file ) {
	# print("Loading $file\n");
	eval `cat $file`;
    }
}

if (defined($ARGV[0]) && ($ARGV[0] eq '-c' || $ARGV[0] eq '--config')) {
    print("os_user=$os_user\n");
    print("username=$username\n");
    print("password=$password\n");
    print("host=$host\n");
    print("port=$port\n");
    # print("path: $ENV{LD_LIBRARY_PATH}\n");
    my $sidstr = '';
    foreach my $sid (@sids) {
	$sidstr .= $sid . ',';
    }
    chop($sidstr);
    print("sids=$sidstr\n");
    print("disable_filestat=$disable_filestat\n");
    print("disable_object_cache=$disable_object_cache\n");
    exit(0);
}

my $status = 0;
foreach my $sid (@sids) {
    print("Attempting Oracle login SID=$sid ... ");
    my $db = DBI->connect("dbi:Oracle:host=$host;port=$port;sid=$sid", $username, $password, { PrintError => 0});
    if (defined($db)) {
	$db->disconnect();
	print("ok.\n");
    } else {
	printf("failed!\n%s\n", $DBI::errstr);
	$status = 1;
    }
}
exit($status);
