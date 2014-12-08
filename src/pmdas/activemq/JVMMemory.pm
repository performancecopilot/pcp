#!/usr/bin/perl
use strict;

package JVMMemory;

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

    my $response = $self->{_rest_client}->get("/api/jolokia/read/java.lang:type=Memory");
    return undef unless defined($response);
    return $response->{'value'}->{$metric_group}->{$value};
}

1;
