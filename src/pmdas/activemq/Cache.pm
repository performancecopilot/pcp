#
# Copyright (c) 2015 Aconex
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

package PCP::Cache;

sub new {
    my $class = shift;
    my $self = {
	_timesource => shift,
	_time_to_live => shift,
	_cache => (),
    };
    bless $self, $class;
    return $self;
}

sub get {
    my ($self,$cache_key) = @_;
    unless(exists($self->{_cache}->{$cache_key})) {
	return undef;  
    }
    if($self->{_cache}->{$cache_key}->{created} < $self->{_timesource}->get_time - $self->{_time_to_live}) {
	return undef;
    }
    return $self->{_cache}->{$cache_key}->{data};
}

sub put {
    my ($self,$cache_key,$data) = @_;
    %{$self->{_cache}->{$cache_key}} = (created => $self->{_timesource}->get_time, data => $data);
}
1;
