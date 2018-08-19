#!/usr/bin/env perl
#
# Copyright (c) 2016 Lukas Oliva (plhu@seznam.cz)
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

#
# Notes:
#
# - descriptions of metrics were retrieved from http://redis.io/commands/INFO
# - the PMDA was written thanks to mentoring by fche at #pcp@irc.freenode.net
#     and support of myllynen
# - debugging messages can be enabled by setting $cfg{debug} to true value and
#     are usually visible at /var/log/pcp/pmcd/redis.log including message
#     dumps
# - configuration file is present at ./redis.conf and has following options:
#     - keyspace_name
#       - gives name of the keyspaces whose metrics should be enabled
#
#       Example:
#
#       keyspace_name = db0
#
#     - host = <hostname or IP address>:<port number> - redis host
#       Example: Enabling four redis instances running on localhost
#
#       host=localhost:6380
#       host=localhost:6381
#       host=localhost:6382
#       host=localhost:6383
#
# - static metrics (those present in every INFO command) may be extended (e.g.
#     in case of future metric extension or with extended configuration)
#     in $cfg{metrics}. Variable metrics - those present for every keyspace
#     can be extended in $cfg{variant_metrics}
#
# - few instances of redis can be started using ./t/start_redises.pl
# - to add new metrics, do not forget to increase the id key. Do not change
#     these id values so that the old archives are still readable
#
# TODOs:
#  - complete short and long help lines
#  - add support for multiple changeable db instances
#  - check units of all the metrics
#  - add measurement of request-response time, timeout or error of INFO
#      queries
#  - add some check that at most 1 redis request is performed at a time
#  - use persistent TCP connection to Redis and reconnect only when needed
#    -> use dynamic replace_indom in case of system unavailability
#  - test with IPv6 addresses in configuration
#

use strict;
use warnings;
use autodie;

use PCP::PMDA;
use IO::Socket::INET;
use File::Spec::Functions qw(catfile);
use Time::HiRes   qw(gettimeofday alarm);
use Data::Dumper;

use vars qw( $pmda %cfg %id2metrics %cur_data %var_metrics);

