#
# Copyright (c) 2014 Aconex
# 
# This program is free software; you can redistribute it and/or modify it
# under the terms of the GNU General Public License as published by the
# Free Software Foundation; either version 2 of the License, or (at your
# option) any later version.
# 
# This program is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
# or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
# for more details.
# 
use strict;

package PCP::ActiveMQ;
use PCP::Queue;

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
    $self->{_rest_client}->get("/api/jolokia/exec/org.apache.activemq:type=Broker,brokerName=localhost,service=Health/health?ignoreErrors=true");
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

    my $response = $self->{_rest_client}->get("/api/jolokia/read/org.apache.activemq:type=Broker,brokerName=localhost" . $service_name . "?ignoreErrors=true");
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
