#
# Copyright (c) 2015 Red Hat.
# Copyright (c) 2013 Ryan Doyle.
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
use warnings;
use PCP::PMDA;
use LWP::UserAgent;

my @nginx_status = ();
my $nginx_status_url = "http://localhost/nginx_status";
my $nginx_status_available = 0;
my $nginx_fetch_timeout = 1;
my $http_client = LWP::UserAgent->new;

# Configuration files for overriding the above settings
for my $file (pmda_config('PCP_PMDAS_DIR') . '/nginx/nginx.conf', 'nginx.conf') {
	eval `cat $file` unless ! -f $file;
}

$http_client->agent('pmdanginx');
$http_client->timeout($nginx_fetch_timeout);

sub update_nginx_status 
{
	my $response = $http_client->get($nginx_status_url);
	if ($response->is_success) {
	    # All the content on the status page are digits. Map the array
	    # index to the metric item ID.
	    @nginx_status = ($response->decoded_content =~ m/(\d+)/gm);
	    $nginx_status_available = 1;
	} else {
	    @nginx_status = undef;
	    $nginx_status_available = 0;
	}
}

sub nginx_fetch_callback
{
	my ($cluster, $item, $inst) = @_;
	unless ($nginx_status_available == 1) {
	    return (PM_ERR_AGAIN, 0);
	}
	unless ($cluster == 0 && defined($nginx_status[$item])) {
	    return (PM_ERR_PMID, 0);
	}
	return ($nginx_status[$item], 1);
}

my $pmda = PCP::PMDA->new('nginx', 117);

$pmda->add_metric(pmda_pmid(0,0), PM_TYPE_U32, PM_INDOM_NULL,
	PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
	'nginx.active',
	'Number of active connections', '');
$pmda->add_metric(pmda_pmid(0,1), PM_TYPE_U64, PM_INDOM_NULL,
	PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
	'nginx.accepts_count',
	'Total number of accepted connections', '');
$pmda->add_metric(pmda_pmid(0,2), PM_TYPE_U64, PM_INDOM_NULL,
	PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
	'nginx.handled_count',
	'Total number of handled connections', '');
$pmda->add_metric(pmda_pmid(0,3), PM_TYPE_U64, PM_INDOM_NULL,
	PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
	'nginx.requests_count',
	'Total number of requests', '');
$pmda->add_metric(pmda_pmid(0,4), PM_TYPE_U32, PM_INDOM_NULL,
	PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
	'nginx.reading',
	'Reading the request header', '');
$pmda->add_metric(pmda_pmid(0,5), PM_TYPE_U32, PM_INDOM_NULL,
	PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
	'nginx.writing',
	'Reading the request body, processing the request or writing response', '');
$pmda->add_metric(pmda_pmid(0,6), PM_TYPE_U32, PM_INDOM_NULL,
	PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
	'nginx.waiting',
	'Keepalive connections', '');

$pmda->set_fetch_callback(\&nginx_fetch_callback);
$pmda->set_refresh(\&update_nginx_status);
$pmda->set_user('pcp');
$pmda->run;
