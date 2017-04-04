# Before `make install' is performed this script should be runnable with
# `make test'. After `make install' it should work as `perl test.t'

#########################

# change 'tests => 1' to 'tests => last_test_to_print';

use Test;
BEGIN { plan tests => 15 };
use PCP::LogSummary;
ok(1); # If we made it this far, we're ok.

#########################

my $archive = 't/db/20081125';
my @metrics = ( 'kernel.all.cpu.user', 'kernel.all.cpu.sys' );
my $results = PCP::LogSummary->new($archive, \@metrics);
ok(1, defined($results), "log summarised");

foreach my $metric ( sort keys %$results ) {
    my $summary = $$results{$metric};
    #print("metric=", $metric, "\n");
    #print("  average=", $$summary{'average'}, "\n");
    #print("  samples=", $$summary{'samples'}, "\n");
    ok(1, ($$summary{'samples'} == 5758), "samples verified");
    ok(1, ($$summary{'average'} > 0),     "average lower bounds check");
    ok(1, ($$summary{'average'} < 1),     "average upper bounds check") ;
}

$results = PCP::LogSummary->new($archive, \@metrics, '@09:00', '@17:00');
ok(1, defined($results), "restricted log summarised");

foreach my $metric ( sort keys %$results ) {
    my $summary = $$results{$metric};
    #print("metric=", $metric, "\n");
    #print("  average=", $$summary{'average'}, "\n");
    #print("  samples=", $$summary{'samples'}, "\n");
    ok(1, ($$summary{'samples'} == 1919), "restricted samples verified");
    ok(1, ($$summary{'average'} > 0),     "average lower bounds check");
    ok(1, ($$summary{'average'} < 1),     "average upper bounds check") ;
}
