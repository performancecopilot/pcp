#!/usr/bin/perl
#
use LWP::UserAgent;

package ActiveMQ;

sub new {
  my $class = shift;
  my $self = {
    _connection => shift,
  };
  $self->{_connection}->credentials("localhost:8161", "ActiveMQRealm", "admin", "admin");
  bless $self, $class;
  return $self;
}

sub total_message_count {
    my ($self)  = @_;
    my $response = $self->{_connection}->get("http://localhost:8161/api/jolokia/read/org.apache.activemq:type=Broker,brokerName=localhost");
    return undef unless $response->is_success;
    return $response->decoded_content;
}
1;
