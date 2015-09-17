#
# Copyright (c) 2015 Martins Innus.
# All rights reserved.
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
use Net::Domain qw( hostname );

use Slurm;
use Slurm::Hostlist;

use threads;
use threads::shared;

use vars qw( $pmda $slurm );


# indoms
my ( $slurm_indom, $node_job_indom, $job_indom, $user_indom, $node_indom ) = ( 0, 1, 2, 3, 4);

my $GEN_CLUSTER = 0;
my $NODE_JOB_CLUSTER = 1;
my $GLOBAL_JOB_CLUSTER = 2;
my $NODE_CLUSTER = 3;
my $USER_CLUSTER = 4;
my $PARTITION_CLUSTER = 5;

# Slurm parameters we actually care about
# Order matches the metric item number to make the fetch logic easier
# Names match the struct entries in slurm.h:job_info

my @slurm_job_stats = ( "FILLER_FOR_NUM_JOBS", "job_id", "name", "job_state", "user_id", "batch_host", "submit_time", "start_time", "end_time", "features", "gres", "nodes", "num_nodes", "num_cpus", "ntasks_per_node", "work_dir" );

#
# Shared Variables between slurm gather thread and main pmda thread
#
# Will lock on the variable we want to use.

our %nodejobs :shared = ();
our $numnodes :shared = 0;

#
# End Shared Variables
#

#
# Subs for the slurm polling thread
#

# Who am i. To check for jobs in this host
our $host = hostname;

# Don't need to share these.  Only used in the slurm thread
my $jobs_update_time = 0;
sub slurm_init {
   $slurm = Slurm::new();
}

# Update General Stuff
sub slurm_update_cluster_gen {

    # This could take a while
    # Returns a hash ref that mirrors : node_info_msg_t from slurm.h
    my $nodemsg = $slurm->load_node();

    unless($nodemsg) {
        # This can fail if the slurm controller has not come up yet
        # No good way to figure out if this is just a delay or fatal error
        # So just log it and try again later
        warn "Failed to load node info: " . $slurm->strerror();
        return;
    }

    my @nodes = @{ $nodemsg->{node_array} };

    {
        lock($numnodes);
        $numnodes = @nodes;
    }

}

# Update job information for this node
sub slurm_update_cluster_jobs {

    # Use the time so we only update on changes
    #
    # This could take a while
    # Returns a hash ref where the main element we care about is the job_array array
    my $jobmsg = $slurm->load_jobs($jobs_update_time, 0);

    unless($jobmsg) {
        # This can fail if the slurm controller has not come up yet
        # Or if there is no state change from the previous try.
        # If there is no state change, the main thread will just use existing data.
        # No good way to figure this out
        # So just try again later
        return;
    }

    # Grab the update time to use for the next query
    $jobs_update_time = $jobmsg->{last_update};

    %nodejobs = ();

    {
        lock(%nodejobs);

        for my $job ( @{ $jobmsg->{job_array} } ){

            my $jid = $job->{job_id};
            my $nodelist = $job->{nodes};

            if( $nodelist){

                my $hostlist = Slurm::Hostlist::create( $nodelist );
                my $num_hosts = $hostlist->count();
            
                if ( $num_hosts > 0){
                    my $mypos = $hostlist->find($host);
                    if( $mypos >= 0){
                        # We only want jobs that are on this host
                        if ( Slurm::IS_JOB_RUNNING($job) ){
                            # And Running
                       
                            # Ugh, shared perl hashes
                            $nodejobs{$jid} = &share( {} );
                            while ( my ($key, $value) = each(%$job)){
                                # Only grab the things we really want, to keep memory down
                                if ( grep{$_ eq $key} @slurm_job_stats ){
                                    $nodejobs{$jid}{$key} = $value;
                                }
                            }
                        }
                    }
                }
            }
        }
    } # unlock %nodejobs
}

# slurm fetch worker thread
sub poll_slurm {
    while (1){
        slurm_update_cluster_gen();
        slurm_update_cluster_jobs();
        sleep 9;
    }
}

#
# Subs for the main thread
#

#
# fetch is called once by pcp for each refresh and then the fetch callback is
# called to query each statistic individually
#
sub slurm_fetch {

        my @nodejobs_array;

        {
            lock(%nodejobs);

            # Create a [key1, "key1", key2, "key2", .....] array so we can control the inst ids
            while ( my ($key, $val) = each %nodejobs){
                push( @nodejobs_array, $key );
                push( @nodejobs_array, "$key" );
            }
        } # unlock %nodejobs

        $pmda->replace_indom($node_job_indom, \@nodejobs_array);
}

sub slurm_fetch_node_job_callback {
    my ($item, $inst) = @_;

    if ( $item == 0 ){
        my $numjobs;
        {
            lock(%nodejobs);
            $numjobs = keys %nodejobs;
        }
        return ($numjobs, 1);
    }

    my $lookup;
    {
        lock(%nodejobs);
        # Will unlock on scope loss, return or otherwise
        if( !exists $nodejobs{$inst} ){
            return (PM_ERR_INST, 0);
        }
        else {
            $lookup = $nodejobs{$inst};
        }
    }

    if ( $item == 3 ){
        # slurm.node.job.state
        # List all jobs as running since those are the only jobs we grabbed
        return ("running", 1);
    }
    elsif ( $item < scalar @slurm_job_stats ){
        my $rv = $lookup->{ $slurm_job_stats[$item] };
        $rv = "" if !defined $rv;
        return ( $rv, 1);
    }
    else{
        return (PM_ERR_PMID, 0);
    }
}

