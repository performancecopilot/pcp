#
# Copyright (c) 2010 Josef 'Jeff' Sipek <jeffpc@josefsipek.net>
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
use JSON;
use Time::HiRes qw ( time );

use vars qw( $pmda );
use vars qw( %devdata %satdata );

my $gpspipe = "gpspipe -w";

my $json;

my $gpsd_release = "?";
my $gpsd_rev = "?";
my $gpsd_proto_maj = -1;
my $gpsd_proto_min = -1;

# FIXME: the following will need changing to support multiple devices
my $gpsd_device = undef;
my %devdata = (
    "time"           => -1,
    "ept"            => -1,
    "lat"            => -1,
    "lon"            => -1,
    "alt"            => -1,
    "track"          => -1,
    "speed"          => -1,
    "climb"          => -1,
    "mode"           => 0,  # 0 = no fix
    "xdop"           => -1,
    "ydop"           => -1,
    "vdop"           => -1,
    "tdop"           => -1,
    "hdop"           => -1,
    "gdop"           => -1,
    "pdop"           => -1,
    "num_satellites" => 0,
);
my $gpsd_sat_indom = 0;
my @gpsd_sat_dom = ();

sub gpsd_pipe_callback
{
    (undef, $_) = @_;

    # $pmda->log("pipe got: $_");

    my $jtxt = $json->decode($_);

    if ($jtxt->{"class"} eq "VERSION") {
        $gpsd_release   = $jtxt->{"release"};
        $gpsd_rev       = $jtxt->{"rev"};
        $gpsd_proto_maj = $jtxt->{"proto_major"};
        $gpsd_proto_min = $jtxt->{"proto_minor"};
    } elsif ($jtxt->{"class"} eq "DEVICES") {
        foreach my $dev (@{$jtxt->{"devices"}}) {
            if ($dev->{"class"} eq "DEVICE") {
                $pmda->log("gpsd_pipe_callback: oops!  multiple " .
                    "devices detected, only using the first " .
                    "($gpsd_device)") if (defined($gpsd_device) and
                                          $dev->{"path"} ne $gpsd_device);

                $gpsd_device = $dev->{"path"};
            } else {
                $pmda->log("gpsd_pipe_callback: unknown class '" .
                    $dev->{"class"} . "'");
            }
        }
    } elsif ($jtxt->{"class"} eq "DEVICE") {
        # nothing to do
    } elsif ($jtxt->{"class"} eq "WATCH") {
        # nothing to do
    } elsif ($jtxt->{"class"} eq "TPV") {
        return if ($jtxt->{"device"} ne $gpsd_device);

        $devdata{"time"}  = $jtxt->{"time"};
        $devdata{"ept"}   = $jtxt->{"ept"};
        $devdata{"lat"}   = $jtxt->{"lat"};
        $devdata{"lon"}   = $jtxt->{"lon"};
        $devdata{"alt"}   = $jtxt->{"alt"};
        $devdata{"track"} = $jtxt->{"track"};
        $devdata{"speed"} = $jtxt->{"speed"};
        $devdata{"climb"} = $jtxt->{"climb"};
        $devdata{"mode"}  = $jtxt->{"mode"};
    } elsif ($jtxt->{"class"} eq "SKY") {
        return if ($jtxt->{"device"} ne $gpsd_device);

        $devdata{"xdop"} = $jtxt->{"xdop"};
        $devdata{"ydop"} = $jtxt->{"ydop"};
        $devdata{"vdop"} = $jtxt->{"vdop"};
        $devdata{"tdop"} = $jtxt->{"tdop"};
        $devdata{"hdop"} = $jtxt->{"hdop"};
        $devdata{"gdop"} = $jtxt->{"gdop"};
        $devdata{"pdop"} = $jtxt->{"pdop"};

        my %sats = {};
        my @dom = ();
        foreach my $sat (@{$jtxt->{"satellites"}}) {
            push(@dom, $sat->{"PRN"} => "$sat->{'PRN'}");

            $sats{"el"}{$sat->{"PRN"}} = $sat->{"el"};
            $sats{"az"}{$sat->{"PRN"}} = $sat->{"az"};
            $sats{"ss"}{$sat->{"PRN"}} = $sat->{"ss"};
            $sats{"used"}{$sat->{"PRN"}} = $sat->{"used"};
        }
        %satdata = %sats;
        $pmda->replace_indom($gpsd_sat_indom, \@dom);

        $devdata{"num_satellites"} = scalar(@dom) / 2;
    } else {
        $pmda->log("gpsd_pipe_callback: unknown class '" . $jtxt->{"class"} . "'");
    }
}

