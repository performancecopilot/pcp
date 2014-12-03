#!/usr/bin/perl
use strict;

use File::Basename;
use lib dirname (__FILE__);

use Test::More;
use Test::Magpie qw(mock when);
use Data::Dumper;
use ActiveMQ;

BEGIN {
  plan(tests => 6)
}

my $user_agent = mock;
my $queue1 = Queue->new('org.apache.activemq:brokerName=gG,destinationName=queue1,destinationType=Queue,type=Broker', $user_agent);
my $queue2 = Queue->new('org.apache.activemq:brokerName=localhost,destinationName=queue2,destinationType=Queue,type=Broker', $user_agent);
  
my $activemq = ActiveMQ->new( $user_agent );


when($user_agent)->get('/api/jolokia/read/org.apache.activemq:type=Broker,brokerName=localhost')->then_return({'value' => {'TotalMessageCount' => 5 }});
is($activemq->total_message_count, 5);

when($user_agent)->get('/api/jolokia/read/org.apache.activemq:type=Broker,brokerName=localhost')->then_return({'value' => {'AverageMessageSize' => 1234 }});
is($activemq->average_message_size, 1234);

when($user_agent)->get('/api/jolokia/read/org.apache.activemq:type=Broker,brokerName=localhost')->then_return({'value' => {'BrokerId' => 'myid' }});
is($activemq->broker_id, "myid");

when($user_agent)->get('/api/jolokia/read/org.apache.activemq:type=Broker,brokerName=localhost')->then_return(
  {
    'value' => {   
      'Queues' => [
        {'objectName' => 'org.apache.activemq:brokerName=localhost,destinationName=queue1,destinationType=Queue,type=Broker'},
        {'objectName' => 'org.apache.activemq:brokerName=localhost,destinationName=queue2,destinationType=Queue,type=Broker'},
      ]
    }
  
  }
);

my @actual_queues = $activemq->queues;
my $queue_size = @actual_queues;

is($queue_size, 2);
is($actual_queues[0]->short_name(), "queue1");
is($actual_queues[1]->short_name(), "queue2");

