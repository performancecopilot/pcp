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

my %queue_instances;

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
        $activemq->refresh_health;
    }
    elsif ($cluster == 1) {
        my @queues = $activemq->queues;

        %queue_instances = map {
          ($_->short_name(), $_->short_name());
        } @queues;

        $pmda->replace_indom($queue_indom, \%queue_instances);
    }

    $cluster_cache[$cluster] = $now;
}


sub activemq_value
{
    my ( $value ) = @_;

    return (PM_ERR_APPVERSION, 0) unless (defined($value));
    return ($value, 1);
}

sub activemq_fetch_callback
{
	my ($cluster, $item, $inst) = @_;

    if($cluster ==0) {
        if ($inst != PM_IN_NULL)	{ return (PM_ERR_INST, 0); }
        if($item == 0) {
            return activemq_value($activemq->total_message_count);
        }
        elsif ($item == 1) {
            return activemq_value($activemq->average_message_size);
        }
        elsif ($item == 2) {
            return activemq_value($activemq->broker_id);
        }
        elsif ($item == 3) {
            return activemq_value($activemq->health);
        }
        else {
            return (PM_ERR_PMID, 0);
        }
    }
    elsif ($cluster == 1) {
    	if ($inst == PM_IN_NULL)	{ return (PM_ERR_INST, 0); }

    	my $instance_queue_name = pmda_inst_lookup($queue_indom, $inst);
    	return (PM_ERR_INST, 0) unless defined($instance_queue_name);

	    my $selected_queue = $activemq->queue_by_short_name($instance_queue_name);
	    return (PM_ERR_INST, 0) unless defined($selected_queue);

        my $metric_name = pmda_pmid_name($cluster, $item);
        my @metric_subnames = split(/\./, $metric_name);

        return activemq_value($selected_queue->attribute_for($metric_subnames[-1]));
    }
    else {
        return (PM_ERR_PMID, 0);
    }
}

$pmda->add_metric(pmda_pmid(0,0), PM_TYPE_U64, PM_INDOM_NULL,
	PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
	'activemq.broker.total_message_count',	'Number of unacknowledged messages on the broker', '');
$pmda->add_metric(pmda_pmid(0,1), PM_TYPE_FLOAT, PM_INDOM_NULL,
	PM_SEM_INSTANT, pmda_units(1,0,0,PM_SPACE_BYTE,0,0),
	'activemq.broker.average_message_size', 'Average message size on this broker', '');
$pmda->add_metric(pmda_pmid(0,2), PM_TYPE_STRING, PM_INDOM_NULL,
	PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
	'activemq.broker.id', 'Unique id of the broker', '');
$pmda->add_metric(pmda_pmid(0,3), PM_TYPE_STRING, PM_INDOM_NULL,
	PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
	'activemq.broker.health', 'String representation of current Broker state', '');

