#
# Copyright (c) 2009 Aconex.  All Rights Reserved.
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
use Time::Local;

my $pmda = PCP::PMDA->new('dtsrun', 102);

my ( $package, $package_starttime, $instance_refresh );
my $package_indom = 0;	# instance domain for DTS package
my @package_instances = ();
my ( $step, $step_starttime );
my $steps_indom = 1;	# instance domain for DTS package steps
my @steps_instances = ();

use vars qw ( %lastpkg_times %lastpkg_stamp %allpkgs_times %allpkgs_count
		%laststep_times %laststep_stamp %laststep_offset
		%laststep_status %laststep_message %allsteps_times
		%allsteps_count %allsteps_errors %allpkgs_delta
		%packages %steps
	    );

sub parsetime
{
    shift;
    #$pmda->log("parsetime got dtsrun timestamp: $_");
    m|(\d+)/(\d+)/(\d+) (\d+):(\d+):(\d+) (\w+)|;
    my ($mm, $dd, $year, $hr, $min, $sec, $ampm) = ($1,$2,$3,$4,$5,$6,$7);
    $hr += 12 unless ($ampm eq 'AM');
    $hr -= 1;   # range 0..23 (hours)
    $mm -= 1;   # range 0..11 (months)
    my $tm = timelocal($sec,$min,$hr,$dd,$mm,$year);
    return $tm;
}

sub dtsrun_parser
{
    ( undef, $_ ) = @_;

    s/\r//g;	# cull Windows line endings
    #$pmda->log("dtsrun_parser got line: $_");

    if (/^Step '(\S+)' (.*)/) {
	$step = "$package/$1";
	if (defined($steps{$step})) {
	    $allsteps_count{$step}++;
	} else {	# new step instance
	    $steps{$step} = 1;
	    $allsteps_times{$step} = 0;
	    $allsteps_count{$step} = 1;
	    $allsteps_errors{$step} = 0;
	    $laststep_times{$step} = 0;
	    $laststep_stamp{$step} = '';
	    $laststep_offset{$step} = 0;
	    $laststep_status{$step} = -1;
	    $laststep_message{$step} = '';
	    $instance_refresh = 1;
	}
	if ($2 eq 'succeeded') {
	    $laststep_status{$step} = 0;
	} else {
	    $laststep_status{$step} = -1;
	    $allsteps_errors{$step}++;
	}
	$laststep_message{$step} = $2;
    }
    elsif (/^Step Execution Started: (.*)/) {
	$step_starttime = parsetime($1);
	$laststep_stamp{$step} = $1;
	$laststep_offset{$step} = $step_starttime - $package_starttime;
    }
    elsif (/^Total Step Execution Time: (\S+) seconds/) {
	$laststep_times{$step} = $1;
	$allsteps_times{$step} += $1;
    }
    elsif (/^Package Name: (\w+)/) {
	$package = $1;
	if (defined($packages{$package})) {
	    $allpkgs_count{$package}++;
	} else {	# new package instance
	    $packages{$package} = 1;
	    $allpkgs_delta{$package} = 0;
	    $allpkgs_times{$package} = 0;
	    $allpkgs_count{$package} = 1;
	    $lastpkg_times{$package} = 0;
	    $lastpkg_stamp{$package} = '';
	    $instance_refresh = 1;
	}
    }
    elsif (/^Execution Started: (.*)/) {
	my $latest = parsetime($1);
	if (defined($package_starttime) && $package_starttime >= 0) {
	    $allpkgs_delta{$package} = $latest - $package_starttime;
	} else {
	    $allpkgs_delta{$package} = -1;
	}
	$package_starttime = $latest;
	$lastpkg_stamp{$package} = $1;
    }
    elsif (/^Total Execution Time: (\S+) seconds/) {
	$lastpkg_times{$package} = $1;
	$allpkgs_times{$package} += $1;
    }
    elsif (/^\*\*\*/) {
	# end section, cleanup global state
	undef $step;
	undef $package;
    }
}

sub dtsrun_instance
{
    my ( $pkgcount, $stepcount );

    #$pmda->log("dtsrun_instance");
    return unless (defined($instance_refresh) && $instance_refresh);

    @package_instances = ();
    @steps_instances = ();

    $pkgcount = $stepcount = 0;
    foreach my $pkgname (keys(%packages)) {
	push @package_instances, $pkgcount++, $pkgname;
	foreach my $stepname (sort keys(%steps)) {
	    push @steps_instances, $stepcount++, $stepname;
	}
    }

    #$pmda->log("dtsrun_instance: $pkgcount packages, $stepcount steps");
    $pmda->replace_indom($package_indom, \@package_instances);
    $pmda->replace_indom($steps_indom, \@steps_instances);
    $instance_refresh = 0;
}

