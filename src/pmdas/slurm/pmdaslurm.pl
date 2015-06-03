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

use Data::Dumper;

use PCP::PMDA;
use Slurm;
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


#
# Shared Variables between slurm gather thread and main pmda thread
#
# Will lock on the variable we want to use.


our %jobs :shared = ();
our %nodejobs :shared = ();
#our %users :shared = ();
our %nodes :shared = ();

our $numnodes :shared = 0;

#
# End Shared Variables
#


# Who am i. To check for jobs in this host
our $host = hostname;

# Array ref for all jobs
our $all_jobs_ref;

sub slurm_init {
   # Don't need to share this.  Only used in the slurm thread
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

sub slurm_update_jobs {

    # TODO: use the time so we only update on changes
    #
    # This could take a while
    # Returns a hash ref where the only element we care about is the job_array array
    my $jobmsg = $slurm->load_jobs(0, 0);

    unless($jobmsg) {
        # This can fail if the slurm controller has not come up yet
        # No good way to figure out if this is just a delay or fatal error
        # So just log it and try again later
        warn "Failed to load job info: " . $slurm->strerror();
        return;
    }

    # The array holds hash refs that map to: job_info_t from slurm.h
    $all_jobs_ref = \@{ $jobmsg->{job_array} };

    %jobs = ();

    {
        lock(%jobs);

        for my $job ( @$all_jobs_ref ){
            my $jid = $job->{job_id};

            # Ugh, shared perl hashes
            $jobs{$jid} = &share( {} );
            while ( my ($key, $value) = each(%$job)){
                # Just do simple scalar types for now
                # This will miss select_jobinfo and others like that
                # Will need to look into the types for those
                if( !ref($value) ){
                    #warn("Trying to add $key : $value to shared job hash\n");
                    $jobs{$jid}{$key} = $value;
                }
            }
        }
    } # unlock %jobs
}

# Update jobs specific to this node
sub slurm_update_cluster_node_job {

    # Populate the all jobs reference
    &slurm_update_jobs;

    %nodejobs = ();

    {
        lock(%nodejobs);

        # Find all jobs where I am a member
        for my $job ( @$all_jobs_ref ){
            my $nodelist = $job->{nodes};
            my $jid = $job->{job_id};
        
            if ( !(defined $nodelist and length $nodelist) ){
                # We only care about jobs that are running
                next;
            }
        
            my $hostlist = Slurm::Hostlist::create( $nodelist );
            my $num_hosts = $hostlist->count();
        
            if ( $num_hosts > 0){
                my $mypos = $hostlist->find("k05n28");
                #my $mypos = $hostlist->find($host);
                if( $mypos >= 0){
                    # We only want jobs that are on this host

                    # Ugh, shared perl hashes
                    $nodejobs{$jid} = &share( {} );
                    while ( my ($key, $value) = each(%$job)){
                        # Same comment as above
                        if( !ref($value) ){
                            $nodejobs{$jid}{$key} = $value;
                        }
                    }
                }
            }
        }
    } #unlock %nodejobs
}

# slurm fetch worker thread
sub poll_slurm {


    while (1){
        slurm_update_cluster_gen();
        slurm_update_cluster_node_job();

        sleep 9;
    }
}

#
# fetch is called once by pcp for each refresh and then the fetch callback is
# called to query each statistic individually
#
sub slurm_fetch {

        # Done now in worker thread
	#slurm_update_cluster_gen();
        #slurm_update_cluster_node_job();

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

    if ( $item == 1 ){
        # slurm.node.job.id
        my $rv = $lookup->{job_id};
        return ($rv, 1);
    }
    elsif ( $item == 2 ){
        # slurm.node.job.name
        my $rv = $lookup->{name};
        return ($rv, 1);
    }
    elsif ( $item == 3 ){
        # slurm.node.job.state
        # List all jobs as running for now.  Not sure of the timing issues for starting or ending jobs
        return ("running", 1);
    }
    elsif ( $item == 4 ){
        # slurm.node.job.user_id
        my $rv = $lookup->{user_id};
        return ( $rv, 1);
    }
    elsif ( $item == 5 ){
        # slurm.node.job.batch_host
        my $rv = $lookup->{batch_host};
        $rv = "" if !defined $rv;
        return ( $rv, 1);
    }
    elsif ( $item == 6 ){
        # slurm.node.job.submit_time
        my $rv = $lookup->{submit_time};
        return ( $rv, 1);
    }
    elsif ( $item == 7 ){
        # slurm.node.job.start_time
        my $rv = $lookup->{start_time};
        return ( $rv, 1);
    }
    elsif ( $item == 8 ){
        # slurm.node.job.end_time
        my $rv = $lookup->{end_time};
        return ( $rv, 1);
    }
    elsif ( $item == 9 ){
        # slurm.node.job.features
        my $rv = $lookup->{features};
        $rv = "" if !defined $rv;
        return ( $rv, 1);
    }
    elsif ( $item == 10 ){
        # slurm.node.job.gres
        my $rv = $lookup->{gres};
        $rv = "" if !defined $rv;
        return ( $rv, 1);
    }
    elsif ( $item == 11 ){
        # slurm.node.job.nodes
        my $rv = $lookup->{nodes};
        return ( $rv, 1);
    }
    elsif ( $item == 12 ){
        # slurm.node.job.num_nodes
        my $rv = $lookup->{num_nodes};
        return ( $rv, 1);
    }
    elsif ( $item == 13 ){
        # 'slurm.node.job.num_cpus
        my $rv = $lookup->{num_cpus};
        return ( $rv, 1);
    }
    elsif ( $item == 14 ){
        # slurm.node.job.ntasks_per_node
        my $rv = $lookup->{ntasks_per_node};
        return ( $rv, 1);
    }
    elsif ( $item == 15 ){
        # slurm.node.job.work_dir
        my $rv = $lookup->{work_dir};
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
