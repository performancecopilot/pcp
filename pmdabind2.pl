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
use Time::HiRes            qw{gettimeofday};
use LWP::Simple;
use XML::LibXML;
use List::MoreUtils        qw{uniq};
use File::Spec::Functions  qw{catfile};
use File::Basename         qw{basename};
use Data::Dumper;
#use Devel::Trace;

#use Carp;

#$SIG{ __DIE__ } = sub { Carp::confess( @_ ) };

$Data::Dumper::Sortkeys = 1;
#$Devel::Trace::TRACE = 1;

# Assumes following options to be enabled in named.conf:
#  - statistics-channels { inet <addr> port <port> allow { any }; }
#  - options { zone-statistics yes; }

# TODO: Make addr, port and allow_addr, allow_port in configuration
# TODO: Make zone-statistics possible to be disabled

# my $dom = XML::LibXML->load_xml(
#     location => $ARGV[0],
#     # parser options ...
# );

my %cfg = (
    config_fname => "bind2.conf",
    pmda_prefix  => "bind2",
    bind_uri     => "http://perf-v45.cz.intinfra.com:9999",
    pmda_id      => 250,

    debug        => 1,
);

our %current_data;

$0 = "pmda$cfg{pmda_prefix}";

# Enable PCP debugging
$ENV{PCP_DEBUG} = 65535
    if $cfg{debug};

# Config check
#TODO config_check(\%cfg);

#print STDERR "Starting myredis PMDA\n";
my $pmda = PCP::PMDA->new($cfg{pmda_prefix}, $cfg{pmda_id});

die "Failed to load config file"
    unless $cfg{loaded} = load_config(catfile(pmda_config('PCP_PMDAS_DIR'),
                                              $cfg{pmda_prefix},
                                              $cfg{config_fname}));
$pmda->connect_pmcd;

#$Devel::Trace::TRACE = 1;
mydebug("Connected to PMDA");

fetch_bind_stats($cfg{bind_uri});

#TODO: Add warnings/unknown_stats counter

## Subroutines
# log_me ($level_prefix,@lines) - logs lines with given log level prefix
#                            - it is not intended to be run directly but from logging subroutines
# sub log_me {
#     my ($level,@args) = @_;
#     my ($pkg,$fname,$lineno,$subroutine) = caller(2);

#     #print Data::Dumper->Dump([caller(2)],[qw{caller}]);
#     #
#     #print STDERR Data::Dumper->Dump([\$pkg,\$fname,\$lineno,\$subroutine],
#     #                                [qw{pkg fname lineno subroutine}]);
#     #my $basename = basename $fname;

#     # Note: caller can do even more, but the four are of my interest

#     chomp @args;
#     #$subroutine =~ s/main:://;

#     if ($level eq "FATAL" or $level eq "ERROR") {
#         $pmda->error("[$subroutine #$lineno] " . ($_ // "<undef>"))
#             foreach @args;
#     } else {
#         $pmda->log("[$subroutine #$lineno] " . ($_ // "<undef>"))
#             foreach @args;
#     }
# }

# sub myfatal {
#     my ($exit_val,@args) = @_;

#     log_me("FATAL", @args);
#     exit $exit_val
# }
# sub myerror { log_me("ERROR",@_) }
# #sub mywarn  { log_me("WARN", @_) }
# sub mydebug {
#     return
#         unless $cfg{debug};

#     log_me("DEBUG", @_);
# }

sub mydebug {
    my @args = @_;

    chomp @args;
    print STDERR "DEBUG " . $_ . "\n"
        foreach @args;
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
        } elsif ($aline =~ /\A\s*host\s*=\s*(?<uri>(?:(?<proto>\S+):\/\/)?(?<host>\S+):(?<port>\d+)?)\s*\Z/) {
            mydebug("#$lineno: proto: '$+{proto}', host: '$+{host}', port: '$+{port}', uri: $+{uri} from '$aline'");

            $refh_res->{host} = { proto => $+{proto},
                                  host  => $+{host},
                                  port  => $+{port},
                                  uri   => $+{uri}};
        } else {
            warn "#$lineno: Unexpected line '$aline', skipping it";
        }
    }

    # Check mandatory options
    die "No mandatory keys found"
        unless keys %$refh_res;

    mydebug(Dumper($refh_res))
        if $cfg{debug};

    die "No host to be monitored found in '$in_fname'"
        unless exists $refh_res->{host} and defined $refh_res->{host};

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

