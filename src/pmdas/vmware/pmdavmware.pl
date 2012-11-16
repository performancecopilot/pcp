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
use PCP::PMDA;
use VMware::VIRuntime;

use vars qw( $host $server $username $password );
use vars qw( $pmda $entity $view $interval $metric_info %metric_values );

# Configuration files for setting VMware host connection parameters
for my $file (	pmda_config('PCP_PMDAS_DIR') . '/vmware/vmware.conf',
		'./vmware.conf' ) {
	eval `cat $file` unless ! -f $file;
}
die 'VMware host not setup, stopped' unless defined($host);
die 'VMware server not setup, stopped' unless defined($server);
die 'VMware username not setup, stopped' unless defined($username);
die 'VMware password not setup, stopped' unless defined($password);

Opts::set_option('server' => $server);
Opts::set_option('username' => $username);
Opts::set_option('password' => $password);
Opts::parse();

sub vmware_connect
{
    $Util::script_version = '1.0';
    Util::connect();
    $entity = Vim::find_entity_view(view_type => 'HostSystem',
				    filter => {'name' => $host});
    return unless defined($entity);

    $view = Vim::get_view(mo_ref => Vim::get_service_content()->perfManager);
    my $summary = $view->QueryPerfProviderSummary(entity => $entity);
    $interval = $summary->refreshRate;	# absolute, in seconds
    $interval = 20 unless defined($interval);	# the vmware default
}

sub vmware_timer_callback
{
    return unless !defined($entity);
    $pmda->log("Connecting to VMware services on $host ($server)");
    vmware_connect();
    return unless defined($entity);
    $pmda->log("Successfully connected.");
}

sub vmware_disconnect
{
    Util::disconnect();
}

sub vmware_metric_ids
{
    my @filtered_list;
    my $counter_info = $view->perfCounter;
    my $available_id = $view->QueryAvailablePerfMetric(entity => $entity);

    foreach (@$counter_info) {
	my $key = $_->key;
	$metric_info->{$key} = $_;
    }

    foreach (@$available_id) {
	my $id = $_->counterId;
	if (defined $metric_info->{$id}) {
	    my $metric = PerfMetricId->new(counterId => $id, instance => '');
	    push @filtered_list, $metric;
	}
    }
    return \@filtered_list;
}

sub vmware_fetch
{
    return unless defined($entity);

    my @metric_ids = vmware_metric_ids();
    my $query_spec = PerfQuerySpec->new(entity => $entity,
					metricId => @metric_ids,
					'format' => 'normal',
					intervalId => $interval,
					maxSample => 1);
    my $values;
    eval {
	$values = $view->QueryPerf(querySpec => $query_spec);
    };
    if ($@) {
	if (ref($@) eq 'SoapFault') {
	    if (ref($@->detail) eq 'InvalidArgument') {
		$pmda->log('QueryPerf parameters are not correct');
	    } else {
		$pmda->log('QueryPerf failed - Soap protocol error');
	    }
	} else {
	    $pmda->log('QueryPerf failed - cause unknown - good luck!');
	}
    }
    elsif (!@$values) {
	$pmda->log('VMware performance data unavailable');
    }

    foreach (@$values) {
	my $value_array = $_->value;
	foreach (@$value_array) {
	    my $counter_id = $_->id->counterId;
	    my $counter = $metric_info->{$counter_id};
	    my $key = $counter->nameInfo->label;
	    $metric_values{$key} = $_;
	    # $pmda->log("Value found: $key maps to $counter_id\n");
	}
    }
}

sub vmware_fetch_callback
{
    my ($cluster, $item, $inst) = @_;

    if ($cluster == 0) {
	if ($item == 0)	{ return ($host, 1); }
	if ($item == 1)	{ return ($interval, 1); }
    }

    return (PM_ERR_NOTCONN, 0) unless defined($entity);

    my $key = pmda_pmid_text($cluster, $item);
    return (PM_ERR_PMID, 0) unless defined($key);

    # $pmda->log("vmware_fetch_callback $key $cluster:$item ($inst)\n");

    my $pmvalue = $metric_values{$key};
    return (PM_ERR_AGAIN, 0) unless defined($pmvalue);
    my $counters = ($pmvalue->value)[0];

    if ($cluster == 0 && $item == 3) {
	my $uptime = pmda_uptime($counters->[0]);
	return ($uptime, 1);
    }
    return ($counters->[0], 1);
}


$pmda = PCP::PMDA->new('vmware', 90);