sub dtsrun_fetch_callback
{
    my ($cluster, $item, $inst) = @_;
    my $instance;

    #$pmda->log("dtsrun_fetch_callback for PMID: $cluster.$item ($inst)");
    if ($inst == PM_IN_NULL)	{ return (PM_ERR_INST, 0); }

    if ($cluster == 0 || $cluster == 1 || $cluster == 4) {
	$instance = pmda_inst_name($package_indom, $inst);
    } else {
	$instance = pmda_inst_name($steps_indom, $inst);
    }
    if (!defined($instance))	{ return (PM_ERR_INST, 0); }
    #$pmda->log("dtsrun_fetch_callback using instance $instance");

    if ($cluster == 0) {
	# dtsrun.package.last.time
	if ($item == 0)		{ return ($lastpkg_times{$instance}, 1); }
	# dtsrun.package.last.timestamp
	elsif ($item == 1)	{ return ($lastpkg_stamp{$instance}, 1); }
    }
    elsif ($cluster == 1) {
	# dtsrun.package.total.time
	if ($item == 0)		{ return ($allpkgs_times{$instance}, 1); }
	# dtsrun.package.total.count
	elsif ($item == 1)	{ return ($allpkgs_count{$instance}, 1); }
    }
    elsif ($cluster == 2) {
	# dtsrun.package.steps.last.time
	if ($item == 0)		{ return ($laststep_times{$instance}, 1); }
	# dtsrun.package.steps.last.timestamp
	if ($item == 1)		{ return ($laststep_stamp{$instance}, 1); }
	# dtsrun.package.steps.last.offset
	if ($item == 2)		{ return ($laststep_offset{$instance}, 1); }
	# dtsrun.package.steps.last.status
	if ($item == 3)		{ return ($laststep_status{$instance}, 1); }
	# dtsrun.package.steps.last.message
	if ($item == 4)		{ return ($laststep_message{$instance}, 1); }
    }
    elsif ($cluster == 3) {
	# dtsrun.package.steps.total.time
	if ($item == 0)		{ return ($allsteps_times{$instance}, 1); }
	# dtsrun.package.steps.total.count
	if ($item == 1)		{ return ($allsteps_count{$instance}, 1); }
	# dtsrun.package.steps.total.errors
	if ($item == 2)		{ return ($allsteps_errors{$instance}, 1); }
    }
    elsif ($cluster == 4) {
	# dtsrun.package.interval
	if ($item == 0)		{ return ($allpkgs_delta{$instance}, 1); }
    }
    return (PM_ERR_PMID, 0);
}

