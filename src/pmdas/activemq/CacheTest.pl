#!/usr/bin/env perl
use strict;

use File::Basename;
use lib dirname (__FILE__);

use Test::More;
use Test::Magpie qw(mock when);

use Cache;

BEGIN {
    plan(tests => 3)
}

my $time_source = mock;

my $cache = Cache->new( $time_source, 1); # TTL of 1 second

# undefined when attempting to fetch something from the cache that does not exist
is($cache->get("test"), undef);

# stores data in the cache
when($time_source)->get_time->then_return(1421626502.000000);
$cache->put("my_key", "some_data");
is($cache->get("my_key"), "some_data");

# expires from the cache when the time has gone past the time to live
when($time_source)->get_time->then_return(1421626502.000000);
$cache->put("another key", "other_data");
when($time_source)->get_time->then_return(1421626504.000000); # 2 seconds later
is($cache->get("another key"), undef);
