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

sub dequeue_count {
  my ($self) = @_;
  return $self->query('DequeueCount');
}

sub enqueue_count {
  my ($self) = @_;
  return $self->query('EnqueueCount');
}

sub average_enqueue_time {
  my ($self) = @_;
  return $self->query('AverageEnqueueTime');
}

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
