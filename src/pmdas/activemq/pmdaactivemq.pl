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
use Data::Dumper;

my $queue_indom = 0;
my $http_client = LWP::UserAgent->new;
my $rest_client = RESTClient->new($http_client, 'localhost', 8161, 'admin', 'admin', 'ActiveMQRealm');
my $activemq = ActiveMQ->new($rest_client);

my @queue_instances;

my @cluster_cache;		# time of last refresh for each cluster
my $cache_interval = 2;		# min secs between refreshes for clusters

my $pmda = PCP::PMDA->new('activemq', 133);

sub update_activemq_status
{
    my ($cluster) = @_;
    my $now = time;

    if (defined($cluster_cache[$cluster]) && $now - $cluster_cache[$cluster] <= $cache_interval) {
        return;
    }

    if ($cluster == 0) {

    }
    elsif ($cluster == 1) {
        my @queues = $activemq->queues;

        @queue_instances = map {
          ($_->uid(), $_->short_name);
        } @queues;

        $pmda->replace_indom($queue_indom, \@queue_instances);
    }

    $cluster_cache[$cluster] = $now;
}


sub activemq_fetch_callback
{
#	my $FILE;
#
#    open $FILE, ">>", "/tmp/activemq_pmda.log";
#
#print $FILE "\nfetch * ";
	my ($cluster, $item, $inst) = @_;

    if($cluster ==0) {
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
    elsif ($cluster == 1) {
	    my $selected_queue = $activemq->queue_by_uid($inst);
        if($item == 0) {
            return ($selected_queue->queue_size(), 1);
        }
        elsif($item == 1) {
            return ($selected_queue->short_name(), 1);
        }
        elsif($item == 2) {
            return ($selected_queue->dequeue_count(), 1);
        }
        elsif($item == 3) {
            return ($selected_queue->enqueue_count(), 1);
        }
        elsif($item == 4) {
            return ($selected_queue->average_enqueue_time(), 1);
        }
        else {
            return (PM_ERR_PMID, 0);
        }
    }
    else {
        return (PM_ERR_PMID, 0);
    }
}


#print $FILE "We are alive...";

$pmda->add_metric(pmda_pmid(0,0), PM_TYPE_U32, PM_INDOM_NULL,
	PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
	'activemq.total_message_count',	'Number of unacknowledged messages on the broker', '');
$pmda->add_metric(pmda_pmid(0,1), PM_TYPE_U32, PM_INDOM_NULL,
	PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
	'activemq.average_message_size', 'Average message size on this broker', '');
$pmda->add_metric(pmda_pmid(0,2), PM_TYPE_STRING, PM_INDOM_NULL,
	PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
	'activemq.broker_id', 'Unique id of the broker', '');

$pmda->add_metric(pmda_pmid(1,0), PM_TYPE_U32, $queue_indom,
    PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
    'activemq.queue.queue_size', 'Number of messages in the destination which are yet to be consumed', '');

$pmda->add_metric(pmda_pmid(1,1), PM_TYPE_STRING, $queue_indom,
    PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
    'activemq.queue.queue_name', 'Name of the queue', '');

$pmda->add_metric(pmda_pmid(1,2), PM_TYPE_U32, $queue_indom,
    PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
    'activemq.queue.dequeue_count', 'Number of messages that have been acknowledged (and removed from) from the destination', '');

$pmda->add_metric(pmda_pmid(1,3), PM_TYPE_U32, $queue_indom,
    PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
    'activemq.queue.enqueue_count', 'Number of messages that have been sent to the destination', '');

$pmda->add_metric(pmda_pmid(1,4), PM_TYPE_U32, $queue_indom,
    PM_SEM_INSTANT, pmda_units(0,1,0,0,PM_TIME_MSEC,0),
    'activemq.queue.average_enqueue_time', 'Average time a message has been held this destination', '');

$pmda->add_indom($queue_indom, \@queue_instances,
		'Instance domain exporting each queue', '');

$pmda->set_fetch_callback(\&activemq_fetch_callback);
$pmda->set_refresh(\&update_activemq_status);
$pmda->set_user('pcp');
$pmda->run;
