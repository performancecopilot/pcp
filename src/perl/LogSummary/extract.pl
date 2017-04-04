#!/usr/bin/env perl

use strict;
use warnings;
use PCP::LogSummary;

my $archive = 't/db/20081125';
my @metrics = ( 'kernel.all.cpu.user', 'kernel.all.cpu.idle',
		'kernel.all.cpu.intr', 'kernel.all.cpu.sys' );
my $results = PCP::LogSummary->new($archive, \@metrics, '@09:00', '@17:00');
#my $results = PCP::LogSummary->new($archive, \@metrics);

foreach my $metric ( keys %$results ) {
    my $summary = $$results{$metric};
    print "metric=", $metric, "\n";
    print "  average=", $$summary{'average'}, "\n";
    print "  samples=", $$summary{'samples'}, "\n";
}