%cfg = (
    config_fname => "redis.conf",

    metrics     => {
        redis_version => { type => PM_TYPE_STRING,
                           semantics => PM_SEM_DISCRETE,
                           help => "Version of the Redis server",
                           id => 0,
                       },
        redis_git_sha1 => { type => PM_TYPE_STRING,
                            semantics => PM_SEM_DISCRETE,
                            help => "Git SHA1",
                            id => 1,
                        },
        redis_git_dirty => { type => PM_TYPE_U32,
                             semantics => PM_SEM_DISCRETE,
                             help => "Git dirty flag",
                             id => 2,
                         },
        redis_build_id => { type => PM_TYPE_STRING,
                            semantics => PM_SEM_DISCRETE,
                            help => "Build ID",
                            id => 3,
                        },
        redis_mode => { type => PM_TYPE_STRING,
                        semantics => PM_SEM_DISCRETE,
                        help => "Redis mode",
                        longhelp => "Standalone or cluster (or more?)",
                        id => 4,
                    },
        os => { type => PM_TYPE_STRING,
                semantics => PM_SEM_DISCRETE,
                help => "Kernel uname line",
                longhelp => "Unix/Linux, kernel version, architecture",
                id => 5,
            },
        arch_bits => { type => PM_TYPE_U32,
                       semantics => PM_SEM_DISCRETE,
                       help => "Number of bits for the architecture",
                       longhelp => "64/32 for x86 and x86_64",
                       id => 6,
                   },
        multiplexing_api => { type => PM_TYPE_STRING,
                              semantics => PM_SEM_DISCRETE,
                              help => "event loop mechanism used by Redis",
                              longhelp => "Poll/Epoll (or even select?)",
                              id => 7,
                          },
        gcc_version => { type => PM_TYPE_STRING,
                         semantics => PM_SEM_DISCRETE,
                         help => "Version of gcc used for build",
                         longhelp => "Version of the GCC compiler used to compile the Redis server",
                         id => 8,
                     },
        process_id => { type => PM_TYPE_U32,
                        semantics => PM_SEM_DISCRETE,
                        help => "Process ID",
                        longhelp => "Process ID of redis instance",
                        id => 9,
                    },
        run_id  => { type => PM_TYPE_STRING,
                     semantics => PM_SEM_DISCRETE,
                     help => "Random value identifying the Redis server (to be used by Sentinel and Cluster)",
                     longhelp => "Random value identifying the Redis server (to be used by Sentinel and Cluster)",
                     id => 10,
                 },

        tcp_port => { type => PM_TYPE_U32,
                      semantics => PM_SEM_DISCRETE,
                      help => "TCP/IP listen port",
                      longhelp => "TCP/IP listen port",
                      id => 11,
                  },
        uptime_in_seconds => { type => PM_TYPE_U32,
                               semantics => PM_SEM_COUNTER,
                               help => "Number of seconds since Redis server start",
                               longhelp => "Number of seconds since Redis server start",
                               id => 12,
                           },
        uptime_in_days => { type => PM_TYPE_U32,
                            semantics => PM_SEM_COUNTER,
                            help => "Same value expressed in days",
                            longhelp => "Same value expressed in days",
                            id => 13,
                        },
        hz => { type => PM_TYPE_U32,
                semantics => PM_SEM_DISCRETE,
                help => "N/A",
                longhelp => "N/A",
                id => 14,
            },
        lru_clock => { type => PM_TYPE_U32,
                       semantics => PM_SEM_COUNTER,
                       help => "Clock incrementing every minute, for LRU management",
                       longhelp => "Clock incrementing every minute, for LRU management",
                       id => 15,
                   },
        config_file => { type => PM_TYPE_STRING,
                         semantics => PM_SEM_DISCRETE,
                         help => "Where the configuration file is placed",
                         longhelp => "Where the configuration file is placed",
                         id => 16,
                     },

        # Clients
        connected_clients => { type => PM_TYPE_U32,
                               semantics => PM_SEM_INSTANT,
                               help => "Number of client connections (excluding connections from slaves)",
                               longhelp => "Number of client connections (excluding connections from slaves)",
                               id => 17,
                           },
        client_longest_output_list => { type => PM_TYPE_U32,
                                        semantics => PM_SEM_INSTANT,
                                        help => "longest output list among current client connections",
                                        longhelp => "longest output list among current client connections",
                                        id => 18,
                                    },
        client_biggest_input_buf => { type => PM_TYPE_U32,
                                      semantics => PM_SEM_INSTANT,
                                      help => "biggest input buffer among current client connections",
                                      longhelp => "biggest input buffer among current client connections",
                                      id => 19,
                                  },
        blocked_clients => { type => PM_TYPE_U32,
                             semantics => PM_SEM_DISCRETE,
                             help => "Number of clients pending on a blocking call (BLPOP, BRPOP, BRPOPLPUSH)",
                             longhelp => "Number of clients pending on a blocking call (BLPOP, BRPOP, BRPOPLPUSH)",
                             id => 20,
                         },

        # Memory
        used_memory => { type => PM_TYPE_U32,
                         semantics => PM_SEM_INSTANT,
                         help => "total number of bytes allocated by Redis using its allocator",
                         longhelp => "Total number of bytes allocated by Redis using its allocator (either\nstandard libc, jemalloc, or an alternative allocator such as tcmalloc)",
                         id => 21
                     },
        used_memory_human => { type => PM_TYPE_STRING,
                               semantics => PM_SEM_INSTANT,
                               help => "Human readable representation of previous value",
                               longhelp => "Human readable representation of previous value",
                               id => 22,
                           },
        used_memory_rss => { type => PM_TYPE_U32,
                             semantics => PM_SEM_INSTANT,
                             help => "Memory Redis allocated as seen by the operating system",
                             longhelp => "Number of bytes that Redis allocated as seen by the operating system\n(a.k.a resident set size).",
                             id => 23
                         },
        used_memory_peak => { type => PM_TYPE_U32,
                              semantics => PM_SEM_INSTANT,
                              help => "Peak memory consumed by Redis (in bytes)",
                              longhelp => "Peak memory consumed by Redis (in bytes)",
                              id => 24,
                          },
        used_memory_peak_human => { type => PM_TYPE_STRING,
                                    semantics => PM_SEM_INSTANT,
                                    help => "Human readable representation of previous value",
                                    longhelp => "Human readable representation of previous value",
                                    id => 25,
                                },
        used_memory_lua => { type => PM_TYPE_U32,
                             semantics => PM_SEM_INSTANT,
                             help => "Number of bytes used by the Lua engine",
                             longhelp => "Number of bytes used by the Lua engine",
                             id => 26,
                         },
        mem_fragmentation_ratio => { type => PM_TYPE_FLOAT,
                                     semantics => PM_SEM_INSTANT,
                                     help => "Ratio between used_memory_rss and used_memory",
                                     longhelp => "Ratio between used_memory_rss and used_memory",
                                     id => 27
                                 },
        mem_allocator => { type => PM_TYPE_STRING,
                           semantics => PM_SEM_DISCRETE,
                           help => "Memory allocator, chosen at compile time",
                           longhelp => "Memory allocator, chosen at compile time",
                           id => 28,
                       },

        # Persistence
        loading => { type => PM_TYPE_U32,
                     semantics => PM_SEM_INSTANT,
                     help => "Flag indicating if the load of a dump file is on-going",
                     longhelp => "Flag indicating if the load of a dump file is on-going",
                     id => 29,
                 },
        rdb_changes_since_last_save => { type => PM_TYPE_U64,
                                         semantics => PM_SEM_INSTANT,
                                         help => "Number of changes since the last dump",
                                         longhelp => "Number of changes since the last dump",
                                         id => 30,
                                     },
        rdb_bgsave_in_progress => { type => PM_TYPE_U32,
                                    semantics => PM_SEM_INSTANT,
                                    help => "Flag indicating a RDB save is on-going",
                                    longhelp => "Flag indicating a RDB save is on-going",
                                    id => 31,
                                },
        rdb_last_save_time => { type => PM_TYPE_U32,
                                semantics => PM_SEM_INSTANT,
                                help => "Epoch-based timestamp of last successful RDB save",
                                longhelp => "Epoch-based timestamp of last successful RDB save",
                                id => 32,
                            },
        rdb_last_bgsave_status => { type => PM_TYPE_STRING,
                                    semantics => PM_SEM_INSTANT,
                                    help => "Status of the last RDB save operation",
                                    longhelp => "Status of the last RDB save operation",
                                    id => 33,
                                },
        rdb_last_bgsave_time_sec => { type => PM_TYPE_32,
                                      semantics => PM_SEM_INSTANT,
                                      help => "Duration of the last RDB save operation in seconds",
                                      longhelp => "Duration of the last RDB save operation in seconds",
                                      id => 34,
                                  },
        rdb_current_bgsave_time_sec => { type => PM_TYPE_32,
                                         semantics => PM_SEM_INSTANT,
                                         help => "Duration of the on-going RDB save operation if any",
                                         longhelp => "If set to -1, no bgsaves will be done",
                                         id => 35,
                                     },
        aof_enabled => { type => PM_TYPE_STRING,
                         semantics => PM_SEM_INSTANT,
                         help => "Flag indicating AOF logging is activated",
                         longhelp => "Flag indicating AOF logging is activated",
                         id => 36,
                     },
        aof_rewrite_in_progress => { type => PM_TYPE_32,
                                     semantics => PM_SEM_INSTANT,
                                     help => "Flag indicating a AOF rewrite operation is on-going",
                                     longhelp => "Flag indicating a AOF rewrite operation is on-going",
                                     id => 37,
                                 },
        aof_rewrite_scheduled => { type => PM_TYPE_32,
                                   semantics => PM_SEM_INSTANT,
                                   help => "Flag indicating an AOF rewrite operation will be scheduled once the on-going RDB save is complete.",
                                   longhelp => "Flag indicating an AOF rewrite operation will be scheduled\nonce the on-going RDB save is complete.",
                                   id => 38,
                               },
        aof_last_rewrite_time_sec => { type => PM_TYPE_32,
                                       semantics => PM_SEM_INSTANT,
                                       help => "Duration of the last AOF rewrite operation in seconds",
                                       longhelp => "Duration of the last AOF rewrite operation in seconds",
                                       id => 39,
                                   },
        aof_current_rewrite_time_sec => { type => PM_TYPE_32,
                                          semantics => PM_SEM_INSTANT,
                                          help => "Duration of the on-going AOF rewrite operation if any",
                                          longhelp => "Duration of the on-going AOF rewrite operation if any",
                                          id => 40,
                                      },
        aof_last_bgrewrite_status => { type => PM_TYPE_STRING,
                                       semantics => PM_SEM_INSTANT,
                                       help => "Status of the last AOF rewrite operation changes_since_last_save",
                                       longhelp => "Status of the last AOF rewrite operation changes_since_last_save refers\nto the number of operations that produced some kind of changes in the\ndataset since the last time either SAVE or BGSAVE was called.",
                                       id => 41,
                                   },
        aof_last_write_status => { type => PM_TYPE_STRING,
                                   semantics => PM_SEM_INSTANT,
                                   help => "N/A",
                                   longhelp => "N/A",
                                   id => 42,
                               },

        # Stats
        total_connections_received => { type => PM_TYPE_32,
                                        semantics => PM_SEM_COUNTER,
                                        help => "Total number of connections accepted by the server",
                                        longhelp => "Total number of connections accepted by the server",
                                        id => 43,
                                    },
        total_commands_processed => { type => PM_TYPE_U64,
                                      semantics => PM_SEM_COUNTER,
                                      help => "Total number of commands processed by the server",
                                      longhelp => "Total number of commands processed by the server",
                                      id => 44,
                                  },
        instantaneous_ops_per_sec => { type => PM_TYPE_U32,
                                       semantics => PM_SEM_INSTANT,
                                       help => "Number of commands processed per second",
                                       longhelp => "Number of commands processed per second",
                                       id => 45,
                                   },
        rejected_connections => { type => PM_TYPE_U32,
                                  semantics => PM_SEM_INSTANT,
                                  help => "Number of connections rejected because of maxclients limit",
                                  longhelp => "Number of connections rejected because of maxclients limit",
                                  id => 46,
                              },
        sync_full => { type => PM_TYPE_U32,
                       semantics => PM_SEM_INSTANT,
                       help => "N/A",
                       longhelp => "N/A",
                       id => 47,
                   },
        sync_partial_ok => { type => PM_TYPE_U32,
                             semantics => PM_SEM_INSTANT,
                             help => "N/A",
                             longhelp => "N/A",
                             id => 48,
                         },
        sync_partial_err => { type => PM_TYPE_U32,
                              semantics => PM_SEM_INSTANT,
                              help => "N/A",
                              longhelp => "N/A",
                              id => 49,
                          },
        expired_keys => { type => PM_TYPE_U64,
                          semantics => PM_SEM_COUNTER,
                          help => "Total number of key expiration events",
                          longhelp => "Total number of key expiration events",
                          id => 50,
                      },
        evicted_keys => { type => PM_TYPE_U64,
                          semantics => PM_SEM_COUNTER,
                          help => "Number of evicted keys due to maxmemory limit",
                          longhelp => "Number of evicted keys due to maxmemory limit",
                          id => 51,
                      },
        keyspace_hits => { type => PM_TYPE_U64,
                           semantics => PM_SEM_COUNTER,
                           help => "Number of successful lookup of keys in the main dictionary",
                           longhelp => "Number of successful lookup of keys in the main dictionary",
                           id => 52,
                       },
        keyspace_misses => { type => PM_TYPE_U64,
                             semantics => PM_SEM_COUNTER,
                             help => "Number of failed lookup of keys in the main dictionary",
                             longhelp => "Number of failed lookup of keys in the main dictionary",
                             id => 53,
                         },
        pubsub_channels => { type => PM_TYPE_U32,
                             semantics => PM_SEM_INSTANT,
                             help => "Global number of pub/sub channels with client subscriptions",
                             longhelp => "Global number of pub/sub channels with client subscriptions",
                             id => 54,
                         },
        pubsub_patterns => { type => PM_TYPE_U32,
                             semantics => PM_SEM_INSTANT,
                             help => "Global number of pub/sub pattern with client subscriptions",
                             longhelp => "Global number of pub/sub pattern with client subscriptions",
                             id => 55,
                         },
        latest_fork_usec => { type => PM_TYPE_U64,
                              semantics => PM_SEM_INSTANT,
                              help => "Duration of the latest fork operation in microseconds",
                              longhelp => "Duration of the latest fork operation in microseconds",
                              id => 56,
                          },

        # Replication
        role => { type => PM_TYPE_STRING,
                  semantics => PM_SEM_INSTANT,
                  help => "Replication \"master\" or \"slave\" role",
                  longhelp => "Value is \"master\" if the instance is slave of no one, or \"slave\" if the\ninstance is enslaved to a master. Note that a slave can be master of\nanother slave (daisy chaining).",
                  id => 57,
              },
        connected_slaves => { type => PM_TYPE_U32,
                              semantics => PM_SEM_INSTANT,
                              help => "Number of connected slaves",
                              longhelp => "Number of connected slaves",
                              id => 58,
                          },
        master_repl_offset => { type => PM_TYPE_32,
                                semantics => PM_SEM_INSTANT,
                                help => "N/A",
                                longhelp => "N/A",
                                id => 59,
                            },
        repl_backlog_active => { type => PM_TYPE_32,
                                 semantics => PM_SEM_INSTANT,
                                 help => "N/A",
                                 longhelp => "N/A",
                                 id => 60,
                             },
        repl_backlog_size => { type => PM_TYPE_U64,
                               semantics => PM_SEM_INSTANT,
                               help => "N/A",
                               longhelp => "N/A",
                               id => 61,
                           },
        repl_backlog_first_byte_offset => { type => PM_TYPE_STRING,
                                            semantics => PM_SEM_INSTANT,
                                            help => "N/A",
                                            longhelp => "N/A",
                                            id => 62,
                                        },
        repl_backlog_histlen => { type => PM_TYPE_U64,
                                  semantics => PM_SEM_INSTANT,
                                  help => "N/A",
                                  longhelp => "N/A",
                                  id => 63,
                              },

        # CPU
        used_cpu_sys => { type => PM_TYPE_FLOAT,
                          semantics => PM_SEM_INSTANT,
                          help => "System CPU consumed by the Redis server",
                          longhelp => "System CPU consumed by the Redis server",
                          id => 64,
                      },
        used_cpu_user => { type => PM_TYPE_FLOAT,
                           semantics => PM_SEM_INSTANT,
                           help => "User CPU consumed by the Redis server",
                           longhelp => "User CPU consumed by the Redis server",
                           id => 65,
                       },
        used_cpu_sys_children => { type => PM_TYPE_FLOAT,
                                   semantics => PM_SEM_INSTANT,
                                   help => "System CPU consumed by the background processes",
                                   longhelp => "System CPU consumed by the background processes",
                                   id => 66,
                               },
        used_cpu_user_children => { type => PM_TYPE_FLOAT,
                                    semantics => PM_SEM_INSTANT,
                                    help => "User CPU consumed by the background processes",
                                    longhelp => "User CPU consumed by the background processes",
                                    id => 67,
                                },
    },

    variant_metrics => {
        # Keyspace
        _keys => { type      => PM_TYPE_U64,
                   semantics => PM_SEM_INSTANT,
                   help      => "Count of keys in the keyspace",
                   longhelp  => "Count of keys in the keyspace",
                   id        => 68,
               },
        _expires => { type      => PM_TYPE_U64,
                      semantics => PM_SEM_INSTANT,
                      help      => "Count of keys with expiration",
                      longhelp  => "Count of keys with expiration",
                      id        => 69,
                  },
        _avg_ttl => { type      => PM_TYPE_U64,
                      semantics => PM_SEM_INSTANT,
                      help      => "Average TTL",
                      longhelp  => "Average TTL",
                      id        => 70,
                  },
    },

    # Maximum time in seconds (may also be a fraction) to keep the data for responses
    max_delta_sec => 0.5,

    # Maximum time in seconds to wait for recv
    recv_wait_sec => 0.5,
    max_recv_len  => 10240,

    debug => 0,
);

