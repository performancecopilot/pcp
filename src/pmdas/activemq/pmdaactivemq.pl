#
# Copyright (c) 2014 Aconex
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
use File::Basename;
use lib dirname (__FILE__);

use PCP::PMDA;
use LWP::UserAgent;
use RESTClient;
use ActiveMQ;

my $http_client = LWP::UserAgent->new;
my $rest_client = RESTClient->new($http_client, 'localhost', 8161, 'admin', 'admin', 'ActiveMQRealm');
my $activemq = ActiveMQ->new($rest_client);


sub update_activemq_status 
{
}

sub activemq_fetch_callback
{
	my ($cluster, $item, $inst) = @_;
    # 
    if($item == 0) {
        return ($activemq->total_message_count, 1);
    } 
    elsif ($item == 1) {
        return ($activemq->average_message_size, 1);
    }
    elsif ($item == 2) {
        return ($activemq->broker_id, 1);
    }
    else {
	    return (PM_ERR_PMID, 0);
    }


    
}

my $pmda = PCP::PMDA->new('activemq', 133);

$pmda->add_metric(pmda_pmid(0,0), PM_TYPE_U32, PM_INDOM_NULL,
	PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
	'activemq.total_message_count',	'Number of unacknowledged messages on the broker', '');
$pmda->add_metric(pmda_pmid(0,1), PM_TYPE_U32, PM_INDOM_NULL,
	PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
	'activemq.average_message_size', 'Average message size on this broker', '');
$pmda->add_metric(pmda_pmid(0,2), PM_TYPE_STRING, PM_INDOM_NULL,
	PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
	'activemq.broker_id', 'Unique id of the broker', '');

$pmda->set_fetch_callback(\&activemq_fetch_callback);
$pmda->set_refresh(\&update_activemq_status);
$pmda->set_user('pcp');
$pmda->run;
