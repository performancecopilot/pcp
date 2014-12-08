#!/usr/bin/perl
use strict;

package JVMMemoryPool;

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
