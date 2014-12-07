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

sub health {
    my ($self)  = @_;
    return $self->query('CurrentStatus', 'Health');
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

	my $FILE;

    open $FILE, ">>", "/tmp/activemq_pmda.log";



  my @queues = $self->queues;
  print $FILE "\n@queues = $self->queues :" . Dumper(@queues);
  foreach my $queue (@queues) {
    print $FILE "\ninside $queue (@queues)" . Dumper($queue);
    if ($queue->short_name eq $short_name) {
      return $queue;
    }
  }

  return undef;
}

1;
