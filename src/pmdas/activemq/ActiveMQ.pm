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
    my $query_result = $self->query('Queues');

    return undef unless defined($query_result);

    my @queues = @{$query_result};
    my @queue_instances = map {
      Queue->new($_->{'objectName'}, $self->{_rest_client});
    } @queues;
    return @queue_instances;
}

sub query {
    my ($self, $value) = @_;
    my $response = $self->{_rest_client}->get("/api/jolokia/read/org.apache.activemq:type=Broker,brokerName=localhost");
    return undef unless defined($response);
    return $response->{'value'}->{$value};
}

sub queue_by_short_name {
  my ($self, $short_name) = @_;

  my @queues = $self->queues;
  foreach my $queue (@queues) {
    if ($queue->short_name eq $short_name) {
      return $queue;
    }
  }

  return undef;
}

1;
