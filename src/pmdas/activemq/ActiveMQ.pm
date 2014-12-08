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

sub attribute_for {
  my ($self, $attribute, $service_name) = @_;

  my $camel_case_attribute = "_" . $attribute;
  $camel_case_attribute =~ s/_(\w)/\U$1/g;
  return $self->query($camel_case_attribute, $service_name);
}

sub refresh_health {
    my ($self)  = @_;
    $self->{_rest_client}->get("/api/jolokia/exec/org.apache.activemq:type=Broker,brokerName=localhost,service=Health/health");
}

sub queues {
    my ($self)  = @_;
    my $query_result = $self->query('Queues');

    return () unless defined($query_result);

    my @queues = @{$query_result};
    my @queue_instances = map {
      Queue->new($_->{'objectName'}, $self->{_rest_client});
    } @queues;
    return @queue_instances;
}

sub query {
    my ($self, $value, $service_name) = @_;
    $service_name = ",service=" . $service_name if defined($service_name);

    my $response = $self->{_rest_client}->get("/api/jolokia/read/org.apache.activemq:type=Broker,brokerName=localhost" . $service_name);
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
