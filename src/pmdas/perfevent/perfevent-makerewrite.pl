#!/usr/bin/env perl

use warnings;
use strict;

open PERFEVENTS, "pminfo perfevent |"
    or die "Can't load perfevents: $!";

open CONFIG, ">perfevent_rewrite.conf"
    or die "Can't open perfevent_rewrite.conf for writing";

while (<PERFEVENTS>) {
    my $line = $_;
    chomp $line;
    if ($line =~ /[^a-zA-Z0-9_\.]/){
        my $newline = $line;
        $newline =~ s/[^a-zA-Z0-9_\.]/_/g;

        print CONFIG "METRIC $line { NAME -> $newline }\n"
    }
} 

close PERFEVENTS;
close CONFIG;
