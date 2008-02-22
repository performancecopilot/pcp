#!/usr/bin/perl -w
# $Id: dbprobe.pl,v 1.2 2004/08/24 01:01:58 kenmcd Exp $
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
use DBI;
use Time::HiRes qw(gettimeofday);	# Time::HiRes not default?

# ---this section needs user-customisation---
my $database	= 'DBI:mysql:localhost:3306';
my $username	= 'dbmonitor';
my $passwd	= 'dbmonitor';
my $select	= 'SELECT 1';
my $delay	= 60;	# delay in seconds between database ping's
# -----end of user-customisation section-----


use vars qw( $dbh, $sth );

sub dbping {	# must return array of (response_time, status, time_stamp)
    my $before;

    unless defined ($dbh) {	# reconnect if necessary
	$dbh = DBI->connect($database, $user, $passwd, undef) || return;
	$pmda->log("Connected to database.\n");
	undef $sth;
    }
    unless defined ($sth) {	# prepare SQL statement once only
	$sth = $dbh->prepare($select) || return;
    }
    $before = gettimeofday;
    $sth->execute || return;
    while (my @row = $sth->fetchrow_array) {
	($sth->err) && return;
    }

    print locatime, "\t", gettimeofday - $before, "\n";
}

$dbh = DBI->connect($database, $user, $passwd, undef) ||
		    $pmda->log("Failed initial connect: $dbh->errstr\n");

# XXX: TODO - merge with pmdadbping & convert to using the add_timer interface?

for (;;) {
    sleep($delay);
    dbping;
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
