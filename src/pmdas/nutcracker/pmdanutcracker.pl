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
# - descriptions of metrics were retrieved from
#     https://github.com/twitter/twemproxy#observability
# - the PMDA was written thanks to mentoring by fche at #pcp@irc.freenode.net
#     and support of myllynen
# - debugging messages can be enabled by setting $cfg{debug} to true value and
#     are usually visible at /var/log/pcp/pmcd/nutcracker.log including
#     message dumps
# - configuration file is present at ./nutcracker.conf and has following options:
#     - host = <hostname or IP address>:<port number> - NutCracker host
#       Example: Enabling two NutCracker instances running on localhost
#
#       host=localhost:13000
#       host=otherhost:13001
#
# - static metrics (those present in every JSON output) may be extended (e.g.
#     in case of future metric extension or with extended configuration)
#     in $cfg{metrics}. Variable metrics - those present for every keyspace
#     can be extended in $cfg{variant_metrics}
# - few instances of nutcracker and redis can be started using ./t/start_nutcracker.pl
# - to add new metrics, do not forget to increase the id key. Do not change
#     these id values so that the old archives are still readable
#
# TODOs:
#  - complete adoption from myredis to nutcracker
#  - complete short and long help lines
#  - check units of all the metrics
#  - define the domain ID in one file only (while currently there are more)
#  - add hostname:port to the metrics, so that the mapping can be checked
#  - add measurement of request-response time, timeout or error of INFO
#      queries
#  - add some check that at most 1 NutCracker request is performed at a time
#  - add configurable timer for fetch on-time error check (< 1 sec)
#  - use persistent TCP connection to NutCracker and reconnect only when needed
#    -> use dynamic replace_indom in case of system unavailability
#  - make the memoization resistant to time changes (e.g. read counting?)
#

use strict;
use warnings;
use autodie;
use feature qw{state};

use PCP::PMDA;
use IO::Socket::INET;
use File::Spec::Functions qw(catfile);
use Time::HiRes   qw(gettimeofday);
use JSON          qw(decode_json);;
use YAML::XS      qw(LoadFile);
use Data::Dumper;

use vars qw($pmda %cfg %id2metrics %indom2name %cur_data %indom2ids);

# Important variables:
#
# $pmda       - the PMDA object
# %cfg        - configuration hash holding metric description and important constants
# %id2metrics - map of metric IDs to the metric names in PMNS
# %indom2name - map of instance domain IDs to its readable name
# %cur_data   - hash storing current values of fetched data

