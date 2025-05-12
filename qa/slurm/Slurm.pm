package Slurm;
use strict;
use warnings;

# PMDA uses
# use Slurm qw(:constant);
# so dummy this up ...
my $foo = 42;
use Exporter qw(import);
our @EXPORT = qw($foo);
our %EXPORT_TAGS = ( 
    'constant' => [ 
	qw($foo)
    ]
);

sub new {
    my $self = {};
    bless $self;
    return $self;
};

sub IS_JOB_RUNNING {
    return 1;
};
sub SHOW_DETAIL {
    return 1;
};

sub load_node {

    my $nodemsg = {
          'node_array' => [
                            {
                              'node_state' => 3,
                              'tmp_disk' => 0,
                              'arch' => 'x86_64',
                              'reason_time' => 0,
                              'os' => 'Linux',
                              'sockets' => 2,
                              'weight' => 23,
                              'err_cpus' => 0,
                              'alloc_cpus' => 8,
                              'features' => 'IB,CPU-L5630',
                              'name' => 'cpn-d07-04-01',
                              'boards' => 1,
                              'threads' => 1,
                              'cores' => 4,
                              'slurmd_start_time' => 1441121296,
                              'real_memory' => 23000,
                              'reason_uid' => 4294967294,
                              'boot_time' => 1441121272,
                              'cpu_load' => 875,
                              'cpus' => 8
                            },
                            {
                              'node_state' => 3,
                              'tmp_disk' => 0,
                              'arch' => 'x86_64',
                              'reason_time' => 0,
                              'os' => 'Linux',
                              'sockets' => 2,
                              'weight' => 23,
                              'err_cpus' => 0,
                              'alloc_cpus' => 8,
                              'features' => 'IB,CPU-L5630',
                              'name' => 'cpn-d07-04-02',
                              'boards' => 1,
                              'threads' => 1,
                              'cores' => 4,
                              'slurmd_start_time' => 1441822517,
                              'real_memory' => 23000,
                              'reason_uid' => 4294967294,
                              'boot_time' => 1441121253,
                              'cpu_load' => 952,
                              'cpus' => 8
                            },
        ],
          'last_update' => 1441907249,
          'node_scaling' => 1
        };
    
    return $nodemsg;
};

sub load_jobs {
my $jobmsg = {
          'job_array' => [
                            {
                             'work_dir' => '/projects',
                             'priority' => 775724,
                             'threads_per_core' => 4294967294,
                             'profile' => 0,
                             'eligible_time' => 1441385391,
                             'alloc_node' => 'cpn-d07-04-01',
                             'num_nodes' => 1,
                             'assoc_id' => 3295,
                             'qos' => 'supporters',
                             'show_flags' => 0,
                             'shared' => 0,
                             'pn_min_tmp_disk' => 0,
                             'end_time' => 1442132881,
                             'wait4switch' => 0,
                             'time_min' => 0,
                             'ntasks_per_core' => 4294967295,
                             'job_id' => 4452824,
                             'pn_min_cpus' => 8,
                             'start_time' => 1441873681,
                             'ntasks_per_socket' => 4294967295,
                             'name' => 'Ni-P-Hcp-0',
                             'sockets_per_node' => 4294967294,
                             'alloc_sid' => 6682,
                             'pn_min_memory' => 2147491648,
                             'suspend_time' => 0,
                             'user_id' => 366972,
                             'nodes' => 'cpn-d07-04-01',
                             'partition' => 'largemem',
                             'resize_time' => 0,
                             'batch_flag' => 1,
                             'std_in' => '/dev/null',
                             'submit_time' => 1441385391,
                             'pre_sus_time' => 0,
                             'exc_node_inx' => [],
                             'contiguous' => 0,
                             'nice' => 10000,
                             'req_node_inx' => [],
                             'derived_ec' => 0,
                             'array_job_id' => 0,
                             'std_err' => '/projects/job.err',
                             'exit_code' => 0,
                             'num_cpus' => 8,
                             'command' => '/projects/slurmscript',
                             'req_switch' => 0,
                             'max_cpus' => 0,
                             'array_task_id' => 4294967294,
                             'account' => 'piacct',
                             'std_out' => '/projects/job.out',
                             'cpus_per_task' => 1,
                             'ntasks_per_node' => 8,
                             'group_id' => 104475,
                             'state_reason' => 0,
                             'restart_cnt' => 0,
                             'max_nodes' => 0,
                             'requeue' => 0,
                             'time_limit' => 4320,
                             'job_state' => 1,
                             'cores_per_socket' => 4294967294
                           }
            ],
          'last_update' => 1441912161,
        };

    return $jobmsg;

};

sub sprint_job_info {
    return "JobId=4452824 JobName=Ni-P-Hcp-0 UserId=testuser(366972) GroupId=testgroup(104475) Priority=775724 Nice=0 Account=piacct QOS=normal JobState=RUNNING Reason=None Dependency=(null) Requeue=0 Restarts=0 BatchFlag=1 Reboot=0 ExitCode=0:0 DerivedExitCode=0:0 RunTime=22:23:34 TimeLimit=1-00:00:00 TimeMin=N/A SubmitTime=2016-03-17T11:58:10 EligibleTime=2016-03-17T11:58:10 StartTime=2016-03-17T11:58:11 EndTime=2016-03-18T11:58:11 PreemptTime=None SuspendTime=None SecsPreSuspend=0 Partition=largemem AllocNode:Sid=cpn-d07-04-01:48594 ReqNodeList=cpn-d07-04-01 ExcNodeList=(null) NodeList=cpn-d07-04-01 NumNodes=1 NumCPUs=8 CPUs/Task=1 ReqB:S:C:T=0:0:*:* TRES=(null) Socks/Node=* NtasksPerN:B:S:C=8:0:*:* CoreSpec=0   Nodes=cpn-d07-04-01 CPU_IDs=0-7 Mem=40000 MinCPUsNode=8 MinMemoryNode=40000M MinTmpDiskNode=0 Features=dcv Gres=(null) Reservation=(null) Shared=OK Contiguous=0 Licenses=(null) Network=(null) Command=(null) WorkDir=/projects StdErr=/projects/job.err StdIn=/dev/null StdOut=/projects/job.out Power= SICP=0";
}

1;
