#!/usr/bin/perl 
use strict;

use File::Basename;
use lib dirname (__FILE__);

use Test::More;
use Test::Magpie;

use Queue;

BEGIN {
  plan(tests => 2)
}

my $rest_client = mock;

my $queue = Queue->new("org.apache.activemq:type=Broker,brokerName=localhost,destinationType=Queue,destinationName=queuename", $rest_client);

when($rest_client)->get("/api/jolokia/read/org.apache.activemq:type=Broker,brokerName=localhost,destinationType=Queue,destinationName=queuename")->then_return({'value' => { 'DequeueCount' => 99}});
is($queue->attribute_for('dequeue_count'), 99, "DequeueCount is available as an attribute_for");

is($queue->short_name, "queuename", "short_name is available");

