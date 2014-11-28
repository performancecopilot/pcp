#!/usr/bin/perl
use strict;

use File::Basename;
use lib dirname (__FILE__);

use Test::More;
use Test::MockObject;
use ActiveMQ;

BEGIN {
  plan(tests => 1)
}

my $user_agent = Test::MockObject->new;
my $response = Test::MockObject->new;
$user_agent->mock('credentials');
$user_agent->mock('get', sub { $response });
$response->mock('is_success', sub { 1 });
  
  
$response->mock('decoded_content', sub {'{"timestamp":1417141755,"status":200,"request":{"mbean":"org.apache.activemq:brokerName=localhost,type=Broker","type":"read"},"value":{"BrokerId":"ID:ryanacxdesktop2-53124-1417048523515-0:1","TemporaryQueueSubscribers":[],"TemporaryQueues":[],"DynamicDestinationProducers":[],"MemoryPercentUsage":0,"StompSslURL":"","TemporaryTopicProducers":[],"MemoryLimit":720791142,"Queues":[],"TotalConnectionsCount":0,"StoreLimit":66529800786,"TotalConsumerCount":0,"DurableTopicSubscribers":[],"Slave":false,"StompURL":"stomp:\/\/ryanacxdesktop2:61613?maximumConnections=1000&wireFormat.maxFrameSize=104857600","TopicSubscribers":[],"QueueProducers":[],"VMURL":"vm:\/\/localhost","Uptime":"1 day 1 hour","DataDirectory":"\/home\/ryan\/.apps\/apache-activemq-5.10.0\/data","TotalMessageCount":0,"Topics":[{"objectName":"org.apache.activemq:brokerName=localhost,destinationName=ActiveMQ.Advisory.MasterBroker,destinationType=Topic,type=Broker"}],"TempLimit":53687091200,"BrokerName":"localhost","TopicProducers":[],"OpenWireURL":"tcp:\/\/ryanacxdesktop2:61616?maximumConnections=1000&wireFormat.maxFrameSize=104857600","JobSchedulerStoreLimit":0,"MinMessageSize":1024,"JobSchedulerStorePercentUsage":0,"SslURL":"","QueueSubscribers":[],"TotalDequeueCount":0,"TemporaryTopics":[],"BrokerVersion":"5.10.0","AverageMessageSize":1024,"StatisticsEnabled":true,"JMSJobScheduler":null,"CurrentConnectionsCount":0,"TotalProducerCount":0,"StorePercentUsage":0,"TemporaryQueueProducers":[],"MaxMessageSize":1024,"TotalEnqueueCount":1,"TempPercentUsage":0,"Persistent":true,"InactiveDurableTopicSubscribers":[],"TemporaryTopicSubscribers":[],"TransportConnectors":{"amqp":"amqp:\/\/ryanacxdesktop2:5672?maximumConnections=1000&wireFormat.maxFrameSize=104857600","mqtt":"mqtt:\/\/ryanacxdesktop2:1883?maximumConnections=1000&wireFormat.maxFrameSize=104857600","openwire":"tcp:\/\/ryanacxdesktop2:61616?maximumConnections=1000&wireFormat.maxFrameSize=104857600","ws":"ws:\/\/ryanacxdesktop2:61614?maximumConnections=1000&wireFormat.maxFrameSize=104857600","stomp":"stomp:\/\/ryanacxdesktop2:61613?maximumConnections=1000&wireFormat.maxFrameSize=104857600"}}}'});
my $activemq = ActiveMQ->new( $user_agent );



is($activemq->total_message_count, "this will fail");

