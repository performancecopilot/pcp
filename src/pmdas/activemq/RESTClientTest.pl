#!/usr/bin/env perl
use strict;

use File::Basename;
use lib dirname (__FILE__);

use Test::More;
use Test::Magpie qw(mock when verify);

use PCP::RESTClient;

BEGIN {
    plan(tests => 4)
}

my $http_client = mock;
my $cache = mock;
my $successful_response = mock;
my $failed_response = mock;
my $decoded_json = { "status" => 200 };

when($http_client)->get('http://localhost:1234/success')->then_return($successful_response);
when($http_client)->get('http://localhost:1234/fail')->then_return($failed_response);

when($successful_response)->is_success->then_return(1);
when($successful_response)->decoded_content->then_return('{"status": 200}');
when($failed_response)->is_success->then_return(0);

my $rest_client = RESTClient->new( $http_client, $cache, "localhost", "1234", "myuser", "mypass", "myrealm" );

# successful requests return valid JSON
is_deeply($rest_client->get("/success"), $decoded_json);

# failed requests return undef
is($rest_client->get("/fail"), undef);

# data is stored in the cache when it does not exist in the cache
when($cache)->get("/success")->then_return(undef);
$rest_client->get("/success");
verify($cache)->put('/success', $decoded_json);

# data is fetched from the cache when is already exists in the cache
when($cache)->get("/success_cached")->then_return($successful_response);
$rest_client->get("/success_cached");
verify($http_client, times => 0)->get("http://localhost:1234/success_cached");
