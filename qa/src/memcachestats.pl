#!/usr/bin/perl -w

#
# liberally borrowed from memcache-top at
# https://code.google.com/p/memcache-top/
#

use strict;
use IO::Socket;
use Getopt::Long;

my (@default_instances, @instances, $remote, $default_port);

# List of servers/ ports to query.
@default_instances = ( '127.0.0.1:11211' );

# Default port to connect to, if not specified
$default_port = "11211";

if (@ARGV) {
    GetOptions (
	'instances=s'	=> \@instances,
	'port=i'	=> \$default_port,
    );
    if (@instances) {
	@instances = split(/,/,join(',',@instances));
    } else {
	@instances = @default_instances;
    }
}
else {
    @instances = @default_instances;
}

foreach my $instance (@instances) {

    my ($server);

    my @split = split(/:/,$instance);
    unless ( $split[1] ) {
	$instance = $instance . ":" . $default_port;
    }

    $remote = IO::Socket::INET->new($instance);
    unless ( defined($remote) ) { 
	print "no cigar: $!\n";
	exit(1);
    }
    $remote->autoflush(1);

    my @commands = ("stats", "stats slabs", "stats items" );
    foreach my $command (@commands) {
	print $remote "$command\n";

        LINE: while ( defined ( my $line = <$remote> ) ) {
	    last LINE if ( $line =~ /^END/ );
	    chomp $line;
	    $line =~ s/^STAT //;
	    $line =~ s///;
	    print $line . "\n";
	    next LINE;
	}
    }

    close $remote;
}
