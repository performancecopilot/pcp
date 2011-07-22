#
# Copyright (c) 2011 Aconex.  All Rights Reserved.
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
# You should have received a copy of the GNU General Public License along
# with this program; if not, write to the Free Software Foundation, Inc.,
# 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
# 

use strict;
use warnings;
use JSON;
use Switch;
use PCP::PMDA;
use LWP::Simple;

my $es_port = 9200;
my $es_instance = 'localhost';
use vars qw($pmda $es_cluster $es_nodes $es_nodestats);
my $nodes_indom = 0;
my @nodes_instances;
my @nodes_instance_ids;
my @cluster_cache = ( 0, 0, 0 );	# time of last refresh for each cluster

# Configuration files for overriding the above settings
for my $file (  pmda_config('PCP_PMDAS_DIR') . '/elasticsearch/es.conf', './es.conf' ) {
    eval `cat $file` unless ! -f $file;
}
my $baseurl = "http://$es_instance:$es_port/";

# crack json data structure, extract node names
sub es_instances
{
    my $nodeIDs = shift;
    my $i = 0;

    @nodes_instances = ();
    @nodes_instance_ids = ();
    foreach my $node (keys %$nodeIDs) {
	my $name = $nodeIDs->{$node}->{'name'};
	$nodes_instances[$i*2] = $i;
	$nodes_instances[($i*2)+1] = $name;
	$nodes_instance_ids[$i*2] = $i;
	$nodes_instance_ids[($i*2)+1] = $node;
	$i++;
	# $pmda->log("es_instances added node: $name ($node)");
    }
    $pmda->replace_indom($nodes_indom, \@nodes_instances);
}

sub es_refresh
{
    my ($cluster) = @_;
    my $content;
    my $now = time;

    if ($now - $cluster_cache[$cluster] <= 1.0) {
	# $pmda->log("es_refresh $cluster - no refresh needed yet");
	return;
    }

    if ($cluster == 0) {	# Update the cluster metrics
	$content = get($baseurl . "_cluster/health");
	if (defined($content)) {
	    $es_cluster = decode_json($content);
	} else {
	    # $pmda->log("es_refresh $cluster failed $content");
	    $es_cluster = undef;
	}
    } elsif ($cluster == 1) {	# Update the node metrics
	$content = get($baseurl . "_cluster/nodes/stats");
	if (defined($content)) {
	    $es_nodestats = decode_json($content);
	    es_instances($es_nodestats->{'nodes'});
	} else {
	    # $pmda->log("es_refresh $cluster failed $content");
	    $es_nodestats = undef;
	}
    } elsif ($cluster == 2) {	# Update the other node metrics
	$content = get($baseurl . "_cluster/nodes");
	if (defined($content)) {
	    $es_nodes = decode_json($content);
	    es_instances($es_nodes->{'nodes'});
	} else {
	    # $pmda->log("es_refresh $cluster failed $content");
	    $es_nodes = undef;
	}
    }
    $cluster_cache[$cluster] = $now;
}

sub es_lookup_node
{
    my ($json, $inst) = @_;
    my $nodeID = $nodes_instance_ids[$inst+1];
    return $json->{'nodes'}->{$nodeID};
}

sub es_value
{
    my ($value) = @_;

    if (!defined($value)) {
	return (PM_ERR_APPVERSION, 0);
    }
    return ($value, 1);
}

# translate status string into a numeric code for pmie and friends
sub es_status
{
    my ($value) = @_;

    if (!defined($value)) {
	return (PM_ERR_AGAIN, 0);
    } elsif ($value eq "green" || $value eq "false") {
	return (0, 1);
    } elsif ($value eq "yellow" || $value eq "true") {
	return (1, 1);
    } elsif ($value eq "red") {
	return (2, 1);
    }
    return (-1, 1);	# unknown
}

