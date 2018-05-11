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

package PCP::JVMMemoryPool;

sub new {
    my $class = shift;
    my $self = {
	_rest_client => shift,
    };
    bless $self, $class;
    return $self;
}

sub attribute_for {
    my ($self, $memory_generation, $metric_group, $attribute) = @_;

    my $memory_generation_names = {
	'code_cache' => 'Code%20Cache',
	'ps_survivor_space' => 'PS%20Survivor%20Space',
	'ps_eden_space' => 'PS%20Eden%20Space',
	'ps_perm_gen' => 'PS%20Perm%20Gen',
	'ps_old_gen' => 'PS%20Old%20Gen',
    };

    my $escaped_memory_generation = $memory_generation_names->{$memory_generation};

    my $camel_case_metric_group = "_" . $metric_group;
    $camel_case_metric_group =~ s/_(\w)/\U$1/g;
    return $self->query($escaped_memory_generation, $camel_case_metric_group, $attribute);
}

sub query {
    my ($self, $escaped_memory_generation, $metric_group, $value) = @_;

    my $response = $self->{_rest_client}->get("/api/jolokia/read/java.lang:type=MemoryPool,name=" . $escaped_memory_generation . "?ignoreErrors=true");
    return undef unless defined($response);
    return $response->{'value'}->{$metric_group}->{$value};
}

1;
