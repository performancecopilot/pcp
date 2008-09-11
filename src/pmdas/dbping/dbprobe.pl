#
# Copyright (c) 2008 Aconex.  All Rights Reserved.
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

use strict;
use warnings;
use DBI;
use PCP::PMDA qw(pmda_config);
use Time::HiRes qw(gettimeofday);

my $database = 'DBI:mysql:mysql';
my $username = 'dbmonitor';
my $password = 'dbmonitor';
my $response = 'SELECT 1';
my $delay = $ARGV[0];	# delay in seconds between database ping's
$delay = 60 unless defined($delay);

# Configuration files for overriding the above settings
for my $file (	'/etc/pcpdbi.conf',	# system defaults (lowest priority)
		pmda_config('PCP_PMDAS_DIR') . '/dbping/dbprobe.conf',
		'./dbprobe.conf' ) {	# current directory (high priority)
    eval `cat $file` unless ! -f $file;
}

use vars qw( $pmda $dbh $sth );

sub dbping {	# must return array of (response_time, status, time_stamp)
    my $before;

    if (!defined($dbh)) {	# reconnect if necessary
	$dbh = DBI->connect($database, $username, $password, undef) || return;
	$pmda->log("Connected to database.\n");
	undef $sth;
    }
    if (!defined($sth)) {	# prepare SQL statement once only
	$sth = $dbh->prepare($response) || return;
    }
    $before = gettimeofday;
    $sth->execute || return;
    while (my @row = $sth->fetchrow_array) {
	($sth->err) && return;
    }

    my $timenow = localtime;
    print "$timenow\t", gettimeofday - $before, "\n";
}

$dbh = DBI->connect($database, $username, $password, undef) ||
		    $pmda->log("Failed initial connect: $dbh->errstr\n");

$| = 1;	# IMPORTANT! Enables auto-flush, for piping hot pipes.
for (;;) {
    dbping;
    sleep($delay);
}

__END__

=head1 NAME
dbprobe - database response time and availability information

=head1 SYNOPSIS

=head1 DESCRIPTION

blah, blah B<dbprobe> blah, blah...

=head1 SEE ALSO

L<pmdadbping> - the database "ping" performance metrics domain agent
L<PMDA> - the Performance Metric Domain Agent documentation

=cut
