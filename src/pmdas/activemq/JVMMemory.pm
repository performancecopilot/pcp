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

package PCP::JVMMemory;

sub new {
    my $class = shift;
    my $self = {
	_rest_client => shift,
    };
    bless $self, $class;
    return $self;
}

sub attribute_for {
    my ($self, $metric_group, $attribute) = @_;

    my $camel_case_metric_group = "_" . $metric_group;
    $camel_case_metric_group =~ s/_(\w)/\U$1/g;
    return $self->query($camel_case_metric_group, $attribute);
}

sub query {
    my ($self, $metric_group, $value) = @_;

    my $response = $self->{_rest_client}->get("/api/jolokia/read/java.lang:type=Memory?ignoreErrors=true");
    return undef unless defined($response);
    return $response->{'value'}->{$metric_group}->{$value};
}

1;