$pmda->add_metric(pmda_pmid(0,0), PM_TYPE_STRING, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'vmware.sys.host',
		  'Name of monitored VMware host', '');
$pmda->add_metric(pmda_pmid(0,1), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,1,0,0,PM_TIME_SEC,0),
		  'vmware.sys.interval',
		  'Interval at which VMware internally refreshes values', '');
$pmda->add_metric(pmda_pmid(0,2), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,1,0,0,PM_TIME_SEC,0),
		  'vmware.sys.uptime', 'Uptime',
		  'Total time elapsed since last startup');
$pmda->add_metric(pmda_pmid(0,3), PM_TYPE_STRING, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'vmware.sys.uptime_s', 'Uptime',
		  'Total time elapsed since last startup');

$pmda->add_metric(pmda_pmid(1,0), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT,
		  pmda_units(1,-1,0,PM_SPACE_KBYTE,PM_TIME_SEC,0),
		  'vmware.net.usage', 'Network Usage (Average/Rate)',
		  'Aggregated network performance statistics.');

$pmda->add_metric(pmda_pmid(2,0), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT,
		  pmda_units(1,-1,0,PM_SPACE_KBYTE,PM_TIME_SEC,0),
		  'vmware.disk.usage', 'Disk Usage (Average/Rate)',
		  'Aggregated storage performance statistics.');

$pmda->add_metric(pmda_pmid(3,0), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'vmware.mem.usage', 'Memory Usage (Average/Absolute)',
		  'Memory usage as percentage of total configured or available memory');
$pmda->add_metric(pmda_pmid(3,1), PM_TYPE_U64, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(1,0,0,PM_SPACE_MBYTE,0,0),
		  'vmware.mem.reserved_capacity', 'Memory Reserved Capacity',
		  'Amount of memory reserved by the virtual machines');
$pmda->add_metric(pmda_pmid(3,2), PM_TYPE_U64, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(1,0,0,PM_SPACE_KBYTE,0,0),
		  'vmware.mem.swap_in', 'Memory Swap In (Average/Absolute)',
		  'Amount of memory that is swapped in');
$pmda->add_metric(pmda_pmid(3,3), PM_TYPE_U64, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(1,0,0,PM_SPACE_KBYTE,0,0),
		  'vmware.mem.balloon', 'Memory Balloon (Average/Absolute)',
		  'Amount of memory used by memory control');
$pmda->add_metric(pmda_pmid(3,4), PM_TYPE_U64, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(1,0,0,PM_SPACE_KBYTE,0,0),
		  'vmware.mem.swap_out', 'Memory Swap Out (Average/Absolute)',
		  'Amount of memory that is swapped out');
$pmda->add_metric(pmda_pmid(3,5), PM_TYPE_U64, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(1,0,0,PM_SPACE_KBYTE,0,0),
		  'vmware.mem.unreserved',
		  'Memory Unreserved (Average/Absolute)',
		  'Amount of memory that is unreserved');
$pmda->add_metric(pmda_pmid(3,6), PM_TYPE_U64, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(1,0,0,PM_SPACE_KBYTE,0,0),
		  'vmware.mem.heap', 'Memory Heap (Average/Absolute)',
		  'Amount of memory allocated for heap');
$pmda->add_metric(pmda_pmid(3,7), PM_TYPE_U64, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(1,0,0,PM_SPACE_KBYTE,0,0),
		  'vmware.mem.overhead', 'Memory Overhead (Average/Absolute)',
		  'Amount of additional host memory allocated to the virtual machine');
$pmda->add_metric(pmda_pmid(3,8), PM_TYPE_U64, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(1,0,0,PM_SPACE_KBYTE,0,0),
		  'vmware.mem.zeroed', 'Memory Zero (Average/Absolute)',
		  'Amount of memory that is zeroed out');
$pmda->add_metric(pmda_pmid(3,9), PM_TYPE_U64, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(1,0,0,PM_SPACE_KBYTE,0,0),
		  'vmware.mem.active', 'Memory Active (Average/Absolute)',
		  'Amount of memory that is actively used');
$pmda->add_metric(pmda_pmid(3,10), PM_TYPE_U64, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(1,0,0,PM_SPACE_KBYTE,0,0),
		  'vmware.mem.swap_used', 'Memory Swap Used (Average/Absolute)',
		  'Amount of memory that is used by swap');
$pmda->add_metric(pmda_pmid(3,11), PM_TYPE_U64, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(1,0,0,PM_SPACE_KBYTE,0,0),
		  'vmware.mem.shared', 'Memory Shared (Average/Absolute)',
		  'Amount of memory that is shared');
