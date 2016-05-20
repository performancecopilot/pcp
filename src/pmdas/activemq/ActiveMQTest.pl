#!/usr/bin/env perl
#
# Copyright (c) 2014 Aconex
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
use strict;

use File::Basename;
use lib dirname (__FILE__);

use Test::More;
use Test::Magpie qw(mock when verify);
use ActiveMQ;

BEGIN {
    plan(tests => 9)
}

my $user_agent = mock;
my $queue1 = Queue->new('org.apache.activemq:brokerName=gG,destinationName=queue1,destinationType=Queue,type=Broker', $user_agent);
my $queue2 = Queue->new('org.apache.activemq:brokerName=localhost,destinationName=queue2,destinationType=Queue,type=Broker', $user_agent);
my $actual_queue_response = {
    'value' => {
	'Queues' => [
	    {'objectName' => 'org.apache.activemq:brokerName=localhost,destinationName=queue1,destinationType=Queue,type=Broker'},
	    {'objectName' => 'org.apache.activemq:brokerName=localhost,destinationName=queue2,destinationType=Queue,type=Broker'},
	]
    }
};

my $activemq = ActiveMQ->new( $user_agent );

when($user_agent)->get('/api/jolokia/read/org.apache.activemq:type=Broker,brokerName=localhost?ignoreErrors=true')->then_return({'value' => {'BrokerId' => 'myid' }});
is($activemq->attribute_for('broker_id'), "myid", "broker_id is available");

when($user_agent)->get('/api/jolokia/read/org.apache.activemq:type=Broker,brokerName=localhost,service=Health?ignoreErrors=true')->then_return({'value' => {'CurrentStatus' => 'Good' }});
is($activemq->attribute_for('current_status', 'Health'), "Good", "attribute with service name is available");

$activemq->refresh_health;
verify($user_agent)->get('/api/jolokia/exec/org.apache.activemq:type=Broker,brokerName=localhost,service=Health/health?ignoreErrors=true');

when($user_agent)->get('/api/jolokia/read/org.apache.activemq:type=Broker,brokerName=localhost?ignoreErrors=true')->then_return($actual_queue_response);

my @actual_queues = $activemq->queues;
my $queue_size = @actual_queues;

is($queue_size, 2, "queues() returns collection of correct size");
is($actual_queues[0]->short_name(), "queue1", "queues() response contains queue1");
is($actual_queues[1]->short_name(), "queue2", "queues() response contains queue2");

when($user_agent)->get('/api/jolokia/read/org.apache.activemq:type=Broker,brokerName=localhost?ignoreErrors=true')->then_return(undef);

@actual_queues = $activemq->queues;
$queue_size = @actual_queues;
is($queue_size, 0, "queues() returns 0 size collection when user agent returns undef");

when($user_agent)->get('/api/jolokia/read/org.apache.activemq:type=Broker,brokerName=localhost?ignoreErrors=true')->then_return($actual_queue_response);
is($activemq->queue_by_short_name("queue2")->short_name(), "queue2", "Find queue by short name");

when($user_agent)->get('/api/jolokia/read/org.apache.activemq:type=Broker,brokerName=localhost?ignoreErrors=true')->then_return($actual_queue_response);
is($activemq->queue_by_short_name("unknown_queue"), undef, "Unknown short name should be undefined");