sub es_fetch_callback
{
    my ($cluster, $item, $inst) = @_;
    my ($node, $json);

    # If the PMDA cluster is 0, we return from the $es_cluster scalar
    # $pmda->log("es_fetch_callback: $cluster.$item ($inst)");
    if ($cluster == 0) {
	if (!defined($es_cluster))	{ return (PM_ERR_AGAIN, 0); }
	if ($inst != PM_IN_NULL)	{ return (PM_ERR_INST, 0); }

	switch ($item) {
	    case 0  { return es_value($es_cluster->{'cluster_name'}); }
	    case 1  { return es_value($es_cluster->{'status'}); }
	    case 2  { return es_status($es_cluster->{'timed_out'}); }
	    case 3  { return es_value($es_cluster->{'number_of_nodes'}); }
	    case 4  { return es_value($es_cluster->{'number_of_data_nodes'}); }
	    case 5  { return es_value($es_cluster->{'active_primary_shards'}); }
	    case 6  { return es_value($es_cluster->{'active_shards'}); }
	    case 7  { return es_value($es_cluster->{'relocating_shards'}); }
	    case 8  { return es_value($es_cluster->{'initializing_shards'}); }
	    case 9  { return es_value($es_cluster->{'unassigned_shards'}); }
	    case 10 { return es_status($es_cluster->{'status'}); }
	    else    { return (PM_ERR_PMID, 0); }
	}
    }
    elsif ($cluster == 1) {
	if ($inst == PM_IN_NULL)	{ return (PM_ERR_INST, 0); }
	if ($inst > @nodes_instances)	{ return (PM_ERR_INST, 0); }

	$node = es_lookup_node($es_nodestats, $inst);
	if (!defined($node))		{ return (PM_ERR_AGAIN, 0); }

	switch ($item) {
	    case 0  { return es_value($node->{'indices'}->{'size_in_bytes'}); }
	    case 1  { return es_value($node->{'indices'}->{'docs'}->{'num_docs'}); }
	    case 2  { return es_value($node->{'indices'}->{'docs'}->{'num_docs'}); }
	    case 3  { return es_value($node->{'indices'}->{'cache'}->{'field_evictions'}); }
	    case 4  { return es_value($node->{'indices'}->{'cache'}->{'field_size_in_bytes'}); }
	    case 5  { return es_value($node->{'indices'}->{'cache'}->{'filter_count'}); }
	    case 6  { return es_value($node->{'indices'}->{'cache'}->{'filter_evictions'}); }
	    case 7  { return es_value($node->{'indices'}->{'cache'}->{'filter_size_in_bytes'}); }
	    case 8  { return es_value($node->{'indices'}->{'merges'}->{'current'}); }
	    case 9  { return es_value($node->{'indices'}->{'merges'}->{'total'}); }
	    case 10 { return es_value($node->{'indices'}->{'merges'}->{'total_time_in_millis'}); }
	    case 11 { return es_value($node->{'jvm'}->{'uptime_in_millis'}); }
	    case 12 { return es_value($node->{'jvm'}->{'uptime'}); }
	    case 13 { return es_value($node->{'jvm'}->{'mem'}->{'heap_used_in_bytes'}); }
	    case 14 { return es_value($node->{'jvm'}->{'mem'}->{'heap_committed_in_bytes'}); }
	    case 15 { return es_value($node->{'jvm'}->{'mem'}->{'non_heap_used_in_bytes'}); }
	    case 16 { return es_value($node->{'jvm'}->{'mem'}->{'non_heap_committed_in_bytes'}); }
	    case 17 { return es_value($node->{'jvm'}->{'threads'}->{'count'}); }
	    case 18 { return es_value($node->{'jvm'}->{'threads'}->{'peak_count'}); }
	    case 19 { return es_value($node->{'jvm'}->{'gc'}->{'collection_count'}); }
	    case 20 { return es_value($node->{'jvm'}->{'gc'}->{'collection_time_in_millis'}); }
	    case 21 { return es_value($node->{'jvm'}->{'gc'}->{'collectors'}->{'Copy'}->{'collection_count'}); }
	    case 22 { return es_value($node->{'jvm'}->{'gc'}->{'collectors'}->{'Copy'}->{'collection_time_in_millis'}); }
	    case 23 { return es_value($node->{'jvm'}->{'gc'}->{'collectors'}->{'ParNew'}->{'collection_count'}); }
	    case 24 { return es_value($node->{'jvm'}->{'gc'}->{'collectors'}->{'ParNew'}->{'collection_time_in_millis'}); }
	    case 25 { return es_value($node->{'jvm'}->{'gc'}->{'collectors'}->{'ConcurrentMarkSweep'}->{'collection_count'}); }
	    case 26 { return es_value($node->{'jvm'}->{'gc'}->{'collectors'}->{'ConcurrentMarkSweep'}->{'collection_time_in_millis'}); }
	    else    { return (PM_ERR_PMID, 0); }
	}
    }
    elsif ($cluster == 2) {
	if ($inst == PM_IN_NULL)	{ return (PM_ERR_INST, 0); }
	if ($inst > @nodes_instances)	{ return (PM_ERR_INST, 0); }

	$node = es_lookup_node($es_nodes, $inst);
	if (!defined($node))		{ return (PM_ERR_AGAIN, 0); }

	switch ($item) {
	    case 0  { return es_value($node->{'jvm'}->{'pid'}); }
	    case 1  { return es_value($node->{'jvm'}->{'version'}); }
	    case 2  { return es_value($node->{'jvm'}->{'vm_name'}); }
	    case 3  { return es_value($node->{'jvm'}->{'vm_version'}); }
	    case 4  { return es_value($node->{'jvm'}->{'mem'}->{'heap_init_in_bytes'}); }
	    case 5  { return es_value($node->{'jvm'}->{'mem'}->{'heap_max_in_bytes'}); }
	    case 6  { return es_value($node->{'jvm'}->{'mem'}->{'non_heap_init_in_bytes'}); }
	    case 7  { return es_value($node->{'jvm'}->{'mem'}->{'non_heap_max_in_bytes'}); }
	    else    { return (PM_ERR_PMID, 0); }
	}
    }
    return (PM_ERR_PMID, 0);
}

