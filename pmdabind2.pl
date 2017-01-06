#!/usr/bin/perl -w
# pmdabind2.pl --- Few attempts
# Author:  <lukas@sam>
# Created: 09 Sep 2016
# Version: 0.01

use warnings  FATAL => qw{uninitialized};
use strict;
use autodie;

#TODO: Consider removing autodie

use PCP::PMDA;
use Time::HiRes            qw{gettimeofday alarm};
use LWP;
use XML::LibXML;
use List::MoreUtils        qw{uniq};
use File::Spec::Functions  qw{catfile};
use File::Basename         qw{basename};
use Data::Dumper;
#use Devel::Trace;

#use Carp;

#$SIG{ __DIE__ } = sub { Carp::confess( @_ ) };

$Data::Dumper::Sortkeys = 1;
$Data::Dumper::Pad = "   <dump> ";
#$Devel::Trace::TRACE = 1;

# Assumes following options to be enabled in named.conf:
#  - statistics-channels { inet <addr> port <port> allow { any }; }
#  - options { zone-statistics yes; }

#TODO: Make addr, port and allow_addr, allow_port in configuration
#TODO: Make zone-statistics possible to be disabled
#TODO: Better handle bind2.memory.contexts.blocksize set to '-'
#TODO: Better handle undef in bind2.sockets.peer-address
#TODO: Make the instance handling adaptive
#TODO: Separate the Bind stats reading data into a module
#TODO: Check bind2.nsstat.RateDropped, bind2.nsstat.RateSlipped, bind2.resolver.total.Lame
#TODO: Review all the metrics for type/semantics/units/help/longhelp
#TODO: Make somehow the metric id constant even if the metrics change so that the archives are comparable (or doesn't the PCP do it itself?)
#TODO: Add warnings/unknown_stats counter
#TODO: Add .total metrics for memory and sockstat metrics
#TODO: Add .pmda metrics for config, errors and reasons
#TODO: Make the intentionally deleted data available once they are considered useful. Note that some of these may grow large, so test under load at first. Once enabling these, make their ignoring configurable.
#TODO: Make the bind2.pmda.errors.http_fetch
#TODO: Make the bind2.pmda.errors.timeout
#TODO: Make the bind2.pmda.delays.{data_fetch,fetch,fetch_callback}[actual,1min,5min,10min]


our %cfg = (
    config_fname => "bind2.conf",
    pmda_prefix  => "bind2",
    pmda_id      => 250,

    metrics => {
        types => {
            # Give a list of metrics with non-default type here

            PM_TYPE_STRING() => [qw{bind2.memory.contexts.name
                                    bind2.boot-time
                                    bind2.current-time

                                    bind2.sockets.local-address
                                    bind2.sockets.name
                                    bind2.sockets.peer-address
                                    bind2.sockets.type
                                    bind2.tasks.ADB.state
                                    bind2.tasks.client.state
                                    bind2.tasks.other.state
                               }],
        },

        semantics => {
            # Give a list of metrics with non-default semantics here

            PM_SEM_DISCRETE() => [qw{bind2.memory.total.BlockSize
                                     bind2.memory.total.ContextSize
                                     bind2.memory.contexts.name
                                     bind2.sockets.local-address
                                     bind2.sockets.name
                                     bind2.sockets.peer-address
                                     bind2.tasks.ADB
                                     bind2.tasks.client
                                     bind2.tasks.other
                              }],
            PM_SEM_INSTANT()  => [qw{bind2.boot-time
                                     bind2.current-time

                                     bind2.memory.total.InUse
                                     bind2.memory.total.Lost
                                     bind2.memory.total.TotalUse
                                     bind2.memory.contexts.blocksize
                                     bind2.memory.contexts.hiwater
                                     bind2.memory.contexts.inuse
                                     bind2.memory.contexts.lowater
                                     bind2.memory.contexts.maxinuse
                                     bind2.memory.contexts.pools
                                     bind2.memory.contexts.references
                                     bind2.memory.contexts.total

                                     bind2.sockets.references
                                     bind2.sockets.states.bound
                                     bind2.sockets.states.connected
                                     bind2.sockets.states.listener

                                     bind2.sockets.total
                                }],
        },

        defaults => {
            # Default metric properties here

            type      => PM_TYPE_U32,
            semantics => PM_SEM_COUNTER,
        },
    },

    # Maximum time in seconds (may also be a fraction) to keep the data for responses
    max_delta_sec      => 0.9,
    max_get_delay_sec  => 0.7,

    debug              => 1,
);

our (%current_data,%metrics,%indoms,%id2metrics,$lwp_user_agent);

