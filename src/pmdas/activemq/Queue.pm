#!/usr/bin/perl 
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

sub attribute_for {
    my ($self, $attribute) = @_;

    my $camel_case_attribute = "_" . $attribute;
    $camel_case_attribute =~ s/_(\w)/\U$1/g;
    return $self->query($camel_case_attribute);
}

sub query {
    my ($self, $value) = @_;

    my $response = $self->{_rest_client}->get("/api/jolokia/read/" . $self->{_name} . "?ignoreErrors=true");
    return undef unless defined($response);
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

1;