$pmda = PCP::PMDA->new('elasticsearch', 108);

# cluster stats
$pmda->add_metric(pmda_pmid(0,0), PM_TYPE_STRING, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'elasticsearch.cluster.cluster_name',
		  'Name of the elasticsearch cluster', '');
$pmda->add_metric(pmda_pmid(0,1), PM_TYPE_STRING, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'elasticsearch.cluster.status.colour',
		  'Status (green,yellow,red) of the elasticsearch cluster', '');
$pmda->add_metric(pmda_pmid(0,2), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'elasticsearch.cluster.timed_out',
		  'Timed out status (0:false,1:true) of the elasticsearch cluster',
		  'Maps the cluster timed-out status to a numeric value for alarming');
$pmda->add_metric(pmda_pmid(0,3), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'elasticsearch.cluster.number_of_nodes',
		  'Number of nodes in the elasticsearch cluster', '');
$pmda->add_metric(pmda_pmid(0,4), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'elasticsearch.cluster.number_of_data_nodes',
		  'Number of data nodes in the elasticsearch cluster', '');
$pmda->add_metric(pmda_pmid(0,5), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'elasticsearch.cluster.active_primary_shards',
		  'Number of active primary shards in the elasticsearch cluster', '');
$pmda->add_metric(pmda_pmid(0,6), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'elasticsearch.cluster.active_shards',
		  'Number of primary shards in the elasticsearch cluster', '');
$pmda->add_metric(pmda_pmid(0,7), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'elasticsearch.cluster.relocating_shards',
		  'Number of relocating shards in the elasticsearch cluster', '');
$pmda->add_metric(pmda_pmid(0,8), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'elasticsearch.cluster.initializing_shards',
		  'Number of initializing shards in the elasticsearch cluster', '');
$pmda->add_metric(pmda_pmid(0,9), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'elasticsearch.cluster.unassigned_shards',
		  'Number of unassigned shards in the elasticsearch cluster', '');
$pmda->add_metric(pmda_pmid(0,10), PM_TYPE_32, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'elasticsearch.cluster.status.code',
		  'Status code (0:green,1:yellow,2:red) of the elasticsearch cluster',
		  'Maps the cluster status colour to a numeric value for alarming');

# node stats
$pmda->add_metric(pmda_pmid(1,0), PM_TYPE_U64, $nodes_indom,
		PM_SEM_INSTANT, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		'elasticsearch.nodes.indices.size', '', '');
$pmda->add_metric(pmda_pmid(1,1), PM_TYPE_U64, $nodes_indom,
		PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		'elasticsearch.nodes.docs.count', '', '');
$pmda->add_metric(pmda_pmid(1,2), PM_TYPE_U64, $nodes_indom,
		PM_SEM_INSTANT, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		'elasticsearch.nodes.docs.num_docs', '', '');
$pmda->add_metric(pmda_pmid(1,3), PM_TYPE_U64, $nodes_indom,
		PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		'elasticsearch.nodes.cache.field_evictions', '', '');
$pmda->add_metric(pmda_pmid(1,4), PM_TYPE_U64, $nodes_indom,
		PM_SEM_INSTANT, pmda_units(1,0,0,PM_SPACE_BYTE,0,0),
		'elasticsearch.nodes.cache.field_size', '', '');