sub slurm_fetch_callback {
    my ($cluster, $item, $inst) = @_; 

    my $pmid_name = pmda_pmid_name($cluster, $item)
        or return (PM_ERR_PMID, 0);
        #or die "Unknown metric name: cluster $cluster item $item\n";

    if( $cluster == 0 && $item == 0 ){
        return ($numnodes, 1);
    }
    elsif( $cluster == $NODE_JOB_CLUSTER){
        return slurm_fetch_node_job_callback( $item, $inst );
    }
    else{
        return (PM_ERR_PMID, 0);
    }
}

# the PCP::PMDA->new line is parsed by the check_domain rule of the PMDA build
# process, so there are special requirements:  no comments, the domain has to
# be a bare number.
#
our $pmda = PCP::PMDA->new('slurm', 23);

# metrics go here, with full descriptions

# general - cluster 0
$pmda->add_metric(pmda_pmid($GEN_CLUSTER,0), PM_TYPE_U32, PM_INDOM_NULL,
    PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
    'slurm.numnodes',
    'Number of slurm nodes',
    '');

# To add - cluster name, controller name, slurm version

# node job cluster 
$pmda->add_metric(pmda_pmid($NODE_JOB_CLUSTER,0), PM_TYPE_U32, PM_INDOM_NULL,
    PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
    'slurm.node.job.numjobs',
    'Number of jobs running on this node',
    '');

$pmda->add_metric(pmda_pmid($NODE_JOB_CLUSTER,1), PM_TYPE_U32, $node_job_indom,
    PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
    'slurm.node.job.id',
    'Slurm jobid',
    '');

$pmda->add_metric(pmda_pmid($NODE_JOB_CLUSTER,2), PM_TYPE_STRING, $node_job_indom,
    PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
    'slurm.node.job.name',
    'Job name',
    '');

$pmda->add_metric(pmda_pmid($NODE_JOB_CLUSTER,3), PM_TYPE_STRING, $node_job_indom,
    PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
    'slurm.node.job.state',
    'Job state',
    '');

$pmda->add_metric(pmda_pmid($NODE_JOB_CLUSTER,4), PM_TYPE_U32, $node_job_indom,
    PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
    'slurm.node.job.user_id',
    'UID owning this job',
    '');

$pmda->add_metric(pmda_pmid($NODE_JOB_CLUSTER,5), PM_TYPE_STRING, $node_job_indom,
    PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
    'slurm.node.job.batch_host',
    'First host in the allocation, the one running the batch script',
    '');

$pmda->add_metric(pmda_pmid($NODE_JOB_CLUSTER,6), PM_TYPE_STRING, $node_job_indom,
    PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
    'slurm.node.job.submit_time',
    'Time the job was submitted',
    '');

$pmda->add_metric(pmda_pmid($NODE_JOB_CLUSTER,7), PM_TYPE_STRING, $node_job_indom,
    PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
    'slurm.node.job.start_time',
    'Time the job started',
    '');

$pmda->add_metric(pmda_pmid($NODE_JOB_CLUSTER,8), PM_TYPE_STRING, $node_job_indom,
    PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
    'slurm.node.job.end_time',
    'Time the job ended',
    '');

$pmda->add_metric(pmda_pmid($NODE_JOB_CLUSTER,9), PM_TYPE_STRING, $node_job_indom,
    PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
    'slurm.node.job.features',
    'Features requested by the job',
    '');

$pmda->add_metric(pmda_pmid($NODE_JOB_CLUSTER,10), PM_TYPE_STRING, $node_job_indom,
    PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
    'slurm.node.job.gres',
    'Generic resources (gres) requested by the job',
    '');

$pmda->add_metric(pmda_pmid($NODE_JOB_CLUSTER,11), PM_TYPE_STRING, $node_job_indom,
    PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
    'slurm.node.job.nodes',
    'Nodes allocated to the job',
    '');

$pmda->add_metric(pmda_pmid($NODE_JOB_CLUSTER,12), PM_TYPE_U32, $node_job_indom,
    PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
    'slurm.node.job.num_nodes',
    'Number of nodes requested by the job',
    '');

$pmda->add_metric(pmda_pmid($NODE_JOB_CLUSTER,13), PM_TYPE_U32, $node_job_indom,
    PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
    'slurm.node.job.num_cpus',
    'Total cpus requested by the job',
    '');

$pmda->add_metric(pmda_pmid($NODE_JOB_CLUSTER,14), PM_TYPE_U32, $node_job_indom,
    PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
    'slurm.node.job.ntasks_per_node',
    'Number of tasks per node requested by the job',
    '');

$pmda->add_metric(pmda_pmid($NODE_JOB_CLUSTER,15), PM_TYPE_STRING, $node_job_indom,
    PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
    'slurm.node.job.work_dir',
    'Working directory of the job',
    '');

# To add - batch script, alloc_node, account, partition ?

# global job cluster 
#
# ( priorities for queued jobs, running jobs keep last priortiy)
# Need to add priority func to api/job_info.c take from sprio.c
# then perl wrap.  need to use list fcns

# user cluster

# node cluster

# per cluster cluster - may mirror metrics in general above ???

&slurm_init;

# Thread that queries the slurm state.
#
# Slurm calls can block for a while so need a seperate thread
# so pmcd doesn't kill us if fetch's take too long.

my $slurm_thread = threads->create(\&poll_slurm);
$slurm_thread->detach();

$node_job_indom = $pmda->add_indom($node_job_indom, [], '', '');

$pmda->set_fetch(\&slurm_fetch);
$pmda->set_fetch_callback(\&slurm_fetch_callback);

$pmda->run;
