#!/usr/bin/env perl

use strict;
use warnings;
use Cache::Memcached;
use vars qw( $memd $val );

$memd = new Cache::Memcached {
	'servers' => [ "127.0.0.1:11211" ],
	'debug' => 0,
	'compress_threshold' => 10_000,
};

$memd->set("my_key", "Some value");
$memd->set("object_key", { 'complex' => [ "object", 2, 4 ]});

$val = $memd->get("my_key");
$val = $memd->get("object_key");
if ($val) { print $val->{'complex'}->[2]; }

$memd->incr("key");
$memd->decr("key");
$memd->incr("key", 2);