$pmda->add_metric(pmda_pmid(1,5), PM_TYPE_U64, $nodes_indom,
		PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		'elasticsearch.nodes.cache.filter_count', '', '');
$pmda->add_metric(pmda_pmid(1,6), PM_TYPE_U64, $nodes_indom,
		PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		'elasticsearch.nodes.cache.filter_evictions', '', '');
$pmda->add_metric(pmda_pmid(1,7), PM_TYPE_U64, $nodes_indom,
		PM_SEM_INSTANT, pmda_units(1,0,0,PM_SPACE_BYTE,0,0),
		'elasticsearch.nodes.cache.filter_size', '', '');
$pmda->add_metric(pmda_pmid(1,8), PM_TYPE_U64, $nodes_indom,
		PM_SEM_INSTANT, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		'elasticsearch.nodes.merges.current', '', '');
$pmda->add_metric(pmda_pmid(1,9), PM_TYPE_U64, $nodes_indom,
		PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		'elasticsearch.nodes.merges.total', '', '');
$pmda->add_metric(pmda_pmid(1,10), PM_TYPE_U64, $nodes_indom,
		PM_SEM_COUNTER, pmda_units(0,1,0,0,PM_TIME_MSEC,0),
		'elasticsearch.nodes.merges.total_time', '', '');
$pmda->add_metric(pmda_pmid(1,11), PM_TYPE_U64, $nodes_indom,
		PM_SEM_INSTANT, pmda_units(0,1,0,0,PM_TIME_MSEC,0),
		'elasticsearch.nodes.jvm.uptime', '', '');
$pmda->add_metric(pmda_pmid(1,12), PM_TYPE_STRING, $nodes_indom,
		PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		'elasticsearch.nodes.jvm.uptime_s', '', '');
$pmda->add_metric(pmda_pmid(1,13), PM_TYPE_U64, $nodes_indom,
		PM_SEM_INSTANT, pmda_units(1,0,0,PM_SPACE_BYTE,0,0),
		'elasticsearch.nodes.jvm.mem.heap_used', '', '');
$pmda->add_metric(pmda_pmid(1,14), PM_TYPE_U64, $nodes_indom,
		PM_SEM_INSTANT, pmda_units(1,0,0,PM_SPACE_BYTE,0,0),
		'elasticsearch.nodes.jvm.mem.heap_committed', '', '');
$pmda->add_metric(pmda_pmid(1,15), PM_TYPE_U64, $nodes_indom,
		PM_SEM_INSTANT, pmda_units(1,0,0,PM_SPACE_BYTE,0,0),
		'elasticsearch.nodes.jvm.mem.non_heap_used', '', '');
$pmda->add_metric(pmda_pmid(1,16), PM_TYPE_U64, $nodes_indom,
		PM_SEM_INSTANT, pmda_units(1,0,0,PM_SPACE_BYTE,0,0),
		'elasticsearch.nodes.jvm.mem.non_heap_committed', '', '');
$pmda->add_metric(pmda_pmid(1,17), PM_TYPE_U32, $nodes_indom,
		PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		'elasticsearch.nodes.jvm.threads.count', '', '');
$pmda->add_metric(pmda_pmid(1,18), PM_TYPE_U32, $nodes_indom,
		PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		'elasticsearch.nodes.jvm.threads.peak_count', '', '');
$pmda->add_metric(pmda_pmid(1,19), PM_TYPE_U64, $nodes_indom,
		PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		'elasticsearch.nodes.jvm.gc.count', '', '');
$pmda->add_metric(pmda_pmid(1,20), PM_TYPE_U64, $nodes_indom,
		PM_SEM_COUNTER, pmda_units(0,1,0,0,PM_TIME_MSEC,0),
		'elasticsearch.nodes.jvm.gc.time', '', '');
$pmda->add_metric(pmda_pmid(1,21), PM_TYPE_U64, $nodes_indom,
		PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		'elasticsearch.nodes.jvm.gc.collectors.Copy.count', '', '');
$pmda->add_metric(pmda_pmid(1,22), PM_TYPE_U64, $nodes_indom,
		PM_SEM_COUNTER, pmda_units(0,1,0,0,PM_TIME_MSEC,0),
		'elasticsearch.nodes.jvm.gc.collectors.Copy.time', '', '');
$pmda->add_metric(pmda_pmid(1,23), PM_TYPE_U64, $nodes_indom,
		PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		'elasticsearch.nodes.jvm.gc.collectors.ParNew.count', '', '');
