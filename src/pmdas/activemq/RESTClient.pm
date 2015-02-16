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

package RESTClient;
use JSON;

sub new {
    my $class = shift;
    my $self = {
	_http_client => shift,
	_cache => shift,
	_host => shift,
	_port => shift,
	_username => shift,
	_password => shift,
	_realm => shift,
    };
    $self->{_http_client}->credentials($self->{_host} . ":" . $self->{_port}, $self->{_realm}, $self->{_username}, $self->{_password});
    bless $self, $class;
    return $self;
}

sub get {
    my ($self, $url) = @_;
    my $json = undef;

    my $cached_json = $self->{_cache}->get($url);
    if(defined($cached_json)) {
	$json = $cached_json;
    }
    else {
	my $response = $self->{_http_client}->get("http://" . $self->{_host} . ":" . $self->{_port} . $url);
	return undef unless defined($response);
	return undef unless $response->is_success;
	$json = decode_json($response->decoded_content);
	$self->{_cache}->put($url,$json);
    }

    return $json;
}

1;