my %queue_metrics = (
    'dequeue_count'  => {
        description	=> '',
        metric_type	=> PM_SEM_COUNTER,
        data_type	=> PM_TYPE_U64,
        units	=> pmda_units(0,0,1,0,0,PM_COUNT_ONE)},
    'dispatch_count'  => {
        description	=> '',
        metric_type	=> PM_SEM_COUNTER,
        data_type	=> PM_TYPE_U64,
        units	=> pmda_units(0,0,1,0,0,PM_COUNT_ONE)},
    'enqueue_count'  => {
        description	=> '',
        metric_type	=> PM_SEM_COUNTER,
        data_type	=> PM_TYPE_U64,
        units	=> pmda_units(0,0,1,0,0,PM_COUNT_ONE)},
    'expired_count'  => {
        description	=> '',
        metric_type	=> PM_SEM_COUNTER,
        data_type	=> PM_TYPE_U64,
        units	=> pmda_units(0,0,1,0,0,PM_COUNT_ONE)},
    'average_blocked_time'  => {
        description	=> '',
        metric_type	=> PM_SEM_INSTANT,
        data_type	=> PM_TYPE_FLOAT,
        units	=> pmda_units(0,1,0,0,PM_TIME_MSEC,0)},
    'max_enqueue_time'  => {
        description	=> '',
        metric_type	=> PM_SEM_INSTANT,
        data_type	=> PM_TYPE_U64,
        units	=> pmda_units(0,1,0,0,PM_TIME_MSEC,0)},
    'average_enqueue_time'  => {
        description	=> '',
        metric_type	=> PM_SEM_INSTANT,
        data_type	=> PM_TYPE_FLOAT,
        units	=> pmda_units(0,1,0,0,PM_TIME_MSEC,0)},
    'total_blocked_time'  => {
        description	=> '',
        metric_type	=> PM_SEM_COUNTER,
        data_type	=> PM_TYPE_U64,
        units	=> pmda_units(0,1,0,0,PM_TIME_MSEC,0)},
    'min_enqueue_time'  => {
        description	=> '',
        metric_type	=> PM_SEM_INSTANT,
        data_type	=> PM_TYPE_U64,
        units	=> pmda_units(0,1,0,0,PM_TIME_MSEC,0)},
    'blocked_producer_warning_interval'  => {
        description	=> '',
        metric_type	=> PM_SEM_INSTANT,
        data_type	=> PM_TYPE_U64,
        units	=> pmda_units(0,1,0,0,PM_TIME_MSEC,0)},
    'average_message_size'  => {
        description	=> '',
        metric_type	=> PM_SEM_INSTANT,
        data_type	=> PM_TYPE_FLOAT,
        units	=> pmda_units(1,0,0,PM_SPACE_BYTE,0,0)},
    'cursor_memory_usage'  => {
        description	=> '',
        metric_type	=> PM_SEM_INSTANT,
        data_type	=> PM_TYPE_U64,
        units	=> pmda_units(1,0,0,PM_SPACE_BYTE,0,0)},
    'max_message_size'  => {
        description	=> '',
        metric_type	=> PM_SEM_INSTANT,
        data_type	=> PM_TYPE_U64,
        units	=> pmda_units(1,0,0,PM_SPACE_BYTE,0,0)},
    'min_message_size'  => {
        description	=> '',
        metric_type	=> PM_SEM_INSTANT,
        data_type	=> PM_TYPE_U64,
        units	=> pmda_units(1,0,0,PM_SPACE_BYTE,0,0)},
    'memory_usage_byte_count'  => {
        description	=> '',
        metric_type	=> PM_SEM_INSTANT,
        data_type	=> PM_TYPE_U64,
        units	=> pmda_units(1,0,0,PM_SPACE_BYTE,0,0)},
    'memory_limit'  => {
        description	=> '',
        metric_type	=> PM_SEM_INSTANT,
        data_type	=> PM_TYPE_U64,
        units	=> pmda_units(1,0,0,PM_SPACE_BYTE,0,0)},
    'blocked_sends'  => {
        description	=> '',
        metric_type	=> PM_SEM_INSTANT,
        data_type	=> PM_TYPE_U64,
        units	=> pmda_units(0,0,1,0,0,PM_COUNT_ONE)},
    'consumer_count'  => {
        description	=> '',
        metric_type	=> PM_SEM_INSTANT,
        data_type	=> PM_TYPE_U64,
        units	=> pmda_units(0,0,1,0,0,PM_COUNT_ONE)},
    'in_flight_count'  => {
        description	=> '',
        metric_type	=> PM_SEM_INSTANT,
        data_type	=> PM_TYPE_U64,
        units	=> pmda_units(0,0,1,0,0,PM_COUNT_ONE)},
    'max_audit_depth'  => {
        description	=> '',
        metric_type	=> PM_SEM_INSTANT,
        data_type	=> PM_TYPE_U64,
        units	=> pmda_units(0,0,1,0,0,PM_COUNT_ONE)},
    'max_page_size'  => {
        description	=> '',
        metric_type	=> PM_SEM_INSTANT,
        data_type	=> PM_TYPE_U64,
        units	=> pmda_units(0,0,1,0,0,PM_COUNT_ONE)},
    'max_producers_to_audit'  => {
        description	=> '',
        metric_type	=> PM_SEM_INSTANT,
        data_type	=> PM_TYPE_U64,
        units	=> pmda_units(0,0,1,0,0,PM_COUNT_ONE)},
    'producer_count'  => {
        description	=> '',
        metric_type	=> PM_SEM_INSTANT,
        data_type	=> PM_TYPE_U64,
        units	=> pmda_units(0,0,1,0,0,PM_COUNT_ONE)},
    'queue_size'  => {
        description	=> '',
        metric_type	=> PM_SEM_INSTANT,
        data_type	=> PM_TYPE_U64,
        units	=> pmda_units(0,0,1,0,0,PM_COUNT_ONE)},
    'cursor_percent_usage'  => {
        description	=> '',
        metric_type	=> PM_SEM_INSTANT,
        data_type	=> PM_TYPE_U32,
        units	=> pmda_units(0,0,0,0,0,PM_COUNT_ONE)},
    'memory_percent_usage'  => {
        description	=> '',
        metric_type	=> PM_SEM_INSTANT,
        data_type	=> PM_TYPE_U32,
        units	=> pmda_units(0,0,0,0,0,PM_COUNT_ONE)},
    'memory_usage_portion'  => {
        description	=> '',
        metric_type	=> PM_SEM_INSTANT,
        data_type	=> PM_TYPE_FLOAT,
        units	=> pmda_units(0,0,0,0,0,PM_COUNT_ONE)},
);

my $metricCounter = 0;

foreach my $metricName (sort (keys %queue_metrics)) {
    my %metricDetails = %{$queue_metrics{$metricName}};
    $pmda->add_metric(pmda_pmid(1,$metricCounter), PM_TYPE_U64, $queue_indom,
        $metricDetails{metric_type}, $metricDetails{units},
        'activemq.queue.' . $metricName, $metricDetails{description}, '');
     $metricCounter++;
}

$pmda->add_indom($queue_indom, \%queue_instances,
		'Instance domain exporting each queue', '');

$pmda->set_fetch_callback(\&activemq_fetch_callback);
$pmda->set_refresh(\&update_activemq_status);
$pmda->set_user('pcp');
$pmda->run;
