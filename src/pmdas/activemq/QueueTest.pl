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
use Test::Magpie;

use PCP::Queue;

BEGIN {
    plan(tests => 2)
}

my $rest_client = mock;

my $queue = Queue->new("org.apache.activemq:type=Broker,brokerName=localhost,destinationType=Queue,destinationName=queuename", $rest_client);

when($rest_client)->get("/api/jolokia/read/org.apache.activemq:type=Broker,brokerName=localhost,destinationType=Queue,destinationName=queuename?ignoreErrors=true")->then_return({'value' => { 'DequeueCount' => 99}});
is($queue->attribute_for('dequeue_count'), 99, "DequeueCount is available as an attribute_for");

is($queue->short_name, "queuename", "short_name is available");