$0 = "pmdaredis";

# Enable PCP debugging
$ENV{PCP_DEBUG} = 65535
    if $cfg{debug};

# Config check
config_check(\%cfg);

#print STDERR "Starting redis PMDA\n";
$pmda = PCP::PMDA->new('redis', 24);

die "Failed to load config file"
    unless $cfg{loaded} = load_config(catfile(pmda_config('PCP_PMDAS_DIR'),"redis",$cfg{config_fname}));
$pmda->connect_pmcd;
mydebug("Connected to PMDA");

# Assumption: All the redises offer same metrics (so e.g. no major config changes, same versions)

my ($pm_instdom) = (-1,-1);
my $res;

# Add instance domains - Note that it has to be run before addition of metrics
$pm_instdom++;

mydebug("redis adding instance domain $pm_instdom," . Dumper($cfg{loaded}{hosts}))
    if $cfg{debug};

my $inst = -1;
my $refh_indom = [ map { ++$inst => $_ } sort keys %{$cfg{loaded}{hosts}} ];

$res = $pmda->add_indom($pm_instdom,
                        $refh_indom,
                        "Redis instances",
                        "Redis instances in form <host>:<TCP port number>");
mydebug("add_indom returned: " . Dumper($res))
    if $cfg{debug};

# Add all the metrics
foreach my $metric (sort keys %{$cfg{metrics}}) {
    my $refh_metric = $cfg{metrics}->{$metric};
    my $pmid = $refh_metric->{id};

    mydebug("redis adding metric redis.$metric using PMID: $pmid ...");

    $res = $pmda->add_metric(
        pmda_pmid(0,$pmid),                              # PMID
        $refh_metric->{type},                            # data type
        $pm_instdom,                                     # indom
        ($refh_metric->{semantics} // PM_SEM_DISCRETE),  # semantics
        pmda_units(0,0,1,0,0,PM_COUNT_ONE),              # units
        "redis.$metric",                               # key name
        ($refh_metric->{help} // ""),                    # short help
        ($refh_metric->{longhelp} // ""));               # long help
    $id2metrics{$pmid} = $metric;

    mydebug("... returned '" . ($res // "<undef>") . "'");
}

# ... also variant metrics (those that depend on configuration and keyspace)
foreach my $metric_suffix (sort keys %{$cfg{variant_metrics}}) {
    my $refh_metric = $cfg{variant_metrics}->{$metric_suffix};
    my $pmid = $refh_metric->{id};

    foreach my $keyspace (sort keys %{$cfg{loaded}{dbs}}) {
        my $metric_name = "redis.${keyspace}$metric_suffix";

        mydebug("redis adding metric redis.$keyspace:"
                    . Data::Dumper->Dump([pmda_pmid(0,$pmid),
                                          $refh_metric->{type},
                                          $pm_instdom,
                                          ($refh_metric->{semantics} // PM_SEM_DISCRETE),
                                          pmda_units(0,0,1,0,0,PM_COUNT_ONE),
                                          $metric_name,
                                          ($refh_metric->{help} // ""),
                                          ($refh_metric->{longhelp} // ""),
                                      ],
                                         [qw(PMID data_type instance_domain semantics units metric_name help longhelp)]))
            if $cfg{debug};

        $res = $pmda->add_metric(
            pmda_pmid(0,$pmid),                              # PMID
            $refh_metric->{type},                            # data type
            $pm_instdom,                                     # indom
            ($refh_metric->{semantics} // PM_SEM_DISCRETE),  # semantics
            pmda_units(0,0,1,0,0,PM_COUNT_ONE),              # units
            $metric_name,                                    # metric name
            ($refh_metric->{help} // ""),                    # short help
            ($refh_metric->{longhelp} // ""));               # long help
        $id2metrics{$pmid} = $metric_name;
        $var_metrics{$metric_name}{keyspace} = $keyspace;

        mydebug("... returned '" . ($res // "<undef>") . "'");
    }
}

$pmda->set_fetch(\&redis_fetch);
$pmda->set_fetch_callback(\&redis_fetch_callback);
$pmda->set_user('pcp');
$pmda->run;

# This should never happen as run should never return
mydebug("... run terminated");


## Subroutines
sub mydebug {
    my (@args) = @_;

    return 1
        unless $cfg{debug};

    chomp @args;
    $pmda->log("DEBUG: $_")
        foreach @args;
}

sub config_check {
    my $refh_cfg = @_;

    # Check that there aren't two metrics with same metric IDs
    my ($err_count,%ids);

    $ids{$cfg{metrics}{$_}{id}}{$_}++
        foreach keys %{$cfg{metrics}};
    $ids{$cfg{variant_metrics}{$_}{id}}{$_}++
        foreach keys %{$cfg{variant_metrics}};

    foreach my $id (sort keys %ids) {
        if (keys %{$ids{$id}} > 1) {
            $err_count++;

            warn "ID is used more than once for following metrics: " . join(" ",sort keys %{$ids{$id}});
        }
    }

    die "Errors in configuration found, exiting"
        if $err_count;
}

sub load_config {
    my ($in_fname) = @_;
    my $refh_res;

    open my $fh_in,"<",$in_fname;

    my ($host_id,$db_id,$lineno) = (0,0,0);
    while (my $aline = <$fh_in>) {
        $lineno++;
        chomp $aline;

        if ($aline =~ /\A\s*#/) {
            mydebug("#$lineno: Skipping line '$aline'");

            next
        } elsif ($aline =~ /\A\s*\Z/) {
            mydebug("#$lineno: Skipping line '$aline'");

            next
        } elsif ($aline =~ /\A\s*host\s*=\s*(\S+):(\d+)\s*\Z/) {
            mydebug("#$lineno: host: '$1', port: '$2' from '$aline'");

            $refh_res->{hosts}->{join(':',$1,$2)} = { id   => $host_id++,
                                                      host => $1,
                                                      port => $2 };
        } elsif ($aline =~ /\A\s*db_name\s*=\s*(\S+)\s*\Z/) {
            mydebug("#$lineno: keyspace: '$1' from '$aline'");

            $refh_res->{dbs}->{$1} = $db_id++;
        } else {
            warn "#$lineno: Unexpected line '$aline', skipping it";
        }
    }

    # Check mandatory options
    die "No mandatory keys found"
        unless keys %$refh_res;

    mydebug(Dumper($refh_res))
        if $cfg{debug};

    die "No hosts to be monitored found in '$in_fname'"
        unless exists $refh_res->{hosts} and keys %{$refh_res->{hosts}};
    die "It seems that '$in_fname' contains non-unique hosts entries"
        unless keys %{$refh_res->{hosts}} == $host_id;
    die "No keyspaces to be monitored found in '$in_fname'"
        unless exists $refh_res->{dbs} and keys %{$refh_res->{dbs}};

    # Check that host names/addresses and port numbers are valid
    my $err_count = 0;

    foreach my $host_port (sort keys %{$refh_res->{hosts}}) {
        my ($host,$port);

        unless (($host,$port) = ($host_port =~ /(\S+):(\d+)/)) {
            $err_count++;

            $pmda->err("Failed to detect host name/address and port number from '$host_port'");
            next;
        }

        mydebug("Detected - host: '$host', port: '$port'");

        $err_count++,$pmda->err("Failed to gethostbyname($host)")
            unless $host and gethostbyname $host;
        $err_count++,$pmda->err("Unexpected port number $port")
            unless $port and $port >= 1 and $port <= 65535;
    }

    die "$err_count errors in config file detected, exiting"
        if $err_count;

    $refh_res
}

sub redis_fetch {
    my ($cluster, $item, $inst) = @_;
    my ($t0_sec,$t0_msec) = gettimeofday;
    #my $searched_key = $id2metrics{$item};
    #my $metric_name = pmda_pmid_name($cluster, $item);
    #
    #mydebug("redis_fetch metric:"
    #            . Data::Dumper->Dump([\$metric_name,\$cluster,\$item,\$inst,\$searched_key],
    #                                 [qw(metric_name cluster item inst searched_key)]))
    #    if $cfg{debug};

    # Clean deprecated data for all the instances
    my ($cur_date_sec,$cur_date_msec) = gettimeofday;
    my $cur_date = $cur_date_sec + $cur_date_msec/1e6;

    foreach my $inst (sort keys %{$cfg{loaded}{hosts}}) {
        if (exists $cur_data{$inst} and ($cur_date - $cur_data{$inst}{timestamp}) > $cfg{max_delta_sec}) {
            mydebug("Removing cache data for '$inst' as too old - cur_date: $cur_date, timestamp: $cur_data{$inst}{timestamp}");

            delete $cur_data{$inst};
        }
    }

    mydebug("redis_fetch finished");
}

sub redis_fetch_callback {
    my ($cluster, $item, $inst) = @_;
    my ($t0_sec,$t0_msec) = gettimeofday;
    my $searched_key = $id2metrics{$item};
    my $metric_name = pmda_pmid_name($cluster, $item);

    #mydebug("redis_fetch_callback metric:'$metric_name' cluster:'$cluster', item:'$item' inst:'$inst' -> searched_key: $searched_key");
    mydebug("redis_fetch_callback:"
                . Data::Dumper->Dump([\$metric_name,\$cluster,\$item,\$inst,\$searched_key],
                                     [qw(metric_name cluster item inst searched_key)]))
        if $cfg{debug};

    if ($inst == PM_IN_NULL) {
        # Return error if instance number was not given
        mydebug("Given instance was PM_IN_NULL");

        return (PM_ERR_INST, 0)
    } elsif (not defined $metric_name) {
        # Return error if metric was not given
        mydebug("Given metric is not defined");

        return (PM_ERR_PMID, 0)
    }

    # Get redis hostname and port number from config
    my @host_ports = grep {$cfg{loaded}{hosts}{$_}->{id} == $inst} keys %{$cfg{loaded}{hosts}};

    mydebug(Data::Dumper->Dump([\@host_ports,
                                $cfg{loaded}],
                               [qw(host_ports cfg{loaded})]))
        if $cfg{debug};

    die "Assertion error - more than one hostports seen in loaded config having instance '$inst'"
        if @host_ports > 1;
    die "Assertion error - no hostport of instance '$inst' found in loaded config"
        unless @host_ports;

    my ($host,$port) = split /:/,$host_ports[0];

    warn "Assertion error - no host:port detected in '$host_ports[0]'"
        unless $host and $port;

    mydebug("Host: '$host', port: '$port'");

    # Fetch redis info
    my ($refh_redis_info,$refh_inst_keys);
    my ($cur_date_sec,$cur_date_msec) = gettimeofday;
    my $cur_date = $cur_date_sec + $cur_date_msec/1e6;

    if (exists $cur_data{$host_ports[0]} and ($cur_date - $cur_data{$host_ports[0]}{timestamp}) < $cfg{max_delta_sec}) {
        $refh_redis_info = $cur_data{$host_ports[0]}{redis_info};
        $refh_inst_keys  = $cur_data{$host_ports[0]}{inst_keys};
    } else {
        mydebug("Actual data not found, refetching");

        ($refh_redis_info,$refh_inst_keys) = get_redis_data($host,$port);

        unless (defined $refh_redis_info) {
            $pmda->err("Reading from socket ($host:$port) timed out after $cfg{recv_wait_sec} seconds");

            return (PM_ERR_AGAIN, 0)
        }

        mydebug(Data::Dumper->Dump([$refh_redis_info, $refh_inst_keys],[qw(refh_redis_info refh_inst_keys)]))
            if $cfg{debug};

        my ($cur_date_sec,$cur_date_msec) = gettimeofday;
        my $cur_date = $cur_date + $cur_date_msec/1e6;

        $cur_data{$host_ports[0]}{timestamp} = $cur_date;
        $cur_data{$host_ports[0]}{redis_info} = $refh_redis_info;
        $cur_data{$host_ports[0]}{inst_keys} = $refh_inst_keys;
    }

    my ($t1_sec,$t1_msec) = gettimeofday;
    my $dt = $t1_sec + $t1_msec/1e6 - ($t0_sec + $t0_msec/1e6);

    mydebug("fetch with processing lasted: $dt seconds");

    # Check if the key is a variant metric
    my @found = grep { $_ eq $searched_key } keys %{var_metrics};

    mydebug(Data::Dumper->Dump([\@found],[qw(found)]));

    if (@found > 1) {
        $pmda->err("Assertion error: More than 1 keys found: @found");

        return (PM_ERR_AGAIN, 0)
    } elsif (@found == 1) {
        my $keyspace = $var_metrics{$searched_key}{keyspace};
        my ($key_to_search) = ($searched_key =~ /\Aredis.${keyspace}_(\S+)/);

        mydebug("key '$searched_key'\n", Data::Dumper->Dump([$keyspace,$key_to_search],
                                                            [qw{keyspace key_to_search}]))
            if $cfg{debug};
        mydebug(Data::Dumper->Dump([$refh_inst_keys],[qw(refh_inst_keys)]))
            if $cfg{debug};

        if (exists $refh_inst_keys->{$keyspace}->{$key_to_search}) {
            return ($refh_inst_keys->{$keyspace}->{$key_to_search},1)
        }

        return (PM_ERR_AGAIN, 0);
    }

    if (not(exists $refh_redis_info->{$searched_key} and defined $refh_redis_info->{$searched_key})) {
        # Return error if the atom value was not succesfully retrieved
        mydebug("Required metric '$searched_key' was not found in redis INFO or was undefined");

        return (PM_ERR_APPVERSION, 0)
    } else {
        # Success - return (<value>,<success code>)
        mydebug("Returning success");

        return ($refh_redis_info->{$searched_key}, 1);
    }
}

sub get_redis_data {
    my ($host,$port) = @_;
    my ($refh_keys,$refh_inst_keys);

    mydebug("Opening socket to host:'$host', port:'$port'");

    # Enable autoflush
    local $| = 1;

    my $socket = IO::Socket::INET->new( PeerAddr => $host,
                                        PeerPort => $port,
                                        Proto    => 'tcp',
                                        Type     => SOCK_STREAM );
    unless ($socket) {
        $pmda->err("Can't create socket to host '$host', port: '$port' - $@");

        return undef
    }

    my $size = $socket->send("INFO\r\n");

    mydebug("Sent INFO request with $size bytes");

    my ($cur_resp,$resp,$header,$len) = ("","","",0);

    eval {
        local $SIG{ALRM} = sub {
            mydebug("Alarm timeouted");

            die "Timeout alarm"
        };

        mydebug("Set alarm to $cfg{recv_wait_sec} seconds ...");
        alarm $cfg{recv_wait_sec};

        while ($socket->recv($cur_resp,$cfg{max_recv_len}),$cur_resp) {
            $resp .= $cur_resp;

            if (not $len and not (($header,$len) = ($resp =~ /\A(\$(\d+)[\r\n]+)/))) {
                mydebug("... still do not have enough data to detect header");

                next;
            }

            mydebug("Len: $len, header: '$header', response length: " . length($resp));

            if ($len) {
                # Check length = detected length - header length - 2 Bytes for final /r/n
                if ($header and (length($resp) - length($header) - 2 == $len)) {
                    mydebug("Got the complete response");
                    last;
                }

                mydebug("Still expecting some more data");
            }

            mydebug(Data::Dumper->Dump([\$cur_resp,$!],[q(cur_resp !)]));
        }

        alarm 0;
        mydebug("Alarm disarmed");
    };

    if ($@ and $@ =~ /Timeout alarm/) {
        mydebug("Exception while waiting for recv: '$@'");

        $socket->close;

        return undef;
    }

    mydebug("Response: '$resp'");
    $socket->close;
    mydebug("... socket closed");

    my ($ans,@buffer);
    my $lineno = 0;

    foreach my $ans (split /[\r\n]+/,$resp) {
        $lineno++;
        $ans =~ s/\s+\Z//;
        push @buffer,$ans;

        mydebug("Line #$lineno read: '$ans'");
        # Skip empty lines, comments and strange first line line '$1963'
        if ($ans =~ /\A#|\A\s*\Z|\A\$/) {
            mydebug("... comment or empty line");

            next
        }

        # Decode the keyspace data
        my ($name,$value);

        if ($ans =~ /keys.*expires.*avg_ttl/) {
            my ($db,$keys,$expires,$avg_ttl) = ($ans =~ /\A(\S+):keys=(\d+),expires=(\d+),avg_ttl=(\d+)/);

            mydebug("... db='$db', keys='$keys', expires='$expires', avg_ttl='$avg_ttl'");

            $refh_inst_keys->{$db} = { keys    => $keys,
                                       expires => $expires,
                                       avg_ttl => $avg_ttl };
        } elsif (($name,$value) = ($ans =~ /\A(\S+):([\S\s]+)\Z/)) {
            $refh_keys->{$name} = $value;

            mydebug("... key='$name', value='$value'");
        } else {
            mydebug("Assertion error - unexp line format: '$ans'");
        }
    }

    ($refh_keys,$refh_inst_keys)
}
