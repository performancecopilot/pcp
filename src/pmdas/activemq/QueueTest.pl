#!/usr/bin/perl 
use strict;

use File::Basename;
use lib dirname (__FILE__);

use Test::More;
use Test::Magpie;
use Data::Dumper;

use Queue;

BEGIN {
  plan(tests => 3)
}

my $rest_client = mock;

my $queue = Queue->new("org.apache.activemq:type=Broker,brokerName=localhost,destinationType=Queue,destinationName=queuename", $rest_client);

when($rest_client)->get("api/jolokia/read/org.apache.activemq:type=Broker,brokerName=localhost,destinationType=Queue,destinationName=queuename")->then_return({'value' => { 'QueueSize' => 123}});

is($queue->queue_size, 123);

is($queue->short_name, "queuename");

is($queue->uid, "3987426830");
