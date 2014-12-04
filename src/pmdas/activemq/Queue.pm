#!/usr/bin/perl 
use strict;

package Queue;

use Digest::MD5 qw(md5);

sub new {
  my $class = shift;
  my $self = {
    _name => shift,
    _rest_client => shift,
  };
  bless $self, $class;
  return $self;
}

sub queue_size {
  my ($self) = @_;
  return $self->query('QueueSize');
}

#{
#  "timestamp":1417655260,
#  "status":200,
#  "request":{"mbean":"org.apache.activemq:brokerName=localhost,destinationName=second_one,destinationType=Queue,type=Broker","type":"read"},
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
#    "QueueSize":0,
#    "BlockedProducerWarningInterval":30000,
#    "DequeueCount":0,
#    "AverageEnqueueTime":0.0,
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
#    "Name":"second_one",
#    "MaxProducersToAudit":1024,
#    "CursorPercentUsage":0,
#    "BlockedSends":0,
#    "ExpiredCount":0,
#    "AverageMessageSize":0.0,
#    "Subscriptions":[],
#    "EnqueueCount":0,
#    "MaxMessageSize":0,
#    "ConsumerCount":0}
#  }

sub query {
  my ($self, $value) = @_;
  my $response = $self->{_rest_client}->get("/api/jolokia/read/" . $self->{_name});
  return $response->{'value'}->{$value};
}

sub short_name {
  my ($self) = @_;
  # parse fully qualified bean name such as ...
  #     "org.apache.activemq:brokerName=localhost,destinationName=queue1,destinationType=Queue,type=Broker"
  #   ... ame into a hash of key/values.
  my %bean_name = split(/[=,]/, $self->{_name});
  return $bean_name{"destinationName"};
}

sub uid {
  my ($self) = @_;

  my $md5 = substr( md5($self->{_name}), 0, 4 );
  return unpack('L', $md5);
}

1;