sub gpsd_fetch_callback
{
    my ($cluster, $item, $inst) = @_;
    my $metric_name = pmda_pmid_name($cluster, $item);

    # $pmda->log("gpsd_fetch_callback $metric_name $cluster:$item ($inst)\n");
    
    return (PM_ERR_PMID, 0) if (!defined($metric_name));

    if ($cluster == 0) {
        return (PM_ERR_INST, 0) if ($inst != PM_IN_NULL);

        if ($metric_name eq "gpsd.release") {
            return ($gpsd_release, 1);
        } elsif ($metric_name eq "gpsd.rev") {
            return ($gpsd_rev, 1);
        } elsif ($metric_name eq "gpsd.proto.major") {
            return ($gpsd_proto_maj, 1);
        } elsif ($metric_name eq "gpsd.proto.minor") {
            return ($gpsd_proto_min, 1);
        }
    }

    $metric_name =~ s/^gpsd\.devices\.dev0\.//;

    if ($metric_name =~ m/^satellites\./) {
        # satellite info
        $metric_name =~ s/^satellites\.//;
    
        # $pmda->log("gpsd_fetch_callbac2 $metric_name $cluster:$item ($inst): ${satdata{$metric_name}}\n");
        # $pmda->log("gpsd_fetch_callbac2 $metric_name $cluster:$item ($inst): ${satdata{$metric_name}{$inst}}\n");
        return (PM_ERR_INST, 0) if ($inst == PM_IN_NULL);
        return (PM_ERR_PMID, 0) if (!defined($satdata{$metric_name}{$inst}));
        return ($satdata{$metric_name}{$inst}, 1);
    }
        
    return (PM_ERR_INST, 0) if ($inst != PM_IN_NULL);
    return (PM_ERR_PMID, 0) if (!defined($devdata{$metric_name}));
    return ($devdata{$metric_name}, 1);
}

$json = new JSON;

$pmda = PCP::PMDA->new('gpsd', 105);

$pmda->add_metric(pmda_pmid(0,0), PM_TYPE_STRING, PM_INDOM_NULL, PM_SEM_INSTANT,
                  pmda_units(0,0,0,0,0,0),
                  "gpsd.release", '', '');
$pmda->add_metric(pmda_pmid(0,1), PM_TYPE_STRING, PM_INDOM_NULL, PM_SEM_INSTANT,
                  pmda_units(0,0,0,0,0,0),
                  "gpsd.rev", '', '');
$pmda->add_metric(pmda_pmid(0,2), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT,
                  pmda_units(0,0,0,0,0,0),
                  "gpsd.proto.major", '', '');
$pmda->add_metric(pmda_pmid(0,3), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT,
                  pmda_units(0,0,0,0,0,0),
                  "gpsd.proto.minor", '', '');

$pmda->add_metric(pmda_pmid(1,0), PM_TYPE_FLOAT, PM_INDOM_NULL, PM_SEM_INSTANT,
                  pmda_units(0,0,0,0,0,0),
                  "gpsd.devices.dev0.time", '', '');
$pmda->add_metric(pmda_pmid(1,1), PM_TYPE_FLOAT, PM_INDOM_NULL, PM_SEM_INSTANT,
                  pmda_units(0,0,0,0,0,0),
                  "gpsd.devices.dev0.ept", '', '');
$pmda->add_metric(pmda_pmid(1,2), PM_TYPE_FLOAT, PM_INDOM_NULL, PM_SEM_INSTANT,
                  pmda_units(0,0,0,0,0,0),
                  "gpsd.devices.dev0.lat", '', '');
$pmda->add_metric(pmda_pmid(1,3), PM_TYPE_FLOAT, PM_INDOM_NULL, PM_SEM_INSTANT,
                  pmda_units(0,0,0,0,0,0),
                  "gpsd.devices.dev0.lon", '', '');
$pmda->add_metric(pmda_pmid(1,4), PM_TYPE_FLOAT, PM_INDOM_NULL, PM_SEM_INSTANT,
                  pmda_units(0,0,0,0,0,0),
                  "gpsd.devices.dev0.alt", '', '');
$pmda->add_metric(pmda_pmid(1,5), PM_TYPE_FLOAT, PM_INDOM_NULL, PM_SEM_INSTANT,
                  pmda_units(0,0,0,0,0,0),
                  "gpsd.devices.dev0.track", '', '');
$pmda->add_metric(pmda_pmid(1,6), PM_TYPE_FLOAT, PM_INDOM_NULL, PM_SEM_INSTANT,
                  pmda_units(0,0,0,0,0,0),
                  "gpsd.devices.dev0.speed", '', '');
$pmda->add_metric(pmda_pmid(1,7), PM_TYPE_FLOAT, PM_INDOM_NULL, PM_SEM_INSTANT,
                  pmda_units(0,0,0,0,0,0),
                  "gpsd.devices.dev0.climb", '', '');
