#!/usr/bin/perl 
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
