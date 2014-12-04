#!/usr/bin/perl 
use strict;

use File::Basename;
use lib dirname (__FILE__);

use Test::More;
use Test::Magpie;
use Data::Dumper;

use Queue;

BEGIN {
  plan(tests => 6)
}

my $rest_client = mock;

my $queue = Queue->new("org.apache.activemq:type=Broker,brokerName=localhost,destinationType=Queue,destinationName=queuename", $rest_client);

when($rest_client)->get("/api/jolokia/read/org.apache.activemq:type=Broker,brokerName=localhost,destinationType=Queue,destinationName=queuename")->then_return({'value' => { 'QueueSize' => 123}});
is($queue->queue_size, 123, "queue_size is available");

when($rest_client)->get("/api/jolokia/read/org.apache.activemq:type=Broker,brokerName=localhost,destinationType=Queue,destinationName=queuename")->then_return({'value' => { 'DequeueCount' => 99}});
is($queue->dequeue_count, 99, "dequeue_count is available");

when($rest_client)->get("/api/jolokia/read/org.apache.activemq:type=Broker,brokerName=localhost,destinationType=Queue,destinationName=queuename")->then_return({'value' => { 'EnqueueCount' => 53}});
is($queue->enqueue_count, 53, "enqueue_count is available");

when($rest_client)->get("/api/jolokia/read/org.apache.activemq:type=Broker,brokerName=localhost,destinationType=Queue,destinationName=queuename")->then_return({'value' => { 'AverageEnqueueTime' => 543}});
is($queue->average_enqueue_time, 543, "average_enqueue_time is available");

#{
#  "value":{
#    "MemoryUsageByteCount":0,
#    "ProducerCount":0,
#    "UseCache":true,
#    "ProducerFlowControl":true,
#    "MaxAuditDepth":2048,
#    "MaxPageSize":200,
#    "CursorMemoryUsage":0,
#    "DLQ":false,
#    "AlwaysRetroactive":false,
#    "MemoryPercentUsage":0,
#    "MessageGroups":{},
#    "PrioritizedMessages":false,
#    "MaxEnqueueTime":0,
#    "CursorFull":false,
#    "MemoryLimit":720791142,
#    "DispatchCount":0,
#    "BlockedProducerWarningInterval":30000,
#-    "DequeueCount":0,
#-    "AverageEnqueueTime":0.0,
#    "TotalBlockedTime":0,
#    "MinEnqueueTime":0,
#    "CacheEnabled":true,
#    "MessageGroupType":"cached",
#    "MemoryUsagePortion":1.0,
#    "InFlightCount":0,
#    "Options":"",
#    "SlowConsumerStrategy":null,
#    "MinMessageSize":0,
#    "AverageBlockedTime":0.0,
#    "MaxProducersToAudit":1024,
#    "CursorPercentUsage":0,
#    "BlockedSends":0,
#    "ExpiredCount":0,
#    "AverageMessageSize":0.0,
#    "Subscriptions":[],
#-    "EnqueueCount":0,
#    "MaxMessageSize":0,
#    "ConsumerCount":0}
#  }

is($queue->short_name, "queuename", "short_name is available");

is($queue->uid, "3987426830", "uid for queue is available");
