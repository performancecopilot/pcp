#!/usr/bin/perl 
use strict;

package RESTClient;
use JSON;

sub new {
  my $class = shift;
  my $self = {
    _http_client => shift,
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
  my $response = $self->{_http_client}->get("http://" . $self->{_host} . ":" . $self->{_port} . $url);
  return undef unless $response->is_success;
  return decode_json($response->decoded_content);
}

1;
