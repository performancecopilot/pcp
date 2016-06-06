#!/usr/bin/env perl
#
# Copyright (c) 2012 Red Hat.
# Copyright (c) 2008 Aconex.  All Rights Reserved.
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
use autodie;

use PCP::PMDA;
use IO::Socket::INET;
use File::Spec::Functions qw(catfile);
use Data::Dumper;

use vars qw( $pmda );

# Descriptions retrieved from http://redis.io/commands/INFO

my %cfg = (
    config_fname => "myredis.conf",
    #redis_ports => [6380..6383],
    #remote_host => "localhost",

    metrics     => {
        redis_version => { type => PM_TYPE_STRING,
                           semantics => PM_SEM_DISCRETE,
                           help => "Version of the Redis server" },
        redis_git_sha1 => { type => PM_TYPE_STRING,
                            semantics => PM_SEM_DISCRETE,
                            help => "Git SHA1"},
        redis_git_dirty => { type => PM_TYPE_U32,
                             semantics => PM_SEM_DISCRETE,
                             help => "Git dirty flag" },
        redis_build_id => { type => PM_TYPE_STRING,
                            semantics => PM_SEM_DISCRETE,
                            help => "Build ID" },
        redis_mode => { type => PM_TYPE_STRING,
                        semantics => PM_SEM_DISCRETE,
                        help => "Redis mode",
                        longhelp => "Standalone or cluster (or more?)" },
        os => { type => PM_TYPE_STRING,
                semantics => PM_SEM_DISCRETE,
                help => "Kernel uname line",
                longhelp => "Unix/Linux, kernel version, architecture" },
        arch_bits => { type => PM_TYPE_U32,
                       semantics => PM_SEM_DISCRETE,
                       help => "Number of bits for the architecture",
                       longhelp => "64/32 for x86 and x86_64" },
        multiplexing_api => { type => PM_TYPE_STRING,
                              semantics => PM_SEM_DISCRETE,
                              help => "event loop mechanism used by Redis",
                              longhelp => "Poll/Epoll (or even select?)" },
        gcc_version => { type => PM_TYPE_STRING,
                         semantics => PM_SEM_DISCRETE,
                         help => "Version of gcc used for build",
                         longhelp => "Version of the GCC compiler used to compile the Redis server" },
        process_id => { type => PM_TYPE_U32,
                        semantics => PM_SEM_DISCRETE,
                        help => "Process ID",
                        longhelp => "Process ID of redis instance" },
        run_id  => { type => PM_TYPE_STRING,
                     semantics => PM_SEM_DISCRETE,
                     help => "Random value identifying the Redis server (to be used by Sentinel and Cluster)",
                     longhelp => "Random value identifying the Redis server (to be used by Sentinel and Cluster)" },

        tcp_port => { type => PM_TYPE_U32,
                      semantics => PM_SEM_DISCRETE,
                      help => "TCP/IP listen port",
                      longhelp => "TCP/IP listen port" },
        uptime_in_seconds => { type => PM_TYPE_U32,
                               semantics => PM_SEM_COUNTER,
                               help => "Number of seconds since Redis server start",
                               longhelp => "Number of seconds since Redis server start" },
        uptime_in_days => { type => PM_TYPE_U32,
                            semantics => PM_SEM_COUNTER,
                            help => "Same value expressed in days",
                            longhelp => "Same value expressed in days" },
        hz => { type => PM_TYPE_U32,
                semantics => PM_SEM_DISCRETE,
                help => "TODO",
                longhelp => "TODO" },
        lru_clock => { type => PM_TYPE_U32,
                       semantics => PM_SEM_COUNTER,
                       help => "Clock incrementing every minute, for LRU management",
                       longhelp => "Clock incrementing every minute, for LRU management" },
        config_file => { type => PM_TYPE_STRING,
                         semantics => PM_SEM_DISCRETE,
                         help => "Where the configuration file is placed",
                         longhelp => "Where the configuration file is placed" },

        # Clients
        connected_clients => { type => PM_TYPE_U32,
                               semantics => PM_SEM_INSTANT,
                               help => "Number of client connections (excluding connections from slaves)",
                               longhelp => "Number of client connections (excluding connections from slaves)" },
        client_longest_output_list => { type => PM_TYPE_U32,
                                        semantics => PM_SEM_INSTANT,
                                        help => "longest output list among current client connections",
                                        longhelp => "longest output list among current client connections" },
        client_biggest_input_buf => { type => PM_TYPE_U32,
                                      semantics => PM_SEM_INSTANT,
                                      help => "biggest input buffer among current client connections",
                                      longhelp => "biggest input buffer among current client connections" },
        blocked_clients => { type => PM_TYPE_U32,
                             semantics => PM_SEM_DISCRETE,
                             help => "Number of clients pending on a blocking call (BLPOP, BRPOP, BRPOPLPUSH)",
                             longhelp => "Number of clients pending on a blocking call (BLPOP, BRPOP, BRPOPLPUSH)" },

        #TODO: Check units for all the metrics

        # Memory
        used_memory => { type => PM_TYPE_U32,
                         semantics => PM_SEM_INSTANT,
                         help => "total number of bytes allocated by Redis using its allocator (either standard libc, jemalloc, or an alternative allocator such as tcmalloc",
                         longhelp => "total number of bytes allocated by Redis using its allocator (either standard libc, jemalloc, or an alternative allocator such as tcmalloc" },
        used_memory_human => { type => PM_TYPE_STRING,
                               semantics => PM_SEM_INSTANT,
                               help => "Human readable representation of previous value",
                               longhelp => "Human readable representation of previous value"},
        used_memory_rss => { type => PM_TYPE_U32,
                             semantics => PM_SEM_INSTANT,
                             help => "Number of bytes that Redis allocated as seen by the operating system (a.k.a resident set size).",
                             longhelp => "Number of bytes that Redis allocated as seen by the operating system (a.k.a resident set size)." },
        used_memory_peak => { type => PM_TYPE_U32,
                              semantics => PM_SEM_INSTANT,
                              help => "Peak memory consumed by Redis (in bytes)",
                              longhelp => "Peak memory consumed by Redis (in bytes)" },
        used_memory_peak_human => { type => PM_TYPE_STRING,
                                    semantics => PM_SEM_INSTANT,
                                    help => "Human readable representation of previous value",
                                    longhelp => "Human readable representation of previous value" },
        used_memory_lua => { type => PM_TYPE_U32,
                             semantics => PM_SEM_INSTANT,
                             help => "Number of bytes used by the Lua engine",
                             longhelp => "Number of bytes used by the Lua engine" },
        mem_fragmentation_ratio => { type => PM_TYPE_FLOAT,
                                     semantics => PM_SEM_INSTANT,
                                     help => "Ratio between used_memory_rss and used_memory",
                                     longhelp => "Ratio between used_memory_rss and used_memory" },
        mem_allocator => { type => PM_TYPE_STRING,
                           semantics => PM_SEM_DISCRETE,
                           help => "Memory allocator, chosen at compile time",
                           longhelp => "Memory allocator, chosen at compile time" },

        # Persistence
        loading => { type => PM_TYPE_U32,
                     semantics => PM_SEM_INSTANT,
                     help => "Flag indicating if the load of a dump file is on-going",
                     longhelp => "Flag indicating if the load of a dump file is on-going" },
        rdb_changes_since_last_save => { type => PM_TYPE_U64,
                                         semantics => PM_SEM_INSTANT,
                                         help => "Number of changes since the last dump",
                                         longhelp => "Number of changes since the last dump" },
        rdb_bgsave_in_progress => { type => PM_TYPE_U32,
                                    semantics => PM_SEM_INSTANT,
                                    help => "Flag indicating a RDB save is on-going",
                                    longhelp => "Flag indicating a RDB save is on-going" },
        rdb_last_save_time => { type => PM_TYPE_U32,
                                semantics => PM_SEM_INSTANT,
                                help => "Epoch-based timestamp of last successful RDB save",
                                longhelp => "Epoch-based timestamp of last successful RDB save" },
        rdb_last_bgsave_status => { type => PM_TYPE_STRING,
                                    semantics => PM_SEM_INSTANT,
                                    help => "Status of the last RDB save operation",
                                    longhelp => "Status of the last RDB save operation" },
        rdb_last_bgsave_time_sec => { type => PM_TYPE_32,
                                      semantics => PM_SEM_INSTANT,
                                      help => "Duration of the last RDB save operation in seconds",
                                      longhelp => "Duration of the last RDB save operation in seconds" },
        rdb_current_bgsave_time_sec => { type => PM_TYPE_32,
                                         semantics => PM_SEM_INSTANT,
                                         help => "Duration of the on-going RDB save operation if any",
                                         longhelp => "If set to -1, no bgsaves will be done" },
        aof_enabled => { type => PM_TYPE_STRING,
                         semantics => PM_SEM_INSTANT,
                         help => "Flag indicating AOF logging is activated",
                         longhelp => "Flag indicating AOF logging is activated" },
        aof_rewrite_in_progress => { type => PM_TYPE_32,
                                     semantics => PM_SEM_INSTANT,
                                     help => "Flag indicating a AOF rewrite operation is on-going",
                                     longhelp => "Flag indicating a AOF rewrite operation is on-going" },
        aof_rewrite_scheduled => { type => PM_TYPE_32,
                                   semantics => PM_SEM_INSTANT,
                                   help => "Flag indicating an AOF rewrite operation will be scheduled once the on-going RDB save is complete.",
                                   longhelp => "Flag indicating an AOF rewrite operation will be scheduled once the on-going RDB save is complete." },
        aof_last_rewrite_time_sec => { type => PM_TYPE_32,
                                       semantics => PM_SEM_INSTANT,
                                       help => "Duration of the last AOF rewrite operation in seconds",
                                       longhelp => "Duration of the last AOF rewrite operation in seconds" },
        aof_current_rewrite_time_sec => { type => PM_TYPE_32,
                                          semantics => PM_SEM_INSTANT,
                                          help => "Duration of the on-going AOF rewrite operation if any",
                                          longhelp => "Duration of the on-going AOF rewrite operation if any" },
        aof_last_bgrewrite_status => { type => PM_TYPE_STRING,
                                       semantics => PM_SEM_INSTANT,
                                       help => "Status of the last AOF rewrite operation changes_since_last_save refers to the number of operations that produced some kind of changes in the dataset since the last time either SAVE or BGSAVE was called.",
                                       longhelp => "Status of the last AOF rewrite operation changes_since_last_save refers to the number of operations that produced some kind of changes in the dataset since the last time either SAVE or BGSAVE was called." },
        aof_last_write_status => { type => PM_TYPE_STRING,
                                   semantics => PM_SEM_INSTANT,
                                   help => "N/A",
                                   longhelp => "N/A" },

        # Stats
        total_connections_received => { type => PM_TYPE_32,
                                        semantics => PM_SEM_COUNTER,
                                        help => "Total number of connections accepted by the server",
                                        longhelp => "Total number of connections accepted by the server" },
        total_commands_processed => { type => PM_TYPE_U64,
                                      semantics => PM_SEM_COUNTER,
                                      help => "Total number of commands processed by the server",
                                      longhelp => "Total number of commands processed by the server" },
        instantaneous_ops_per_sec => { type => PM_TYPE_U32,
                                       semantics => PM_SEM_INSTANT,
                                       help => "Number of commands processed per second",
                                       longhelp => "Number of commands processed per second" },
        rejected_connections => { type => PM_TYPE_U32,
                                  semantics => PM_SEM_INSTANT,
                                  help => "Number of connections rejected because of maxclients limit",
                                  longhelp => "Number of connections rejected because of maxclients limit" },
        sync_full => { type => PM_TYPE_U32,
                       semantics => PM_SEM_INSTANT,
                       help => "N/A",
                       longhelp => "N/A" },
        sync_partial_ok => { type => PM_TYPE_U32,
                             semantics => PM_SEM_INSTANT,
                             help => "N/A",
                             longhelp => "N/A" },
        sync_partial_err => { type => PM_TYPE_U32,
                              semantics => PM_SEM_INSTANT,
                              help => "N/A",
                              longhelp => "N/A" },
        expired_keys => { type => PM_TYPE_U64,
                          semantics => PM_SEM_COUNTER,
                          help => "Total number of key expiration events",
                          longhelp => "Total number of key expiration events" },
        evicted_keys => { type => PM_TYPE_U64,
                          semantics => PM_SEM_COUNTER,
                          help => "Number of evicted keys due to maxmemory limit",
                          longhelp => "Number of evicted keys due to maxmemory limit" },
        keyspace_hits => { type => PM_TYPE_U64,
                           semantics => PM_SEM_COUNTER,
                           help => "Number of successful lookup of keys in the main dictionary",
                           longhelp => "Number of successful lookup of keys in the main dictionary" },
        keyspace_misses => { type => PM_TYPE_U64,
                             semantics => PM_SEM_COUNTER,
                             help => "Number of failed lookup of keys in the main dictionary",
                             longhelp => "Number of failed lookup of keys in the main dictionary" },
        pubsub_channels => { type => PM_TYPE_U32,
                             semantics => PM_SEM_INSTANT,
                             help => "Global number of pub/sub channels with client subscriptions",
                             longhelp => "Global number of pub/sub channels with client subscriptions" },
        pubsub_patterns => { type => PM_TYPE_U32,
                             semantics => PM_SEM_INSTANT,
                             help => "Global number of pub/sub pattern with client subscriptions",
                             longhelp => "Global number of pub/sub pattern with client subscriptions" },
        latest_fork_usec => { type => PM_TYPE_U64,
                              semantics => PM_SEM_INSTANT,
                              help => "Duration of the latest fork operation in microseconds",
                              longhelp => "Duration of the latest fork operation in microseconds" },

        # Replication
        role => { type => PM_TYPE_STRING,
                  semantics => PM_SEM_INSTANT,
                  help => q{Value is "master" if the instance is slave of no one, or "slave" if the instance is enslaved to a master. Note that a slave can be master of another slave (daisy chaining).},
                  longhelp => q{Value is "master" if the instance is slave of no one, or "slave" if the instance is enslaved to a master. Note that a slave can be master of another slave (daisy chaining).} },
        connected_slaves => { type => PM_TYPE_U32,
                              semantics => PM_SEM_INSTANT,
                              help => "Number of connected slaves",
                              longhelp => "Number of connected slaves" },
        master_repl_offset => { type => PM_TYPE_32,
                                semantics => PM_SEM_INSTANT,
                                help => "N/A",
                                longhelp => "N/A" },
        repl_backlog_active => { type => PM_TYPE_32,
                                 semantics => PM_SEM_INSTANT,
                                 help => "N/A",
                                 longhelp => "N/A" },
        repl_backlog_size => { type => PM_TYPE_U64,
                               semantics => PM_SEM_INSTANT,
                               help => "N/A",
                               longhelp => "N/A" },
        repl_backlog_first_byte_offset => { type => PM_TYPE_STRING,
                                            semantics => PM_SEM_INSTANT,
                                            help => "N/A",
                                            longhelp => "N/A" },
        repl_backlog_histlen => { type => PM_TYPE_U64,
                                  semantics => PM_SEM_INSTANT,
                                  help => "N/A",
                                  longhelp => "N/A" },

        # CPU
        used_cpu_sys => { type => PM_TYPE_FLOAT,
                          semantics => PM_SEM_INSTANT,
                          help => "System CPU consumed by the Redis server",
                          longhelp => "System CPU consumed by the Redis server" },
        used_cpu_user => { type => PM_TYPE_FLOAT,
                           semantics => PM_SEM_INSTANT,
                           help => "User CPU consumed by the Redis server",
                           longhelp => "User CPU consumed by the Redis server" },
        used_cpu_sys_children => { type => PM_TYPE_FLOAT,
                                   semantics => PM_SEM_INSTANT,
                                   help => "System CPU consumed by the background processes",
                                   longhelp => "System CPU consumed by the background processes" },
        used_cpu_user_children => { type => PM_TYPE_FLOAT,
                                    semantics => PM_SEM_INSTANT,
                                    help => "User CPU consumed by the background processes",
                                    longhelp => "User CPU consumed by the background processes" },

        # Keyspace
        #TODO: Change this to possibility of multiple keyspaces
        db0_keys => { type => PM_TYPE_U64,
                      semantics => PM_SEM_INSTANT,
                      help => "Count of keys in the db0 keyspace",
                      longhelp => "Count of keys in the db0 keyspace" },
        db0_expires => { type => PM_TYPE_U64,
                         semantics => PM_SEM_INSTANT,
                         help => "Count of keys with expiration",
                         longhelp => "Count of keys with expiration" },
        db0_avg_ttl => { type => PM_TYPE_U64,
                         semantics => PM_SEM_INSTANT,
                         help => "Average TTL",
                         longhelp => "Average TTL" },
    },

    debug => 1,
);