$pmda->add_metric(pmda_pmid(3,12), PM_TYPE_U64, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(1,0,0,PM_SPACE_KBYTE,0,0),
		  'vmware.mem.granted', 'Memory Granted (Average/Absolute)',
		  'Amount of memory granted.');
$pmda->add_metric(pmda_pmid(3,13), PM_TYPE_U64, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(1,0,0,PM_SPACE_KBYTE,0,0),
		  'vmware.mem.consumed', 'Memory Consumed (Average/Absolute)',
		  'Amount of host memory consumed by the virtual machine for guest memory');
$pmda->add_metric(pmda_pmid(3,14), PM_TYPE_U64, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(1,0,0,PM_SPACE_KBYTE,0,0),
		  'vmware.mem.heap_free', 'Memory Heap Free (Average/Absolute)',
		  'Free space in memory heap');
$pmda->add_metric(pmda_pmid(3,15), PM_TYPE_U64, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(1,0,0,PM_SPACE_KBYTE,0,0),
		  'vmware.mem.state', 'Memory State',
		  'Memory State');
$pmda->add_metric(pmda_pmid(3,16), PM_TYPE_U64, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(1,0,0,PM_SPACE_KBYTE,0,0),
		  'vmware.mem.shared_common',
		  'Memory Shared Common (Average/Absolute)',
		  'Amount of memory that is shared by common');
$pmda->add_metric(pmda_pmid(3,17), PM_TYPE_U64, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(1,0,0,PM_SPACE_KBYTE,0,0),
		  'vmware.mem.vmkernel', 'Memory Used by vmkernel',
		  'Amount of memory used by the vmkernel');

$pmda->add_metric(pmda_pmid(4,0), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,-1,1,0,PM_TIME_SEC,PM_COUNT_ONE),
		  'vmware.cpu.usage', 'CPU Usage (Average/Rate)',
		  'CPU usage as a percentage over the collected interval');
$pmda->add_metric(pmda_pmid(4,1), PM_TYPE_U64, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,-1,1,0,PM_TIME_SEC,PM_COUNT_ONE),
		  'vmware.cpu.usage_mhz', 'CPU Usage in MHz (Average/Rate)',
		  'CPU usage in MHz over the collected interval.');
$pmda->add_metric(pmda_pmid(4,2), PM_TYPE_U64, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,-1,1,0,PM_TIME_SEC,PM_COUNT_ONE),
		  'vmware.cpu.reserved_capacity', 'CPU Reserved Capacity',
		  'Total CPU capacity reserved by the virtual machines');

$pmda->add_metric(pmda_pmid(5,0), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'vmware.rescpu.throttled.one_min_average',
		  'CPU Throttled (1 min. average)',
		  'Amount of CPU resources over the limit that were refused, average over 1 minute');
$pmda->add_metric(pmda_pmid(5,1), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'vmware.rescpu.throttled.five_min_average',
		  'CPU Throttled (5 min. average)',
		  'Amount of CPU resources over the limit that were refused, average over 5 minutes');
$pmda->add_metric(pmda_pmid(5,2), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'vmware.rescpu.throttled.fifteen_min_average',
		  'CPU Throttled (15 min. average)',
		  "Amount of CPU resources over the limit that were refused,\n"
		  . "average over 15 minutes\n");
$pmda->add_metric(pmda_pmid(5,3), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'vmware.rescpu.running.one_min_peak',
		  'CPU running peak over 1 minute',
		  '');
$pmda->add_metric(pmda_pmid(5,4), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'vmware.rescpu.running.five_min_peak',
		  'CPU running peak over 5 minutes',
		  '');
$pmda->add_metric(pmda_pmid(5,5), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'vmware.rescpu.running.fifteen_min_peak',
		  'CPU running peak over 15 minutes',
		  '');
$pmda->add_metric(pmda_pmid(5,6), PM_TYPE_U64, PM_INDOM_NULL,
		  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  'vmware.rescpu.group_sample_count',
		  'Group CPU Sample Count', 'Group CPU sample count');
$pmda->add_metric(pmda_pmid(5,7), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'vmware.rescpu.active.one_min_peak',
		  'CPU Active (1 min. peak)',
		  'CPU active peak over 1 minute');
$pmda->add_metric(pmda_pmid(5,8), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'vmware.rescpu.active.five_min_peak',
		  'CPU Active (5 min. peak)',
		  'CPU active peak over 5 minutes');