sub dtsrun_setup_metrics
{
    $pmda->add_metric(pmda_pmid(0,0), PM_TYPE_FLOAT, $package_indom,
	PM_SEM_INSTANT, pmda_units(0,1,0,0,PM_TIME_SEC,0),
	'dtsrun.package.lastrun.time', '',
	'Time taken for DTS package to complete the last time it was run');
    $pmda->add_metric(pmda_pmid(0,1), PM_TYPE_STRING, $package_indom,
	PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
	'dtsrun.package.lastrun.timestamp', '',
	'Time stamp (string) of the last package run for each DTS package');

    $pmda->add_metric(pmda_pmid(1,0), PM_TYPE_FLOAT, $package_indom,
	PM_SEM_COUNTER, pmda_units(0,1,0,0,PM_TIME_SEC,0),
	'dtsrun.package.total.time', '',
	'Cumulative time taken to date for all DTS package runs.');
    $pmda->add_metric(pmda_pmid(1,1), PM_TYPE_U32, $package_indom,
	PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
	'dtsrun.package.total.count', '',
	'Cumulative count of all DTS package runs to date.');

    $pmda->add_metric(pmda_pmid(2,0), PM_TYPE_U32, $steps_indom,
	PM_SEM_INSTANT, pmda_units(0,1,0,0,PM_TIME_SEC,0),
	'dtsrun.package.steps.last.time', '',
	'Time taken to complete the last iteration of each step.');
    $pmda->add_metric(pmda_pmid(2,1), PM_TYPE_STRING, $steps_indom,
	PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
	'dtsrun.package.steps.last.timestamp', '',
	'Time of completion of the last iteration of each step.');
    $pmda->add_metric(pmda_pmid(2,2), PM_TYPE_U32, $steps_indom,
	PM_SEM_INSTANT, pmda_units(0,1,0,0,PM_TIME_SEC,0),
	'dtsrun.package.steps.last.offset', '',
	'Offset from package start time for this step, in seconds.');
    $pmda->add_metric(pmda_pmid(2,3), PM_TYPE_32, $steps_indom,
	PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
	'dtsrun.package.steps.last.status', '',
	'Indicator of success (0) or failure of last run for each step.');
    $pmda->add_metric(pmda_pmid(2,4), PM_TYPE_STRING, $steps_indom,
	PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
	'dtsrun.package.steps.last.message', '',
	'Message string used to determine status of last run for each step.');

    $pmda->add_metric(pmda_pmid(3,0), PM_TYPE_FLOAT, $steps_indom,
	PM_SEM_COUNTER, pmda_units(0,1,0,0,PM_TIME_SEC,0),
	'dtsrun.package.steps.total.time', '',
	'Cumulative count of time taken for runs of each DTS package step.');
    $pmda->add_metric(pmda_pmid(3,1), PM_TYPE_U32, $steps_indom,
	PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
	'dtsrun.package.steps.total.count', '',
	'Cumulative count of runs of each DTS package step.');
    $pmda->add_metric(pmda_pmid(3,2), PM_TYPE_U32, $steps_indom,
	PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
	'dtsrun.package.steps.total.errors', '',
	'Cumulative count of errors observed for each DTS package step.');

    $pmda->add_metric(pmda_pmid(4,0), PM_TYPE_32, $package_indom,
	PM_SEM_INSTANT, pmda_units(0,1,0,0,PM_TIME_SEC,0),
	'dtsrun.package.interval', '',
	'Time between consecutive observations of each DTS package run.  Unknown is -1.');
}

sub dtsrun_setup_instances
{
    $package_indom = $pmda->add_indom($package_indom, [], '', '');
    $steps_indom = $pmda->add_indom($steps_indom, [], '', '');
}

my $logfile = '';
if (!defined($ENV{PCP_PERL_PMNS}) && !defined($ENV{PCP_PERL_DOMAIN})) {
    die "No dtsrun statistics file specified\n" unless (defined($ARGV[0]));
    $logfile = $ARGV[0];
    die "Cannot find a valid dtsrun statistics file\n" unless -f $logfile;
}
$pmda->log("Using logfile: $logfile");

dtsrun_setup_metrics;
dtsrun_setup_instances;

$pmda->add_tail($logfile, \&dtsrun_parser, 0);
$pmda->set_fetch(\&dtsrun_instance);
$pmda->set_instance(\&dtsrun_instance);
$pmda->set_fetch_callback(\&dtsrun_fetch_callback);
$pmda->set_user('pcp');
$pmda->run;

=pod

=head1 NAME

pmdadtsrun - Data Transformation Services process (dtsrun) log file PMDA

=head1 DESCRIPTION

B<pmdadtsrun> is a Performance Metrics Domain Agent (PMDA) which
exports metric values from the DTS run executable, which is a data
transformation server, used with SQL Server.
Further details on DTS can be found at http://www.dtssql.com/.

=head1 INSTALLATION

If you want access to the names and values for the dtsrun performance
metrics, do the following as root:

	# cd $PCP_PMDAS_DIR/dtsrun
	# ./Install

If you want to undo the installation, do the following as root:

	# cd $PCP_PMDAS_DIR/dtsrun
	# ./Remove

B<pmdadtsrun> is launched by pmcd(1) and should never be executed
directly.  The Install and Remove scripts notify pmcd(1) when
the agent is installed or removed.

=head1 FILES

=over

=item /var/log/dtsrun.log

log file showing status and elapsed times exported from dtsrun

=item $PCP_PMDAS_DIR/dtsrun/Install

installation script for the B<pmdadtsrun> agent

=item $PCP_PMDAS_DIR/dtsrun/Remove

undo installation script for the B<pmdadtsrun> agent

=item $PCP_LOG_DIR/pmcd/dtsrun.log

default log file for error messages from B<pmdadtsrun>

=back

=head1 SEE ALSO

pmcd(1).
