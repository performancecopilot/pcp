#!/usr/bin/perl
use strict;

package ActiveMQ;
use Queue;
use Data::Dumper;

sub new {
  my $class = shift;
  my $self = {
    _rest_client => shift,
  };
  bless $self, $class;
  return $self;
}

sub average_message_size {
    my ($self)  = @_;
    return $self->query('AverageMessageSize');
}

sub broker_id {
    my ($self)  = @_;
    return $self->query('BrokerId');
}

sub total_message_count {
    my ($self)  = @_;
    return $self->query('TotalMessageCount');
}

sub queues {
    my ($self)  = @_;
    my @queues = @{$self->query('Queues')};

    my @queue_instances = map {
      Queue->new($_->{'objectName'}, $self->{_rest_client});
    } @queues;
#    my @queue_instances = (Queue->new('org.apache.activemq:brokerName=localhost,destinationName=queue1,destinationType=Queue,type=Broker', $self->{_rest_client}));
    return @queue_instances;
}

sub query {
    my ($self, $value) = @_;
    my $response = $self->{_rest_client}->get("/api/jolokia/read/org.apache.activemq:type=Broker,brokerName=localhost");
    return $response->{'value'}->{$value};
}

1;
