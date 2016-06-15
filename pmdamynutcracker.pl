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
use Time::HiRes   qw(gettimeofday);
use JSON          qw(decode_json);;
use Data::Dumper;

use vars qw($pmda %cfg %id2metrics %cur_data %var_metrics);

#
# Notes:
#
# - descriptions of metrics were retrieved from
#     https://github.com/twitter/twemproxy#observability
# - the PMDA was written thanks to mentoring by fche at #pcp@irc.freenode.net
#     and support of myllynen
# - debugging messages can be enabled by setting $cfg{debug} to true value and
#     are usually visible at /var/log/pcp/pmcd/mynutcracker.log including
#     message dumps
# - configuration file is present at ./mynutcracker.conf and has following options:
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
#  - complete adoption from myredis to mynutcracker
#  - complete short and long help lines
#  - check units of all the metrics
#  - define the domain ID in one file only (while currently there are more)
#  - add hostname:port to the metrics, so that the mapping can be checked
#  - add measurement of request-response time, timeout or error of INFO
#      queries
#  - add some check that at most 1 NutCracker request is performed at a time
#  - add configurable timer for on-time error check (< 1 sec)
#  - use persistent TCP connection to NutCracker and reconnect only when needed
#    -> use dynamic replace_indom in case of system unavailability
#

%cfg = (
    config_fname => "mynutcracker.conf",

    metrics     => {
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
    },

    # Maximum time in seconds (may also be a fraction) to keep the data for responses
    max_delta_sec => 0.5,
    pmdaname      => "mynutcracker",
    max_recv_len  => 10240,

    debug => 1,
);

$0 = "pmda$cfg{pmdaname}";

# Enable PCP debugging
$ENV{PCP_DEBUG} = 65535
    if $cfg{debug};

# Config check
config_check(\%cfg);

#print STDERR "Starting $cfg{pmdaname} PMDA\n";
$pmda = PCP::PMDA->new($cfg{pmdaname}, 253);

die "Failed to load config file"
    unless $cfg{loaded} = load_config(catfile(pmda_config('PCP_PMDAS_DIR'),$cfg{pmdaname},$cfg{config_fname}));
$pmda->connect_pmcd;
mydebug("Connected to PMDA");

# Assumption: All the NutCracker instances offer same metrics (so e.g. no major config changes, same versions)

my ($pm_instdom) = (-1,-1);
my $res;

# Add instance domains - Note that it has to be run before addition of metrics
$pm_instdom++;

mydebug("$cfg{pmdaname} adding instance domain $pm_instdom," . Dumper($cfg{loaded}{hosts}))
    if $cfg{debug};

my %dom2ids = ( general => 0,
                pool    => 1,
                server  => 2 );

$res = $pmda->add_indom($pm_instdom,
                        $cfg{loaded}{hosts},
                        "Memcached/NutCracker Server pools",
                        "Memcached/NutCracker Server pools names seen in nutcrackermynutcracker.conf");
mydebug("add_indom returned: " . Dumper($res))
    if $cfg{debug};

# Add all the general metrics
mydebug("Adding general metrics");