%cfg = (
    config_fname => "nutcracker.conf",

    general_metrics     => {
        source => { type => PM_TYPE_STRING,
                    semantics => PM_SEM_DISCRETE,
                    help => "Hostname of the host running nutcracker",
                    id => 0,
                },
        service => { type => PM_TYPE_STRING,
                     semantics => PM_SEM_DISCRETE,
                     help => "Redis or memcached servers on the server side",
                     id => 1,
                 },
        uptime => { type => PM_TYPE_U32,
                    semantics => PM_SEM_DISCRETE,
                    help => "Uptime of Nutcracker service in seconds",
                    id => 2,
                },
        version => { type => PM_TYPE_STRING,
                     semantics => PM_SEM_DISCRETE,
                     help => "Nutcrakcer version",
                     id => 3,
                 },
        timestamp => { type => PM_TYPE_STRING,
                       semantics => PM_SEM_COUNTER,
                       help => "Unix time of data capture",
                       longhelp => "Standalone or cluster (or more?)",
                       id => 4,
                   },
        total_connections => { type      => PM_TYPE_U64,
                               semantics => PM_SEM_INSTANT,
                               help      => "Number of total connections",
                               id        => 23,
                           },
        curr_connections => { type      => PM_TYPE_U64,
                              semantics => PM_SEM_INSTANT,
                              help      => "Number of total connections",
                              id        => 24,
                          },
    },

    pool_metrics => {
        forward_error => { type      => PM_TYPE_U64,
                           semantics => PM_SEM_COUNTER,
                           help      => "Number of times that forwarding error was encountered per pool",
                           id        => 5,
                       },
        client_connections => { type      => PM_TYPE_U64,
                                semantics => PM_SEM_INSTANT,
                                help      => "Count of active client connections per pool",
                                id        => 6,
                            },
        client_err => { type      => PM_TYPE_U64,
                        semantics => PM_SEM_COUNTER,
                        help      => "Count of errors on client connections",
                        id        => 7,
                    },
        client_eof => { type      => PM_TYPE_U64,
                        semantics => PM_SEM_COUNTER,
                        help      => "Count of EOF on client connections",
                        id        => 8,
                    },
        fragments => { type      => PM_TYPE_U64,
                       semantics => PM_SEM_COUNTER,
                       help      => "Count of fragments created from a multi-vector request",
                       id        => 9,
                   },
        server_ejects => { type      => PM_TYPE_U64,
                           semantics => PM_SEM_COUNTER,
                           help      => "Number of times backend server was ejected",
                           id        => 10,
                       },
    },

    server_metrics => {
        requests => { type      => PM_TYPE_U64,
                      semantics => PM_SEM_COUNTER,
                      help      => "Number of requests",
                      id        => 11,
                  },
        request_bytes => { type      => PM_TYPE_U64,
                           semantics => PM_SEM_COUNTER,
                           help      => "Total request bytes",
                           id        => 12,
                       },
        responses => { type      => PM_TYPE_U64,
                       semantics => PM_SEM_COUNTER,
                       help      => "Number of requests",
                       id        => 13,
                   },
        response_bytes => { type      => PM_TYPE_U64,
                            semantics => PM_SEM_COUNTER,
                            help      => "Total response bytes",
                            id        => 14,
                        },
        server_timedout => { type      => PM_TYPE_U64,
                             semantics => PM_SEM_COUNTER,
                             help      => "Count of timeouts on server connections",
                             id        => 15,
                         },
        server_connections => { type      => PM_TYPE_U64,
                                semantics => PM_SEM_COUNTER,
                                help      => "Number of active server connections",
                                id        => 16,
                            },
        server_err => { type      => PM_TYPE_U64,
                        semantics => PM_SEM_COUNTER,
                        help      => "Number of errors on server connections",
                        id        => 17,
                    },
        in_queue => { type      => PM_TYPE_U64,
                        semantics => PM_SEM_COUNTER,
                        help      => "Number of errors on server connections",
                        id        => 18,
                    },
        in_queue_bytes => { type      => PM_TYPE_U64,
                            semantics => PM_SEM_COUNTER,
                            help      => "Number of errors on server connections",
                            id        => 19,
                        },
        out_queue => { type      => PM_TYPE_U64,
                       semantics => PM_SEM_COUNTER,
                       help      => "Number of errors on server connections",
                       id        => 20,
                   },
        out_queue_bytes => { type      => PM_TYPE_U64,
                             semantics => PM_SEM_COUNTER,
                             help      => "Number of errors on server connections",
                             id        => 21,
                         },
        server_ejected_at => { type      => PM_TYPE_U64,
                               semantics => PM_SEM_COUNTER,
                               #help      => "N/A",
                               id        => 22,
                           },
        server_eof => { type      => PM_TYPE_U64,
                        semantics => PM_SEM_COUNTER,
                        help      => "Count of server EOFs",
                        id        => 25,
                    },
    },

    # Maximum time in seconds (may also be a fraction) to keep the data for responses
    max_delta_sec => 0.5,
    pmdaname      => "nutcracker",
    max_recv_len  => 10240,
    indom_separator => ":::",

    debug => 0,
);

$0 = "pmda$cfg{pmdaname}";

# Enable PCP debugging
$ENV{PCP_DEBUG} = 65535
    if $cfg{debug};

# Config check
config_check(\%cfg);

#print STDERR "Starting $cfg{pmdaname} PMDA\n";
$pmda = PCP::PMDA->new($cfg{pmdaname}, 26);

