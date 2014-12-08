#!/usr/bin/perl
use strict;

package JVMGarbageCollection;

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
        'ps_scavenge' => 'PS%20Scavenge',
        'ps_mark_sweep' => 'PS%20MarkSweep',
    };

    my $escaped_memory_generation = $memory_generation_names->{$memory_generation};

    my $camel_case_metric_group = "_" . $metric_group;
    $camel_case_metric_group =~ s/_(\w)/\U$1/g;
    return $self->query($escaped_memory_generation, $camel_case_metric_group, $attribute);
}

sub query {
    my ($self, $gc, $value) = @_;

    my $response = $self->{_rest_client}->get("/api/jolokia/read/java.lang:type=GarbageCollector,name=" . $gc . "?ignoreErrors=true");
    return undef unless defined($response);
    return $response->{'value'}->{$value};
}

1;