$pmda->add_metric(pmda_pmid(5,9), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'vmware.rescpu.active.fifteen_min_peak',
		  'CPU Active (15 min. peak)',
		  'CPU active peak over 15 minutes');
$pmda->add_metric(pmda_pmid(5,10), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'vmware.rescpu.running.one_min_average',
		  'CPU Running (1 min. average)',
		  'CPU running average over 1 minute');
$pmda->add_metric(pmda_pmid(5,11), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'vmware.rescpu.running.five_min_average',
		  'CPU Running (5 min. average)',
		  'CPU running average over 5 minutes');
$pmda->add_metric(pmda_pmid(5,12), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'vmware.rescpu.running.fifteen_min_average',
		  'CPU Running (15 min. average)',
		  'CPU running average over 15 minutes');
$pmda->add_metric(pmda_pmid(5,13), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'vmware.rescpu.running.one_min_peak',
		  'CPU Running (1 min. peak)',
		  'CPU running peak over 1 minute');
$pmda->add_metric(pmda_pmid(5,14), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'vmware.rescpu.running.five_min_peak',
		  'CPU Running (5 min. peak)',
		  'CPU running peak over 5 minutes');
$pmda->add_metric(pmda_pmid(5,15), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'vmware.rescpu.running.fifteen_min_peak',
		  'CPU Running (15 min. peak)',
		  'CPU running peak over 15 minutes');
$pmda->add_metric(pmda_pmid(5,16), PM_TYPE_U64, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,1,0,0,PM_TIME_MSEC,0),
		  'vmware.rescpu.group_sample_period',
		  'Group CPU Sample Period', 'Group CPU sample period');
$pmda->add_metric(pmda_pmid(5,17), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'vmware.rescpu.active.one_min_average',
		  'CPU Active (1 min. average)',
		  'CPU active average over 1 minute');
$pmda->add_metric(pmda_pmid(5,18), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'vmware.rescpu.active.five_min_average',
		  'CPU Active (5 min. average)',
		  'CPU active average over 5 minutes');
$pmda->add_metric(pmda_pmid(5,19), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'vmware.rescpu.active.fifteen_min_average',
		  'CPU Active (15 min. average)',
		  'CPU active average over 15 minutes');

$pmda->set_fetch(\&vmware_fetch);
$pmda->set_fetch_callback(\&vmware_fetch_callback);
$pmda->add_timer(5, \&vmware_timer_callback, 0);
$pmda->set_user('pcp');
$pmda->run;
vmware_disconnect();

=pod

=head1 NAME

pmdavmware - VMware performance metrics domain agent (PMDA)

=head1 DESCRIPTION

B<pmdavmware> is a Performance Metrics Domain Agent (PMDA) which exports
metric values from a (possibly remote) VMware virtualisation host.

This implementation uses the VMare Perl API (refer to the online
docs at http://www.vmware.com/support/developer/viperltoolkit).
VIPerl is a prerequisite for this PMDA, it needs to be installed
and configured before attempting to use this agent.  It is highly
recommended that you test your VIPerl installation using the
demo programs that are shipped with VIPerl, before attempting to
use this PMDA.

=head1 INSTALLATION

In order to access performance data using the VIPerl API, it is
necessary to be able to login to the metrics source.  Hence, a
valid VMware server name, user name and pass word are needed by
the PMDA.  These can be passed in on the command line (via the
pmcd.conf file) or via a vmware.conf file in the PMDA directory.

	# cd $PCP_PMDAS_DIR/vmware
	# [ edit vmware.conf ]

This file should contain three lines, such as:

	$server = 'vm.server.net';
	$username = 'XXXX';
	$password  = 'YYYY';

Once this is setup, you can access the names and values for the
vmware performance metrics by doing the following as root:

	# cd $PCP_PMDAS_DIR/vmware
	# ./Install

If you want to undo the installation, do the following as root:

	# cd $PCP_PMDAS_DIR/vmware
	# ./Remove

B<pmdavmware> is launched by pmcd(1) and should never be executed
directly.  The Install and Remove scripts notify pmcd(1) when
the agent is installed or removed.

=head1 FILES

=over

=item $PCP_PMDAS_DIR/vmware/vmware.conf

configuration file for the B<pmdavmware> agent 

=item $PCP_PMDAS_DIR/vmware/Install

installation script for the B<pmdavmware> agent

=item $PCP_PMDAS_DIR/vmware/Remove

undo installation script for the B<pmdavmware> agent

=item $PCP_LOG_DIR/pmcd/vmware.log

default log file for error messages from B<pmdavmware>

=back

=head1 SEE ALSO

pmcd(1).
