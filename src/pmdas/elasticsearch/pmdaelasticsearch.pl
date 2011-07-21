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
use PCP::PMDA;
use JSON;
use LWP::Simple;
use Switch;

use vars qw($pmda $id);



# Scalar to hold the state of the cluster
my $es_cluster;

sub update_es_status
{
	my $url = "http://localhost:9200/_cluster/health";
	my $content = get($url);

	# Update the es_cluster scalar
	$es_cluster = decode_json($content);


}

sub es_fetch_callback
{
    my ($cluster, $item, $inst) = @_;

    # If the PMDA cluster is 0, we return from the $es_cluster scalar
    if ($cluster == 0){
	switch($item){
		case 0	{ return ($es_cluster->{'cluster_name'}, 1); }
		case 1	{ return ($es_cluster->{'status'}, 1); }
		case 2	{ return ($es_cluster->{'timed_out'}, 1); }
		case 3  { return ($es_cluster->{'number_of_nodes'}, 1); }
		case 4	{ return ($es_cluster->{'number_of_data_nodes'}, 1); }
		case 5	{ return ($es_cluster->{'active_primary_shards'}, 1); }
		case 6	{ return ($es_cluster->{'active_shards'}, 1); }
		case 7	{ return ($es_cluster->{'relocating_shards'}, 1); }
		case 8	{ return ($es_cluster->{'initializing_shards'}, 1); }
		case 9	{ return ($es_cluster->{'unassigned_shards'}, 1); }
		else	{ return (PM_ERR_PMID, 0); }
	}
    }
    
    return (PM_ERR_PMID, 0);
}

$pmda = PCP::PMDA->new('elasticsearch', 108);

# elasticsearch.cluster.cluster_name
$pmda->add_metric(pmda_pmid(0,0), PM_TYPE_STRING, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'elasticsearch.cluster.cluster_name',
		  'Name of the elasticsearch cluster', '');
# elasticsearch.cluster.status
$pmda->add_metric(pmda_pmid(0,1), PM_TYPE_STRING, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'elasticsearch.cluster.status',
		  'Status (green,yellow,red) of the elasticsearch cluster', '');

# elasticsearch.cluster.timed_out - Ideally type would be boolean
$pmda->add_metric(pmda_pmid(0,2), PM_TYPE_STRING, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'elasticsearch.cluster.timed_out',
		  'Timed out status of the elasticsearch cluster', '');

# elasticsearch.cluster.number_of_nodes
$pmda->add_metric(pmda_pmid(0,3), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'elasticsearch.cluster.number_of_nodes',
		  'Number of nodes in the elasticsearch cluster', '');

# elasticsearch.cluster.number_of_data_nodes
$pmda->add_metric(pmda_pmid(0,4), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'elasticsearch.cluster.number_of_data_nodes',
		  'Number of data nodes in the elasticsearch cluster', '');

# elasticsearch.cluster.active_primary_shards
$pmda->add_metric(pmda_pmid(0,5), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'elasticsearch.cluster.active_primary_shards',
		  'Number of active primary shards in the elasticsearch cluster', '');

# elasticsearch.cluster.active_shards
$pmda->add_metric(pmda_pmid(0,6), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'elasticsearch.cluster.active_shards',
		  'Number of primary shards in the elasticsearch cluster', '');

# elasticsearch.cluster.relocating_shards
$pmda->add_metric(pmda_pmid(0,7), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'elasticsearch.cluster.relocating_shards',
		  'Number of relocating shards in the elasticsearch cluster', '');

# elasticsearch.cluster.initializing_shards
$pmda->add_metric(pmda_pmid(0,8), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'elasticsearch.cluster.initializing_shards',
		  'Number of initializing shards in the elasticsearch cluster', '');

# elasticsearch.cluster.unassigned_shards
$pmda->add_metric(pmda_pmid(0,9), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'elasticsearch.cluster.unassigned_shards',
		  'Number of unassigned shards in the elasticsearch cluster', '');

update_es_status();

$pmda->set_fetch_callback(\&es_fetch_callback);
$pmda->set_fetch(\&update_es_status);
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