sub get_server_stats {
    my ($xml) = @_;
    my $refh_res;

    foreach my $node ($xml->findnodes("/isc/bind/statistics/server/*")) {
        mydebug("node: ", Data::Dumper->Dump([$node->nodeName,$node->textContent],
                                             [qw{name content}]))
            if $cfg{debug};

        if ($node->nodeName eq "#text") {
            mydebug("skipping");

            next
        } elsif ($node->nodeName =~ /\A(boot-time|current-time)\Z/) {
            mydebug("times - '"
                . $node->nodeName
                . "': '"
                . $node->textContent);

            $refh_res->{join ".",$cfg{pmda_prefix},$1} = $node->textContent
        } elsif ($node->nodeName eq "requests") {
            mydebug("requests: ",
                    Data::Dumper->Dump([@{$node->childNodes}],
                                       [qw{child_nodes}]))
                if $cfg{debug};

            foreach my $opcode ($node->childNodes) {
                mydebug("opcode ",
                        Data::Dumper->Dump([$opcode->nodeName,$opcode->textContent],
                                           [qw{name value}]))
                    if $cfg{debug};

                next
                    if $opcode->nodeName eq "#text";

                my %data;

                foreach my $child_node ($opcode->childNodes) {
                    mydebug("child_node ",
                            Data::Dumper->Dump([$child_node->nodeName,$child_node->textContent],
                                               [qw{name content}]))
                        if $cfg{debug};

                    next
                        if $child_node->nodeName eq "#text";

                    $data{$child_node->nodeName} = $child_node->textContent;
                }

                mydebug("opcode ",
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

    mydebug("get_server_stats: ", Dumper($refh_res))
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

    mydebug("get_task_stats - thread-model: ", Dumper(\$refh_res))
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

        mydebug("task: ", Data::Dumper->Dump([$id,$name,\%data],
                                             [qw{id name data}]))
            if $cfg{debug};

        $refh_res->{join ".",
                    $cfg{pmda_prefix},
                    "tasks",
                    $name,
                    $_}{$id} = $data{$_}
            foreach keys %data;
    }

    mydebug("get_task_stats: ", Dumper(\$refh_res))
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
            mydebug("child_node: " . $child_node->nodeName);

            # foreach my $name (keys %data) {
            if ($child_node->nodeName eq "states") {
                mydebug("states: " . Dumper($child_node->childNodes->to_literal))
                    if $cfg{debug};

                foreach my $state ($child_node->childNodes) {
                    my $state_name = $state->textContent;
                    my $name = $state->nodeName;

                    mydebug(Data::Dumper->Dump([\$name,\$state_name],
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

        mydebug("id: '$data{id}'");

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

    mydebug("get_socket_stats: ", Dumper(\$refh_res))
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

            mydebug("node_name: '$node_name'");

            if ($node_name eq "name") {
                $name = $child_node->textContent;

                mydebug("name: '$name'");
            } elsif ($node_name eq "counter") {
                $value = $child_node->textContent;

                mydebug("value: '$value'");
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

    mydebug("params: ", Dumper(\$refh_res))
        if $cfg{debug};

    $refh_res
}

sub get_zone_stats {
    my ($xml,$view) = @_;
    my $refh_res;

    mydebug(Data::Dumper->Dump([\$xml,\$view],[qw{xml view}]))
        if $cfg{debug};

    foreach my $zone ($xml->findnodes("/isc/bind/statistics/views/view[name='$view']/zones/*")) {
        my ($name) = map { $_->textContent } $zone->findnodes("./name");

        $name =~ s/\/\S+//;
        mydebug(Data::Dumper->Dump([\$zone,\$name],[qw{zone name}]))
            if $cfg{debug};

        foreach my $node ($zone->childNodes) {
            mydebug("node: ", Dumper($node->nodeName))
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

    mydebug("params: ", Dumper(\$refh_res))
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

    mydebug("get_memory_stats - total: ", Dumper(\$refh_res))
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

        mydebug(Data::Dumper->Dump([$id,\%data],[qw{id data}]))
            if $cfg{debug};

        foreach (keys %data) {
            $refh_res->{join ".",
                        $cfg{pmda_prefix},
                        "memory",
                        "contexts",
                        $_}{$id} = $data{$_};
        }
    }

    mydebug("get_memory_stats: ", Dumper(\$refh_res))
        if $cfg{debug};

    $refh_res
}

sub fetch_bind_stats {
    my ($uri) = @_;
    my @time1 = gettimeofday;
    my $content = get $uri
        or return undef;
    my @time2 = gettimeofday;

    print "Fetch delay: ", ($time2[0] - $time1[0]) + ($time2[1] - $time1[1])/1e6, " secs\n";

    mydebug("content: '" . $content)
        if $cfg{debug};

    my $parser = XML::LibXML->new
        or die "Failed to create XML parser";
    my $dom = $parser->load_xml(
        # location => $uri,
        string => $content,
    ) or die "Failed to parse '$uri'";

    my ($refh_res,$refh_foo);
    my %subs = (
        server           => sub { get_server_stats($_[0]) },
        task             => sub { get_task_stats($_[0]) },
        socket           => sub { get_socket_stats($_[0]) },
        resolver_default => sub { get_resstat($_[0],"_default") },
        resolver_bind    => sub { get_resstat($_[0],"_bind") },
        zone_default     => sub { get_zone_stats($_[0],"_default") },
        zone_bind        => sub { get_zone_stats($_[0],"_bind") },
        memory           => sub { get_memory_stats($_[0])});

    mydebug(Data::Dumper->Dump([\%subs],[qw{subs}]))
        if $cfg{debug};

    foreach my $type (sort keys %subs) {
        mydebug(Data::Dumper->Dump([\$type],[qw{type}]))
            if $cfg{debug};

        $refh_foo = $subs{$type}->($dom)
            or die "failed to get $type stats";

        merge_hashrefs_to_first($refh_res,$refh_foo)
            or die "Failed to merge $type stats";
    }

    $refh_res
}

sub merge_hashrefs_to_first {
    my ($x,$y) = @_;

    mydebug(Data::Dumper->Dump([\$x,\$y],[qw{x y}]))
        if $cfg{debug};

    return undef
        unless defined $y and ref $y eq ref {};

    # print "DEBUG: ... still there\n";

    foreach (keys %$y) {
        die "Assertion error - key $_ already exists"
            if exists $x->{$_};

        $x->{$_} = $y->{$_}
    }

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