# TODO: Check behaviour on errors
$cfg{loaded} = LoadFile(catfile(pmda_config('PCP_PMDAS_DIR'),$cfg{pmdaname},$cfg{config_fname}));

mydebug("Loaded config: ", Dumper(\$cfg{loaded}));

die "Failed to load config file $cfg{config_fname}"
    unless defined $cfg{loaded} and $cfg{loaded};

# Derive the list of instances in instance domains
#
# general: e.g. [localhost:1234, ...]
# pool:    e.g. [localhost:1234:::redis_1, ...]
# server:  e.g. [localhost:1234:::redis_1:::redis-1, ...]
#

my ($general_inst_id,$pool_inst_id,$server_inst_id) = (-1,-1,-1);

foreach my $host_port (sort keys %{$cfg{loaded}{hosts}}) {
    $indom2name{general}{++$general_inst_id} = $host_port;

    foreach my $pool (sort keys %{$cfg{loaded}{hosts}{$host_port}}) {
        $indom2name{pool}{++$pool_inst_id} = join $cfg{indom_separator},$host_port,$pool;

        foreach my $server (sort @{$cfg{loaded}{hosts}{$host_port}{$pool}}) {
            $indom2name{server}{++$server_inst_id} = join $cfg{indom_separator},$host_port,$pool,$server;
        }
    }
}

mydebug(Data::Dumper->Dump([\%indom2name],[qw(indom2name)]));

$pmda->connect_pmcd;
mydebug("Connected to PMDA");

# Assumption: All the NutCracker instances offer same metrics (so e.g. no major config changes, same versions)

my $res;

# Add instance domains - Note that it has to be run before addition of metrics
mydebug("$cfg{pmdaname} adding instance domains based on config:" . Dumper($cfg{loaded}))
    if $cfg{debug};

%indom2ids = (
    general => 0,
    pool    => 1,
    server  => 2 );

my %indom2insts = (
    general => $cfg{loaded}{hosts},
    pool    => [
        map {
            my $host = $_;
            state $foo = -1;

            map {
                my $pool_name = join $cfg{indom_separator},$host,$_;

                ++$foo => $pool_name
            } sort keys %{$cfg{loaded}{hosts}{$host}};
        } sort keys %{$cfg{loaded}{hosts}}],
    server  => [
        map {
            my $host = $_;

            map {
                my $pool = $_;
                my $pool_name = join $cfg{indom_separator},$host,$_;
                state $foo = -1;

                map {
                    my $server_name = join $cfg{indom_separator},$host,$pool,$_;

                    ++$foo => $server_name
                } sort @{$cfg{loaded}{hosts}{$host}{$pool}};
            } sort keys %{$cfg{loaded}{hosts}{$host}};
        } sort keys %{$cfg{loaded}{hosts}}],
);

foreach my $indom (qw{general pool server}) {
    my ($indom_id,$insts,$help,$longhelp) = ($indom2ids{$indom},
                                             $indom2insts{$indom},
                                             "FIXME",
                                             "long FIXME");

    $res = $pmda->add_indom($indom_id,  # indom ID
                            $insts,     # instances
                            $help,      # help
                            $longhelp); # longhelp
    mydebug("add_indom: " . Data::Dumper->Dump([\$indom_id,\$insts,\$help,\$longhelp,\$res],
                                               [qw(indom_id insts help longhelp res)]))
        if $cfg{debug};
}

# # ... general indom
# $res = $pmda->add_indom($indom2ids{general},                                # indom ID
#                         $cfg{loaded}{hosts},                              # instances
#                         "General Nutcracker metrics",                     # help
#                         "Metrics not specific for any pools or servers"); # longhelp
# mydebug("add_indom: " . Dumper($res))
#     if $cfg{debug};

# # ... pool indom
# my $refh_pools = { map {
#     my $host = $_;

#     map {
#         my $pool_name = join $cfg{indom_separator},$host,$_;