foreach my $metric (sort keys %{$cfg{metrics}}) {
    my $refh_metric = $cfg{metrics}->{$metric};
    my $pmid = $refh_metric->{id};

    mydebug("$cfg{pmdaname} adding metric $cfg{pmdaname}.$metric using PMID: $pmid ...");

    $res = $pmda->add_metric(
        pmda_pmid(0,$pmid),                              # PMID
        $refh_metric->{type},                            # data type
        $dom2ids{general},                               # indom
        ($refh_metric->{semantics} // PM_SEM_DISCRETE),  # semantics
        pmda_units(0,0,1,0,0,PM_COUNT_ONE),              # units
        "$cfg{pmdaname}.$metric",                        # key name
        ($refh_metric->{help}      // ""),               # short help
        ($refh_metric->{longhelp}  // ""));              # long help
    $id2metrics{$pmid} = $metric;

    mydebug("... returned '" . ($res // "<undef>") . "'");
}

# ... and also pool metrics (those that depend on configuration and keyspace)
mydebug("Adding pool metrics");

foreach my $metric_suffix (sort keys %{$cfg{pool_metrics}}) {
    my $refh_metric = $cfg{variant_metrics}->{$metric_suffix};
    my $pmid = $refh_metric->{id};

    foreach my $keyspace (sort keys %{$cfg{loaded}{dbs}}) {
        my $metric_name = "$cfg{pmdaname}.${keyspace}$metric_suffix";

        mydebug("$cfg{pmdaname} adding metric $cfg{pmdaname}.$keyspace:"
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

# ... and also server metrics (those that depend on configuration and keyspace)
mydebug("Adding server metrics");

foreach my $metric_suffix (sort keys %{$cfg{variant_metrics}}) {
    my $refh_metric = $cfg{variant_metrics}->{$metric_suffix};
    my $pmid = $refh_metric->{id};

    foreach my $keyspace (sort keys %{$cfg{loaded}{dbs}}) {
        my $metric_name = "$cfg{pmdaname}.${keyspace}$metric_suffix";

        mydebug("$cfg{pmdaname} adding metric $cfg{pmdaname}.$keyspace:"
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


$pmda->set_fetch(\&mynutcracker_fetch);
$pmda->set_fetch_callback(\&mynutcracker_fetch_callback);
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

sub load_config {
    my ($in_fname) = @_;
    my $refh_res;

    open my $fh_in,"<",$in_fname;

    my ($host_id,$lineno) = (0,0,0);
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
        } else {
            warn "#$lineno: Unexpected line '$aline', skipping it";
        }
    }

    # Check mandatory options
    die "No mandatory keys found"
        unless keys %$refh_res;

    my $err_count = 0;

    mydebug(Dumper($refh_res))
        if $cfg{debug};

    #TODO: Check also gethostbyname and port number to be >0 and < 65535
    die "No hosts to be monitored found in '$in_fname'"
        unless exists $refh_res->{hosts} and keys %{$refh_res->{hosts}};
    die "It seems that '$in_fname' contains non-unique hosts entries"
        unless keys %{$refh_res->{hosts}} == $host_id;

    $refh_res
}

sub mynutcracker_fetch {
    my ($cluster, $item, $inst) = @_;
    my ($t0_sec,$t0_msec) = gettimeofday;
    #my $searched_key = $id2metrics{$item};
    #my $metric_name = pmda_pmid_name($cluster, $item);
    #
    #mydebug("$cfg{pmdaname}_fetch metric:"
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

    mydebug("$cfg{pmdaname}_fetch finished");
}

sub mynutcracker_fetch_callback {
    my ($cluster, $item, $inst) = @_;
    my ($t0_sec,$t0_msec) = gettimeofday;
    my $searched_key = $id2metrics{$item};
    my $metric_name = pmda_pmid_name($cluster, $item);

    #mydebug("mynutcracker_fetch_callback metric:'$metric_name' cluster:'$cluster', item:'$item' inst:'$inst' -> searched_key: $searched_key");
    mydebug("mynutcracker_fetch_callback:"
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

    # Get NutCracker hostname and port number from config
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

    # Fetch NutCracker info
    #TODO: Add support for db instances here
    my ($refh_nc_info,$refh_inst_keys);
    my ($cur_date_sec,$cur_date_msec) = gettimeofday;
    my $cur_date = $cur_date_sec + $cur_date_msec/1e6;

    if (exists $cur_data{$host_ports[0]} and ($cur_date - $cur_data{$host_ports[0]}{timestamp}) < $cfg{max_delta_sec}) {
        $refh_nc_info = $cur_data{$host_ports[0]}{nc_info};
        $refh_inst_keys  = $cur_data{$host_ports[0]}{inst_keys};
    } else {
        mydebug("Actual data not found, refetching");

        ($refh_nc_info,$refh_inst_keys) = get_nc_data($host,$port);

        mydebug(Data::Dumper->Dump([$refh_nc_info, $refh_inst_keys],[qw(refh_nc_info refh_inst_keys)]))
            if $cfg{debug};

        my ($cur_date_sec,$cur_date_msec) = gettimeofday;
        my $cur_date = $cur_date + $cur_date_msec/1e6;

        $cur_data{$host_ports[0]}{timestamp} = $cur_date;
        $cur_data{$host_ports[0]}{nc_info} = $refh_nc_info;
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
        my ($key_to_search) = ($searched_key =~ /\A$cfg{pmdaname}.${keyspace}_(\S+)/);

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

    if (not(exists $refh_nc_info->{$searched_key} and defined $refh_nc_info->{$searched_key})) {
        # Return error if the atom value was not succesfully retrieved
        mydebug("Required metric '$searched_key' was not found in NutCracker statistics or was undefined");

        return (PM_ERR_AGAIN, 0)
    } else {
        # Success - return (<value>,<success code>)
        mydebug("Returning success");

        return ($refh_nc_info->{$searched_key}, 1);
    }
}

sub get_nc_data {
    my ($host,$port) = @_;
    my ($refh_general_stats,$refh_pool_stats,$refh_server_stats);

    mydebug("Opening socket to host:'$host', port:'$port'");

    # Enable autoflush
    local $| = 1;

    my $socket = IO::Socket::INET->new( PeerAddr => $host,
                                        PeerPort => $port,
                                        Proto    => 'tcp',
                                        Type     => SOCK_STREAM )
        or die "Can't bind : $@\n";;
    #my $size = $socket->send("INFO\r\n");
    #
    #mydebug("Sent INFO request with $size bytes");

    my $resp;

    $socket->recv($resp,$cfg{max_recv_len});
    mydebug("Response: '$resp'");
    $socket->close;
    mydebug("... socket closed");

    my $json = decode_json($resp);

    mydebug("Decoded JSON: ", Dumper(\$json))
        if $cfg{debug};

    # Sort the has keys to general, pool and server
    foreach my $key (sort keys %$json) {
        mydebug("key '$key': '$$json{$key}'");

        if (ref $json->{$key} eq ref "constant") {
            # General metric
            mydebug("... is a general metric");

            $refh_general_stats->{$key} = $json->{$key};
        } elsif (ref $json->{$key} eq ref {}) {
            foreach my $key2 (sort keys %{$json->{$key}}) {
                mydebug("key2 '$key2': '$$json{$key}{$key2}'");

                if (ref $json->{$key}->{$key2} eq ref "const") {
                    # Pool metrics
                    mydebug("... is a pool metric");

                    $refh_pool_stats->{$key}->{$key2} = $json->{$key}->{$key2};
                } elsif (ref $json->{$key}->{$key2} eq ref {}) {
                    # Server metrics

                    foreach my $key3 (sort keys %{$json->{$key}->{$key2}}) {
                        mydebug("key3 '$key3': '$$json{$key}{$key2}{$key3}'");

                        unless (ref $json->{$key}->{$key2}->{$key3} eq ref "const") {
                            $pmda->err("Unexpected key3 '$key3' value (nor constant, nor hash reference): " . Dumper($json->{$key}->{$key2}->{$key3}));

                            next;
                        }

                        mydebug("... is a server metric");

                        $refh_server_stats->{$key2}->{$key3} = $json->{$key}->{$key2}->{$key3};
                    }
                } else {
                    $pmda->err("Unexpected key2 '$key2' value (nor constant, nor hash reference): " . Dumper($json->{$key}->{$key2}));
                }
            }

        } else {
            $pmda->err("Unexpected key '$key' value (nor constant, nor hash reference): " . Dumper($json->{$key}));
        }
    }

    mydebug("Fetch callback results:" . Data::Dumper->Dump([$refh_general_stats,
                                                            $refh_pool_stats,
                                                            $refh_server_stats],[qw{refh_general_stats
                                                                                    refh_pool_stats
                                                                                    refh_server_stats}]));

    ($refh_general_stats,$refh_pool_stats,$refh_server_stats)
}
