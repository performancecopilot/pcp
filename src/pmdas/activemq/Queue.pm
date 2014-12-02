#!/usr/bin/perl 
use strict;

package Queue;

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

sub query {
  my ($self, $value) = @_;
  my $response = $self->{_rest_client}->get("api/jolokia/read/org.apache.activemq:type=Broker,brokerName=localhost,destinationType=Queue,destinationName=" . $self->{_name});
  return $response->{'value'}->{$value};
}

1;
