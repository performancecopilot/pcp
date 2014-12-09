#!/usr/bin/perl
use strict;

use File::Basename;
use lib dirname (__FILE__);

use Test::More;
use Test::Magpie qw(mock when);

use RESTClient;

BEGIN {
    plan(tests => 2)
}

my $http_client = mock;
my $successful_response = mock;
my $failed_response = mock;

when($http_client)->get('http://localhost:1234/success')->then_return($successful_response);
when($http_client)->get('http://localhost:1234/fail')->then_return($failed_response);

when($successful_response)->is_success->then_return(1);
when($successful_response)->decoded_content->then_return('{"status": 200}');
when($failed_response)->is_success->then_return(0);

my $rest_client = RESTClient->new( $http_client, "localhost", "1234", "myuser", "mypass", "myrealm" );

is_deeply($rest_client->get("/success"), { "status" => 200 });

is($rest_client->get("/fail"), undef);
