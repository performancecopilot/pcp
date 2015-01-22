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
use JVMMemory;
use JVMMemoryPool;
use JVMGarbageCollection;
use TimeSource;
use Cache;

my $rest_hostname = 'localhost';
my $rest_port = 8161;
my $rest_username = 'admin';
my $rest_password = 'admin';
my $rest_realm = 'ActiveMQRealm';
my $rest_timeout = 1;
my $cache_time = 0.5;

my $queue_indom = 0;

my $broker_cluster = 0;
my $health_cluster = 1;
my $queue_cluster = 2;
my $jvm_memory_cluster = 3;
my $jvm_memory_pool_cluster = 4;
my $jvm_garbage_collection_cluster = 5;

# Configuration files for overriding the above settings
for my $file (pmda_config('PCP_PMDAS_DIR') . '/activemq/activemq.conf', 'activemq.conf') {
    eval `cat $file` unless ! -f $file;
}
my $timesource = TimeSource->new;
my $cache = Cache->new($timesource, $cache_time);
my $http_client = LWP::UserAgent->new;
$http_client->agent('pmdaactivemq');
$http_client->timeout($rest_timeout);
my $rest_client = RESTClient->new($http_client, $cache, $rest_hostname, $rest_port, $rest_username, $rest_password, $rest_realm);
my $activemq = ActiveMQ->new($rest_client);
my $jvm_memory = JVMMemory->new($rest_client);
my $jvm_memory_pool = JVMMemoryPool->new($rest_client);
my $jvm_garbage_collection = JVMGarbageCollection->new($rest_client);

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

    if ($cluster == $health_cluster) {
	$activemq->refresh_health;
    }
    elsif ($cluster == $queue_cluster) {
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

sub metric_subnames
{
    my ($cluster, $item) = @_;

    my $metric_name = pmda_pmid_name($cluster, $item);
    my @metric_subnames = split(/\./, $metric_name);
}

sub metric_subname
{
    my ($cluster, $item) = @_;

    my @metric_subnames = metric_subnames($cluster, $item);
    return $metric_subnames[-1];
}

sub activemq_fetch_callback
{
    my ($cluster, $item, $inst) = @_;

    if($cluster == $broker_cluster) {
	if ($inst != PM_IN_NULL) { return (PM_ERR_INST, 0); }
	return activemq_value($activemq->attribute_for(metric_subname($cluster, $item)));
    }
    elsif($cluster == $health_cluster) {
	if ($inst != PM_IN_NULL) { return (PM_ERR_INST, 0); }
	return activemq_value($activemq->attribute_for(metric_subname($cluster, $item), 'Health'));
    }
    elsif ($cluster == $queue_cluster) {
	if ($inst == PM_IN_NULL) { return (PM_ERR_INST, 0); }

	my $instance_queue_name = pmda_inst_lookup($queue_indom, $inst);
	return (PM_ERR_INST, 0) unless defined($instance_queue_name);

	my $selected_queue = $activemq->queue_by_short_name($instance_queue_name);
	return (PM_ERR_INST, 0) unless defined($selected_queue);
	return activemq_value($selected_queue->attribute_for(metric_subname($cluster, $item)));
    }
    elsif ($cluster == $jvm_memory_cluster) {
	if ($inst != PM_IN_NULL) { return (PM_ERR_INST, 0); }

	my @metric_subnames = metric_subnames($cluster, $item);
	return activemq_value($jvm_memory->attribute_for($metric_subnames[-2], $metric_subnames[-1]));
    }
    elsif ($cluster == $jvm_memory_pool_cluster) {
	if ($inst != PM_IN_NULL) { return (PM_ERR_INST, 0); }

	my @metric_subnames = metric_subnames($cluster, $item);
	return activemq_value($jvm_memory_pool->attribute_for($metric_subnames[-3],$metric_subnames[-2], $metric_subnames[-1]));
    }
    elsif ($cluster == $jvm_garbage_collection_cluster) {
	if ($inst != PM_IN_NULL) { return (PM_ERR_INST, 0); }

	my @metric_subnames = metric_subnames($cluster, $item);
	return activemq_value($jvm_garbage_collection->attribute_for($metric_subnames[-2], $metric_subnames[-1]));
    }
    else {
	return (PM_ERR_PMID, 0);
    }
}

my %broker_metrics = (
    'total_message_count' => {
	description	=> 'Number of unacknowledged messages on the broker',
	metric_type	=> PM_SEM_INSTANT,
	data_type	=> PM_TYPE_64,
	units	=> pmda_units(0,0,1,0,0,PM_COUNT_ONE)},
    'average_message_size' => {
	description	=> 'Average message size on this broker',
	metric_type	=> PM_SEM_INSTANT,
	data_type	=> PM_TYPE_64,
	units	=> pmda_units(1,0,0,PM_SPACE_BYTE,0,0)},
    'broker_id' => {
	description	=> 'Unique id of the broker',
	metric_type	=> PM_SEM_INSTANT,
	data_type	=> PM_TYPE_STRING,
	units	=> pmda_units(0,0,0,0,0,0)},
    'memory_percent_usage' => {
	description	=> 'Percent of memory limit used',
	metric_type	=> PM_SEM_INSTANT,
	data_type	=> PM_TYPE_32,
	units	=> pmda_units(0,0,0,0,0,PM_COUNT_ONE)},
    'memory_limit' => {
	description	=> 'Memory limit, in bytes, used for holding undelivered messages before paging to temporary storage',
	metric_type	=> PM_SEM_INSTANT,
	data_type	=> PM_TYPE_64,
	units	=> pmda_units(1,0,0,PM_SPACE_BYTE,0,0)},
    'total_connections_count' => {
	description	=> 'Total number of connections',
	metric_type	=> PM_SEM_COUNTER,
	data_type	=> PM_TYPE_64,
	units	=> pmda_units(0,0,1,0,0,PM_COUNT_ONE)},
    'store_limit' => {
	description	=> 'Disk limit, in bytes, used for persistent messages before producers are blocked',
	metric_type	=> PM_SEM_INSTANT,
	data_type	=> PM_TYPE_64,
	units	=> pmda_units(1,0,0,PM_SPACE_BYTE,0,0)},
    'total_consumer_count' => {
	description	=> 'Number of message consumers subscribed to destinations on the broker',
	metric_type	=> PM_SEM_INSTANT,
	data_type	=> PM_TYPE_64,
	units	=> pmda_units(0,0,1,0,0,PM_COUNT_ONE)},
    'slave' => {
	description	=> 'Slave broker',
	metric_type	=> PM_SEM_INSTANT,
	data_type	=> PM_TYPE_STRING,
	units	=> pmda_units(0,0,0,0,0,0)},
    'data_directory' => {
	description	=> 'The location of the data directory',
	metric_type	=> PM_SEM_INSTANT,
	data_type	=> PM_TYPE_STRING,
	units	=> pmda_units(0,0,0,0,0,0)},
    'uptime' => {
	description	=> 'Uptime of the broker',
	metric_type	=> PM_SEM_INSTANT,
	data_type	=> PM_TYPE_STRING,
	units	=> pmda_units(0,0,0,0,0,0)},
    'temp_limit' => {
	description	=> 'Disk limit, in bytes, used for non-persistent messages and temporary data before producers are blocked',
	metric_type	=> PM_SEM_INSTANT,
	data_type	=> PM_TYPE_64,
	units	=> pmda_units(1,0,0,PM_SPACE_BYTE,0,0)},
    'broker_name' => {
	description	=> 'The name of the broker',
	metric_type	=> PM_SEM_INSTANT,
	data_type	=> PM_TYPE_STRING,
	units	=> pmda_units(0,0,0,0,0,0)},
    'job_scheduler_store_limit' => {
	description	=> 'Disk limit, in bytes, used for scheduled messages before producers are blocked',
	metric_type	=> PM_SEM_INSTANT,
	data_type	=> PM_TYPE_64,
	units	=> pmda_units(1,0,0,PM_SPACE_BYTE,0,0)},
    'min_message_size' => {
	description	=> 'Min message size on this broker',
	metric_type	=> PM_SEM_INSTANT,
	data_type	=> PM_TYPE_64,
	units	=> pmda_units(1,0,0,PM_SPACE_BYTE,0,0)},
    'job_scheduler_store_percent_usage' => {
	description	=> 'Percent of job store limit used',
	metric_type	=> PM_SEM_INSTANT,
	data_type	=> PM_TYPE_32,
	units	=> pmda_units(0,0,0,0,0,PM_COUNT_ONE)},
    'total_dequeue_count' => {
	description	=> 'Number of messages that have been acknowledged on the broker',
	metric_type	=> PM_SEM_COUNTER,
	data_type	=> PM_TYPE_64,
	units	=> pmda_units(0,0,1,0,0,PM_COUNT_ONE)},
    'broker_version' => {
	description	=> 'The version of the broker',
	metric_type	=> PM_SEM_INSTANT,
	data_type	=> PM_TYPE_STRING,
	units	=> pmda_units(0,0,0,0,0,0)},
    'average_message_size' => {
	description	=> 'Average message size on this broker',
	metric_type	=> PM_SEM_INSTANT,
	data_type	=> PM_TYPE_64,
	units	=> pmda_units(1,0,0,PM_SPACE_BYTE,0,0)},
    'statistics_enabled' => {
	description	=> 'Broker statistics enabled',
	metric_type	=> PM_SEM_INSTANT,
	data_type	=> PM_TYPE_STRING,
	units	=> pmda_units(0,0,0,0,0,0)},
    'current_connections_count' => {
	description	=> 'Current connections count',
	metric_type	=> PM_SEM_INSTANT,
	data_type	=> PM_TYPE_64,
	units	=> pmda_units(0,0,1,0,0,PM_COUNT_ONE)},
    'total_producer_count' => {
	description	=> 'Number of message producers active on destinations on the broker',
	metric_type	=> PM_SEM_INSTANT,
	data_type	=> PM_TYPE_64,
	units	=> pmda_units(0,0,1,0,0,PM_COUNT_ONE)},
    'store_percent_usage' => {
	description	=> 'Percent of store limit used',
	metric_type	=> PM_SEM_INSTANT,
	data_type	=> PM_TYPE_32,
	units	=> pmda_units(0,0,0,0,0,PM_COUNT_ONE)},
    'max_message_size' => {
	description	=> 'Max message size on this broker',
	metric_type	=> PM_SEM_INSTANT,
	data_type	=> PM_TYPE_64,
	units	=> pmda_units(1,0,0,PM_SPACE_BYTE,0,0)},
    'total_enqueue_count' => {
	description	=> 'Number of messages that have been sent to the broker',
	metric_type	=> PM_SEM_COUNTER,
	data_type	=> PM_TYPE_64,
	units	=> pmda_units(0,0,1,0,0,PM_COUNT_ONE)},
    'temp_percent_usage' => {
	description	=> 'Percent of temp limit used',
	metric_type	=> PM_SEM_INSTANT,
	data_type	=> PM_TYPE_32,
	units	=> pmda_units(0,0,0,0,0,PM_COUNT_ONE)},
    'persistent' => {
	description	=> 'Messages are synchronized to disk',
	metric_type	=> PM_SEM_INSTANT,
	data_type	=> PM_TYPE_STRING,
	units	=> pmda_units(0,0,0,0,0,0)},
);

my $metricCounter = 0;

foreach my $metricName (sort (keys %broker_metrics)) {
    my %metricDetails = %{$broker_metrics{$metricName}};
    $pmda->add_metric(pmda_pmid(0,$metricCounter), $metricDetails{data_type}, PM_INDOM_NULL,
	$metricDetails{metric_type}, $metricDetails{units},
	'activemq.broker.' . $metricName, $metricDetails{description}, '');
     $metricCounter++;
}

my %health_metrics = (
    'current_status' => {
	description	=> 'String representation of current Broker state',
	metric_type	=> PM_SEM_INSTANT,
	data_type	=> PM_TYPE_STRING,
	units	=> pmda_units(0,0,0,0,0,0)},
);

$metricCounter = 0;

foreach my $metricName (sort (keys %health_metrics)) {
    my %metricDetails = %{$health_metrics{$metricName}};
    $pmda->add_metric(pmda_pmid(1,$metricCounter), $metricDetails{data_type}, PM_INDOM_NULL,
	$metricDetails{metric_type}, $metricDetails{units},
	'activemq.broker.' . $metricName, $metricDetails{description}, '');
     $metricCounter++;
}

my %queue_metrics = (
    'dequeue_count'  => {
	description	=> 'Number of messages that have been acknowledged (and removed from) from this destination',
	metric_type	=> PM_SEM_COUNTER,
	data_type	=> PM_TYPE_64,
	units	=> pmda_units(0,0,1,0,0,PM_COUNT_ONE)},
    'dispatch_count'  => {
	description	=> 'Number of messages that have been delivered (but potentially not acknowledged) to consumers',
	metric_type	=> PM_SEM_COUNTER,
	data_type	=> PM_TYPE_64,
	units	=> pmda_units(0,0,1,0,0,PM_COUNT_ONE)},
    'enqueue_count'  => {
	description	=> 'Number of messages that have been sent to this destination',
	metric_type	=> PM_SEM_COUNTER,
	data_type	=> PM_TYPE_64,
	units	=> pmda_units(0,0,1,0,0,PM_COUNT_ONE)},
    'expired_count'  => {
	description	=> 'Number of messages that have been expired',
	metric_type	=> PM_SEM_COUNTER,
	data_type	=> PM_TYPE_64,
	units	=> pmda_units(0,0,1,0,0,PM_COUNT_ONE)},
    'average_blocked_time'  => {
	description	=> 'Get the average time (ms) a message is blocked for Flow Control',
	metric_type	=> PM_SEM_INSTANT,
	data_type	=> PM_TYPE_DOUBLE,
	units	=> pmda_units(0,1,0,0,PM_TIME_MSEC,0)},
    'max_enqueue_time'  => {
	description	=> 'The longest time a message has been held at this destination',
	metric_type	=> PM_SEM_INSTANT,
	data_type	=> PM_TYPE_64,
	units	=> pmda_units(0,1,0,0,PM_TIME_MSEC,0)},
    'average_enqueue_time'  => {
	description	=> 'Average time a message has been held at this destination',
	metric_type	=> PM_SEM_INSTANT,
	data_type	=> PM_TYPE_DOUBLE,
	units	=> pmda_units(0,1,0,0,PM_TIME_MSEC,0)},
    'total_blocked_time'  => {
	description	=> 'Get the total time (ms) messages are blocked for Flow Control',
	metric_type	=> PM_SEM_COUNTER,
	data_type	=> PM_TYPE_64,
	units	=> pmda_units(0,1,0,0,PM_TIME_MSEC,0)},
    'min_enqueue_time'  => {
	description	=> 'Average time a message has been held at this destination',
	metric_type	=> PM_SEM_INSTANT,
	data_type	=> PM_TYPE_64,
	units	=> pmda_units(0,1,0,0,PM_TIME_MSEC,0)},
    'blocked_producer_warning_interval'  => {
	description	=> 'Blocked producer warning interval',
	metric_type	=> PM_SEM_INSTANT,
	data_type	=> PM_TYPE_64,
	units	=> pmda_units(0,1,0,0,PM_TIME_MSEC,0)},
    'average_message_size'  => {
	description	=> 'Average message size on this destination',
	metric_type	=> PM_SEM_INSTANT,
	data_type	=> PM_TYPE_DOUBLE,
	units	=> pmda_units(1,0,0,PM_SPACE_BYTE,0,0)},
    'cursor_memory_usage'  => {
	description	=> 'Message cursor memory usage, in bytes',
	metric_type	=> PM_SEM_INSTANT,
	data_type	=> PM_TYPE_64,
	units	=> pmda_units(1,0,0,PM_SPACE_BYTE,0,0)},
    'max_message_size'  => {
	description	=> 'Max message size on this destination',
	metric_type	=> PM_SEM_INSTANT,
	data_type	=> PM_TYPE_64,
	units	=> pmda_units(1,0,0,PM_SPACE_BYTE,0,0)},
    'min_message_size'  => {
	description	=> 'Min message size on this destination',
	metric_type	=> PM_SEM_INSTANT,
	data_type	=> PM_TYPE_64,
	units	=> pmda_units(1,0,0,PM_SPACE_BYTE,0,0)},
    'memory_usage_byte_count'  => {
	description	=> 'Memory usage, in bytes, used by undelivered messages',
	metric_type	=> PM_SEM_INSTANT,
	data_type	=> PM_TYPE_64,
	units	=> pmda_units(1,0,0,PM_SPACE_BYTE,0,0)},
    'memory_limit'  => {
	description	=> 'Memory limit, in bytes, used for holding undelivered messages before paging to temporary storage',
	metric_type	=> PM_SEM_INSTANT,
	data_type	=> PM_TYPE_64,
	units	=> pmda_units(1,0,0,PM_SPACE_BYTE,0,0)},
    'blocked_sends'  => {
	description	=> 'Get number of messages blocked for Flow Control',
	metric_type	=> PM_SEM_INSTANT,
	data_type	=> PM_TYPE_64,
	units	=> pmda_units(0,0,1,0,0,PM_COUNT_ONE)},
    'consumer_count'  => {
	description	=> 'Number of consumers subscribed to this destination',
	metric_type	=> PM_SEM_INSTANT,
	data_type	=> PM_TYPE_64,
	units	=> pmda_units(0,0,1,0,0,PM_COUNT_ONE)},
    'in_flight_count'  => {
	description	=> 'Number of messages that have been dispatched to, but not acknowledged by, consumers',
	metric_type	=> PM_SEM_INSTANT,
	data_type	=> PM_TYPE_64,
	units	=> pmda_units(0,0,1,0,0,PM_COUNT_ONE)},
    'max_audit_depth'  => {
	description	=> 'Max audit depth',
	metric_type	=> PM_SEM_INSTANT,
	data_type	=> PM_TYPE_64,
	units	=> pmda_units(0,0,1,0,0,PM_COUNT_ONE)},
    'max_page_size'  => {
	description	=> 'Maximum number of messages to be paged in',
	metric_type	=> PM_SEM_INSTANT,
	data_type	=> PM_TYPE_64,
	units	=> pmda_units(0,0,1,0,0,PM_COUNT_ONE)},
    'max_producers_to_audit'  => {
	description	=> 'Maximum number of producers to audit',
	metric_type	=> PM_SEM_INSTANT,
	data_type	=> PM_TYPE_64,
	units	=> pmda_units(0,0,1,0,0,PM_COUNT_ONE)},
    'producer_count'  => {
	description	=> 'Number of producers publishing to this destination',
	metric_type	=> PM_SEM_INSTANT,
	data_type	=> PM_TYPE_64,
	units	=> pmda_units(0,0,1,0,0,PM_COUNT_ONE)},
    'queue_size'  => {
	description	=> 'Number of messages in the destination which are yet to be consumed.  Potentially dispatched but unacknowledged',
	metric_type	=> PM_SEM_INSTANT,
	data_type	=> PM_TYPE_64,
	units	=> pmda_units(0,0,1,0,0,PM_COUNT_ONE)},
    'cursor_percent_usage'  => {
	description	=> 'Percentage of memory limit used',
	metric_type	=> PM_SEM_INSTANT,
	data_type	=> PM_TYPE_32,
	units	=> pmda_units(0,0,0,0,0,PM_COUNT_ONE)},
    'memory_percent_usage'  => {
	description	=> 'The percentage of the memory limit used',
	metric_type	=> PM_SEM_INSTANT,
	data_type	=> PM_TYPE_32,
	units	=> pmda_units(0,0,0,0,0,PM_COUNT_ONE)},
    'memory_usage_portion'  => {
	description	=> 'Portion of memory from the broker memory limit for this destination',
	metric_type	=> PM_SEM_INSTANT,
	data_type	=> PM_TYPE_FLOAT,
	units	=> pmda_units(0,0,0,0,0,PM_COUNT_ONE)},
);

$metricCounter = 0;

foreach my $metricName (sort (keys %queue_metrics)) {
    my %metricDetails = %{$queue_metrics{$metricName}};
    $pmda->add_metric(pmda_pmid(2,$metricCounter), $metricDetails{data_type}, $queue_indom,
	$metricDetails{metric_type}, $metricDetails{units},
	'activemq.queue.' . $metricName, $metricDetails{description}, '');
     $metricCounter++;
}

my %memory_metrics = (
    'non_heap_memory_usage.max' => {
	description	=> '',
	metric_type	=> PM_SEM_INSTANT,
	data_type	=> PM_TYPE_64,
	units	=> pmda_units(1,0,0,PM_SPACE_BYTE,0,0)},
    'non_heap_memory_usage.committed' => {
	description	=> '',
	metric_type	=> PM_SEM_INSTANT,
	data_type	=> PM_TYPE_64,
	units	=> pmda_units(1,0,0,PM_SPACE_BYTE,0,0)},
    'non_heap_memory_usage.init' => {
	description	=> '',
	metric_type	=> PM_SEM_INSTANT,
	data_type	=> PM_TYPE_64,
	units	=> pmda_units(1,0,0,PM_SPACE_BYTE,0,0)},
    'non_heap_memory_usage.used' => {
	description	=> '',
	metric_type	=> PM_SEM_INSTANT,
	data_type	=> PM_TYPE_64,
	units	=> pmda_units(1,0,0,PM_SPACE_BYTE,0,0)},
    'heap_memory_usage.max' => {
	description	=> '',
	metric_type	=> PM_SEM_INSTANT,
	data_type	=> PM_TYPE_64,
	units	=> pmda_units(1,0,0,PM_SPACE_BYTE,0,0)},
    'heap_memory_usage.committed' => {
	description	=> '',
	metric_type	=> PM_SEM_INSTANT,
	data_type	=> PM_TYPE_64,
	units	=> pmda_units(1,0,0,PM_SPACE_BYTE,0,0)},
    'heap_memory_usage.init' => {
	description	=> '',
	metric_type	=> PM_SEM_INSTANT,
	data_type	=> PM_TYPE_64,
	units	=> pmda_units(1,0,0,PM_SPACE_BYTE,0,0)},
    'heap_memory_usage.used' => {
	description	=> '',
	metric_type	=> PM_SEM_INSTANT,
	data_type	=> PM_TYPE_64,
	units	=> pmda_units(1,0,0,PM_SPACE_BYTE,0,0)},
);

$metricCounter = 0;

foreach my $metricName (sort (keys %memory_metrics)) {
    my %metricDetails = %{$memory_metrics{$metricName}};
    $pmda->add_metric(pmda_pmid(3,$metricCounter), $metricDetails{data_type}, PM_INDOM_NULL,
	$metricDetails{metric_type}, $metricDetails{units},
	'activemq.jvm.memory.' . $metricName, $metricDetails{description}, '');
     $metricCounter++;
}

my %memory_pool_metrics = (
    'code_cache.peak_usage.max' => {
	description	=> '',
	metric_type	=> PM_SEM_INSTANT,
	data_type	=> PM_TYPE_64,
	units	=> pmda_units(1,0,0,PM_SPACE_BYTE,0,0)},
    'code_cache.peak_usage.committed' => {
	description	=> '',
	metric_type	=> PM_SEM_INSTANT,
	data_type	=> PM_TYPE_64,
	units	=> pmda_units(1,0,0,PM_SPACE_BYTE,0,0)},
    'code_cache.peak_usage.init' => {
	description	=> '',
	metric_type	=> PM_SEM_INSTANT,
	data_type	=> PM_TYPE_64,
	units	=> pmda_units(1,0,0,PM_SPACE_BYTE,0,0)},
    'code_cache.peak_usage.used' => {
	description	=> '',
	metric_type	=> PM_SEM_INSTANT,
	data_type	=> PM_TYPE_64,
	units	=> pmda_units(1,0,0,PM_SPACE_BYTE,0,0)},
    'code_cache.usage.max' => {
	description	=> '',
	metric_type	=> PM_SEM_INSTANT,
	data_type	=> PM_TYPE_64,
	units	=> pmda_units(1,0,0,PM_SPACE_BYTE,0,0)},
    'code_cache.usage.committed' => {
	description	=> '',
	metric_type	=> PM_SEM_INSTANT,
	data_type	=> PM_TYPE_64,
	units	=> pmda_units(1,0,0,PM_SPACE_BYTE,0,0)},
    'code_cache.usage.init' => {
	description	=> '',
	metric_type	=> PM_SEM_INSTANT,
	data_type	=> PM_TYPE_64,
	units	=> pmda_units(1,0,0,PM_SPACE_BYTE,0,0)},
    'code_cache.usage.used' => {
	description	=> '',
	metric_type	=> PM_SEM_INSTANT,
	data_type	=> PM_TYPE_64,
	units	=> pmda_units(1,0,0,PM_SPACE_BYTE,0,0)},
    'ps_survivor_space.peak_usage.max' => {
	description	=> '',
	metric_type	=> PM_SEM_INSTANT,
	data_type	=> PM_TYPE_64,
	units	=> pmda_units(1,0,0,PM_SPACE_BYTE,0,0)},
    'ps_survivor_space.peak_usage.committed' => {
	description	=> '',
	metric_type	=> PM_SEM_INSTANT,
	data_type	=> PM_TYPE_64,
	units	=> pmda_units(1,0,0,PM_SPACE_BYTE,0,0)},
    'ps_survivor_space.peak_usage.init' => {
	description	=> '',
	metric_type	=> PM_SEM_INSTANT,
	data_type	=> PM_TYPE_64,
	units	=> pmda_units(1,0,0,PM_SPACE_BYTE,0,0)},
    'ps_survivor_space.peak_usage.used' => {
	description	=> '',
	metric_type	=> PM_SEM_INSTANT,
	data_type	=> PM_TYPE_64,
	units	=> pmda_units(1,0,0,PM_SPACE_BYTE,0,0)},
    'ps_survivor_space.usage.max' => {
	description	=> '',
	metric_type	=> PM_SEM_INSTANT,
	data_type	=> PM_TYPE_64,
	units	=> pmda_units(1,0,0,PM_SPACE_BYTE,0,0)},
    'ps_survivor_space.usage.committed' => {
	description	=> '',
	metric_type	=> PM_SEM_INSTANT,
	data_type	=> PM_TYPE_64,
	units	=> pmda_units(1,0,0,PM_SPACE_BYTE,0,0)},
    'ps_survivor_space.usage.init' => {
	description	=> '',
	metric_type	=> PM_SEM_INSTANT,
	data_type	=> PM_TYPE_64,
	units	=> pmda_units(1,0,0,PM_SPACE_BYTE,0,0)},
    'ps_survivor_space.usage.used' => {
	description	=> '',
	metric_type	=> PM_SEM_INSTANT,
	data_type	=> PM_TYPE_64,
	units	=> pmda_units(1,0,0,PM_SPACE_BYTE,0,0)},
    'ps_survivor_space.collection_usage.max' => {
	description	=> '',
	metric_type	=> PM_SEM_INSTANT,
	data_type	=> PM_TYPE_64,
	units	=> pmda_units(1,0,0,PM_SPACE_BYTE,0,0)},
    'ps_survivor_space.collection_usage.committed' => {
	description	=> '',
	metric_type	=> PM_SEM_INSTANT,
	data_type	=> PM_TYPE_64,
	units	=> pmda_units(1,0,0,PM_SPACE_BYTE,0,0)},
    'ps_survivor_space.collection_usage.init' => {
	description	=> '',
	metric_type	=> PM_SEM_INSTANT,
	data_type	=> PM_TYPE_64,
	units	=> pmda_units(1,0,0,PM_SPACE_BYTE,0,0)},
    'ps_survivor_space.collection_usage.used' => {
	description	=> '',
	metric_type	=> PM_SEM_INSTANT,
	data_type	=> PM_TYPE_64,
	units	=> pmda_units(1,0,0,PM_SPACE_BYTE,0,0)},
    'ps_eden_space.peak_usage.max' => {
	description	=> '',
	metric_type	=> PM_SEM_INSTANT,
	data_type	=> PM_TYPE_64,
	units	=> pmda_units(1,0,0,PM_SPACE_BYTE,0,0)},
    'ps_eden_space.peak_usage.committed' => {
	description	=> '',
	metric_type	=> PM_SEM_INSTANT,
	data_type	=> PM_TYPE_64,
	units	=> pmda_units(1,0,0,PM_SPACE_BYTE,0,0)},
    'ps_eden_space.peak_usage.init' => {
	description	=> '',
	metric_type	=> PM_SEM_INSTANT,
	data_type	=> PM_TYPE_64,
	units	=> pmda_units(1,0,0,PM_SPACE_BYTE,0,0)},
    'ps_eden_space.peak_usage.used' => {
	description	=> '',
	metric_type	=> PM_SEM_INSTANT,
	data_type	=> PM_TYPE_64,
	units	=> pmda_units(1,0,0,PM_SPACE_BYTE,0,0)},
    'ps_eden_space.usage.max' => {
	description	=> '',
	metric_type	=> PM_SEM_INSTANT,
	data_type	=> PM_TYPE_64,
	units	=> pmda_units(1,0,0,PM_SPACE_BYTE,0,0)},
    'ps_eden_space.usage.committed' => {
	description	=> '',
	metric_type	=> PM_SEM_INSTANT,
	data_type	=> PM_TYPE_64,
	units	=> pmda_units(1,0,0,PM_SPACE_BYTE,0,0)},
    'ps_eden_space.usage.init' => {
	description	=> '',
	metric_type	=> PM_SEM_INSTANT,
	data_type	=> PM_TYPE_64,
	units	=> pmda_units(1,0,0,PM_SPACE_BYTE,0,0)},
    'ps_eden_space.usage.used' => {
	description	=> '',
	metric_type	=> PM_SEM_INSTANT,
	data_type	=> PM_TYPE_64,
	units	=> pmda_units(1,0,0,PM_SPACE_BYTE,0,0)},
    'ps_eden_space.collection_usage.max' => {
	description	=> '',
	metric_type	=> PM_SEM_INSTANT,
	data_type	=> PM_TYPE_64,
	units	=> pmda_units(1,0,0,PM_SPACE_BYTE,0,0)},
    'ps_eden_space.collection_usage.committed' => {
	description	=> '',
	metric_type	=> PM_SEM_INSTANT,
	data_type	=> PM_TYPE_64,
	units	=> pmda_units(1,0,0,PM_SPACE_BYTE,0,0)},
    'ps_eden_space.collection_usage.init' => {
	description	=> '',
	metric_type	=> PM_SEM_INSTANT,
	data_type	=> PM_TYPE_64,
	units	=> pmda_units(1,0,0,PM_SPACE_BYTE,0,0)},
    'ps_eden_space.collection_usage.used' => {
	description	=> '',
	metric_type	=> PM_SEM_INSTANT,
	data_type	=> PM_TYPE_64,
	units	=> pmda_units(1,0,0,PM_SPACE_BYTE,0,0)},
    'ps_perm_gen.peak_usage.max' => {
	description	=> '',
	metric_type	=> PM_SEM_INSTANT,
	data_type	=> PM_TYPE_64,
	units	=> pmda_units(1,0,0,PM_SPACE_BYTE,0,0)},
    'ps_perm_gen.peak_usage.committed' => {
	description	=> '',
	metric_type	=> PM_SEM_INSTANT,
	data_type	=> PM_TYPE_64,
	units	=> pmda_units(1,0,0,PM_SPACE_BYTE,0,0)},
    'ps_perm_gen.peak_usage.init' => {
	description	=> '',
	metric_type	=> PM_SEM_INSTANT,
	data_type	=> PM_TYPE_64,
	units	=> pmda_units(1,0,0,PM_SPACE_BYTE,0,0)},
    'ps_perm_gen.peak_usage.used' => {
	description	=> '',
	metric_type	=> PM_SEM_INSTANT,
	data_type	=> PM_TYPE_64,
	units	=> pmda_units(1,0,0,PM_SPACE_BYTE,0,0)},
    'ps_perm_gen.usage.max' => {
	description	=> '',
	metric_type	=> PM_SEM_INSTANT,
	data_type	=> PM_TYPE_64,
	units	=> pmda_units(1,0,0,PM_SPACE_BYTE,0,0)},
    'ps_perm_gen.usage.committed' => {
	description	=> '',
	metric_type	=> PM_SEM_INSTANT,
	data_type	=> PM_TYPE_64,
	units	=> pmda_units(1,0,0,PM_SPACE_BYTE,0,0)},
    'ps_perm_gen.usage.init' => {
	description	=> '',
	metric_type	=> PM_SEM_INSTANT,
	data_type	=> PM_TYPE_64,
	units	=> pmda_units(1,0,0,PM_SPACE_BYTE,0,0)},
    'ps_perm_gen.usage.used' => {
	description	=> '',
	metric_type	=> PM_SEM_INSTANT,
	data_type	=> PM_TYPE_64,
	units	=> pmda_units(1,0,0,PM_SPACE_BYTE,0,0)},
    'ps_perm_gen.collection_usage.max' => {
	description	=> '',
	metric_type	=> PM_SEM_INSTANT,
	data_type	=> PM_TYPE_64,
	units	=> pmda_units(1,0,0,PM_SPACE_BYTE,0,0)},
    'ps_perm_gen.collection_usage.committed' => {
	description	=> '',
	metric_type	=> PM_SEM_INSTANT,
	data_type	=> PM_TYPE_64,
	units	=> pmda_units(1,0,0,PM_SPACE_BYTE,0,0)},
    'ps_perm_gen.collection_usage.init' => {
	description	=> '',
	metric_type	=> PM_SEM_INSTANT,
	data_type	=> PM_TYPE_64,
	units	=> pmda_units(1,0,0,PM_SPACE_BYTE,0,0)},
    'ps_perm_gen.collection_usage.used' => {
	description	=> '',
	metric_type	=> PM_SEM_INSTANT,
	data_type	=> PM_TYPE_64,
	units	=> pmda_units(1,0,0,PM_SPACE_BYTE,0,0)},
    'ps_old_gen.peak_usage.max' => {
	description	=> '',
	metric_type	=> PM_SEM_INSTANT,
	data_type	=> PM_TYPE_64,
	units	=> pmda_units(1,0,0,PM_SPACE_BYTE,0,0)},
    'ps_old_gen.peak_usage.committed' => {
	description	=> '',
	metric_type	=> PM_SEM_INSTANT,
	data_type	=> PM_TYPE_64,
	units	=> pmda_units(1,0,0,PM_SPACE_BYTE,0,0)},
    'ps_old_gen.peak_usage.init' => {
	description	=> '',
	metric_type	=> PM_SEM_INSTANT,
	data_type	=> PM_TYPE_64,
	units	=> pmda_units(1,0,0,PM_SPACE_BYTE,0,0)},
    'ps_old_gen.peak_usage.used' => {
	description	=> '',
	metric_type	=> PM_SEM_INSTANT,
	data_type	=> PM_TYPE_64,
	units	=> pmda_units(1,0,0,PM_SPACE_BYTE,0,0)},
    'ps_old_gen.usage.max' => {
	description	=> '',
	metric_type	=> PM_SEM_INSTANT,
	data_type	=> PM_TYPE_64,
	units	=> pmda_units(1,0,0,PM_SPACE_BYTE,0,0)},
    'ps_old_gen.usage.committed' => {
	description	=> '',
	metric_type	=> PM_SEM_INSTANT,
	data_type	=> PM_TYPE_64,
	units	=> pmda_units(1,0,0,PM_SPACE_BYTE,0,0)},
    'ps_old_gen.usage.init' => {
	description	=> '',
	metric_type	=> PM_SEM_INSTANT,
	data_type	=> PM_TYPE_64,
	units	=> pmda_units(1,0,0,PM_SPACE_BYTE,0,0)},
    'ps_old_gen.usage.used' => {
	description	=> '',
	metric_type	=> PM_SEM_INSTANT,
	data_type	=> PM_TYPE_64,
	units	=> pmda_units(1,0,0,PM_SPACE_BYTE,0,0)},
    'ps_old_gen.collection_usage.max' => {
	description	=> '',
	metric_type	=> PM_SEM_INSTANT,
	data_type	=> PM_TYPE_64,
	units	=> pmda_units(1,0,0,PM_SPACE_BYTE,0,0)},
    'ps_old_gen.collection_usage.committed' => {
	description	=> '',
	metric_type	=> PM_SEM_INSTANT,
	data_type	=> PM_TYPE_64,
	units	=> pmda_units(1,0,0,PM_SPACE_BYTE,0,0)},
    'ps_old_gen.collection_usage.init' => {
	description	=> '',
	metric_type	=> PM_SEM_INSTANT,
	data_type	=> PM_TYPE_64,
	units	=> pmda_units(1,0,0,PM_SPACE_BYTE,0,0)},
    'ps_old_gen.collection_usage.used' => {
	description	=> '',
	metric_type	=> PM_SEM_INSTANT,
	data_type	=> PM_TYPE_64,
	units	=> pmda_units(1,0,0,PM_SPACE_BYTE,0,0)},
);

$metricCounter = 0;

foreach my $metricName (sort (keys %memory_pool_metrics)) {
    my %metricDetails = %{$memory_pool_metrics{$metricName}};
    $pmda->add_metric(pmda_pmid(4,$metricCounter), $metricDetails{data_type}, PM_INDOM_NULL,
	$metricDetails{metric_type}, $metricDetails{units},
	'activemq.jvm.memory_pool.' . $metricName, $metricDetails{description}, '');
     $metricCounter++;
}

my %gc_metrics = (
    'ps_mark_sweep.collection_count' => {
	description	=> '',
	metric_type	=> PM_SEM_COUNTER,
	data_type	=> PM_TYPE_64,
	units	=> pmda_units(0,0,1,0,0,PM_COUNT_ONE)},
    'ps_mark_sweep.collection_time' => {
	description	=> '',
	metric_type	=> PM_SEM_COUNTER,
	data_type	=> PM_TYPE_64,
	units	=> pmda_units(0,1,0,0,PM_TIME_MSEC,0)},
    'ps_scavenge.collection_count' => {
	description	=> '',
	metric_type	=> PM_SEM_COUNTER,
	data_type	=> PM_TYPE_64,
	units	=> pmda_units(0,0,1,0,0,PM_COUNT_ONE)},
    'ps_scavenge.collection_time' => {
	description	=> '',
	metric_type	=> PM_SEM_COUNTER,
	data_type	=> PM_TYPE_64,
	units	=> pmda_units(0,1,0,0,PM_TIME_MSEC,0)},
);

$metricCounter = 0;

foreach my $metricName (sort (keys %gc_metrics)) {
    my %metricDetails = %{$gc_metrics{$metricName}};
    $pmda->add_metric(pmda_pmid(5,$metricCounter), $metricDetails{data_type}, PM_INDOM_NULL,
	$metricDetails{metric_type}, $metricDetails{units},
	'activemq.jvm.garbage_collection.' . $metricName, $metricDetails{description}, '');
     $metricCounter++;
}

$pmda->add_indom($queue_indom, \%queue_instances,
		'Instance domain exporting each queue', '');

$pmda->set_fetch_callback(\&activemq_fetch_callback);
$pmda->set_refresh(\&update_activemq_status);
$pmda->set_user('pcp');
$pmda->run;