# Enable PCP debugging
$ENV{PCP_DEBUG} = 65535;

#print STDERR "Starting myredis PMDA\n";

#TODO: Add host and port metrics to instances
#TODO: Define the domain ID in one file only
#
$pmda = PCP::PMDA->new('myredis', 252);

die "Failed to load config file"
    unless $cfg{loaded} = load_config($pmda,
                                      catfile(pmda_config('PCP_PMDAS_DIR'),"myredis",$cfg{config_fname}));
#$pmda->connect_pmcd;
#mydebug($pmda,"Connected to PMDA");
#TODO: Load configuration file

# Assumption: All the redises offer same metrics (so e.g. no major config changes, same versions)

#my ($refh_redis_keys,$refh_redis_insta_keys) = get_redis_data($cfg{remote_host},$cfg{redis_ports}->[0]);
my ($pmid,$pm_instdom) = (-1,-1);
my $res;

#TODO: Use dynamicaly replace_indom while reading from the socket
#TODO: Consider multiple instances for multiple hosts for remote monitoring
#TODO: Add measurement of request-response time, timeout or error

foreach my $key (sort keys %{$cfg{metrics}}) {
    $pmid++;
    mydebug($pmda,"myredis adding metric myredis.$key using PMID: $pmid ...");

    $res = $pmda->add_metric(
        pmda_pmid(0,$pmid),                                     # PMID
        $cfg{metrics}->{$key}->{type},                            # data type
        #PM_IN_NULL,                                               # indom
        $pm_instdom,                                             # indom
        ($cfg{metrics}->{$key}->{semantics} // PM_SEM_DISCRETE),  # semantics
        pmda_units(0,0,1,0,0,PM_COUNT_ONE),                     # units
        "myredis.$key",                                         # key name
        ($cfg{metrics}->{$key}->{help} // ""),                  # short help
        ($cfg{metrics}->{$key}->{longhelp} // ""));              # long help

    mydebug($pmda,"... returned '" . ($res // "<undef>") . "'");
}

# Add instance domains
$pm_instdom++;
mydebug($pmda,"myredis adding instance domain $pm_instdom," . Dumper($cfg{loaded}{hosts}));
$res = $pmda->add_indom($pm_instdom,
                        $cfg{loaded}{hosts},
                        "Redis instances",
                        "Redis instances in form <host>:<TCP port number>");
mydebug($pmda,"add_indom returned: " . Dumper($res));

#opendir(DIR, $kvm_path) || $pmda->err("pmdakvm failed to open $kvm_path: $!");
#my @metrics = grep {
#    unless (/^\./) {
#	$pmda->add_metric(pmda_pmid(0,$pmid++), PM_TYPE_U64, PM_INDOM_NULL,
#		PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
#		"kvm.$_", '', '');
#	# mydebug($pmda,"pmdakvm added metric kvm.$_\n");
#    }
#} readdir(DIR);
#closedir DIR;

$pmda->set_fetch_callback(\&myredis_fetch_callback);
$pmda->set_user('pcp');
$pmda->run;

# This should never happen as run should never return
mydebug($pmda,"... run terminated");


## Subroutines
sub mydebug {
    my ($pmda,@args) = @_;

    return 1
        unless $cfg{debug};

    chomp @args;
    $pmda->log("DEBUG: $_")
        foreach @args;
}

sub load_config {
    my ($pmda,$in_fname) = @_;
    my $refh_res;

    open my $fh_in,"<",$in_fname;

    my ($host_id,$db_id,$lineno) = (0,0,0);
    while (<$fh_in>) {
        $lineno++;
        chomp;

        if (/\A\s*#/) {
            next
        } elsif (/\A\s*\Z/) {
            next
        } elsif (/\A\s*host\s*=\s*(\S+):(\d+)\s*\Z/) {
            $refh_res->{hosts}->{join(':',$1,$2)} = $host_id++;
        } elsif (/\A\s*db_name\s*=\s*(\S+)\s*\Z/) {
            $refh_res->{dbs}->{$1} = $db_id++;
        } else {
            warn "Unexpected line #$lineno: '$_', skipping it";
        }
    }

    # Check mandatory options
    die "No mandatory keys found"
        unless keys %$refh_res;

    my $err_count = 0;

    mydebug($pmda,Dumper($refh_res));

    #TODO: Check also gethostbyname and port number to be >0 and < 65535
    die "No hosts to be monitored found in '$in_fname'"
        unless exists $refh_res->{hosts} and keys %{$refh_res->{hosts}};
    die "It seems that '$in_fname' contains non-unique hosts entries"
        unless keys %{$refh_res->{hosts}} == $host_id;
    die "No databases to be monitored found in '$in_fname'"
        unless exists $refh_res->{dbs} and keys %{$refh_res->{dbs}};

    $refh_res
}

sub myredis_add_inst_domain {


    ...
}


sub myredis_add_metrics {

    ...
}


sub myredis_instance {


    ...
}


sub myredis_fetch_callback
{
    my ($cluster, $item, $inst) = @_;
    my $metric_name = pmda_pmid_name($cluster, $item);

    mydebug($pmda,"myredis_fetch_callback metric:'$metric_name' cluster:'$cluster', item:'$item' inst:'$inst'");

    # Fetch redis info
    #FIXME - fixed port number
    my $refh_redis_info = get_redis_data($cfg{remote_host},6380);

    return (PM_ERR_INST, 0)
        unless $inst == PM_IN_NULL;
    return (PM_ERR_PMID, 0)
        unless defined $metric_name;
    return (PM_ERR_APPVERSION, 0)
        unless exists $refh_redis_info->{$item} and defined $refh_redis_info->{$item};
    return ($refh_redis_info->{$item}, 1);
}

sub get_redis_data {
    my ($host,$port) = @_;
    my ($refh_keys,$refh_inst_keys);

    mydebug($pmda,"Opening socket to host:'$host', port:'$port'");

    my $socket = IO::Socket::INET->new( PeerAddr => $host,
                                        PeerPort => $port,
                                        Proto    => 'tcp',
                                        Type     => SOCK_STREAM )
        or die "Can't bind : $@\n";;
    print $socket "INFO\r\n";

    my ($ans,@buffer);
    my $lineno = 0;

    while (defined($ans = <$socket>)) {
#    while (defined($ans = $socket->getline)) {
        #TODO: This while should be left in undefined $ans but for some reason it does not work for me
        #      so I applied the last hack assuming that there will always be just a single keyspace.
        #      Althought this is naughty and should be fixed, I need this quickly.

        $lineno++;
        $ans =~ s/\s+\Z//;
        push @buffer,$ans;

        mydebug($pmda,"Line $lineno read");
        # Skip empty lines, comments and strange first line line '$1963'
        next
            if $ans =~ /\A#|\A\s*\Z|\A\$/;

        # Decode the keyspace data
        my ($name,$value);

        if ($ans =~ /keys.*expires.*avg_ttl/) {
            my ($db,$keys,$expires,$avg_ttl) = ($ans =~ /\A(\S+):keys=(\d+),expires=(\d+),avg_ttl=(\d+)/);

            $refh_inst_keys->{keyspace}->{$db} = { keys    => $keys,
                                                   expires => $expires,
                                                   avg_ttl => $avg_ttl };
            mydebug($pmda,"detected: db='$db', keys='$keys', expires='$expires', avg_ttl='$avg_ttl'");

            last;
        } elsif (($name,$value) = ($ans =~ /\A(\S+):([\S\s]+)\Z/)) {
            $refh_keys->{$name} = $value;

            mydebug($pmda,"detected: key='$name', value='$value'");
        } else {
            mydebug($pmda,"Assertion error - unexp line format: '$ans'");
        }
    }

    #$pmda->log("Redis output:" . Dumper(\@buffer));

    close $socket;

    ($refh_keys,$refh_inst_keys)
}