#         $pool_name => 0
#     } keys %{$cfg{loaded}{hosts}{$host}};
# } keys %{$cfg{loaded}{hosts}}};

# mydebug(Data::Dumper->Dump([\$refh_pools],[qw(refh_pools)]))
#     if $cfg{debug};

# $res = $pmda->add_indom($indom2ids{pool},
#                         $refh_pools,
#                         "Redis/Memcache server pools",
#                         "Redis/Memcache server pools - groups of redis servers specified in nutcracker config");
# mydebug("add_indom returned: " . Dumper($res))
#     if $cfg{debug};

# # ... server indom
# my $refh_servers = { map {
#     my $host = $_;

#     map {
#         my $pool = $_;
#         my $pool_name = join $cfg{indom_separator},$host,$_;

#         map {
#             my $server_name = join $cfg{indom_separator},$host,$pool_name,$_;

#             $server_name => 0
#         } @{$cfg{loaded}{hosts}{$host}{$pool}};
#     } keys %{$cfg{loaded}{hosts}{$host}};
# } keys %{$cfg{loaded}{hosts}}};

# mydebug(Data::Dumper->Dump([\$refh_servers],[qw(refh_servers)]))
#     if $cfg{debug};

# $res = $pmda->add_indom($indom2ids{server},
#                         $cfg{loaded},
#                         "Memcached/NutCracker server domain",
#                         "Memcached/NutCracker server names seen in nutcrackernutcracker.conf");
# mydebug("add_indom returned: " . Dumper($res))
#     if $cfg{debug};