$pmda->add_metric(pmda_pmid(1,24), PM_TYPE_U64, $nodes_indom,
		PM_SEM_COUNTER, pmda_units(0,1,0,0,PM_TIME_MSEC,0),
		'elasticsearch.nodes.jvm.gc.collectors.ParNew.time', '', '');
$pmda->add_metric(pmda_pmid(1,25), PM_TYPE_U64, $nodes_indom,
		PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		'elasticsearch.nodes.jvm.gc.collectors.CMS.count', '', '');
$pmda->add_metric(pmda_pmid(1,26), PM_TYPE_U64, $nodes_indom,
		PM_SEM_COUNTER, pmda_units(0,1,0,0,PM_TIME_MSEC,0),
		'elasticsearch.nodes.jvm.gc.collectors.CMS.time', '', '');

# node info stats
$pmda->add_metric(pmda_pmid(2,0), PM_TYPE_U32, $nodes_indom,
		PM_SEM_DISCRETE, pmda_units(0,0,0,0,0,0),
		'elasticsearch.nodes.jvm.pid', '', '');
$pmda->add_metric(pmda_pmid(2,1), PM_TYPE_STRING, $nodes_indom,
		PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		'elasticsearch.nodes.jvm.version', '', '');
$pmda->add_metric(pmda_pmid(2,2), PM_TYPE_STRING, $nodes_indom,
		PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		'elasticsearch.nodes.jvm.vm_name', '', '');
$pmda->add_metric(pmda_pmid(2,3), PM_TYPE_STRING, $nodes_indom,
		PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		'elasticsearch.nodes.jvm.vm_version', '', '');
$pmda->add_metric(pmda_pmid(2,4), PM_TYPE_U64, $nodes_indom,
		PM_SEM_INSTANT, pmda_units(1,0,0,PM_SPACE_BYTE,0,0),
		'elasticsearch.nodes.jvm.mem.heap_init', '', '');
$pmda->add_metric(pmda_pmid(2,5), PM_TYPE_U64, $nodes_indom,
		PM_SEM_INSTANT, pmda_units(1,0,0,PM_SPACE_BYTE,0,0),
		'elasticsearch.nodes.jvm.mem.heap_max', '', '');
$pmda->add_metric(pmda_pmid(2,6), PM_TYPE_U64, $nodes_indom,
		PM_SEM_INSTANT, pmda_units(1,0,0,PM_SPACE_BYTE,0,0),
		'elasticsearch.nodes.jvm.mem.non_heap_init', '', '');
$pmda->add_metric(pmda_pmid(2,7), PM_TYPE_U64, $nodes_indom,
		PM_SEM_INSTANT, pmda_units(1,0,0,PM_SPACE_BYTE,0,0),
		'elasticsearch.nodes.jvm.mem.non_heap_max', '', '');

$pmda->add_indom($nodes_indom, \@nodes_instances,
                 'Instance domain exporting each elasticsearch node', '');

$pmda->set_fetch_callback(\&es_fetch_callback);
$pmda->set_refresh(\&es_refresh);
$pmda->run;

=pod

=head1 NAME

pmdaelasticsearch - elasticsearch performance metrics domain agent (PMDA)

=head1 DESCRIPTION

B<pmdaelasticsearch> is a Performance Metrics Domain Agent (PMDA) which exports
metric values from elasticsearch.


=head1 INSTALLATION

This PMDA requires that elasticsearch is running on the local host and
is accepting queries on TCP port 9200


Install the elasticsearch PMDA by using the B<Install> script as root:

	# cd $PCP_PMDAS_DIR/elasticsearch
	# ./Install

To uninstall, do the following as root:

	# cd $PCP_PMDAS_DIR/elasticsearch
	# ./Remove

B<pmdaelasticsearch> is launched by pmcd(1) and should never be executed
directly.  The Install and Remove scripts notify pmcd(1) when
the agent is installed or removed.

=head1 FILES

=over

=item $PCP_PMDAS_DIR/elasticsearch/pmdaelasticsearch.pl

elasticsearch PMDA for PCP

=item $PCP_PMDAS_DIR/elasticsearch/Install

installation script for the B<pmdaelasticsearch> agent

=item $PCP_PMDAS_DIR/elasticsearch/Remove

undo installation script for the B<pmdaelasticsearch> agent

=back

=head1 SEE ALSO

pmcd(1).