$pmda->add_metric(pmda_pmid(1,8), PM_TYPE_32, PM_INDOM_NULL, PM_SEM_INSTANT,
                  pmda_units(0,0,0,0,0,0),
                  "gpsd.devices.dev0.mode", '', '');

$pmda->add_metric(pmda_pmid(1,9), PM_TYPE_FLOAT, PM_INDOM_NULL, PM_SEM_INSTANT,
                  pmda_units(0,0,0,0,0,0),
                  "gpsd.devices.dev0.xdop", '', '');
$pmda->add_metric(pmda_pmid(1,10), PM_TYPE_FLOAT, PM_INDOM_NULL, PM_SEM_INSTANT,
                  pmda_units(0,0,0,0,0,0),
                  "gpsd.devices.dev0.ydop", '', '');
$pmda->add_metric(pmda_pmid(1,11), PM_TYPE_FLOAT, PM_INDOM_NULL, PM_SEM_INSTANT,
                  pmda_units(0,0,0,0,0,0),
                  "gpsd.devices.dev0.vdop", '', '');
$pmda->add_metric(pmda_pmid(1,12), PM_TYPE_FLOAT, PM_INDOM_NULL, PM_SEM_INSTANT,
                  pmda_units(0,0,0,0,0,0),
                  "gpsd.devices.dev0.tdop", '', '');
$pmda->add_metric(pmda_pmid(1,13), PM_TYPE_FLOAT, PM_INDOM_NULL, PM_SEM_INSTANT,
                  pmda_units(0,0,0,0,0,0),
                  "gpsd.devices.dev0.hdop", '', '');
$pmda->add_metric(pmda_pmid(1,14), PM_TYPE_FLOAT, PM_INDOM_NULL, PM_SEM_INSTANT,
                  pmda_units(0,0,0,0,0,0),
                  "gpsd.devices.dev0.gdop", '', '');
$pmda->add_metric(pmda_pmid(1,15), PM_TYPE_FLOAT, PM_INDOM_NULL, PM_SEM_INSTANT,
                  pmda_units(0,0,0,0,0,0),
                  "gpsd.devices.dev0.pdop", '', '');

$pmda->add_metric(pmda_pmid(1,16), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT,
                  pmda_units(0,0,0,0,0,0),
                  "gpsd.devices.dev0.num_satellites", '', '');

$pmda->add_metric(pmda_pmid(1,100), PM_TYPE_U32, $gpsd_sat_indom, PM_SEM_INSTANT,
                  pmda_units(0,0,0,0,0,0),
                  "gpsd.devices.dev0.satellites.el", '', '');
$pmda->add_metric(pmda_pmid(1,101), PM_TYPE_U32, $gpsd_sat_indom, PM_SEM_INSTANT,
                  pmda_units(0,0,0,0,0,0),
                  "gpsd.devices.dev0.satellites.az", '', '');
$pmda->add_metric(pmda_pmid(1,102), PM_TYPE_U32, $gpsd_sat_indom, PM_SEM_INSTANT,
                  pmda_units(0,0,0,0,0,0),
                  "gpsd.devices.dev0.satellites.ss", '', '');
$pmda->add_metric(pmda_pmid(1,103), PM_TYPE_U32, $gpsd_sat_indom, PM_SEM_INSTANT,
                  pmda_units(0,0,0,0,0,0),
                  "gpsd.devices.dev0.satellites.used", '', '');

$pmda->add_indom($gpsd_sat_indom, \@gpsd_sat_dom, '', '');
$pmda->add_pipe($gpspipe, \&gpsd_pipe_callback, 0);

$pmda->set_fetch_callback(\&gpsd_fetch_callback);
$pmda->set_user('pcp');
$pmda->run;

=pod

=head1 NAME

pmdagpsd - gpsd performance metrics domain agent (PMDA)

=head1 DESCRIPTION

B<pmdagpsd> is a Performance Metrics Domain Agent (PMDA) which exports
values from the gpsd daemon.

=head1 INSTALLATION

If you want access to the names and values for gpsd, do the following as
root:

    # cd $PCP_PMDAS_DIR/gpsd
    # ./Install

If you want to undo the installation, do the following as root:

    # cd $PCP_PMDAS_DIR/gpsd
    # ./Remove

B<pmdagpsd> is launched by pmcd(1) and should never be executed
directly.  The Install and Remove scripts notify pmcd(1) when
the agent is installed or removed.

=head1 FILES

=over

=item $PCP_PMDAS_DIR/gpsd/Install

installation script for the B<pmdagpsd> agent

=item $PCP_PMDAS_DIR/gpsd/Remove

undo installation script for the B<pmdagpsd> agent

=item $PCP_LOG_DIR/pmcd/gpsd.log

default log file for error messages from B<pmdagpsd>

=back

=head1 SEE ALSO

pmcd(1).