$0 = "pmda$cfg{pmda_prefix}";

# Enable PCP debugging if required
$ENV{PCP_DEBUG} = 65535
    if $cfg{debug};

# Construct the PCP::PMDA object
my $pmda = PCP::PMDA->new($cfg{pmda_prefix}, $cfg{pmda_id});

die "Failed to construct the PMDA object"
    unless $pmda;

# Construct the LWP User Agent object
$lwp_user_agent = LWP::UserAgent->new
    or die "Failed to create the LWP::UserAgent";

mytrace(Data::Dumper->Dump([\$lwp_user_agent],[qw{lwp_user_agent}]));

# Load the configuration
die "Failed to load config file"
    unless $cfg{loaded} = load_config(catfile(pmda_config('PCP_PMDAS_DIR'),
                                              $cfg{pmda_prefix},
                                              $cfg{config_fname}));

$pmda->connect_pmcd;

#$Devel::Trace::TRACE = 1;
myinfo("Connected to PMDA");

# Initialize the list of metrics
init_metrics($cfg{loaded}{uri})
    or die "Failed to init metrics from Bind server at '$cfg{loaded}{uri}'";

myinfo("Metrics initialized");

# Add the instance domains
foreach my $indom (sort {$a <=> $b} keys %indoms) {
    mydebug("$cfg{pmda_prefix} adding instance domain '$indom' ...")
        if $cfg{debug};

    my $res = $pmda->add_indom($indom,
                               $indoms{$indom},
                               "TODO",
                               "long TODO");
    mydebug("add_indom returned: " . Dumper($res))
        if $cfg{debug};
}

# Add all the metrics
my $tmp_pmid = -1;