foreach my $metric_type (qw{general pool server}) {
    my $full_mt_name = $metric_type . "_metrics";

    mydebug("Adding $metric_type metrics");

    foreach my $metric (sort keys %{$cfg{$full_mt_name}}) {
        my $refh_metric = $cfg{$full_mt_name}->{$metric};
        my $metric_full_name = "$cfg{pmdaname}.$metric";
        my $pmid = $refh_metric->{id};

        mydebug("$cfg{pmdaname} adding metric $metric_full_name:"
                    . Data::Dumper->Dump([pmda_pmid(0,$pmid),
                                          $refh_metric->{type},
                                          $indom2ids{$metric_type},
                                          ($refh_metric->{semantics} // PM_SEM_DISCRETE),
                                          pmda_units(0,0,1,0,0,PM_COUNT_ONE),
                                          $metric_full_name,
                                          ($refh_metric->{help} // ""),
                                          ($refh_metric->{longhelp} // ""),
                                      ],
                                         [qw(pmid data_type instance_domain semantics units metric_name help longhelp)]))
            if $cfg{debug};

        $res = $pmda->add_metric(
            pmda_pmid(0,$pmid),                             # pmid
            $refh_metric->{type},                           # data type
            $indom2ids{$metric_type},                         # indom
            ($refh_metric->{semantics} // PM_SEM_DISCRETE), # semantics
            pmda_units(0,0,1,0,0,PM_COUNT_ONE),             # units
            $metric_full_name,                              # metric name
            ($refh_metric->{help}      // ""),              # short help
            ($refh_metric->{longhelp}  // ""));             # long help
        $id2metrics{$pmid} = $metric;

        mydebug("... returned '" . ($res // "<undef>") . "'");
    }
}


# # Add all the general metrics
# mydebug("Adding general metrics");

# foreach my $metric (sort keys %{$cfg{general_metrics}}) {
#     my $refh_metric = $cfg{general_metrics}->{$metric};
#     my $pmid = $refh_metric->{id};

#     mydebug("$cfg{pmdaname} adding metric $cfg{pmdaname}.$metric using PMID: $pmid ...");

#     $res = $pmda->add_metric(
#         pmda_pmid(0,$pmid),                              # PMID
#         $refh_metric->{type},                            # data type
#         $indom2ids{general},                               # indom
#         ($refh_metric->{semantics} // PM_SEM_DISCRETE),  # semantics
#         pmda_units(0,0,1,0,0,PM_COUNT_ONE),              # units
#         "$cfg{pmdaname}.$metric",                        # key name
#         ($refh_metric->{help}      // ""),               # short help
#         ($refh_metric->{longhelp}  // ""));              # long help
#     $id2metrics{$pmid} = $metric;

#     mydebug("... returned '" . ($res // "<undef>") . "'");
# }

# # ... and also pool metrics (those that depend on configuration and keyspace)
# mydebug("Adding pool metrics");

# foreach my $metric (sort keys %{$cfg{pool_metrics}}) {
#     my $refh_metric = $cfg{pool_metrics}->{$metric};
#     my $pmid = $refh_metric->{id};

#     mydebug("$cfg{pmdaname} adding metric $cfg{pmdaname}.$metric:"
#                 . Data::Dumper->Dump([pmda_pmid(0,$pmid),
#                                       $refh_metric->{type},
#                                       $indom2ids{pool},
#                                       ($refh_metric->{semantics} // PM_SEM_DISCRETE),
#                                       pmda_units(0,0,1,0,0,PM_COUNT_ONE),
#                                       $metric,
#                                       ($refh_metric->{help} // ""),
#                                       ($refh_metric->{longhelp} // ""),
#                                   ],
#                                      [qw(pmid data_type instance_domain semantics units metric_name help longhelp)]))
#         if $cfg{debug};

#     $res = $pmda->add_metric(
#         pmda_pmid(0,$pmid),                              # PMID
#         $refh_metric->{type},                            # data type
#         $indom2ids{pool},                                  # indom
#         ($refh_metric->{semantics} // PM_SEM_DISCRETE),  # semantics
#         pmda_units(0,0,1,0,0,PM_COUNT_ONE),              # units
#         $metric,                                         # metric name
#         ($refh_metric->{help}      // ""),               # short help
#         ($refh_metric->{longhelp}  // ""));              # long help
#     $id2metrics{$pmid} = $metric;

#     mydebug("... returned '" . ($res // "<undef>") . "'");
# }

# # ... and also server metrics (those that depend on configuration and keyspace)
# mydebug("Adding server metrics");

# foreach my $metric (sort keys %{$cfg{server_metrics}}) {
#     my $refh_metric = $cfg{server_metrics}->{$metric};
#     my $pmid = $refh_metric->{id};

#     $res = $pmda->add_metric(
#         pmda_pmid(0,$pmid),                              # PMID
#         $refh_metric->{type},                            # data type
#         $indom2ids{server},                                # indom
#         ($refh_metric->{semantics} // PM_SEM_DISCRETE),  # semantics
#         pmda_units(0,0,1,0,0,PM_COUNT_ONE),              # units
#         $metric,                                         # metric name
#         ($refh_metric->{help}      // ""),               # short help
#         ($refh_metric->{longhelp}  // ""));              # long help
#     $id2metrics{$pmid} = $metric;

#     mydebug("... returned '" . ($res // "<undef>") . "'");
# }

$pmda->set_fetch(\&fetch);
$pmda->set_fetch_callback(\&fetch_callback);
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

    $ids{$cfg{general_metrics}{$_}{id}}{$_}++
        foreach keys %{$cfg{general_metrics}};
    $ids{$cfg{pool_metrics}{$_}{id}}{$_}++
        foreach keys %{$cfg{pool_metrics}};
    $ids{$cfg{server_metrics}{$_}{id}}{$_}++
        foreach keys %{$cfg{server_metrics}};

    foreach my $id (sort keys %ids) {
        if (keys %{$ids{$id}} > 1) {
            $err_count++;

            warn "ID is used more than once for following metrics: " . join(" ",sort keys %{$ids{$id}});
        }
    }

    die "Errors in configuration found, exiting"
        if $err_count;
}

sub metric_to_domain_name {
    my ($metric) = @_;

    if (exists $cfg{general_metrics}{$metric}) {
        "general"
    } elsif (exists $cfg{pool_metrics}{$metric}) {
        "pool"
    } elsif (exists $cfg{server_metrics}{$metric}) {
        "server"
    } else {
        $pmda->err("Assertion error - metric '$metric' is nor general, nor pool, nor server metric");

        undef;
    }
}

sub fetch {
    my ($cluster, $item, $inst) = @_;
    my ($t0_sec,$t0_msec) = gettimeofday;

    # Clean deprecated data for all the instances
    my ($cur_date_sec,$cur_date_msec) = gettimeofday;
    my $cur_date = $cur_date_sec + $cur_date_msec/1e6;

    foreach my $inst (sort keys %{$cfg{loaded}}) {
        if (exists $cur_data{$inst} and ($cur_date - $cur_data{$inst}{timestamp}) > $cfg{max_delta_sec}) {
            mydebug("Removing cache data for '$inst' as too old - cur_date: $cur_date, timestamp: $cur_data{$inst}{timestamp}");

            delete $cur_data{$inst};
        }
    }

    mydebug("$cfg{pmdaname}_fetch finished");
}

sub fetch_callback {
    my ($cluster, $item, $inst) = @_;
    my $searched_key = $id2metrics{$item};
    my $metric_name = pmda_pmid_name($cluster, $item);
    my $indom_name = metric_to_domain_name($searched_key);
    my $inst_name = $indom2name{$indom_name}{$inst};

    mydebug("fetch_callback:"
                . Data::Dumper->Dump([\$cluster,\$item,\$inst,\$searched_key,\$metric_name,\$indom_name,\$inst_name],
                                     [qw(cluster item inst searched_key metric_name indom_name inst_name)]))
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

    # Assumption: There are no two metrics with same name in general/pool/server group (TODO: Check it)
    my ($host,$port);

    warn "Assertion error - no host:port detected from indom_name '$indom_name'"
        unless ($host,$port) = ($inst_name =~ /\A(\S+):(\d+)/);

    mydebug("Host: '$host', port: '$port'");

    # Fetch NutCracker info
    my $refh_stats;
    my ($t0_sec,$t0_usec) = gettimeofday;
    my $utime0 = $t0_sec + $t0_usec/1e6;
    my $delta_sec = $utime0 - ($cur_data{$inst}{timestamp} // 0);

    if ($delta_sec < $cfg{max_delta_sec}) {
        $refh_stats = $cur_data{$inst}{stats};
    } else {
        mydebug("Actual data not found, refetching");

        ($refh_stats) = get_nc_data($host,$port);

        return (PM_ERR_AGAIN, 0)
            unless defined $refh_stats;

        my ($t1_sec,$t1_usec) = gettimeofday;
        my $utime1 = $t1_sec + $t1_usec/1e6;
        my $dt_sec = $utime1 - $utime0;

        $cur_data{$inst} = { timestamp      => $utime1,
                             stats          => $refh_stats,
                             fetch_time_sec => $dt_sec,
                         };

        mydebug("fetch lasted: $dt_sec seconds");
    }

    my @inst_parts = split /$cfg{indom_separator}/,$inst_name;

    mydebug(Data::Dumper->Dump([\@inst_parts],[qw(inst_parts)]));

    if ($indom_name eq "general"
            and exists $refh_stats->{general}->{$searched_key}
            and defined $refh_stats->{general}->{$searched_key}) {

        return ($refh_stats->{general}->{$searched_key},1)
    } elsif ($indom_name eq "pool") {
        unless (defined $inst_parts[1]) {
            $pmda->err("Assertion error: inst_part[1] for pool metric '$searched_key' not defined");

            return (PM_ERR_AGAIN,0);
        }

        if (exists $refh_stats->{pool}->{$inst_parts[1]}->{$searched_key}
                and defined $refh_stats->{pool}->{$inst_parts[1]}->{$searched_key}) {

            return ($refh_stats->{pool}->{$inst_parts[1]}->{$searched_key},1)
        } else {
            # Return error if the atom value was not succesfully retrieved
            mydebug("Required pool metric '$searched_key' was not found in NutCracker statistics or was undefined");

            return (PM_ERR_AGAIN, 0)
        }
    } elsif ($indom_name eq "server") {
        unless (defined $inst_parts[2]) {
            $pmda->err("Assertion error: inst_part[2] for server metric '$searched_key' not defined");

            return (PM_ERR_AGAIN,0);
        }

        if (exists $refh_stats->{server}->{$inst_parts[2]}->{$searched_key}
                and defined $refh_stats->{server}->{$inst_parts[2]}->{$searched_key}) {

            return ($refh_stats->{server}->{$inst_parts[2]}->{$searched_key},1)
        } else {
            # Return error if the atom value was not succesfully retrieved
            mydebug("Required server metric '$searched_key' was not found in NutCracker statistics or was undefined");

            return (PM_ERR_AGAIN, 0)
        }
    } else {
        # Success - return (<value>,<success code>)
        mydebug("Assertion error - how could we get there?");

        return ($refh_stats->{$indom_name}->{$searched_key}, 1);
    }
}

sub get_nc_data {
    my ($host,$port) = @_;
    my $refh_stats;

    mydebug("Opening socket to host:'$host', port:'$port'");

    # Enable autoflush
    local $| = 1;

    my $socket = IO::Socket::INET->new( PeerAddr => $host,
                                        PeerPort => $port,
                                        Proto    => 'tcp',
                                        Type     => SOCK_STREAM );
    unless ($socket) {
        mydebug("Failed to create socket: '$@'");

        return undef;
    }

    #my $size = $socket->send("INFO\r\n");
    #
    #mydebug("Sent INFO request with $size bytes");

    my ($resp,$cur_resp) = ("","");

    $resp .= $cur_resp
        while $socket->recv($cur_resp,$cfg{max_recv_len}),$cur_resp;
    mydebug("Response: '$resp'");
    $socket->close;
    mydebug("... socket closed");

    my $json = decode_json($resp);

    #mydebug("Decoded JSON: ", Dumper(\$json))
    #    if $cfg{debug};

    # Sort the has keys to general, pool and server
    foreach my $key (sort keys %$json) {
        #mydebug("key '$key': '$$json{$key}'");

        if (ref $json->{$key} eq ref "constant") {
            # General metric
            #mydebug("... is a general metric");

            $refh_stats->{general}->{$key} = $json->{$key};
        } elsif (ref $json->{$key} eq ref {}) {
            foreach my $key2 (sort keys %{$json->{$key}}) {
                #mydebug("key2 '$key2': '$$json{$key}{$key2}'");

                if (ref $json->{$key}->{$key2} eq ref "const") {
                    # Pool metrics
                    #mydebug("... is a pool metric");

                    $refh_stats->{pool}->{$key}->{$key2} = $json->{$key}->{$key2};
                } elsif (ref $json->{$key}->{$key2} eq ref {}) {
                    # Server metrics

                    foreach my $key3 (sort keys %{$json->{$key}->{$key2}}) {
                        #mydebug("key3 '$key3': '$$json{$key}{$key2}{$key3}'");

                        unless (ref $json->{$key}->{$key2}->{$key3} eq ref "const") {
                            $pmda->err("Unexpected key3 '$key3' value (nor constant, nor hash reference): " . Dumper($json->{$key}->{$key2}->{$key3}));

                            next;
                        }

                        #mydebug("... is a server metric");

                        $refh_stats->{server}->{$key2}->{$key3} = $json->{$key}->{$key2}->{$key3};
                    }
                } else {
                    $pmda->err("Unexpected key2 '$key2' value (nor constant, nor hash reference): " . Dumper($json->{$key}->{$key2}));
                }
            }

        } else {
            $pmda->err("Unexpected key '$key' value (nor constant, nor hash reference): " . Dumper($json->{$key}));
        }
    }

    mydebug("Fetch callback results:" . Data::Dumper->Dump([$refh_stats],[qw{refh_stats}]));

    $refh_stats
}
