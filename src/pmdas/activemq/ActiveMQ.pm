#!/usr/bin/perl
use strict;

package ActiveMQ;
use JSON;

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

sub query {
    my ($self, $value) = @_;
    my $response = $self->{_rest_client}->get("/api/jolokia/read/org.apache.activemq:type=Broker,brokerName=localhost");
    return $response->{'value'}->{$value};
}

1;
