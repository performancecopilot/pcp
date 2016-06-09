#!/usr/bin/env perl

use warnings  FATAL => qw{uninitialized};
use strict;

use FindBin  qw($RealBin);

$0 = "redis_starter";

print STDERR "DEBUG realbin: $RealBin\n";

foreach (glob "$RealBin/redis*conf") {
    print STDERR "DEBUG Starting redis with '$_'\n";

    system "/usr/sbin/redis-server $_ &"
}

foreach (6380 .. 6383) {
    system "/usr/bin/redis-cli -p $_ SET foo 123";
}

sleep 1
    while wait != -1;