foreach my $metric (sort keys %metrics) {
    my $refh_metric = $metrics{$metric};
    my ($pmid,$type,$indom,$semantics,$units,$shorthelp,$longhelp) = ( pmda_pmid(0,++$tmp_pmid),
                                                                       $refh_metric->{type},
                                                                       exists $refh_metric->{indom_id} ? $refh_metric->{indom_id} : PM_INDOM_NULL,
                                                                       $refh_metric->{semantics},
                                                                       pmda_units(0,0,1,0,0,PM_COUNT_ONE),
                                                                       (exists $refh_metric->{help}     ? $refh_metric->{help} : ""),
                                                                       (exists $refh_metric->{longhelp} ? $refh_metric->{longhelp} : ""));

    $pmid++;

    mydebug("Current metric parameters: " . Dumper(\$refh_metric));
    mydebug("$cfg{pmda_prefix} adding metric $metric using:\n" . Data::Dumper->Dump([\$pmid,
                                                                                     \$type,
                                                                                     \$indom,
                                                                                     \$semantics,
                                                                                     \$units,
                                                                                     \$shorthelp,
                                                                                     \$longhelp],
                                                                                    [qw{pmid type indom semantics units shorthelp longhelp}]));

    my $res = $pmda->add_metric(
        $pmid,                   # PMID
        $type,                   # data type
        $indom,                  # indom
        $semantics,              # semantics
        $units,                  # units
        $metric,                 # key name
        $shorthelp,              # short help
        $longhelp);              # long help
    $id2metrics{$pmid} = $metric;

    mydebug("... returned '" . ($res // "<undef>") . "'");
}

# Set all the necessary callbacks and the account to run under
mydebug("Setting fetch method ...");
$pmda->set_fetch(\&fetch);

mydebug("Setting fetch_callback method ...");
$pmda->set_fetch_callback(\&fetch_callback);

$pmda->set_user('pcp');

# Start the PMDA
mydebug("Calling ->run");
$pmda->run;

# This should never happen as run should never return
myerror("Assertion error: ->run() terminated - this should never happen");


## Subroutines
#- Callback routines
sub fetch {
    my ($cluster, $item, $inst) = @_;

    mydebug("Entering fetch(): " . Data::Dumper->Dump([\$cluster,\$item,\$inst],[qw{cluster item inst}]))
        if $cfg{debug};

    # Clean deprecated data for all the instances
    my ($cur_date_sec,$cur_date_msec) = gettimeofday;
    my $cur_date = $cur_date_sec + $cur_date_msec/1e6;

    unless (keys %metrics) {
        mydebug("No metrics found, calling init_metrics ...");

        init_metrics()
    }

    if (keys %current_data and ($cur_date - $current_data{timestamp}) > $cfg{max_delta_sec}) {
        mydebug("Removing cache data as too old - cur_date: $cur_date, timestamp: $current_data{timestamp}");

        delete $current_data{values};
    }

    mydebug("... fetch() finished");
}

sub fetch_callback {
    my ($cluster, $item, $inst) = @_;
    my $searched_key = $id2metrics{$item};
    my $metric_name = pmda_pmid_name($cluster, $item);

    mydebug("fetch_callback(): "
                . Data::Dumper->Dump([\$cluster,\$item,\$inst,\$metric_name,\$searched_key],
                                     [qw(cluster item inst metric_name searched_key)]))
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

    # Fetch Bind metrics info ...
    my ($cur_date_sec,$cur_date_msec) = gettimeofday;
    my $cur_date = $cur_date_sec + $cur_date_msec/1e6;

    if (not keys %metrics) {
        # ... if the initialization did not succeed at the time of PMDA's start
        mydebug("No metrics found, calling init_metrics ...");

        init_metrics()
    } elsif(($cur_date - $current_data{timestamp}) > $cfg{max_delta_sec}) {
        # ... if the data are considered deprecated

        mydebug("Actual data not found, refetching");

        my $refh_bind_stats = fetch_bind_stats($cfg{loaded}{uri});

        unless (defined $refh_bind_stats) {
            $pmda->err("HTTP GET from '$cfg{loaded}{uri}' failed");

            return (PM_ERR_AGAIN, 0)
        }

        $current_data{timestamp} = $cur_date;
        $current_data{values} = $refh_bind_stats;
    }

    # Return the value if available
    if (exists $current_data{$searched_key} and defined $current_data{$searched_key}) {
        # ... key exists

        if (exists $metrics{$searched_key}{indom_id}) {
            # ... and isn't a scalar

            # Derive the instance name
            my $inst_name;
            my $indom_id = $metrics{$searched_key}{indom_id};

            if (defined $inst) {
                while (my ($key,$value) = %{$indoms{$indom_id}}) {
                    if ($value = $inst) {
                        $inst_name = $key;

                        last
                    }
                }

                unless (defined $inst_name and exists $current_data{$searched_key}{$inst_name}) {
                    myerror("Either inst_name cannot be derived or it is not present in fetched data:" . Data::Dumper->Dump([$searched_key,$inst,$inst_name],[qw{searched_key inst inst_name}]));

                    return (PM_ERR_AGAIN, 0);
                }
            } else {
                myerror("Assertion error - inst should be defined for metric '$searched_key'");

                return (PM_ERR_AGAIN, 0);
            }

            return ($current_data{$searched_key}{$inst_name},1)
        } else {
            # ... is a scalar - return (<value>,<success code>)
            mydebug("Returning success");

            return ($current_data{$searched_key}, 1);
        }

        return (PM_ERR_AGAIN, 0);
    } else {
        # Return error if the atom value was not succesfully retrieved
        mydebug("Required metric '$searched_key' was not found in Bind INFO or was undefined");

        return (PM_ERR_AGAIN, 0)
    }

    myerror("Assertion Error fetch_callback() finished - this line should never be reached");
}


#- Logging subroutines
sub mylog {
    my ($prefix,@args) = @_;

    chomp @args;
    #print STDERR join("",$prefix, " ", $_ , "\n")
    $pmda->log(join("",$prefix, " ", $_))
        foreach @args;
}


sub myinfo  { mylog("INFO",@_) }
sub mydebug { $cfg{debug} < 1 ? return : mylog("DEBUG",@_) }
sub mytrace { $cfg{debug} < 2 ? return : mylog("TRACE",@_) }

sub myerror {
    $pmda->err("ERROR " . $_)
        foreach @_;
}

sub myfatal {
    $pmda->err("FATAL " . $_)
        foreach @_;

    exit 10;
}


#- Configuration-related subroutines
sub load_config {
    my ($in_fname) = @_;
    my $refh_res;

    open my $fh_in,"<",$in_fname;

    my ($host_id,$db_id,$lineno) = (0,0,0);
    my $err_count;

    while (my $aline = <$fh_in>) {
        $lineno++;
        chomp $aline;

        if ($aline =~ /\A\s*#/) {
            mytrace("#$lineno: Skipping line '$aline'");

            next
        } elsif ($aline =~ /\A\s*\Z/) {
            mytrace("#$lineno: Skipping line '$aline'");

            next
        } elsif ($aline =~ /\A\s*host\s*=\s*(?<uri>(?:(?<proto>\S+):\/\/)?(?<host>\S+):(?<port>\d+)?)\s*\Z/) {
            mytrace(Data::Dumper->Dump([\$lineno,$+{proto},$+{host},$+{port},$+{uri},$aline],
                                       [qw{lineno proto host port uri aline}]));

            die "More than single host given"
                if exists $refh_res->{host} and defined $refh_res->{host};

            $refh_res = {
                proto => $+{proto},
                host  => $+{host},
                port  => $+{port},
                uri   => $+{uri}
            };

            # Autoconfigure if necessary
            $refh_res->{proto} //= "http";
            $refh_res->{port}  //= 80;

            $refh_res->{uri} = $refh_res->{proto} . "://" . $refh_res->{host} . ":" . $refh_res->{port};

            # Check that host names/addresses and port numbers are valid
            $err_count++,myerror("Failed to gethostbyname($$refh_res{host})")
                unless $refh_res->{host} and gethostbyname $refh_res->{host};
            $err_count++,myerror("Unexpected port number $$refh_res{port}")
                unless $refh_res->{port} and $refh_res->{port} >= 1 and $refh_res->{port} <= 65535;

            myinfo("Detected - host: '$$refh_res{host}', port: '$$refh_res{port}'");
        } else {
            warn "#$lineno: Unexpected line '$aline', skipping it";
        }

        # if (defined $refh_res
        #         and defined $refh_res->{host}
        #         and defined $refh_res->{port}
        #         and defined $refh_res->{uri}) {

        # } else {
        #     $err_count++;

        #     $pmda->err("Failed to detect host name/address and port number from '$aline'");
        #     next
        # }
    }

    mytrace(Dumper($refh_res));

    die "No host to be monitored found in '$in_fname'"
        unless exists $refh_res->{host} and defined $refh_res->{host};

    die "$err_count errors in config file detected, exiting"
        if $err_count;

    $refh_res
}

#- Data extraction subroutines (XML mongers)
#Note: I am not particularly good at XML. Anyone can find a better way to extract the information
#      and reimplement the subroutines but keep in mind to hold the minimum delay and memoization
#      features for minimum performance impact on the host where the PMDA is running.
sub get_server_stats {
    my ($xml) = @_;
    my $refh_res;

    foreach my $node ($xml->findnodes("/isc/bind/statistics/server/*")) {
        mytrace("node: ", Data::Dumper->Dump([$node->nodeName,$node->textContent],
                                             [qw{name content}]))
            if $cfg{debug};

        if ($node->nodeName eq "#text") {
            mytrace("skipping");

            next
        } elsif ($node->nodeName =~ /\A(boot-time|current-time)\Z/) {
            mytrace("times - '"
                . $node->nodeName
                . "': '"
                . $node->textContent);

            $refh_res->{join ".",$cfg{pmda_prefix},$1} = $node->textContent
        } elsif ($node->nodeName eq "requests") {
            mytrace("requests: ",
                    Data::Dumper->Dump([@{$node->childNodes}],
                                       [qw{child_nodes}]))
                if $cfg{debug};

            foreach my $opcode ($node->childNodes) {
                mytrace("opcode ",
                        Data::Dumper->Dump([$opcode->nodeName,$opcode->textContent],
                                           [qw{name value}]))
                    if $cfg{debug};

                next
                    if $opcode->nodeName eq "#text";

                my %data;

                foreach my $child_node ($opcode->childNodes) {
                    mytrace("child_node ",
                            Data::Dumper->Dump([$child_node->nodeName,$child_node->textContent],
                                               [qw{name content}]))
                        if $cfg{debug};

                    next
                        if $child_node->nodeName eq "#text";

                    $data{$child_node->nodeName} = $child_node->textContent;
                }

                mytrace("opcode ",
                        Data::Dumper->Dump([$data{name},$data{counter}],
                                           [qw{name counter}]))
                    if $cfg{debug};

                $refh_res->{join ".",
                            $cfg{pmda_prefix},
                            "total",
                            "queries",
                            "out",
                            $data{name}} = $data{counter};
            }
        } elsif ($node->nodeName eq "queries-in") {
            foreach my $rdtype ($node->childNodes) {
                next
                    if $rdtype->nodeName eq "#text";

                my %data;

                foreach my $child_node ($rdtype->childNodes) {
                    next
                        if $child_node->nodeName eq "#text";

                    $data{$child_node->nodeName} = $child_node->textContent;
                }

                $refh_res->{join ".",
                            $cfg{pmda_prefix},
                            "queries",
                            "in",
                            $data{name}} = $data{counter};
            }
        } elsif ($node->nodeName =~ /\A(nsstat|zonestat|sockstat)\Z/) {
            my %data;

            foreach my $child_node ($node->childNodes) {
                next
                    if $child_node->nodeName eq "#text";

                $data{$child_node->nodeName} = $child_node->textContent;
            }

            $refh_res->{join ".",$cfg{pmda_prefix},
                        $node->nodeName,
                        $data{name}} = $data{counter};
        } else {
            die "Assertion error - failed to recognize node name '" . $node->nodeName . "'";
        }
    }

    mytrace("get_server_stats: ", Dumper($refh_res))
        if $cfg{debug};

    $refh_res
}

sub get_task_stats {
    my ($xml) = @_;
    my $refh_res;

    # Get the thread model counters
    my %data;

    foreach my $task ($xml->findnodes("/isc/bind/statistics/taskmgr/thread-model/*")) {
        next
            if $task->textContent eq "#text";

        $refh_res->{join '.',$cfg{pmda_prefix},"thread_model",$task->nodeName} = $task->textContent;
    }

    mytrace("get_task_stats - thread-model: ", Dumper(\$refh_res))
        if $cfg{debug};

    # Get the task counters
    foreach my $task ($xml->findnodes("/isc/bind/statistics/taskmgr/tasks/*")) {
        next
            if $task->textContent eq "#text";

        my ($id,$name,%data);

        foreach my $node ($task->childNodes) {
            if ($node->nodeName eq "id") {
                $id = $node->textContent;
            } elsif ($node->nodeName eq "name") {
                $name = $node->textContent;
            } elsif ($node->nodeName eq "#text") {
                next
            } else {
                $data{$node->nodeName} = $node->textContent;
            }
        }

        $name //= "other";
        mytrace("task: ", Data::Dumper->Dump([$id,$name,\%data],
                                             [qw{id name data}]))
            if $cfg{debug};

        $refh_res->{join ".",
                    $cfg{pmda_prefix},
                    "tasks",
                    $name,
                    $_}{$id} = $data{$_}
            foreach keys %data;
    }

    mytrace("get_task_stats: ", Dumper(\$refh_res))
        if $cfg{debug};

    $refh_res
}

sub get_socket_stats {
    my ($xml) = @_;
    my ($refh_res,%stats);

    foreach my $node ($xml->findnodes("/isc/bind/statistics/socketmgr/sockets/*")) {
        my %data = (id              => undef,
                    name            => undef,
                    references      => undef,
                    type            => undef,
                    "local-address" => undef,
                    "peer-address"  => undef);
        my $refh_states;

        foreach my $child_node ($node->childNodes) {
            mytrace("child_node: " . $child_node->nodeName);

            # foreach my $name (keys %data) {
            if ($child_node->nodeName eq "states") {
                mytrace("states: " . Dumper($child_node->childNodes->to_literal))
                    if $cfg{debug};

                foreach my $state ($child_node->childNodes) {
                    my $state_name = $state->textContent;
                    my $name = $state->nodeName;

                    mytrace(Data::Dumper->Dump([\$name,\$state_name],
                                               [qw{name state_name}]))
                        if $cfg{debug};

                    next
                        if $name eq "#text";

                    $refh_states->{$state_name} = 1;
                }
            } elsif (exists $data{$child_node->nodeName}) {
                $data{$child_node->nodeName} = $child_node->textContent;
            } elsif ($child_node->nodeName eq "#text") {
                next
            } else {
                die "Assertion error - unexpected node '" . $child_node->nodeName . "' in <socket>"
            # }
            }
        }

        # Calculate statistics
        $stats{"$cfg{pmda_prefix}.sockets.total.bound-remote"}++
            if defined $refh_res->{"peer-address"};
        $stats{"$cfg{pmda_prefix}.sockets.total.name"}{$data{name}}++
            if defined $refh_res->{name};
        $stats{"$cfg{pmda_prefix}.sockets.total.proto"}{$data{type}}++;

        foreach my $state (qw{bound connected listener}) {
            $stats{"$cfg{pmda_prefix}.sockets.total.state_$state"}++
                if exists $refh_states->{$state};
        }

        mytrace("id: '$data{id}'");

        foreach my $key (grep {not /\Aid\Z/} sort keys %data) {
            $refh_res->{join ".",
                        $cfg{pmda_prefix},
                        "sockets",
                        $key}{$data{id}} = $data{$key};
        }

        foreach my $state (keys %$refh_states) {
            $refh_res->{join ".",
                        $cfg{pmda_prefix},
                        "sockets",
                        "states",
                        $state}{$data{id}} = $refh_states->{$data{id}} // 1;
        }
    }

    $refh_res->{$_} = $stats{$_}
        foreach keys %stats;

    mytrace("get_socket_stats: ", Dumper(\$refh_res))
        if $cfg{debug};

    $refh_res;
}

sub get_resstat {
    my ($xml,$view) = @_;
    my $refh_res;

    foreach my $node ($xml->findnodes("/isc/bind/statistics/views/view[name='$view']/resstat")) {
        my ($name,$value);

        foreach my $child_node ($node->childNodes) {
            my $node_name = $child_node->nodeName;

            mytrace("node_name: '$node_name'");

            if ($node_name eq "name") {
                $name = $child_node->textContent;

                mytrace("name: '$name'");
            } elsif ($node_name eq "counter") {
                $value = $child_node->textContent;

                mytrace("value: '$value'");
            } elsif ($node_name eq "#text") {
                next
            } else {
                die "Unexpected XML node in resstat: '$node_name'";
            }
        }

        $refh_res->{join ".",
                    $cfg{pmda_prefix},
                    "resolver",
                    "total",
                    $name} = $value;
    }

    mytrace("params: ", Dumper(\$refh_res))
        if $cfg{debug};

    $refh_res
}

sub get_zone_stats {
    my ($xml,$view) = @_;
    my $refh_res;

    mytrace("get_zone_stats:" . Data::Dumper->Dump([\$xml,\$view],[qw{xml view}]))
        if $cfg{debug};

    foreach my $zone ($xml->findnodes("/isc/bind/statistics/views/view[name='$view']/zones/*")) {
        my ($name) = map { $_->textContent } $zone->findnodes("./name");

        $name =~ s/\/\S+//;
        mytrace(Data::Dumper->Dump([\$zone,\$name],[qw{zone name}]))
            if $cfg{debug};

        foreach my $node ($zone->childNodes) {
            mytrace(Data::Dumper->Dump([$node->nodeName],[qw{name}]))
                if $cfg{debug};

            if ($node->nodeName =~ /\A(name|#text)\Z/) {
                next
            } elsif ($node->nodeName eq "counters") {
                foreach my $counter ($node->childNodes) {
                    next
                        if $counter->nodeName eq "#text";

                    $refh_res->{join ".",
                                $cfg{pmda_prefix},
                                "zones",
                                $counter->nodeName}->{$name} = $counter->textContent;
                }
            } else {
                $refh_res->{join ".",
                            $cfg{pmda_prefix},
                            "zones",
                            "serial"}{$name} = $node->textContent;
            }
        }
    }

    mytrace("get_zone_stats results: ", Data::Dumper->Dump([$refh_res],[qw{refh_res}]))
        if $cfg{debug};

    $refh_res
}

sub get_memory_stats {
    my ($xml) = @_;
    my $refh_res;

    foreach my $node ($xml->findnodes("/isc/bind/statistics/memory/summary")) {
        next
            if $node->nodeName eq "#text";

        foreach my $child_node ($node->childNodes) {
            next
                if $child_node->nodeName eq "#text";

            $refh_res->{join ".",
                        $cfg{pmda_prefix},
                        "memory",
                        "total",
                        $child_node->nodeName} = $child_node->textContent;
        }
    }

    mytrace("get_memory_stats - total: ", Dumper(\$refh_res))
        if $cfg{debug};

    foreach my $context ($xml->findnodes("/isc/bind/statistics/memory/contexts/*")) {
        next
            if $context->nodeName eq "#text";

        my ($id,%data);

        foreach my $child_node ($context->childNodes) {
            if ($child_node->nodeName eq "#text") {
                next
            } elsif ($child_node->nodeName eq "id") {
                $id = $child_node->textContent
            } else {
                $data{$child_node->nodeName} = $child_node->textContent;
            }
        }

        mytrace(Data::Dumper->Dump([$id,\%data],[qw{id data}]))
            if $cfg{debug};

        foreach (keys %data) {
            $refh_res->{join ".",
                        $cfg{pmda_prefix},
                        "memory",
                        "contexts",
                        $_}{$id} = $data{$_};
        }
    }

    mytrace("get_memory_stats: ", Dumper(\$refh_res))
        if $cfg{debug};

    $refh_res
}

#- Overall data fetch subroutine to get the XML into the PMDA process
sub fetch_bind_stats {
    my ($uri) = @_;
    my @time1 = gettimeofday;
    my $response;

    eval {
        local $SIG{ALRM} = sub {
            mydebug("alarm timeouted");

            die "Timeout alarm"
        };

        mydebug("Set alarm to $cfg{max_get_delay_sec} seconds ...");
        alarm $cfg{max_get_delay_sec};

        #$content = get $uri
        #    or return undef;

        $response = $lwp_user_agent->get($cfg{loaded}{uri});
        mydebug(Data::Dumper->Dump([$response],[qw{response}]));

        alarm 0;
        mydebug("Alarm disarmed");
    };

    if ($@) {
        if ($@ =~ /Timeout alarm/) {
            mydebug("LWP GET timeouted");

            return undef
        } else {
            myerror("Exception while waiting for LWP get: $@");

            return undef
        }
    }

    my @time2 = gettimeofday;
    my $fetch_delay_sec = (($time2[0] - $time1[0]) + ($time2[1] - $time1[1])/1e6);

    mydebug("Fetch delay: $fetch_delay_sec secs");

    # Check the response
    unless ($response->is_success) {
        myerror("Failed to retrieve stats from '$cfg{loaded}{uri}': " . $response->status_line);

        return undef
    }

    mytrace(Data::Dumper->Dump([\$response->content],[qw{content}]))
        if $cfg{debug};

    # Parse the stats
    my $parser = XML::LibXML->new
        or die "Failed to create XML parser";
    my $dom;

    eval {
        $dom = $parser->load_xml(
            # location => $uri,
            string => $response->content,
        );
    };

    if ($@) {
        myerror("Exception caught while parsing content: '$@'");

        return undef
    }

    # unless ($dom) {
    #     myerror("Failed to parse content");

    #     return undef
    # }

    my ($refh_res,$refh_foo);
    my %subs = (
        server           => sub { get_server_stats($_[0])          },
        task             => sub { get_task_stats($_[0])            },
        socket           => sub { get_socket_stats($_[0])          },
        resolver_default => sub { get_resstat($_[0],"_default")    },
        # resolver_bind    => sub { get_resstat($_[0],"_bind")       },
        zone_default     => sub { get_zone_stats($_[0],"_default") },
        # zone_bind        => sub { get_zone_stats($_[0],"_bind")    },
        memory           => sub { get_memory_stats($_[0])          });

    mytrace(Data::Dumper->Dump([\%subs],[qw{subs}]))
        if $cfg{debug};

    foreach my $type (sort keys %subs) {
        mytrace(Data::Dumper->Dump([\$type],[qw{type}]))
            if $cfg{debug};

        $refh_foo = $subs{$type}->($dom)
            or die "failed to get $type stats";

        $refh_res = merge_hashrefs_to_first($refh_res,$refh_foo)
            or die "Failed to merge $type stats";
    }

    # Remove the actually unneeded data
    my @unwanted_metrics = grep {
        my $metric = $_;

        grep {$metric =~ /\A$_/} qw{bind2.memory.contexts
                                    bind2.sockets.local-address
                                    bind2.sockets.peer-address
                                    bind2.sockets.name
                                    bind2.sockets.references
                                    bind2.sockets.states
                                    bind2.sockets.type
                                    bind2.tasks}
    } sort keys %$refh_res;

    mydebug("Intentionally not considered metrics: " . join("\n  ", sort @unwanted_metrics));

    delete $refh_res->{$_}
        foreach @unwanted_metrics;

    {
        local $Data::Dumper::Maxdepth = 2;

        mydebug("fetch_bind_stats - result: " . Data::Dumper->Dump([\$refh_res],[qw{refh_res}]))
            if $cfg{debug};
    }

    my @time3 = gettimeofday;
    my $processing_delay_sec = (($time3[0] - $time2[0]) + ($time3[1] - $time2[1])/1e6);

    mydebug("Processing delay: $processing_delay_sec secs ()");

    $refh_res
}

#- Initialization procedure
sub init_metrics {
    my ($uri) = @_;

    mydebug("Entering init_metrics() ...");

    my $refh_res = fetch_bind_stats($uri);
    mydebug("Failed to fetch Bind server stats")
        unless defined $refh_res;

    mydebug("init_metrics:");

    # Create list of current metrics, type, semantics and values
    foreach my $metric (sort keys %$refh_res) {
        $metrics{$metric}{value} = $refh_res->{$metric};

        # Get the metric type ...
        my (@types,@semantics);

        foreach my $data_type (keys %{$cfg{metrics}{types}}) {
            push @types,$data_type
                foreach grep { $metric =~ /\A$_/ } @{$cfg{metrics}{types}{$data_type}};
        }

        die "More than one data types found for metric '$metric': @types"
            if @types and @types > 1;

        $metrics{$metric}{type} = $types[0] // $cfg{metrics}{defaults}{type};

        #TODO: Test both bind server response time and parsing time for huge (tens of thousands) sockets and use only stats if needed (the same with memory contexts)
        #TODO: Exclude resources and the whole bind2.tasks until they are proved useful

        # ...  and semantics
        foreach my $data_semantics (keys %{$cfg{metrics}{semantics}}) {
            push @semantics,$data_semantics
                foreach grep { $metric =~ /\A$_/ } @{$cfg{metrics}{semantics}{$data_semantics}};
        }

        die "More than one data semantics found for metric '$metric': @semantics"
            if @semantics and @semantics > 1;

        $metrics{$metric}{semantics} = $semantics[0] // $cfg{metrics}{defaults}{semantics};
        mytrace("Metric '$metric' - type: '$metrics{$metric}{type}', semantics: '$metrics{$metric}{semantics}', value: '$metrics{$metric}{value}'");
    }

    #TODO: Make the bind2.sockets.peer-address or other metrics with undefined valued not present
    #TODO: Make the autoconfiguration optional or take it out to a script
    #TODO: bind2.sockets.{local_address,name,peer_address,bound/unbound} -> bind2.sockets.total
    #TODO: Make the bind2.sockets.{stats.{listener,connected},type}, bind2.sockets.type a scalar

    # Create a list of current instance domains
    foreach my $metric (sort keys %metrics) {
        # Skip all the scalar metrics
        next
            if ref $metrics{$metric}{value} ne ref {};

        mydebug("Deriving instance domain of metric: '$metric': " . Dumper($metrics{$metric}{value}));

        # Check if there is an existing metric space - get the difference
        my $indom_id;

        foreach my $cur_indom_id (sort {$a <=> $b} keys %indoms) {
            # Check if instances in existing instance domain are the same (the domain is the same)
            # Note: This way may be prone to instance domains that would contain the same members
            #       at the time of the initialization but would be different in general. For the
            #       moment, I consider this to be unlikely case but in general this may lead to
            #       problems. General configuration generated on demand by script outside
            #       of the PMDA code could resolve this by enabling a check.

            my @difference = grep {
                not exists $indoms{$cur_indom_id}{$_}
            } sort keys %{$metrics{$metric}{value}};

            unless (@difference) {
                $indom_id = $cur_indom_id;

                last
            }
        }

        # mydebug(Data::Dumper->Dump([$indom_id,\%indoms],[qw{indom_id indoms}]));

        # This is a new indom
        unless (defined $indom_id) {
            $indom_id = scalar(keys(%indoms)) + 1;

            # Add the new indom
            my $instance_id = 0;

            $indoms{$indom_id} = { map {
                my $foo = $_;

                mydebug("foo: '$foo', instance_id: '$instance_id'");

                $foo => $instance_id++
            } sort keys %{$metrics{$metric}{value}} };
#            } sort keys %{$current_data{$metric}} };
        }

        mydebug("... added indom: " . Data::Dumper->Dump([$indom_id,$indoms{$indom_id}],
                                                         [qw{indom_id indoms_of_indom_id}]));

        $metrics{$metric}{indom_id} = $indom_id;
    }

    # Copy current values to current_data
    my @current_time = gettimeofday;

    $current_data{timestamp} = $current_time[0] + $current_time[1]/1e6;
    $current_data{values} = { map {
        $_ => $metrics{$_}{value};
    } keys %metrics };

    mydebug("init_metrics result: " . Data::Dumper->Dump([\%metrics,\%indoms,\%current_data],
                                                         [qw{metrics indoms current_data}]));

    1
}

sub merge_hashrefs_to_first {
    my ($x,$y) = @_;

    mytrace("merge_hashrefs_to_first: " . Data::Dumper->Dump([\$x,\$y],[qw{x y}]))
        if $cfg{debug};

    return undef
        unless defined $y and ref $y eq ref {};

    # print "DEBUG: ... still there\n";

    foreach (keys %$y) {
        die "Assertion error - key $_ already exists"
            if exists $x->{$_};

        $x->{$_} = $y->{$_}
    }

    mytrace("merge_hashrefs_to_first - result: " . Data::Dumper->Dump([\$x],[qw{x}]))
        if $cfg{debug};

    $x
}

__END__

=head1 NAME

try_XML-LibXML-Parser.pl - Describe the usage of script briefly

=head1 SYNOPSIS

try_XML-LibXML-Parser.pl [options] args

      -opt --long      Option description

=head1 DESCRIPTION

Stub documentation for try_XML-LibXML-Parser.pl,

=head1 AUTHOR

, E<lt>lukas@samE<gt>

=head1 COPYRIGHT AND LICENSE

Copyright (C) 2016 by

This program is free software; you can redistribute it and/or modify
it under the same terms as Perl itself, either Perl version 5.8.2 or,
at your option, any later version of Perl 5 you may have available.

=head1 BUGS

None reported... yet.

=cut
