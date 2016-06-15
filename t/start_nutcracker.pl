#!/usr/bin/env perl

use warnings  FATAL => qw{uninitialized};
use strict;
use autodie;
use feature  qw{say};

use YAML::XS qw{Dump};
use FindBin  qw($RealBin);
use Data::Dumper;

our %cfg = (
    config => {
        redis => {
            daemonize => "no",
            pidfile => "./redis-PORTNUMBER.pid",
            bind => "127.0.0.1",
            # port          => 12345,
            "tcp-backlog"   => 511,
            "tcp-keepalive" => 0,
            loglevel        => "DEBUG",
            logfile         => "$RealBin/redis-PORTNUMBER.log",
            databases       => 16,
        },

        nutcracker => {
            default => {
                # listen       => "./nutcracker.sock",
                hash         => "fnv1a_64",
                hash_tag     => '{}',
                distribution => "ketama",
                timeout      => 400,
                auto_eject_hosts     => "true",
                server_retry_timeout => 2000,
                server_failure_limit => 1,
                redis                => "true",
                server_connections   => 1,
            },
        },
    },

    instances => {
        1 => {
            stat   => "13000",

            redis1 => {
                listen => "127.0.0.1:11000",
                port   => [ 12001 .. 12004 ],
            },
        },

        2 => {
            stat   => "13001",

            redis2_1 => {
                listen => "127.0.0.1:11001",
                port   => [ 12111 .. 12116 ],
            },

            redis2_2 => {
                listen =>"127.0.0.1:11002",
                port => [ 12121 .. 12123 ],
            },
        },

        3 => {
            stat   => "13002",

            redis3_1 => {
                listen => "127.0.0.1:11003",
                port   => [ 12211 .. 12216 ],
            },

            redis3_2 => {
                listen => "127.0.0.1:11004",
                port => [ 12221 .. 12223 ],
            },

            redis3_3 => {
                listen => "127.0.0.1:11005",
                port => [ 12231 .. 12237 ],
            },
        },
    },

    redis_cmd     => "/usr/sbin/redis-server",

    nc_cmd        => "/usr/sbin/nutcracker",
    nc_start_args => "--verbose=9 -o $RealBin/nutcracker-INST.log -c $RealBin/nutcracker-INST.yml -s STAT_PORT -a 127.0.0.1 -i 1 -p $RealBin/nutcracker-INST.pid --mbuf-size 1024",
);

$0 = "nutcracker_starter";

say STDERR "DEBUG realbin: $RealBin";

say "Config dump: ", Dumper(\%cfg);

# Remove previous redis and nutcracker config files and instances
my @pids;

foreach my $pid_file (glob "*.pid") {
    open my $fh_in,"<",$pid_file;

    my $pid = <$fh_in>;

    chomp $pid;

    push @pids,$pid;

    close $fh_in;
}

say STDERR "Pids to kill: @pids";

system "pkill @pids"
    if @pids;

foreach my $apid (@pids) {
    kill "KILL",$apid
        if -e "/proc/$apid";
}

my @fnames = glob "$RealBin/redis*.conf $RealBin/nutcracker*.yml $RealBin/*.pid $RealBin/*.log";

say STDERR "Files to unlink:\n", map { "  $_\n" } sort @fnames;
unlink $_
    foreach @fnames;

# Generate redis and nutcracker config files
my (@nc_fnames,@redis_fnames);

foreach my $nc_inst (sort keys %{$cfg{instances}}) {
    say STDERR "generating $nc_inst ...";

    my $refh_nc_group = $cfg{instances}{$nc_inst};
    my $refh_nc_config;
    my $nc_fname = "$RealBin/nutcracker-$nc_inst.yml";

    open my $fh_nc_out,">",$nc_fname;
    push @nc_fnames,{cfg_fname => $nc_fname,
                     stat_port => $refh_nc_group->{stat},
                     inst      => $nc_inst,
                 };

    my $redis_index = 0;

    foreach my $redis_group (grep {/\Aredis/} sort keys %$refh_nc_group) {
        $refh_nc_config->{$redis_group}{listen} = $cfg{instances}{$nc_inst}{$redis_group}{listen};

        #- generate common configuration
        $refh_nc_config->{$redis_group}->{$_} = $cfg{config}{nutcracker}{default}{$_}
            foreach keys %{$cfg{config}{nutcracker}{default}};

        foreach my $redis_port (@{$refh_nc_group->{$redis_group}->{port}}) {
            my $redis_fname = "$RealBin/redis-${redis_port}.conf";

            open my $fh_redis_out,">",$redis_fname;
            push @redis_fnames,$redis_fname;

            foreach my $key (sort keys %{$cfg{config}{redis}}) {
                (my $updated_value = $cfg{config}{redis}{$key}) =~ s/PORTNUMBER/$redis_port/g;

                say $fh_redis_out join(" ", $key, $updated_value);
            }

            print $fh_redis_out "port $redis_port";

            close $fh_redis_out;

            $redis_index++;
            push @{$refh_nc_config->{$redis_group}{servers}},"127.0.0.1:$redis_port:1 ${redis_group}_$redis_index";
        }
    }

    my @lines =
        grep {not /\A---\s*\Z/}
        split /[\r\n]+/, Dump($refh_nc_config);

    #say STDERR "DEBUG: lines:", Dumper(\@lines);

    print $fh_nc_out join "\n",@lines;
    close $fh_nc_out;
}

# Start redis instances
say "Starting redis instances using:\n",
    map { "  $_\n" } @redis_fnames;

system "$cfg{redis_cmd} $_ &"
    foreach @redis_fnames;

# Start nutcracker instance
say "Starting NutCracker instances using:\n", map { "  $$_{cfg_fname}\n" } @nc_fnames;

foreach my $refh_nc_inst (@nc_fnames) {
    my $start_args = $cfg{nc_start_args};
    $start_args =~ s/STAT_PORT/$$refh_nc_inst{stat_port}/g;
    $start_args =~ s/INST/$$refh_nc_inst{inst}/g;
    my $cmd = "$cfg{nc_cmd}  $start_args";

    say STDERR " cmd: '$cmd'";

    system "$cmd &";
}

# Wait until redis and nutcracker processes terminate
say STDERR "... all started, waiting for them to terminate";

sleep 1
    while wait != -1;
