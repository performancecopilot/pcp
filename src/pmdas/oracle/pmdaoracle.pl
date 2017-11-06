#
# Copyright (c) 2016 Intel, Inc.  All Rights Reserved.
# Copyright (c) 2012,2016 Red Hat.
# Copyright (c) 2009,2012 Aconex.  All Rights Reserved.
# Copyright (c) 1998 Silicon Graphics, Inc.  All Rights Reserved.
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
use DBI;

# global connection/query parameters which can be overridden by oracle.conf
my $os_user = 'oracle';
my $username = 'SYSTEM';
my $password = 'manager';
my $host = 'localhost';
my $port = '1521';
my @sids = ( 'master' );
my $disable_filestat = 0;	# on/off switch for v$filestat queries
my $disable_object_cache = 0;	# on/off switch for v$db_object_cache queries

# Configuration files for overriding the above settings
for my $file (	'/etc/pcpdbi.conf',	# system defaults (lowest priority)
		pmda_config('PCP_PMDAS_DIR') . '/oracle/oracle.conf',
		pmda_config('PCP_VAR_DIR') . '/config/oracle/oracle.conf',
		'./oracle.conf' ) {	# current directory (high priority)
    eval `cat $file` unless ! -f $file;
}

use vars qw(
	$pmda %sids_by_name %sysstat_map

	%control_instances %sysstat_instances %latch_instances
	%filestat_instances %rollstat_instances %reqdist_instances
	%rowcache_instances %session_instances %object_cache_instances
	%system_event_instances %librarycache_instances %asm_instances
	%waitstat_instances %version_instances %license_instances
	%backup_instances %bufferpool_instances %parameter_instances

	%filestat_valuemap %system_event_valuemap %latch_valuemap
	%rollstat_valuemap %backup_valuemap %rowcache_valuemap
	%reqdist_valuemap %bufferpool_valuemap %asm_valuemap
);

my $latch_indom		= 0;
my $filestat_indom	= 1;
my $rollstat_indom	= 2;
my $reqdist_indom	= 3;
my $rowcache_indom	= 4;
my $session_indom	= 5;
my $object_cache_indom	= 6;
my $system_event_indom	= 7;
my $librarycache_indom	= 8;
my $waitstat_indom	= 9;
my $control_indom	= 10;	
my $license_indom	= 11;
my $version_indom	= 12;
my $sysstat_indom	= 13;
my $bufferpool_indom    = 14;
my $asm_indom		= 15;
my $backup_indom	= 16;
my $parameter_indom	= 17;

my @novalues = ();
my $object_cache_insts_set = 0;
my %object_cache_instances = (
	'INDEX'		=> \@novalues,	'TABLE'		=> \@novalues,
	'CLUSTER'	=> \@novalues,	'VIEW'		=> \@novalues,
	'SET'		=> \@novalues,	'SYNONYM'	=> \@novalues,
	'SEQUENCE'	=> \@novalues,	'PROCEDURE'	=> \@novalues,
	'FUNCTION'	=> \@novalues,	'PACKAGE'	=> \@novalues,
	'PACKAGE_BODY'	=> \@novalues,	'TRIGGER'	=> \@novalues,
	'CLASS'		=> \@novalues,	'OBJECT'	=> \@novalues,
	'USER'		=> \@novalues,	'DBLINK'	=> \@novalues,
	'NON-EXISTENT'	=> \@novalues,	'NOT_LOADED'	=> \@novalues,
	'CURSOR'	=> \@novalues,	'OTHER'		=> \@novalues,
);

my $sysstat_cluster	= 0;
my $license_cluster	= 1;
my $latch_cluster	= 2;
my $filestat_cluster	= 3;
my $rollstat_cluster	= 4;
my $reqdist_cluster	= 5;
my $backup_cluster	= 6;
my $rowcache_cluster	= 7;
#my $sesstat_cluster	= 8;
my $object_cache_cluster= 9;
my $system_event_cluster= 10;
my $version_cluster	= 11;
my $librarycache_cluster= 12;
my $waitstat_cluster	= 13;
my $bufferpool_cluster	= 14;
my $asm_cluster		= 15;
my $control_cluster	= 16;
my $parameter_cluster	= 17;

my %sysstat_table = (
	name		=> 'sysstat',
	indom		=> $sysstat_indom,
	cluster		=> $sysstat_cluster,
	setup_callback	=> \&setup_sysstat,
	insts_callback	=> \&sysstat_insts,
	values_callback	=> \&sysstat_values,
	values_query	=>
		'select statistic#, name, value from v$sysstat',
);

my %license_table = (
	name		=> 'license',
	indom		=> $license_indom,
	cluster		=> $license_cluster,
	setup_callback	=> \&setup_license,
	insts_callback	=> \&license_insts,
	values_callback	=> \&license_values,
	values_query	=>
		'select sessions_max, sessions_current, sessions_warning,' .
		'       sessions_highwater, users_max' .
		' from v$license',
);

my %latch_table = (
	name		=> 'latch',
	indom		=> $latch_indom,
	cluster		=> $latch_cluster,
	valuemap	=> \%latch_valuemap,
	setup_callback	=> \&setup_latch,
	insts_callback	=> \&latch_insts,
	values_callback	=> \&latch_values,
	insts_query	=>
		'select latch#, name from v$latch',
	values_query	=>
		'select latch#, gets, misses, sleeps, immediate_gets, ' .
		'       immediate_misses, waiters_woken,' .
		'       waits_holding_latch, spin_gets' .
		' from v$latch',
);

my %filestat_table = (
	name		=> 'filestat',
	indom		=> $filestat_indom,
	cluster		=> $filestat_cluster,
	valuemap	=> \%filestat_valuemap,
	setup_callback	=> \&setup_filestat,
	insts_callback	=> \&filestat_insts,
	values_callback	=> \&filestat_values,
	insts_query	=>
		'select file#, name from v$datafile',
	values_query	=>
		'select file#, phyrds, phywrts, phyblkrd,' . 
		'        phyblkwrt, readtim, writetim' .
		' from v$filestat',
);

my %rollstat_table = (
	name		=> 'rollstat',
	indom		=> $rollstat_indom,
	cluster		=> $rollstat_cluster,
	valuemap	=> \%rollstat_valuemap,
	setup_callback	=> \&setup_rollstat,
	insts_callback	=> \&rollstat_insts,
	values_callback	=> \&rollstat_values,
	insts_query	=>
		'select usn, name from v$rollname',
	values_query	=>
		'select usn, rssize, writes, xacts, gets, waits, hwmsize,' .
		'        shrinks, wraps, extends, aveshrink, aveactive' .
		' from v$rollstat',
);

my %reqdist_table = (
	name		=> 'reqdist',
	indom		=> $reqdist_indom,
	cluster		=> $reqdist_cluster,
	valuemap	=> \%reqdist_valuemap,
	setup_callback	=> \&setup_reqdist,
	insts_callback	=> \&reqdist_insts,
	values_callback	=> \&reqdist_values,
	insts_query	=>
		'select bucket from v$reqdist',
	values_query	=>
		'select bucket, count from v$reqdist',
);

my %backup_table = (
	name		=> 'backup',
	indom		=> $backup_indom,
	cluster		=> $backup_cluster,
	valuemap	=> \%backup_valuemap,
	setup_callback	=> \&setup_backup,
	insts_callback	=> \&backup_insts,
	values_callback	=> \&backup_values,
	insts_query	=>
		'select file#, name from v$datafile',
	values_query	=>
		'select file#, status from v$backup',
);

my %rowcache_table = (
	name		=> 'rowcache',
	indom		=> $rowcache_indom,
	cluster		=> $rowcache_cluster,
	valuemap	=> \%rowcache_valuemap,
	setup_callback	=> \&setup_rowcache,
	insts_callback	=> \&rowcache_insts,
	values_callback	=> \&rowcache_values,
	insts_query	=>
		'select cache#, subordinate#, parameter from v$rowcache',
	values_query	=>
		'select cache#, subordinate#,' .
		'        count, gets, getmisses, scans, scanmisses' .
		' from v$rowcache',
);

#my %sesstat_table = (
#	name		=> 'sesstat',
#	indom		=> $session_indom,
#	cluster		=> $session_cluster,
#	setup_callback	=> \&setup_sesstat,
#	insts_callback	=> \&sesstat_insts,
#	values_callback	=> \&sesstat_values,
#	values_query	=>
#		'select sid, statistic#, value from v$sesstat',
#);

my %object_cache_table = (
	name		=> 'object_cache',
	indom		=> $object_cache_indom,
	cluster		=> $object_cache_cluster,
	setup_callback	=> \&setup_object_cache,
	insts_callback	=> \&object_cache_insts,
	values_callback	=> \&object_cache_values,
	values_query	=>
		'select type, sharable_mem, loads, locks, pins' .
		 ' from v$db_object_cache',
);

my %system_event_table = (
	name		=> 'system_event',
	indom		=> $system_event_indom,
	cluster		=> $system_event_cluster,
	valuemap	=> \%system_event_valuemap,
	setup_callback	=> \&setup_system_event,
	insts_callback	=> \&system_event_insts,
	values_callback	=> \&system_event_values,
	insts_query	=>
		'select event#, v$event_name.event_id, name' .
		' from v$event_name' .
		' join v$system_event on' .
		' v$event_name.event_id = v$system_event.event_id',
	values_query	=>
		'select event_id, total_waits, total_timeouts,' .
		'       time_waited, average_wait' .
		' from v$system_event',
);

my %version_table = (
	name		=> 'version',
	indom		=> $version_indom,
	cluster		=> $version_cluster,
	setup_callback	=> \&setup_version,
	insts_callback	=> \&version_insts,
	values_callback	=> \&version_values,
	values_query	=>
		'select distinct banner' .
		' from v$version where banner like \'Oracle%\'',
);

my %librarycache_table = (
	name		=> 'librarycache',
	indom		=> $librarycache_indom,
	cluster		=> $librarycache_cluster,
	setup_callback	=> \&setup_librarycache,
	insts_callback	=> \&librarycache_insts,
	values_callback	=> \&librarycache_values,
	insts_query	=>
		'select namespace from v$librarycache',
	values_query	=>
		'select namespace, gets, gethits, gethitratio, pins,' .
		'       pinhits, pinhitratio, reloads, invalidations' .
		' from v$librarycache',
);

my %waitstat_table = (
	name		=> 'waitstat',
	indom		=> $waitstat_indom,
	cluster		=> $waitstat_cluster,
	setup_callback	=> \&setup_waitstat,
	values_callback	=> \&waitstat_values,
	values_query	=>
		'select class, count, time from v$waitstat',
);

my %bufferpool_table = (
	name		=> 'bufferpool',
	indom		=> $bufferpool_indom,
	cluster		=> $bufferpool_cluster,
	valuemap	=> \%bufferpool_valuemap,
	setup_callback	=> \&setup_bufferpool,
	insts_callback	=> \&bufferpool_insts,
	values_callback	=> \&bufferpool_values,
	insts_query	=>
		'select id, name, block_size from v$buffer_pool_statistics',
	values_query	=>
		'select id, set_msize, free_buffer_wait,' .
		'       write_complete_wait, buffer_busy_wait,' .
		'       physical_reads, physical_writes,' .
		'       100 * (1 - (physical_reads / ' .
		'             nullif((db_block_gets + consistent_gets), 0)))' .
                '       hit_ratio ' .
		' from v$buffer_pool_statistics',
);

my %asm_table = (
	name		=> 'asm',
	indom		=> $asm_indom,
	cluster		=> $asm_cluster,
	valuemap	=> \%asm_valuemap,
	setup_callback	=> \&setup_asm,
	insts_callback	=> \&asm_insts,
	values_callback	=> \&asm_values,
	insts_query	=>
		'select group_number, disk_number, name from v$asm_disk_stat',
	values_query	=>
		'select disk_number' .
		'       reads, writes, read_errs, write_errs, read_time,' .
		'       write_time, bytes_read, bytes_written' .
		' from v$asm_disk_stat',
);

my %controls = (
	name		=> 'control',
	indom		=> $control_indom,
	cluster		=> $control_cluster,
	setup_callback	=> \&setup_control,
	insts_callback	=> \&control_insts,
	values_callback	=> \&control_values,
);

my %parameter_table = (
	name		=> 'parameter',
	indom		=> $parameter_indom,
	cluster		=> $parameter_cluster,
	setup_callback	=> \&setup_parameter,
	insts_callback	=> \&parameter_insts,
	values_callback	=> \&parameter_values,
	values_query	=>
		'select name, value from v$parameter' .
		' where name = \'timed_statistics\'' .
		' or    name = \'statistics_level\'',
);

my %tables_by_name = (
	'sysstat'	=> \%sysstat_table,
	'license'	=> \%license_table,
	'latch'		=> \%latch_table,
	'filestat'	=> \%filestat_table,
	'rollstat'	=> \%rollstat_table,
	'reqdist'	=> \%reqdist_table,
	'backup'	=> \%backup_table,
	'rowcache'	=> \%rowcache_table,
#	'sesstat'	=> \%sesstat_table,
	'object_cache'	=> \%object_cache_table,
	'system_event'	=> \%system_event_table,
	'version'	=> \%version_table,
	'librarycache'	=> \%librarycache_table,
	'waitstat'	=> \%waitstat_table,
	'bufferpool'	=> \%bufferpool_table,
	'asm'		=> \%asm_table,
	'parameter'	=> \%parameter_table,
);

my %tables_by_cluster = (
	$sysstat_cluster	=> \%sysstat_table,
	$license_cluster	=> \%license_table,
	$latch_cluster		=> \%latch_table,
	$filestat_cluster	=> \%filestat_table,
	$rollstat_cluster	=> \%rollstat_table,
	$reqdist_cluster	=> \%reqdist_table,
	$backup_cluster		=> \%backup_table,
	$rowcache_cluster	=> \%rowcache_table,
#	$sesstat_cluster	=> \%sesstat_table,
	$object_cache_cluster	=> \%object_cache_table,
	$system_event_cluster	=> \%system_event_table,
	$version_cluster	=> \%version_table,
	$librarycache_cluster	=> \%librarycache_table,
	$waitstat_cluster	=> \%waitstat_table,
	$bufferpool_cluster	=> \%bufferpool_table,
	$asm_cluster		=> \%asm_table,
	$control_cluster	=> \%controls,
	$parameter_cluster	=> \%parameter_table,
);

my %tables_by_indom = (
	$latch_indom		=> \%latch_table,
	$filestat_indom		=> \%filestat_table,
	$rollstat_indom		=> \%rollstat_table,
	$reqdist_indom		=> \%reqdist_table,
	$rowcache_indom		=> \%rowcache_table,
#	$session_indom		=> \%sesstat_table,
	$object_cache_indom	=> \%object_cache_table,
	$system_event_indom	=> \%system_event_table,
	$librarycache_indom	=> \%librarycache_table,
	$waitstat_indom		=> \%waitstat_table,
	$control_indom		=> \%controls,
	$license_indom		=> \%license_table,
	$version_indom		=> \%version_table,
	$sysstat_indom		=> \%sysstat_table,
	$bufferpool_indom	=> \%bufferpool_table,
	$asm_indom		=> \%asm_table,
	$backup_indom		=> \%backup_table,
	$parameter_indom	=> \%parameter_table,
);


sub oracle_sid_connection_setup
{
    my ($sid, $dbh) = @_;

    # do not auto-connect if we were asked not to
    if ($sids_by_name{$sid}{disconnected} == 1) { return undef; }

    if (!defined($dbh)) {
	$dbh = DBI->connect("dbi:Oracle:host=$host;port=$port;sid=$sid", $username, $password);
	if (defined($dbh)) {
	    foreach my $key (keys %tables_by_name) {
		my ($query, $insts, $fetch);

		$insts = $tables_by_name{$key}{insts_query};
		$fetch = $tables_by_name{$key}{values_query};
		if (defined($insts)) {
		    $query = $dbh->prepare($insts);
		    $sids_by_name{$sid}{$key}{insts_handle} = $query
			unless (!defined($query));
		}
		if (defined($fetch)) {
		    $query = $dbh->prepare($fetch);
		    $sids_by_name{$sid}{$key}{values_handle} = $query
			unless (!defined($query));
		}
	    }
	}
    }
    return $dbh;
}

sub oracle_reconnect
{
    my $sid = shift;
    my $db = $sids_by_name{$sid}{db_handle};

    $db = oracle_sid_connection_setup($sid, $db);
    $sids_by_name{$sid}{db_handle} = $db;
    return 0;
}

sub oracle_disconnect
{
    my $sid = shift;
    my $db = $sids_by_name{$sid}{db_handle};

    if ($db) {
	$db->disconnect();
	$db = undef;
	$sids_by_name{$sid}{db_handle} = undef;
    }
    return 0;
}

sub oracle_control_setup
{
    foreach my $sid (@sids) {
	$sids_by_name{$sid}{disconnected} = 0;	# mark as "want up"
	$sids_by_name{$sid}{filestat}{disabled} = $disable_filestat;
	$sids_by_name{$sid}{object_cache}{disabled} = $disable_object_cache;
    }
}

sub oracle_connection_setup
{
    foreach my $sid (@sids) {
	oracle_reconnect($sid);
    }
}

sub oracle_fetch()
{
    # $pmda->log("oracle_fetch");
    oracle_connection_setup();
}

sub oracle_instance
{
    my ($indom) = @_;

    # $pmda->log("indom instances $indom");
    foreach my $sid (@sids) {
	my $db = $sids_by_name{$sid}{db_handle};
	my $table = $tables_by_indom{"$indom"}{name};
	my $insts = $tables_by_indom{"$indom"}{insts_callback};

	next if defined($sids_by_name{$sid}{$table}) &&
			$sids_by_name{$sid}{$table}{disabled};

	# attempt to reconnect if connection failed previously
	unless (defined($db)) {
	    $db = oracle_sid_connection_setup($sid, $db);
	    $sids_by_name{$sid}{db_handle} = $db;
	}
	if (defined($insts)) {
	    &$insts($db, $sid, $sids_by_name{$sid}{$table}{insts_handle});
	}
	if (!defined($db) || $db->err) {
	    $sids_by_name{$sid}{db_handle} = undef;
	}
    }
}

sub oracle_refresh
{
    my ($cluster) = @_;

    # $pmda->log("cluster values $cluster");
    foreach my $sid (@sids) {
	my $db = $sids_by_name{$sid}{db_handle};
	my $table = $tables_by_cluster{$cluster}{name};
	my $insts = $tables_by_cluster{$cluster}{insts_callback};
	my $refresh = $tables_by_cluster{$cluster}{values_callback};

	next if defined($sids_by_name{$sid}{$table}) &&
			$sids_by_name{$sid}{$table}{disabled};

	# attempt to reconnect if connection failed previously
	unless (defined($db)) {
	    $db = oracle_sid_connection_setup($sid, $db);
	    $sids_by_name{$sid}{db_handle} = $db;
	}
	# execute query, marking the connection bad on failure
	if (defined($insts)) {
	    &$insts($db, $sid, $sids_by_name{$sid}{$table}{insts_handle});
	}
	&$refresh($db, $sid, $sids_by_name{$sid}{$table}{values_handle});
	if (!defined($db) || $db->err) {
	    $sids_by_name{$sid}{db_handle} = undef;
	}
    }
}

sub oracle_fetch_callback
{
    my ($cluster, $item, $inst) = @_;
    my ($indom, $table, $key, $value, $valueref);
    my $metric_name = pmda_pmid_name($cluster, $item);

    # $pmda->log("fetch callback $cluster.$item $inst");
    return (PM_ERR_PMID, 0) unless defined($metric_name);
    $table = $metric_name;
    $table =~ s/^oracle\.//;
    $table =~ s/\.*$//;
    $indom = $tables_by_cluster{$cluster}{indom};

    $key = pmda_inst_lookup($indom, $inst);
    return (PM_ERR_INST, 0) unless defined($key);

    # $key can either directly or indirectly point to an arrayref which
    # contains the values (indexed on $item).  The indirect case occurs
    # when a separate query for instances is required - i.e. there is a
    # different query used for values to instances.
    #
    if (ref($key)) {
	$valueref = $key;
    } else {
	my $vhashref = $tables_by_cluster{$cluster}{valuemap};
	$valueref = $vhashref->{$key};
    }
    $value = ${ $valueref }[$item];

    return (PM_ERR_AGAIN, 0) unless defined($value);
    return ($value, 1);
}


# Refresh routines - one per table (cluster) - format database query
# result set for later use by the generic fetch callback routine.
#
sub refresh_results
{
    my ($dbh, $sid, $handle) = @_;

    return undef unless (defined($dbh) && defined($handle));
    unless (defined($handle->execute())) {
	# generate a timestamped failure message to accompany automatically
	# generated DBD error (PrintError is left as on - default setting).
	$pmda->log("Failed SQL query execution on SID $sid");
	return undef;
    }
    return $handle->fetchall_arrayref();
}

sub system_event_insts
{
    my ($dbh, $sid, $handle) = @_;

    if ((my $count = keys(%system_event_instances)) == 0) {
	my $result = refresh_results($dbh, $sid, $handle);

	if (defined($result)) {
	    for my $i (0 .. $#{$result}) {  # for each row (instance) returned
		my $event = $result->[$i][0];
		my $eventid = $result->[$i][1];
		my $eventname = $result->[$i][2];
		my $instname = "$sid/$event $eventname";
		$system_event_valuemap{"$sid/$eventid"} = \@novalues;
		$system_event_instances{$instname} = "$sid/$eventid";
	    }
	}
	$pmda->replace_indom($system_event_indom, \%system_event_instances);
    }
}

sub system_event_values
{
    my ($dbh, $sid, $handle) = @_;
    my $result = refresh_results($dbh, $sid, $handle);

    if (defined($result)) {
	for my $i (0 .. $#{$result}) {  # for each row (instance) returned
	    my $eventid = $result->[$i][0];
	    my $values = $result->[$i];
	    splice(@$values, 0, 1);	# drop event_id column
	    $system_event_valuemap{"$sid/$eventid"} = $values;
	}
    }
}

sub version_insts
{
    if ((my $count = keys(%version_instances)) == 0) {
	foreach my $sid (@sids) {
	    $version_instances{$sid} = \@novalues;
	}
	$pmda->replace_indom($version_indom, \%version_instances);
    }
}

sub version_values
{
    my ($dbh, $sid, $handle) = @_;
    my $result = refresh_results($dbh, $sid, $handle);

    $version_instances{$sid} = \@novalues;
    if (defined($result)) {
	for my $i (0 .. $#{$result}) {
	    $version_instances{$sid} = $result->[$i];
	}
    }
}

sub waitstat_values
{
    my ($dbh, $sid, $handle) = @_;
    my $result = refresh_results($dbh, $sid, $handle);

    %waitstat_instances = ();
    if (defined($result)) {
	for my $i (0 .. $#{$result}) {  
	    my $class = $result->[$i][0];
	    my $count = $result->[$i][1];
	    my $time = $result->[$i][2];
	    my $values = $result->[$i];
	    my $instname = "$sid/$class";
	    $instname =~ s/ /_/g;	# follow inst name rules
	    splice(@$values, 0, 1);	# drop 'class' column
	    $waitstat_instances{$instname} = $values;
	}
    }
    $pmda->replace_indom($waitstat_indom, \%waitstat_instances);
}

sub latch_insts
{
    my ($dbh, $sid, $handle) = @_;
    
    if ((my $count = keys(%latch_instances)) == 0) {
	my $result = refresh_results($dbh, $sid, $handle);

	if (defined($result)) {
	    for my $i (0 .. $#{$result}) {
                my $latchnum = $result->[$i][0];
                my $latchname = $result->[$i][1];
                my $instname = "$sid/$latchnum $latchname";

                $latch_valuemap{"$sid/$latchnum"} = \@novalues;
                $latch_instances{$instname} = "$sid/$latchnum";
            }
        }
        $pmda->replace_indom($latch_indom, \%latch_instances);
    }
}

sub latch_values
{
    my ($dbh, $sid, $handle) = @_;
    my $result = refresh_results($dbh, $sid, $handle);

    $latch_instances{$sid} = \@novalues;
    if (defined($result)) {
	for my $i (0 .. $#{$result}) {
	    my $latch_num = $result->[$i][0];
	    my $values = $result->[$i];
	    splice(@$values, 0, 1);	# drop latch# column
	    $latch_valuemap{"$sid/$latch_num"} = $values;
	}
    }
}

sub license_insts
{
    if ((my $count = keys(%license_instances)) == 0) {
	foreach my $sid (@sids) {
	    $license_instances{$sid} = \@novalues;
	}
	$pmda->replace_indom($license_indom, \%license_instances);
    }
}

sub license_values
{
    my ($dbh, $sid, $handle) = @_;
    my $result = refresh_results($dbh, $sid, $handle);

    $license_instances{$sid} = \@novalues;
    if (defined($result)) {
	for my $i (0 .. $#{$result}) {
	    $license_instances{$sid} = $result->[$i];
	}
    }
}

sub filestat_clear
{
    undef %filestat_instances;
    $pmda->replace_indom($filestat_indom, \%filestat_instances);
}

sub filestat_insts
{
    my ($dbh, $sid, $handle) = @_;
    return if $sids_by_name{$sid}{filestat}{disabled};

    if ((my $count = keys(%filestat_instances)) == 0) {
	my $result = refresh_results($dbh, $sid, $handle);

	if (defined($result)) {
	    for my $i (0 .. $#{$result}) {
		my $filenum = $result->[$i][0];
		my $filepath = $result->[$i][1];
		my $instname = "$sid/$filenum $filepath";

		$filestat_valuemap{"$sid/$filenum"} = \@novalues;
		$filestat_instances{$instname} = "$sid/$filenum";
	    }
	}
	$pmda->replace_indom($filestat_indom, \%filestat_instances);
    }
}

sub filestat_values
{
    my ($dbh, $sid, $handle) = @_;
    return if $sids_by_name{$sid}{filestat}{disabled};
    my $result = refresh_results($dbh, $sid, $handle);

    if (defined($result)) {
	for my $i (0 .. $#{$result}) {
	    my $filenum = $result->[$i][0];
	    my $values = $result->[$i];
	    splice(@$values, 0, 1);	# drop file# column
	    $filestat_valuemap{"$sid/$filenum"} = $values;
	}
    }
}

sub rollstat_insts
{
    my ($dbh, $sid, $handle) = @_;

    if ((my $count = keys(%rollstat_instances)) == 0) {
	my $result = refresh_results($dbh, $sid, $handle);

	if (defined($result)) {
	    for my $i (0 .. $#{$result}) {  # for each row (instance) returned
		my $usn = $result->[$i][0];
		my $name = $result->[$i][1];
		my $instname = "$sid/$usn $name";
		$rollstat_valuemap{"$sid/$usn"} = \@novalues;
		$rollstat_instances{$instname} = "$sid/$usn";
	    }
	}
	$pmda->replace_indom($rollstat_indom, \%rollstat_instances);
    }
}

sub rollstat_values
{
    my ($dbh, $sid, $handle) = @_;
    my $result = refresh_results($dbh, $sid, $handle);

    if (defined($result)) {
	for my $i (0 .. $#{$result}) {
	    my $usn = $result->[$i][0];
	    my $values = $result->[$i];
	    splice(@$values, 0, 1);	# drop usn column
	    $rollstat_valuemap{"$sid/$usn"} = $values;
	}
    }
}

sub backup_insts
{
    my ($dbh, $sid, $handle) = @_;
    
    if ((my $count = keys(%backup_instances)) == 0) {
	my $result = refresh_results($dbh, $sid, $handle);

	if (defined($result)) {
	    for my $i (0 .. $#{$result}) {
		my $filenum = $result->[$i][0];
		my $filepath = $result->[$i][1];
		my $instname = "$sid/$filenum $filepath";

		$backup_valuemap{"$sid/$filenum"} = \@novalues;
		$backup_instances{$instname} = "$sid/$filenum";
	    }
	}
	$pmda->replace_indom($backup_indom, \%backup_instances);
    }
}

sub backup_values
{
    my ($dbh, $sid, $handle) = @_;
    my $result = refresh_results($dbh, $sid, $handle);

    if (defined($result)) {
	for my $i (0 .. $#{$result}) {
	    my $filenum = $result->[$i][0];
	    my $status = $result->[$i][1];
	    my $values = $result->[$i];
	    splice(@$values, 0, 1);	# drop file# column
	    my $code = 63;	# ?
	    if ($status eq 'NOT ACTIVE') {
		$code = 45;	# -
	    } elsif ($status eq 'ACTIVE') {
		$code = 43;	# +
	    } elsif ($status eq 'OFFLINE') {
		$code = 111;	# o
	    } elsif ($status eq 'NORMAL') {
		$code = 110;	# n
	    } elsif ($status eq 'ERROR') {
		$code = 69;	# E
	    }
	    push @$values, ($code);
	    $backup_valuemap{"$sid/$filenum"} = $values;
	}
    }
}

sub rowcache_insts
{
    my ($dbh, $sid, $handle) = @_;
    
    if ((my $count = keys(%rowcache_instances)) == 0) {
	my $result = refresh_results($dbh, $sid, $handle);

	if (defined($result)) {
	    for my $i (0 .. $#{$result}) {
		my $cache_num = $result->[$i][0];
		my $subord_num = $result->[$i][1];
		my $parameter = $result->[$i][2];
		my ($instname, $cache_id);

		if (defined($subord_num)) {
		    $cache_id = "$sid/$cache_num/$subord_num";
		} else {
		    $cache_id = "$sid/$cache_num";
		}
		$instname = "$sid/$cache_id $parameter";
		$rowcache_valuemap{"$sid/$cache_id"} = \@novalues;
		$rowcache_instances{$instname} = "$sid/$cache_id";
	    }
	}
	$pmda->replace_indom($rowcache_indom, \%rowcache_instances);
    }
}

sub rowcache_values
{
    my ($dbh, $sid, $handle) = @_;
    my $result = refresh_results($dbh, $sid, $handle);

    if (defined($result)) {
	for my $i (0 .. $#{$result}) {
	    my $cache_num = $result->[$i][0];
	    my $subord_num = $result->[$i][1];
	    my $parameter = $result->[$i][2];
	    my $values = $result->[$i];
	    my $cache_id;

	    if (defined($subord_num)) {
		$cache_id = "$sid/$cache_num/$subord_num";
	    } else {
		$cache_id = "$sid/$cache_num";
	    }
	    splice(@$values, 0, 2);	# drop cache#, subordinate# columns
	    $rowcache_valuemap{"$sid/$cache_id"} = $values;
	}
    }
    $pmda->replace_indom($rowcache_indom, \%rowcache_instances);
}

sub object_cache_clear
{
    foreach my $key (keys(%object_cache_instances)) {
	# columns - sharable_mem, loads, locks, pins
	my @novalues = ();
	$object_cache_instances{$key} = \@novalues;
    }
}

sub object_cache_insts
{
    my ($dbh, $sid, $handle) = @_;
    return if $sids_by_name{$sid}{object_cache}{disabled};

    $pmda->replace_indom($object_cache_indom, \%object_cache_instances)
	unless($object_cache_insts_set == 1);
    $object_cache_insts_set = 1;
}

sub object_cache_values
{
    my ($dbh, $sid, $handle) = @_;
    return if $sids_by_name{$sid}{object_cache}{disabled};
    my $result = refresh_results($dbh, $sid, $handle);

    # clear all the (accumulated) counts at the start
    foreach my $key (keys(%object_cache_instances)) {
	# columns - sharable_mem, loads, locks, pins
	my @zerovalues = (0, 0, 0, 0);
	$object_cache_instances{$key} = \@zerovalues;
    }
    if (defined($result)) {
	for my $i (0 .. $#{$result}) {
	    my $type = $result->[$i][0];
	    my $instname = "$sid/$type";
	    my $values = $result->[$i];
	    my $valueref = $object_cache_instances{$type};

	    if (!defined($valueref)) {
		$valueref = $object_cache_instances{'OTHER'};
	    }
	    # columns - sharable_mem, loads, locks, pins
	    ${ $valueref }[0] += $result->[$i][1];
	    ${ $valueref }[1] += $result->[$i][2];
	    ${ $valueref }[2] += $result->[$i][3];
	    ${ $valueref }[3] += $result->[$i][4];
	}
    }
}

sub librarycache_insts
{
    my ($dbh, $sid, $handle) = @_;
    
    if ((my $count = keys(%librarycache_instances)) == 0) {
	my $result = refresh_results($dbh, $sid, $handle);

	if (defined($result)) {
	    for my $i (0 .. $#{$result}) {
		my $namespace = $result->[$i][0];
		my $instname = "$sid/$namespace";
		$librarycache_instances{$instname} = \@novalues;
	    }
	}
	$pmda->replace_indom($librarycache_indom, \%librarycache_instances);
    }
}

sub librarycache_values
{
    my ($dbh, $sid, $handle) = @_;
    my $result = refresh_results($dbh, $sid, $handle);

    if (defined($result)) {
	for my $i (0 .. $#{$result}) {
	    my $namespace = $result->[$i][0];
	    my $instname = "$sid/$namespace";
	    my $values = $result->[$i];
	    splice(@$values, 0, 1);	# drop namespace column
	    $instname =~ s/ /_/g;	# follow inst name rules
	    $librarycache_instances{$instname} = $values;
	}
    }
}

sub reqdist_insts
{
    my ($dbh, $sid, $handle) = @_;

    if ((my $count = keys(%reqdist_instances)) == 0) {
	my $result = refresh_results($dbh, $sid, $handle);

        if (defined($result)) {
	    for my $i (0 .. $#{$result}) {
		my $bucket = $result->[$i][0];
		my $lo = ($bucket == 0) ? 0.0 : (4 * (2 ** ($bucket-1))) / 100;
		my $hi = ($bucket >= 11) ? 0.0 : 4 * (2 ** $bucket) / 100;
		my $instname;

		if ($bucket < 11) {
		    $instname = "$sid/bucket$bucket - $lo to $hi seconds";
		} else {
		    $instname = "$sid/bucket$bucket - $lo seconds or above";
		}

		$reqdist_valuemap{"$sid/$bucket"} = \@novalues;
		$reqdist_instances{$instname} = "$sid/$bucket";
	    }
	}
	$pmda->replace_indom($reqdist_indom, \%reqdist_instances);
    }
}

sub reqdist_values
{
    my ($dbh, $sid, $handle) = @_;
    my $result = refresh_results($dbh, $sid, $handle);

    if (defined($result)) {
	for my $i (0 .. $#{$result}) {
	    my $bucket = $result->[$i][0];
	    my $values = $result->[$i];
	    splice(@$values, 0, 1);	# drop bucket column
	    $reqdist_valuemap{"$sid/$bucket"} = $values;
	}
    }
}

sub sysstat_insts
{
    if ((my $count = keys(%sysstat_instances)) == 0) {
	foreach my $sid (@sids) {
	    $sysstat_instances{$sid} = \@novalues;
	}
	$pmda->replace_indom($sysstat_indom, \%sysstat_instances);
    }
}

sub sysstat_values
{
    my ($dbh, $sid, $handle) = @_;
    my $result = refresh_results($dbh, $sid, $handle);
    my @varray;

    if (defined($result)) {
	for my $i (0 .. $#{$result}) {
	    my $statistic_num = $result->[$i][0];
	    my $statistic_name = $result->[$i][1];
	    my $pmID_item_num = $sysstat_map{$statistic_name};

	    if (defined($pmID_item_num)) {
		#
		# pull out the current array of values, and insert this value
		# at the offset specific to the mapped PMID item number, such
		# that a subsequent fetch callback can quickly look it up.
		#
		$varray[$pmID_item_num] = $result->[$i][2];
	    } else {
		#$pmda->log("New v\$sysstat statistic name: $statistic_name");
	    }
	}
	$sysstat_instances{$sid} = \@varray;
    }
}

sub bufferpool_insts
{
    my ($dbh, $sid, $handle) = @_;

    if ((my $count = keys(%bufferpool_instances)) == 0) {
	my $result = refresh_results($dbh, $sid, $handle);

	if (defined($result)) {
	    for my $i (0 .. $#{$result}) {
		my $id = $result->[$i][0];
		my $name = $result->[$i][1];
		my $blocksize  = $result->[$i][2];
		my $instname = "$sid/$id $name/$blocksize";
		$bufferpool_valuemap{"$sid/$id"} = \@novalues;
		$bufferpool_instances{$instname} = "$sid/$id";
	    }
	}
	$pmda->replace_indom($bufferpool_indom, \%bufferpool_instances);
    }
}

sub bufferpool_values
{
    my ($dbh, $sid, $handle) = @_;
    my $result = refresh_results($dbh, $sid, $handle);

    if (defined($result)) {
	for my $i (0 .. $#{$result}) {
	    my $id = $result->[$i][0];
	    my $values = $result->[$i];
	    splice(@$values, 0, 1);	# drop id column
	    $bufferpool_valuemap{"$sid/$id"} = $values;
	}
    }
}

sub asm_insts
{
    my ($dbh, $sid, $handle) = @_;

    if ((my $count = keys(%asm_instances)) == 0) {
	my $result = refresh_results($dbh, $sid, $handle);

	if (defined($result)) {
	    for my $i (0 .. $#{$result}) {
		my $group_num = $result->[$i][0];
		my $disk_num = $result->[$i][1];
		my $name = $result->[$i][2];
		my $keyname = "$sid/$disk_num";
		my $instname = "$keyname $group_num/$name";
		$asm_valuemap{$keyname} = \@novalues;
		$asm_instances{$instname} = $keyname;
	    }
	}
	$pmda->replace_indom($asm_indom, \%asm_instances);
    }
}

sub asm_values
{
    my ($dbh, $sid, $handle) = @_;
    my $result = refresh_results($dbh, $sid, $handle);

    if (defined($result)) {
	for my $i (0 .. $#{$result}) {
	    my $disk_num = $result->[$i][0];
	    my $values = $result->[$i];
	    splice(@$values, 0, 1);	# drop disk_number column
	    $asm_valuemap{"$sid/$disk_num"} = $values;
	}
    }
}

sub control_insts
{
    if ((my $count = keys(%control_instances)) == 0) {
	foreach my $sid (@sids) {
	    $control_instances{$sid} = \@novalues;
	}
	$pmda->replace_indom($control_indom, \%control_instances);
    }
}

sub control_values
{
    my ($dbh, $sid, undef) = @_;
    my $inst;

    for ($inst = 0; $inst <= $#sids; $inst++) {
	if ($sid eq $sids[$inst]) {
	    my @values = $control_instances{$sid};
	    if (defined($dbh)) {
		$values[0] = 1;
	    } else {
		$values[0] = 0;
	    }
	    $values[1] = $sids_by_name{$sid}{object_cache}{disabled};
	    $values[2] = $sids_by_name{$sid}{filestat}{disabled};
	    $control_instances{$sid} = \@values;
	}
    }
}

sub parameter_insts
{
    if ((my $count = keys(%parameter_instances)) == 0) {
	foreach my $sid (@sids) {
	    my @empty_values = ('', '');
	    $parameter_instances{$sid} = \@empty_values;
	}
	$pmda->replace_indom($parameter_indom, \%parameter_instances);
    }
}

sub parameter_values
{
    my ($dbh, $sid, $handle) = @_;
    my $result = refresh_results($dbh, $sid, $handle);

    if (defined($result)) {
	for my $i (0 .. $#{$result}) {
	    my $valueref = $parameter_instances{$sid};
	    my $value = $result->[$i][1];
	    my $name = $result->[$i][0];
	    if (defined($name)) {
		if ($name eq 'timed_statistics') {
		    ${ $valueref }[0] = "$value";
		} elsif ($name eq 'statistics_level') {
		    ${ $valueref }[1] = "$value";
		}
	    }
	    $parameter_instances{$sid} = $valueref;
	}
    }
}

sub oracle_set_timed_statistics
{
    my ($sid, $val) = (@_);
    my $db = $sids_by_name{$sid}{db_handle};
    my $sth = $db->prepare("ALTER SYSTEM SET timed_statistics = $val scope=MEMORY");
    $sth->execute();
    return 0;
}

sub oracle_set_statistics_level
{
    my ($sid, $val) = (@_);
    my $db = $sids_by_name{$sid}{db_handle};
    my $sth = $db->prepare("ALTER SYSTEM SET statistics_level = $val scope=MEMORY");
    $sth->execute();
    return 0;
}

sub oracle_store_callback
{
    my ($cluster, $item, $inst, $val) = @_;

    if ($cluster == 17) {	# oracle.parameter
	if ($inst > $#sids) { return PM_ERR_INST; }
	my $sid = $sids[$inst];

	if ($val !~ /^[A-Za-z]+$/) {
	    return PM_ERR_BADSTORE;
	}
	if ($item == 0) {
	    return oracle_set_timed_statistics($sid, $val);	     
	}
	elsif ($item == 1) {
	    return oracle_set_statistics_level($sid, $val);
	}
    }
    elsif ($cluster == 16) {	# oracle.control
	if ($inst > $#sids) { return PM_ERR_INST; }
	my $sid = $sids[$inst];

	if ($item == 0) {	# [...connected]
	    #
	    # %sids_by_name is used to determine whether a manual
	    # disconnect/reconnect has been requested.  This is
	    # distinct from our default preference of wanting to
	    # be connected (Oracle may go up/down independently).
	    #
	    if ($val == 1) {
		$sids_by_name{$sid}{disconnected} = 0;	# mark as up
		return oracle_reconnect($sid);
	    }
	    elsif ($val == 0) {
		$sids_by_name{$sid}{disconnected} = 1;	# mark as down
		return oracle_disconnect($sid);
	    }
	    return PM_ERR_BADSTORE;
	}
	elsif ($item == 1) {	# [...disabled.object_cache]
	    if ($val == 0 || $val == 1) {
		$sids_by_name{$sid}{object_cache}{disabled} = $val;
		object_cache_clear();
	    } else {
		return PM_ERR_BADSTORE;
	    }
	    return 0;
	}
	elsif ($item == 2) {	# [...disabled.file]
	    if ($val == 0 || $val == 1) {
		$sids_by_name{$sid}{filestat}{disabled} = $val;
		filestat_clear();
	    } else {
		return PM_ERR_BADSTORE;
	    }
	    return 0;
	}
    }
    elsif ($cluster < 16) { return PM_ERR_PERMISSION; }
    return PM_ERR_PMID;
}

sub oracle_indoms_setup
{
    $pmda->add_indom($latch_indom, \%latch_instances,
		'Instance domain "latch" from Oracle PMDA',
'The latches used by the RDBMS.  The latch instance domain does not
change.  Latches are simple, low-level serialization mechanisms which
protect access to structures in the system global area (SGA).');

    $pmda->add_indom($filestat_indom, \%filestat_instances,
		'Instance domain "filestat files" from Oracle PMDA',
'The collection of data files that make up the database.  This instance
domain may change during database operation as files are added to or
removed.');

    $pmda->add_indom($rollstat_indom, \%rollstat_instances,
		'Instance domain "rollback" from Oracle PMDA',
'The collection of rollback segments for the database.  This instance
domain may change during database operation as segments are added to or
removed.');

    $pmda->add_indom($reqdist_indom, \%reqdist_instances,
		'RDBMS Request Distribution from Oracle PMDA',
'Each instance is one of the buckets in the histogram of RDBMS request
service times.  The instances are named according to the longest
service time that will be inserted into its bucket.  The instance
domain does not change.');

    $pmda->add_indom($rowcache_indom, \%rowcache_instances,
		'Instance domain "rowcache" from Oracle PMDA',
'Each instance is a type of data dictionary cache.  The names are
derived from the database parameters that define the number of entries
in the particular cache.  In some cases subordinate caches exist.
Names for such sub-caches are composed of the subordinate cache
parameter name prefixed with parent cache name with a "." as a
separator.  Each cache has an identifying number which appears in
parentheses after the textual portion of the cache name to resolve
naming ambiguities.  The rowcache instance domain does not change.');

    $pmda->add_indom($session_indom, \%session_instances,
		'Instance domain "session" from Oracle PMDA',
'Each instance is a session to the Oracle database.  Sessions may come
and go rapidly.  The instance names correspond to the numeric Oracle
session identifiers.');

    $pmda->add_indom($object_cache_indom, \%object_cache_instances,
		'Instance domain "cache objects" from Oracle PMDA',
'The various types of objects in the database object cache.  This
includes such objects as indices, tables, procedures, packages, users
and dblink.  Any object types not recognized by the Oracle PMDA are
grouped together into a special instance named "other".  The instance
domain may change as various types of objects are bought into and
flushed out of the database object cache.');

    $pmda->add_indom($system_event_indom, \%system_event_instances,
		'Instance domain "system events" from Oracle PMDA',
'The various system events which the database may wait on.  This
includes events such as interprocess communication, control file I/O,
log file I/O, timers.');

    $pmda->add_indom($librarycache_indom, \%librarycache_instances,
		'Instance domain "librarycache" from Oracle PMDA', '');

    $pmda->add_indom($waitstat_indom, \%waitstat_instances,
		'Instance domain "wait statistics" from Oracle PMDA', '');

    $pmda->add_indom($control_indom, \%control_instances,
		'Instance domain "SID" from Oracle PMDA',
'The system identifiers used by the RDBMS and monitored by this PMDA.');

    $pmda->add_indom($license_indom, \%license_instances,
		'Instance domain "license" from Oracle PMDA','');

    $pmda->add_indom($version_indom, \%version_instances,
		'Instance domain "version" from Oracle PMDA','');

    $pmda->add_indom($sysstat_indom, \%sysstat_instances,
		'Instance domain "sysstat" from Oracle PMDA','');

    $pmda->add_indom($bufferpool_indom, \%bufferpool_instances,
		'Instance domain "buffer_pool" from Oracle PMDA','');

    $pmda->add_indom($asm_indom, \%asm_instances,
		'Instance domain "asm" (Automated Storage Management) disks' .
		' from Oracle PMDA','');

    $pmda->add_indom($backup_indom, \%backup_instances,
		'Instance domain "backup files" from Oracle PMDA',
'The collection of backup files that are active for the database.  This
instance domain may change during database operation as files are added
to or removed.');

    $pmda->add_indom($parameter_indom, \%parameter_instances,
		'Instance domain "SID" from Oracle PMDA for parameter setting',
'The system identifiers used by the RDBMS and parameterized by this PMDA.');
}

sub oracle_metrics_setup
{
    foreach my $cluster (sort (keys %tables_by_cluster)) {
	my $setup = $tables_by_cluster{$cluster}{setup_callback};
	my $indom = $tables_by_cluster{$cluster}{indom};
	&$setup($cluster, $indom);
    }
}

#
# Setup routines - one per cluster, add metrics to PMDA
#

sub setup_waitstat	# block contention stats from v$waitstat
{
    my ($cluster, $indom) = @_;
    $pmda->add_metric(pmda_pmid(13,0), PM_TYPE_U32, $waitstat_indom,
	PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
	'oracle.waitstat.count',
	'Number of waits for each block class',
'The number of waits for each class of block.  This value is obtained
from the COUNT column of the V$WAITSTAT view.');

    $pmda->add_metric(pmda_pmid(13,1), PM_TYPE_U32, $waitstat_indom,
	PM_SEM_COUNTER, pmda_units(0,1,0,0,PM_TIME_MSEC,0),
	'oracle.waitstat.time',
	'Sum of all wait times for each block class',
'The sum of all wait times for each block class.  This value is obtained
from the TIME column of the V$WAITSTAT view.');
}

sub setup_bufferpool
{
    $pmda->add_metric(pmda_pmid(14,0), PM_TYPE_U32, $bufferpool_indom,
        PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
        'oracle.bufferpool.set_msize',
        'Buffer pool maximum set size',
'Buffer pool maximum set size.  This value is obtained
from the  SET_MSIZE column of the V$BUFFER_POOL_STATISTICS view.');
 
    $pmda->add_metric(pmda_pmid(14,1), PM_TYPE_U32, $bufferpool_indom,
        PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
        'oracle.bufferpool.free_buffer_wait',
        'Total free buffer wait events',
'Total free buffer wait events.  This value is obtained
from the FREE_BUFFER_WAIT column of the V$BUFFER_POOL_STATISTICS view.');

    $pmda->add_metric(pmda_pmid(14,2), PM_TYPE_U32, $bufferpool_indom,
        PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
        'oracle.bufferpool.write_complete_wait',
        'Total write complete wait events',
'Total write complete wait events.  This value is obtained
from the WRITE_COMPLETE_WAIT column of the V$BUFFER_POOL_STATISTICS view.');

    $pmda->add_metric(pmda_pmid(14,3), PM_TYPE_U32, $bufferpool_indom,
        PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
        'oracle.bufferpool.buffer_busy_wait',
        'Buffer busy wait statistic',
'Buffer busy wait statistic.  This value is obtained
from the BUFFER_BUSY_WAIT column of the V$BUFFER_POOL_STATISTICS view.');

    $pmda->add_metric(pmda_pmid(14,4), PM_TYPE_U32, $bufferpool_indom,
        PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
        'oracle.bufferpool.physical_reads',
        'Total physical reads',
'Total physical reads.  This value is obtained
from the PHYSICAL_READS column of the V$BUFFER_POOL_STATISTICS view.');

    $pmda->add_metric(pmda_pmid(14,5), PM_TYPE_U32, $bufferpool_indom,
        PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
        'oracle.bufferpool.physical_writes',
        'Total physical writes',
'Total physical writes.  This value is obtained
from the PHYSICAL_WRITES column of the V$BUFFER_POOL_STATISTICS view.');

    $pmda->add_metric(pmda_pmid(14,6), PM_TYPE_FLOAT, $bufferpool_indom,
        PM_SEM_INSTANT, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
        'oracle.bufferpool.hit_ratio',
        'Buffer pol hit ratio',
'Buffer pool hit ratio.  This value is obtained
from the HIT_RATIO column of the V$BUFFER_POOL_STATISTICS view.');

}

sub setup_asm
{
    $pmda->add_metric(pmda_pmid(15,0), PM_TYPE_U32, $asm_indom,
        PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
        'oracle.asm.reads',
        'Total number of I/O read requests for the disk',
'Total number of I/O read requests for the disk. This value is obtained
from the READS column of the V$ASM_DISK_STAT view.');

    $pmda->add_metric(pmda_pmid(15,1), PM_TYPE_U32, $asm_indom,
        PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
        'oracle.asm.writes',
        'Total number of I/O write requests for the disk',
'Total number of I/O write requests for the disk. This value is obtained
from the WRITES column of the V$ASM_DISK_STAT view.');

    $pmda->add_metric(pmda_pmid(15,2), PM_TYPE_U32, $asm_indom,
        PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
        'oracle.asm.read_errs',
        'Total number of failed I/O read requests for the disk',
'Total number of failed I/O read requests for the disk. This value is obtained
from the READ_ERRS column of the V$ASM_DISK_STAT view.');

    $pmda->add_metric(pmda_pmid(15,3), PM_TYPE_U32, $asm_indom,
        PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
        'oracle.asm.write_errs',
        'Total number of failed I/O write requests for the disk',
'Total number of failed I/O write requests for the disk. This value is obtained
from the WRITE_ERRS column of the V$ASM_DISK_STAT view.');

    $pmda->add_metric(pmda_pmid(15,4), PM_TYPE_FLOAT, $asm_indom,
        PM_SEM_INSTANT, pmda_units(0,1,0,0,PM_TIME_MSEC,0),
        'oracle.asm.read_time',
        'Total I/O time (in hundredths of a second) for read requests for the disk',
'Total I/O time (in hundredths of a second) for read requests for the disk if 
the TIMED_STATISTICS initialization parameter is set to true (0 if set to false). 
This value is obtained from the READ_TIME column of the V$ASM_DISK_STAT view.');

    $pmda->add_metric(pmda_pmid(15,5), PM_TYPE_FLOAT, $asm_indom,
        PM_SEM_INSTANT, pmda_units(0,1,0,0,PM_TIME_MSEC,0),
        'oracle.asm.write_time',
        'Total I/O time (in hundredths of a second) for write requests for the disk',
'Total I/O time (in hundredths of a second) for write requests for the disk if 
the TIMED_STATISTICS initialization parameter is set to true (0 if set to false). 
This value is obtained from the WRITE_TIME column of the V$ASM_DISK_STAT view.');

    $pmda->add_metric(pmda_pmid(15,6), PM_TYPE_U32, $asm_indom,
        PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
        'oracle.asm.bytes_read',
        'Total number of bytes read from the disk',
'Total number of bytes read from the disk. This value is obtained
from the BYTES_READ column of the V$ASM_DISK_STAT view.');

    $pmda->add_metric(pmda_pmid(15,7), PM_TYPE_U32, $asm_indom,
        PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
        'oracle.asm.bytes_written',
        'Total number of bytes read from the disk',
'Total number of bytes read from the disk. This value is obtained
from the BYTES_WRITTEN column of the V$ASM_DISK_STAT view.');
}

sub setup_control	# are we connected, manual disconnect/reconnect
{
    $pmda->add_metric(pmda_pmid(16,0), PM_TYPE_U32, $control_indom,
        PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
        'oracle.control.connected',
        'Status of Oracle database connection for each SID',
'A value of one or zero reflecting the state of the Oracle connection.
This is a storable metric, allowing manual disconnect and reconnect, which
allows an Oracle instance to be shutdown while the PMDA continues running.');

    $pmda->add_metric(pmda_pmid(16,1), PM_TYPE_U32, $control_indom,
        PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
        'oracle.control.disabled.object_cache',
        'Status of V$DB_OBJECT_CACHE view queries for each SID',
'A value of one or zero reflecting whether queries are disabled for the
V$DB_OBJECT_CACHE view by the Oracle PMDA.  Experience has shown this can
introduce high latency in some situations.  When this query is disabled,
any oracle.object_cache.* metric accesses will return PM_ERR_AGAIN.');

    $pmda->add_metric(pmda_pmid(16,2), PM_TYPE_U32, $control_indom,
        PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
        'oracle.control.disabled.file',
        'Status of V$FILESTAT view queries for each SID',
'A value of one or zero reflecting whether queries are disabled for the
V$FILESTAT view by the Oracle PMDA.  Experience has shown this can be a
source of high latency in some situations.  When this query is disabled,
any oracle.file.* metric accesses will return no values.');
}

sub setup_parameter
{
    $pmda->add_metric(pmda_pmid(17,0), PM_TYPE_STRING, $parameter_indom,
        PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
        'oracle.parameter.timed_statistics',
        'Value of the timed_statistics Oracle parameter',
'A string value of TRUE or FALSE reflecting timed_statistics parameter.
This is a storable metric, allowing modification of the global database
"timed_statistics" parameter');

    $pmda->add_metric(pmda_pmid(17,1), PM_TYPE_STRING, $parameter_indom,
        PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
        'oracle.parameter.statistics_level',
        'Value of the statistics_level Oracle parameter',
'A string value of BASIC, TYPICAL or ALL reflecting the statistics_level
parameter.
This is a storable metric, allowing modification of the global database
"statistics_level" parameter');
}

sub setup_version	# version data from the v$version view
{
    $pmda->add_metric(pmda_pmid(11,0), PM_TYPE_STRING, $version_indom,
	PM_SEM_DISCRETE, pmda_units(0,0,0,0,0,0),
	'oracle.version',
	'Oracle component name and version number', '');
}

sub setup_system_event	# statistics from v$system_event
{
    $pmda->add_metric(pmda_pmid(10,0), PM_TYPE_U32, $system_event_indom,
	PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
	'oracle.event.waits',
	'Number of waits for various system events',
'The total number of waits for various system events.  This value is
obtained from the TOTAL_WAITS column of the V$SYSTEM_EVENT view.');

    $pmda->add_metric(pmda_pmid(10,1), PM_TYPE_U32, $system_event_indom,
	PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
	'oracle.event.timeouts',
	'Number of timeouts for various system events',
'The total number of timeouts for various system events.  This value is
obtained from the TOTAL_TIMEOUTS column of the V$SYSTEM_EVENT view.');

    $pmda->add_metric(pmda_pmid(10,2), PM_TYPE_U32, $system_event_indom,
	PM_SEM_COUNTER, pmda_units(0,1,0,0,PM_TIME_MSEC,0),
	'oracle.event.time_waited',
	'Total time waited for various system events',
'The total amount of time waited for various system events.  This value
is obtained from the TIME_WAITED column of the V$SYSTEM_EVENT view and
converted to units of milliseconds.');

    $pmda->add_metric(pmda_pmid(10,3), PM_TYPE_FLOAT, $system_event_indom,
	PM_SEM_INSTANT, pmda_units(0,1,0,0,PM_TIME_MSEC,0),
	'oracle.event.average_wait',
	'Average time waited for various system events',
'The average time waited for various system events.  This value is
obtained from the AVERAGE_WAIT column of the V$SYSTEM_EVENT view
and converted to units of milliseconds.');
}

sub setup_sysstat	## statistics from v$sysstat
{
    $sysstat_map{'logons cumulative'} = 0;
    $pmda->add_metric(pmda_pmid(0,0), PM_TYPE_U32, $sysstat_indom,
        PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
        'oracle.sysstat.logons', 'Total cumulative logons',
'The "logons cumulative" statistic from the V$SYSSTAT view.  This is the
total number of logons since the instance started.');

    $sysstat_map{'logons current'} = 1;
    $pmda->add_metric(pmda_pmid(0,1), PM_TYPE_U32, $sysstat_indom,
        PM_SEM_INSTANT, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
        'oracle.sysstat.curlogons', 'Total current logons',
'The "logons current" statistic from the V$SYSSTAT view.  This is the
total number of current logons.');

    $sysstat_map{'opened cursors cumulative'} = 2;
    $pmda->add_metric(pmda_pmid(0,2), PM_TYPE_U32, $sysstat_indom,
        PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
        'oracle.sysstat.opencursors', 'Total cumulative opened cursors',
'The "opened cursors cumulative" statistic from the V$SYSSTAT view.
This is the total number of cursors opened since the instance started.');

    $sysstat_map{'opened cursors current'} = 3;
    $pmda->add_metric(pmda_pmid(0,3), PM_TYPE_U32, $sysstat_indom,
        PM_SEM_INSTANT, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
        'oracle.sysstat.current_cursors', 'Total current open cursors',
'The "opened cursors current" statistic from the V$SYSSTAT view.  This
is the total number of current open cursors.');

    $sysstat_map{'user commits'} = 4;
    $pmda->add_metric(pmda_pmid(0,4), PM_TYPE_U32, $sysstat_indom,
        PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
        'oracle.sysstat.user_commits', 'Total user commits',
'The "user commits" statistic from the V$SYSSTAT view.  When a user
commits a transaction, the redo generated that reflects the changes
made to database blocks must be written to disk.  Commits often
represent the closest thing to a user transaction rate.');

    $sysstat_map{'user rollbacks'} = 5;
    $pmda->add_metric(pmda_pmid(0,5), PM_TYPE_U32, $sysstat_indom,
        PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
        'oracle.sysstat.user_rollbacks', 'Total user rollbacks',
'The "user rollbacks" statistic from the V$SYSSTAT view.  This statistic
stores the number of times users manually issue the ROLLBACK statement
or an error occurs during users\' transactions.');

    $sysstat_map{'user calls'} = 6;
    $pmda->add_metric(pmda_pmid(0,6), PM_TYPE_U32, $sysstat_indom,
        PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
        'oracle.sysstat.user_calls', 'Total user calls',
'The "user calls" statistic from the V$SYSSTAT view.  Oracle allocates
resources (Call State Objects) to keep track of relevant user call data
structures every time you log in, parse or execute.  When determining
activity, the ratio of user calls to RPI calls, gives you an indication
of how much internal work gets generated as a result of the type of
requests the user is sending to Oracle.');

    $sysstat_map{'recursive calls'} = 7;
    $pmda->add_metric(pmda_pmid(0,7), PM_TYPE_U32, $sysstat_indom,
        PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
        'oracle.sysstat.recursecalls', 'Total recursive calls',
'The "recursive calls" statistic from the V$SYSSTAT view.  Oracle
maintains tables used for internal processing.  When Oracle needs to
make a change to these tables, it internally generates an SQL
statement.  These internal SQL statements generate recursive calls.');

    $sysstat_map{'recursive cpu'} = 8;
    $pmda->add_metric(pmda_pmid(0,8), PM_TYPE_U32, $sysstat_indom,
        PM_SEM_COUNTER, pmda_units(0,1,0,0,PM_TIME_MSEC,0),
        'oracle.sysstat.recursecpu', 'Total recursive cpu usage',
'The "recursive cpu usage" statistic from the V$SYSSTAT view.  The total
CPU time used by non-user calls (recursive calls).  Subtract this value
from oracle.sysstat.sessioncpu to determine how much CPU time was used
by the user calls.  Units are milliseconds of CPU time.');

    $sysstat_map{'session logical reads'} = 9;
    $pmda->add_metric(pmda_pmid(0,9), PM_TYPE_U32, $sysstat_indom,
        PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
        'oracle.sysstat.session.lreads', 'Total session logical reads',
'The "session logical reads" statistic from the V$SYSSTAT view.  This
statistic is basically the sum of oracle.systat.dbbgets and
oracle.sysstat.consgets.  Refer to the help text for these
individual metrics for more information.');

    $sysstat_map{'session stored procedure space'} = 10;
    $pmda->add_metric(pmda_pmid(0,10), PM_TYPE_U32, $sysstat_indom,
        PM_SEM_INSTANT, pmda_units(1,0,0,PM_SPACE_BYTE,0,0),
        'oracle.sysstat.session.procspace',
'Total session stored procedure space',
'The "session stored procedure space" statistic from the V$SYSSTAT
view.  This metric shows the amount of memory that this session is
using for stored procedures.');

    $sysstat_map{'CPU used when call started'} = 11;
    $pmda->add_metric(pmda_pmid(0,11), PM_TYPE_U32, $sysstat_indom,
        PM_SEM_COUNTER, pmda_units(0,1,0,0,PM_TIME_MSEC,0),
        'oracle.sysstat.cpucall', 'CPU used when call started',
'The "CPU used when call started" statistic from the V$SYSSTAT view.
This is the session CPU when current call started.  Units are
milliseconds of CPU time.');

    $sysstat_map{'CPU used by this session'} = 12;
    $pmda->add_metric(pmda_pmid(0,12), PM_TYPE_U32, $sysstat_indom,
        PM_SEM_COUNTER, pmda_units(0,1,0,0,PM_TIME_MSEC,0),
        'oracle.sysstat.session.cpu', 'Total CPU used by this session',
'The "CPU used by this session" statistic from the V$SYSSTAT view.  This
is the amount of CPU time used by a session between when a user call
started and ended.  Units for the exported metric are milliseconds, but
Oracle uses an internal resolution of tens of milliseconds and some
user calls can complete within 10 milliseconds, resulting in the start
and end user-call times being the same.  In this case, zero
milliseconds are added to the statistic.');

    $sysstat_map{'session connect time'} = 13;
    $pmda->add_metric(pmda_pmid(0,13), PM_TYPE_U32, $sysstat_indom,
        PM_SEM_COUNTER, pmda_units(0,1,0,0,PM_TIME_SEC,0),
        'oracle.sysstat.session.contime', 'Session connect time',
'The "session connect time" statistic from the V$SYSSTAT view.
Wall clock time of when session logon occured.  Units are seconds
since the epoch.');

    $sysstat_map{'process last non-idle time'} = 14;
    $pmda->add_metric(pmda_pmid(0,14), PM_TYPE_U32, $sysstat_indom,
        PM_SEM_INSTANT, pmda_units(0,1,0,0,PM_TIME_SEC,0),
        'oracle.sysstat.procidle', 'Total process last non-idle time',
'The "process last non-idle time" statistic from the V$SYSSTAT view.
This is the last time this process was not idle.  Units are seconds
since the epoch.');

    $sysstat_map{'session uga memory'} = 15;
    $pmda->add_metric(pmda_pmid(0,15), PM_TYPE_U32, $sysstat_indom,
        PM_SEM_INSTANT, pmda_units(1,0,0,PM_SPACE_BYTE,0,0),
        'oracle.sysstat.session.mem', 'Session UGA memory',
'The "session UGA memory" statistic from the V$SYSSTAT view.  This
shows the current session UGA (User Global Area) memory size.');

    $sysstat_map{'session uga memory max'} = 16;
    $pmda->add_metric(pmda_pmid(0,16), PM_TYPE_U32, $sysstat_indom,
        PM_SEM_DISCRETE, pmda_units(1,0,0,PM_SPACE_BYTE,0,0),
        'oracle.sysstat.session.maxmem', 'Maximum session UGA memory',
'The "session UGA memory max" statistic from the V$SYSSTAT view.  This
shows the maximum session UGA (User Global Area) memory size.');

    $sysstat_map{'messages sent'} = 17;
    $pmda->add_metric(pmda_pmid(0,17), PM_TYPE_U32, $sysstat_indom,
        PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
        'oracle.sysstat.msgxmit', 'Total messages sent',
'The "messages sent" statistic from the V$SYSSTAT view.  This is the
total number of messages sent between Oracle processes.  A message is
sent when one Oracle process wants to post another to perform some
action.');

    $sysstat_map{'messages received'} = 18;
    $pmda->add_metric(pmda_pmid(0,18), PM_TYPE_U32, $sysstat_indom,
        PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
        'oracle.sysstat.msgrecv', 'Total messages received',
'The "messages received" statistic from the V$SYSSTAT view.  This is the
total number of messages received.  A message is sent when one Oracle
process wants to post another to perform some action.');

    $sysstat_map{'background timeouts'} = 19;
    $pmda->add_metric(pmda_pmid(0,19), PM_TYPE_U32, $sysstat_indom,
        PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
        'oracle.sysstat.bgtimeouts', 'Total background timeouts',
'The "background timeouts" statistic from the V$SYSSTAT view.  This is
a count of the times where a background process has set an alarm for
itself and the alarm has timed out rather than the background process
being posted by another process to do some work.');

    $sysstat_map{'session pga memory'} = 20;
    $pmda->add_metric(pmda_pmid(0,20), PM_TYPE_U32, $sysstat_indom,
        PM_SEM_INSTANT, pmda_units(1,0,0,PM_SPACE_BYTE,0,0),
        'oracle.sysstat.sepgamem', 'Session PGA memory',
'The "session PGA memory" statistic from the V$SYSSTAT view.  This
shows the current session PGA (Process Global Area) memory size.');

    $sysstat_map{'session pga memory max'} = 21;
    $pmda->add_metric(pmda_pmid(0,21), PM_TYPE_U32, $sysstat_indom,
        PM_SEM_DISCRETE, pmda_units(1,0,0,PM_SPACE_BYTE,0,0),
        'oracle.sysstat.session.maxpgamem', 'Maximum session PGA memory',
'The "session PGA memory max" statistic from the V$SYSSTAT view.  This
shows the maximum session PGA (Process Global Area) memory size.');

    $sysstat_map{'enqueue timeouts'} = 22;
    $pmda->add_metric(pmda_pmid(0,22), PM_TYPE_U32, $sysstat_indom,
        PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
        'oracle.sysstat.enqueue.timeouts', 'Total enqueue timeouts',
'The "enqueue timeouts" statistic from the V$SYSSTAT view.  This is
the total number of enqueue operations (get and convert) that timed
out before they could complete.');

    $sysstat_map{'enqueue waits'} = 23;
    $pmda->add_metric(pmda_pmid(0,23), PM_TYPE_U32, $sysstat_indom,
        PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
        'oracle.sysstat.enqueue.waits', 'Total enqueue waits',
'The "enqueue waits" statistic from the V$SYSSTAT view.  This is the
total number of waits that happened during an enqueue convert or get
because the enqueue could not be immediately granted.');

    $sysstat_map{'enqueue deadlocks'} = 24;
    $pmda->add_metric(pmda_pmid(0,24), PM_TYPE_U32, $sysstat_indom,
        PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
        'oracle.sysstat.enqueue.deadlocks', 'Total enqueue deadlocks',
'The "enqueue deadlocks" statistic from the V$SYSSTAT view.  This is
the total number of enqueue deadlocks between different sessions.');

    $sysstat_map{'enqueue requests'} = 25;
    $pmda->add_metric(pmda_pmid(0,25), PM_TYPE_U32, $sysstat_indom,
        PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
        'oracle.sysstat.enqueue.requests', 'Total enqueue requests',
'The "enqueue requests" statistic from the V$SYSSTAT view.  This is
the total number of enqueue gets.');

    $sysstat_map{'enqueue conversions'} = 26;
    $pmda->add_metric(pmda_pmid(0,26), PM_TYPE_U32, $sysstat_indom,
        PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
        'oracle.sysstat.enqueue.conversions', 'Total enqueue conversions',
'The "enqueue conversions" statistic from the V$SYSSTAT view.  This is
the total number of enqueue converts.');

    $sysstat_map{'enqueue releases'} = 27;
    $pmda->add_metric(pmda_pmid(0,27), PM_TYPE_U32, $sysstat_indom,
        PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
        'oracle.sysstat.enqueue.releases', 'Total enqueue releases',
'The "enqueue releases" statistic from the V$SYSSTAT view.  This is
the total number of enqueue releases.');

    $sysstat_map{'global lock gets (sync)'} = 28;
    $pmda->add_metric(pmda_pmid(0,28), PM_TYPE_U32, $sysstat_indom,
        PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
        'oracle.sysstat.globlock.gets', 'Total global lock gets (sync)',
'The "global lock gets (sync)" statistic from the V$SYSSTAT view.  This
is the total number of synchronous global lock gets.');

    $sysstat_map{'global lock gets (async)'} = 29;
    $pmda->add_metric(pmda_pmid(0,29), PM_TYPE_U32, $sysstat_indom,
        PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
        'oracle.sysstat.globlock.agets', 'Total global lock gets (async)',
'The "global lock gets (async)" statistic from the V$SYSSTAT view.
This is the total number of asynchronous global lock gets.');

    $sysstat_map{'global lock get time'} = 30;
    $pmda->add_metric(pmda_pmid(0,30), PM_TYPE_U32, $sysstat_indom,
        PM_SEM_COUNTER, pmda_units(0,1,0,0,PM_TIME_MSEC,0),
        'oracle.sysstat.globlock.gettime', 'Total global lock get time',
'The "global lock get time" statistic from the V$SYSSTAT view.  This is
the total elapsed time of all synchronous global lock gets.');

    $sysstat_map{'global lock converts (sync)'} = 31;
    $pmda->add_metric(pmda_pmid(0,31), PM_TYPE_U32, $sysstat_indom,
        PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
        'oracle.sysstat.globlock.cvts', 'Total global lock converts (sync)',
'The "global lock converts (sync)" statistic from the V$SYSSTAT view.
This is the total number of synchronous global lock converts.');

    $sysstat_map{'global lock converts (async)'} = 32;
    $pmda->add_metric(pmda_pmid(0,32), PM_TYPE_U32, $sysstat_indom,
        PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
        'oracle.sysstat.globlock.acvts', 'Total global lock converts (async)',
'The "global lock converts (async)" statistic from the V$SYSSTAT view.
This is the total number of asynchronous global lock converts.');

    $sysstat_map{'global lock convert time'} = 33;
    $pmda->add_metric(pmda_pmid(0,33), PM_TYPE_U32, $sysstat_indom,
        PM_SEM_COUNTER, pmda_units(0,1,0,0,PM_TIME_MSEC,0),
        'oracle.sysstat.globlock.cvttime', 'Total global lock convert time',
'The "global lock convert time" statistic from the V$SYSSTAT view.
This is the total elapsed time of all synchronous global lock converts.');

    $sysstat_map{'global lock releases (sync)'} = 34;
    $pmda->add_metric(pmda_pmid(0,34), PM_TYPE_U32, $sysstat_indom,
        PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
        'oracle.sysstat.globlock.rels', 'Total global lock releases (sync)',
'The "global lock releases (sync)" statistic from the V$SYSSTAT view.
This is the total number of synchronous global lock releases.');

    $sysstat_map{'global lock releases (async)'} = 35;
    $pmda->add_metric(pmda_pmid(0,35), PM_TYPE_U32, $sysstat_indom,
        PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
        'oracle.sysstat.globlock.arels', 'Total global lock releases (async)',
'The "global lock releases (async)" statistic from the V$SYSSTAT view.
This is the total number of asynchronous global lock releases.');

    $sysstat_map{'global lock release time'} = 36;
    $pmda->add_metric(pmda_pmid(0,36), PM_TYPE_U32, $sysstat_indom,
        PM_SEM_COUNTER, pmda_units(0,1,0,0,PM_TIME_MSEC,0),
        'oracle.sysstat.globlock.reltime', 'Total global lock release time',
'The "global lock release time" statistic from the V$SYSSTAT view.
This is the elapsed time of all synchronous global lock releases.');

    $sysstat_map{'db block gets'} = 37;
    $pmda->add_metric(pmda_pmid(0,37), PM_TYPE_U32, $sysstat_indom,
        PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
        'oracle.sysstat.dbbgets', 'Total db block gets',
'The "db block gets" statistic from the V$SYSSTAT view.  This tracks
the number of blocks obtained in CURRENT mode.');

    $sysstat_map{'consistent gets'} = 38;
    $pmda->add_metric(pmda_pmid(0,38), PM_TYPE_U32, $sysstat_indom,
        PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
        'oracle.sysstat.consgets', 'Total consistent gets',
'The "consistent gets" statistic from the V$SYSSTAT view.  This is the
number of times a consistent read was requested for a block.  Also see
the help text for oracle.sysstat.conschanges.');

    $sysstat_map{'physical reads'} = 39;
    $pmda->add_metric(pmda_pmid(0,39), PM_TYPE_U32, $sysstat_indom,
        PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
        'oracle.sysstat.preads', 'Total physical reads',
'The "physical reads" statistic from the V$SYSSTAT view.  This is the
number of I/O requests to the operating system to retrieve a database
block from the disk subsystem.  This is a buffer cache miss.
Logical reads = oracle.sysstat.consgets + oracle.sysstat.dbbgets.
Logical reads and physical reads are used to calculate the buffer hit
ratio.');

    $sysstat_map{'physical writes'} = 40;
    $pmda->add_metric(pmda_pmid(0,40), PM_TYPE_U32, $sysstat_indom,
        PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
        'oracle.sysstat.pwrites', 'Total physical writes',
'The "physical writes" statistic from the V$SYSSTAT view.  This is the
number of I/O requests to the operating system to write a database
block to the disk subsystem.  The bulk of the writes are performed
either by DBWR or LGWR.');

    $sysstat_map{'write requests'} = 41;
    $pmda->add_metric(pmda_pmid(0,41), PM_TYPE_U32, $sysstat_indom,
        PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
        'oracle.sysstat.wreqs', 'Total write requests',
'The "write requests" statistic from the V$SYSSTAT view.  This is the
number of times DBWR has flushed sets of dirty buffers to disk.');

    $sysstat_map{'summed dirty queue length'} = 42;
    $pmda->add_metric(pmda_pmid(0,42), PM_TYPE_U32, $sysstat_indom,
        PM_SEM_COUNTER, pmda_units(0,0,0,0,0,0),
        'oracle.sysstat.dirtyqlen', 'Total summed dirty queue length',
'The "summed dirty queue length" statistic from the V$SYSSTAT view.
This is the sum of the dirty LRU queue length after every write
request.
Divide by the write requests (oracle.sysstat.wreqs) to get the
average queue length after write completion.  For more information see
the help text associated with oracle.sysstat.wreqs.');

    $sysstat_map{'db block changes'} = 43;
    $pmda->add_metric(pmda_pmid(0,43), PM_TYPE_U32, $sysstat_indom,
        PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
        'oracle.sysstat.dbbchanges', 'Total db block changes',
'The "db block changes" statistic from the V$SYSSTAT view.  This metric
is closely related to "consistent changes"
(oracle.sysstat.conschanges) and counts the total number of
changes made to all blocks in the SGA that were part of an update or
delete operation.  These are the changes that are generating redo log
entries and hence will be permanent changes to the database if the
transaction is committed.
This metric is a rough indication of total database work and indicates
(possibly on a per-transaction level) the rate at which buffers are
being dirtied.');

    $sysstat_map{'change write time'} = 44;
    $pmda->add_metric(pmda_pmid(0,44), PM_TYPE_U32, $sysstat_indom,
        PM_SEM_COUNTER, pmda_units(0,1,0,0,PM_TIME_MSEC,0),
        'oracle.sysstat.chwrtime', 'Total change write time',
'The "change write time" statistic from the V$SYSSTAT view.  This is
the elapsed time for redo write for changes made to CURRENT blocks.');

    $sysstat_map{'consistent changes'} = 45;
    $pmda->add_metric(pmda_pmid(0,45), PM_TYPE_U32, $sysstat_indom,
        PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
        'oracle.sysstat.conschanges', 'Total consistent changes',
'The "consistent changes" statistic from the V$SYSSTAT view.  This is
the number of times a database block has applied rollback entries to
perform a consistent read on the block.');

    $sysstat_map{'redo synch writes'} = 46;
    $pmda->add_metric(pmda_pmid(0,46), PM_TYPE_U32, $sysstat_indom,
        PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
        'oracle.sysstat.redo.syncwr', 'Total redo sync writes',
'The "redo sync writes" statistic from the V$SYSSTAT view.  Usually,
redo that is generated and copied into the log buffer need not be
flushed out to disk immediately.  The log buffer is a circular buffer
that LGWR periodically flushes.  This metric is incremented when
changes being applied must be written out to disk due to commit.');

    $sysstat_map{'redo synch time'} = 47;
    $pmda->add_metric(pmda_pmid(0,47), PM_TYPE_U32, $sysstat_indom,
        PM_SEM_COUNTER, pmda_units(0,1,0,0,PM_TIME_MSEC,0),
        'oracle.sysstat.redo.synctime', 'Total redo sync time',
'The "redo sync time" statistic from the V$SYSSTAT view.  This is the
elapsed time of all redo sync writes (oracle.sysstat.redo.syncwr).');

    $sysstat_map{'exchange deadlocks'} = 48;
    $pmda->add_metric(pmda_pmid(0,48), PM_TYPE_U32, $sysstat_indom,
        PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
        'oracle.sysstat.exdeadlocks', 'Total exchange deadlocks',
'The "exchange deadlocks" statistic from the V$SYSSTAT view.  This is
the number of times that a process detected a potential deadlock when
exchanging two buffers and raised an internal, restartable error.
Index scans are currently the only operations which perform exchanges.');

    $sysstat_map{'free buffer requested'} = 49;
    $pmda->add_metric(pmda_pmid(0,49), PM_TYPE_U32, $sysstat_indom,
        PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
        'oracle.sysstat.buffer.freereqs', 'Total free buffer requested',
'The "free buffer requested" statistic from the V$SYSSTAT view.  This is
the number of times a reusable buffer or a free buffer was requested to
create or load a block.');

    $sysstat_map{'dirty buffers inspected'} = 50;
    $pmda->add_metric(pmda_pmid(0,50), PM_TYPE_U32, $sysstat_indom,
        PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
        'oracle.sysstat.buffer.dirtyinsp', 'Total dirty buffers inspected',
'The "dirty buffers inspected" statistic from the V$SYSSTAT view.
This is the number of dirty buffers found by the foreground while
the foreground is looking for a buffer to reuse.');

    $sysstat_map{'free buffer inspected'} = 51;
    $pmda->add_metric(pmda_pmid(0,51), PM_TYPE_U32, $sysstat_indom,
        PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
        'oracle.sysstat.buffer.freeinsp', 'Total free buffer inspected',
'The "free buffer inspected" statistic from the V$SYSSTAT view.  This is
the number of buffers skipped over from the end of an LRU queue in
order to find a reusable buffer.  The difference between this metric
and the oracle.sysstat.buffer.dirtyinsp metric is the number of
buffers that could not be used because they were either busy, needed to
be written after rapid aging out, or they have a user, a waiter, or are
being read/written.  Refer to the oracle.sysstat.buffer.dirtyinsp
help text also.');

    $sysstat_map{'DBWR timeouts'} = 52;
    $pmda->add_metric(pmda_pmid(0,52), PM_TYPE_U32, $sysstat_indom,
        PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
        'oracle.sysstat.dbwr.timeouts', 'Total DBWR timeouts',
'The "DBWR timeouts" statistic from the V$SYSSTAT view.  This is the
number of times that the DBWR has been idle since the last timeout.
These are the times that the DBWR looked for buffers to idle write.');

    $sysstat_map{'DBWR make free requests'} = 53;
    $pmda->add_metric(pmda_pmid(0,53), PM_TYPE_U32, $sysstat_indom,
        PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
        'oracle.sysstat.dbwr.mkfreereqs', 'Total DBWR make free requests',
'The "DBWR make free requests" statistic from the V$SYSSTAT view.
This is the number of messages received requesting DBWR to make
some more free buffers for the LRU.');

    $sysstat_map{'DBWR free buffers found'} = 54;
    $pmda->add_metric(pmda_pmid(0,54), PM_TYPE_U32, $sysstat_indom,
        PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
        'oracle.sysstat.dbwr.freebuffnd', 'Total DBWR free buffers found',
'The "DBWR free buffers found" statistic from the V$SYSSTAT view.
This is the number of buffers that DBWR found to be clean when it
was requested to make free buffers.  Divide this by
oracle.sysstat.dbwr.mkfreereqs to find the average number of
reusable buffers at the end of each LRU.');

    $sysstat_map{'DBWR lru scans'} = 55;
    $pmda->add_metric(pmda_pmid(0,55), PM_TYPE_U32, $sysstat_indom,
        PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
        'oracle.sysstat.dbwr.lruscans', 'Total DBWR lru scans',
'The "DBWR lru scans" statistic from the V$SYSSTAT view.  This is the
number of times that DBWR does a scan of the LRU queue looking for
buffers to write.  This includes times when the scan is to fill a batch
being written for another purpose such as a checkpoint.  This metric\'s
value is always greater than oracle.sysstat.dbwr.mkfreereqs.');

    $sysstat_map{'DBWR summed scan depth'} = 56;
    $pmda->add_metric(pmda_pmid(0,56), PM_TYPE_U32, $sysstat_indom,
        PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
        'oracle.sysstat.dbwr.sumscandepth', 'Total DBWR summed scan depth',
'The "DBWR summed scan depth" statistic from the V$SYSSTAT view.  The
current scan depth (number of buffers scanned by DBWR) is added to this
metric every time DBWR scans the LRU for dirty buffers.  Divide by
oracle.sysstat.dbwr.lruscans to find the average scan depth.');

    $sysstat_map{'DBWR buffers scanned'} = 57;
    $pmda->add_metric(pmda_pmid(0,57), PM_TYPE_U32, $sysstat_indom,
        PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
        'oracle.sysstat.dbwr.bufsscanned', 'Total DBWR buffers scanned',
'The "DBWR buffers scanned" statistic from the V$SYSSTAT view.
This is the total number of buffers looked at when scanning each
LRU set for dirty buffers to clean.  This count includes both dirty
and clean buffers.  Divide by oracle.sysstat.dbwr.lruscans to
find the average number of buffers scanned.');

    $sysstat_map{'DBWR checkpoints'} = 58;
    $pmda->add_metric(pmda_pmid(0,58), PM_TYPE_U32, $sysstat_indom,
        PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
        'oracle.sysstat.dbwr.checkpoints', 'Total DBWR checkpoints',
'The "DBWR checkpoints" statistic from the V$SYSSTAT view.
This is the number of times the DBWR was asked to scan the cache
and write all blocks marked for a checkpoint.');

    $sysstat_map{'DBWR cross instance writes'} = 59;
    $pmda->add_metric(pmda_pmid(0,59), PM_TYPE_U32, $sysstat_indom,
        PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
        'oracle.sysstat.dbwr.xinstwrites', 'Total DBWR cross instance writes',
'The "DBWR cross instance writes" statistic from the V$SYSSTAT view.
This is the total number of blocks written for other instances so that
they can access the buffers.');

    $sysstat_map{'remote instance undo writes'} = 60;
    $pmda->add_metric(pmda_pmid(0,60), PM_TYPE_U32, $sysstat_indom,
        PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
        'oracle.sysstat.remote.instundowr',
        'Total remote instance undo writes',
'The "remote instance undo writes" statistic from the V$SYSSTAT view.
This is the number of times this instance performed a dirty undo write
so that another instance could read that data.');

    $sysstat_map{'remote instance undo requests'} = 61;
    $pmda->add_metric(pmda_pmid(0,61), PM_TYPE_U32, $sysstat_indom,
        PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
        'oracle.sysstat.remote.instundoreq',
        'Total remote instance undo requests',
'The "remote instance undo requests" statistic from the V$SYSSTAT view.
This is the number of times this instance requested undo from another
instance so it could be read CR.');

    $sysstat_map{'cross instance CR read'} = 62;
    $pmda->add_metric(pmda_pmid(0,62), PM_TYPE_U32, $sysstat_indom,
        PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
        'oracle.sysstat.xinstcrrd', 'Total cross instance CR read',
'The "cross instance CR read" statistic from the V$SYSSTAT view.  This
is the number of times this instance made a cross instance call to
write a particular block due to timeout on an instance lock get.  The
call allowed the blocks to be read CR rather than CURRENT.');

    $sysstat_map{'calls to kcmgcs'} = 63;
    $pmda->add_metric(pmda_pmid(0,63), PM_TYPE_U32, $sysstat_indom,
        PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
        'oracle.sysstat.kcmg.cscalls', 'Total calls to kcmgcs',
'The "calls to kcmgcs" statistic from the V$SYSSTAT view.  This is the
total number of calls to get the current System Commit Number (SCN).');

$sysstat_map{'calls to kcmgrs'} = 64;
    $pmda->add_metric(pmda_pmid(0,64), PM_TYPE_U32, $sysstat_indom,
        PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
        'oracle.sysstat.kcmg.rscalls', 'Total calls to kcmgrs',
'The "calls to kcmgrs" statistic from the V$SYSSTAT view.  This is the
total number of calls to get a recent System Commit Number (SCN).');

    $sysstat_map{'calls to kcmgas'} = 65;
    $pmda->add_metric(pmda_pmid(0,65), PM_TYPE_U32, $sysstat_indom,
        PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
        'oracle.sysstat.kcmg.ascalls', 'Total calls to kcmgas',
'The "calls to kcmgas" statistic from the V$SYSSTAT view.  This is the
total number of calls that Get and Advance the System Commit Number
(SCN).  Also used when getting a Batch of SCN numbers.');

    $sysstat_map{'next scns gotten without going to DLM'} = 66;
    $pmda->add_metric(pmda_pmid(0,66), PM_TYPE_U32, $sysstat_indom,
        PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
        'oracle.sysstat.nodlmscnsgets',
        'Total next scns gotten without going to DLM',
'The "next scns gotten without going to DLM" statistic from the
V$SYSSTAT view.  This is the number of SCNs (System Commit Numbers)
obtained without going to the DLM (Distributed Lock Manager).');

    $sysstat_map{'redo entries'} = 67;
    $pmda->add_metric(pmda_pmid(0,67), PM_TYPE_U32, $sysstat_indom,
        PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
        'oracle.sysstat.redo.entries', 'Total redo entries',
'The "redo entries" statistic from the V$SYSSTAT view.  This metric
is incremented each time redo entries are copied into the redo log
buffer.');

    $sysstat_map{'redo size'} = 68;
    $pmda->add_metric(pmda_pmid(0,68), PM_TYPE_U32, $sysstat_indom,
        PM_SEM_COUNTER, pmda_units(1,0,0,PM_SPACE_BYTE,0,0),
        'oracle.sysstat.redo.size', 'Total redo size',
'The "redo size" statistic from the V$SYSSTAT view.
This is the number of bytes of redo generated.');

    $sysstat_map{'redo entries linearized'} = 69;
    $pmda->add_metric(pmda_pmid(0,69), PM_TYPE_U32, $sysstat_indom,
        PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
        'oracle.sysstat.redo.entslin', 'Total redo entries linearized',
'The "redo entries linearized" statistic from the V$SYSSTAT view.  This
is the number of entries of size <= REDO_ENTRY_PREBUILD_THRESHOLD.
Building these entries increases CPU time but may increase concurrency.');

    $sysstat_map{'redo buffer allocation retries'} = 70;
    $pmda->add_metric(pmda_pmid(0,70), PM_TYPE_U32, $sysstat_indom,
        PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
        'oracle.sysstat.redo.bufallret',
        'Total redo buffer allocation retries',
'The "redo buffer allocation retries" statistic from the V$SYSSTAT
view.  This is the total number of retries necessary to allocate space
in the redo buffer.  Retries are needed because either the redo writer
has gotten behind, or because an event (such as log switch) is
occuring.');

    $sysstat_map{'redo small copies'} = 71;
    $pmda->add_metric(pmda_pmid(0,71), PM_TYPE_U32, $sysstat_indom,
        PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
        'oracle.sysstat.redo.smallcpys', 'Total redo small copies',
'The "redo small copies" statistic from the V$SYSSTAT view.  This is the
total number of entries where size <= LOG_SMALL_ENTRY_MAX_SIZE.  These
entries are copied using the protection of the allocation latch,
eliminating the overhead of getting the copy latch. This is generally
only useful for multi-processor systems.');

    $sysstat_map{'redo wastage'} = 72;
    $pmda->add_metric(pmda_pmid(0,72), PM_TYPE_U32, $sysstat_indom,
        PM_SEM_COUNTER, pmda_units(1,0,0,PM_SPACE_BYTE,0,0),
        'oracle.sysstat.redo.wastage', 'Total redo wastage',
'The "redo wastage" statistic from the V$SYSSTAT view.  This is the
number of bytes wasted because redo blocks needed to be written before
they are completely full.  Early writing may be needed to commit
transactions, to be able to write a database buffer or to switch logs.');

    $sysstat_map{'redo writer latching time'} = 73;
    $pmda->add_metric(pmda_pmid(0,73), PM_TYPE_U32, $sysstat_indom,
        PM_SEM_COUNTER, pmda_units(0,1,0,0,PM_TIME_MSEC,0),
        'oracle.sysstat.redo.wrlatchtime', 'Total redo writer latching time',
'The "redo writer latching time" statistic from the V$SYSSTAT view.
This is the elapsed time needed by LGWR to obtain and release each copy
latch.  This is only used if the LOG_SIMULTANEOUS_COPIES initialization
parameter is greater than zero.');

    $sysstat_map{'redo writes'} = 74;
    $pmda->add_metric(pmda_pmid(0,74), PM_TYPE_U32, $sysstat_indom,
        PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
        'oracle.sysstat.redo.writes', 'Total redo writes',
'The "redo writes" statistic from the V$SYSSTAT view.
This is the total number of writes by LGWR to the redo log files.');

    $sysstat_map{'redo blocks written'} = 75;
    $pmda->add_metric(pmda_pmid(0,75), PM_TYPE_U32, $sysstat_indom,
        PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
        'oracle.sysstat.redo.bwrites', 'Total redo blocks written',
'The "redo blocks written" statistic from the V$SYSSTAT view.
This metric is not documented by Oracle.');

    $sysstat_map{'redo write time'} = 76;
    $pmda->add_metric(pmda_pmid(0,76), PM_TYPE_U32, $sysstat_indom,
        PM_SEM_COUNTER, pmda_units(0,1,0,0,PM_TIME_MSEC,0),
        'oracle.sysstat.redo.wrtime', 'Total redo write time',
'The "redo write time" statistic from the V$SYSSTAT view.  This is the
total elapsed time of the write from the redo log buffer to the current
redo log file.');

    $sysstat_map{'redo log space requests'} = 77;
    $pmda->add_metric(pmda_pmid(0,77), PM_TYPE_U32, $sysstat_indom,
        PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
        'oracle.sysstat.redo.logspreqs', 'Total redo log space requests',
'The "redo log space requests" statistic from the V$SYSSTAT view.  The
active log file is full and Oracle is waiting for disk space to be
allocated for the redo log entries.  Space is created by performing a
log switch.
Small log files in relation to the size of the SGA or the commit rate
of the work load can cause problems.  When the log switch occurs,
Oracle must ensure that all committed dirty buffers are written to disk
before switching to a new log file.  If you have a large SGA full of
dirty buffers and small redo log files, a log switch must wait for DBWR
to write dirty buffers to disk before continuing.');

    $sysstat_map{'redo log space wait time'} = 78;
    $pmda->add_metric(pmda_pmid(0,78), PM_TYPE_U32, $sysstat_indom,
        PM_SEM_COUNTER, pmda_units(0,1,0,0,PM_TIME_MSEC,0),
        'oracle.sysstat.redo.logspwaittime', 'Total redo log space wait time',
'The "redo log space wait time" statistic from the V$SYSSTAT view.  This
is the total elapsed time spent waiting for redo log space requests
(refer to the oracle.sysstat.redo.logspreqs metric).');

    $sysstat_map{'redo log switch interrupts'} = 79;
    $pmda->add_metric(pmda_pmid(0,79), PM_TYPE_U32, $sysstat_indom,
        PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
        'oracle.sysstat.redo.logswintrs', 'Total redo log switch interrupts',
'The "redo log switch interrupts" statistic from the V$SYSSTAT view.
This is the number of times that another instance asked this instance
to advance to the next log file.');

    $sysstat_map{'redo ordering marks'} = 80;
    $pmda->add_metric(pmda_pmid(0,80), PM_TYPE_U32, $sysstat_indom,
        PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
        'oracle.sysstat.redo.ordermarks', 'Total redo ordering marks',
'The "redo ordering marks" statistic from the V$SYSSTAT view.  This is
the number of times that an SCN (System Commit Number) had to be
allocated to force a redo record to have a higher SCN than a record
generated in another thread using the same block.');

    $sysstat_map{'hash latch wait gets'} = 81;
    $pmda->add_metric(pmda_pmid(0,81), PM_TYPE_U32, $sysstat_indom,
        PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
        'oracle.sysstat.hashlwgets', 'Total hash latch wait gets',
'The "hash latch wait gets" statistic from the V$SYSSTAT view.
This metric is not documented by Oracle.');

    $sysstat_map{'background checkpoints started'} = 82;
    $pmda->add_metric(pmda_pmid(0,82), PM_TYPE_U32, $sysstat_indom,
        PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
        'oracle.sysstat.bgchkpts.started',
        'Total background checkpoints started',
'The "background checkpoints started" statistic from the V$SYSSTAT
view.  This is the number of checkpoints started by the background.  It
can be larger than the number completed if a new checkpoint overrides
an incomplete checkpoint.  This only includes checkpoints of the
thread, not individual file checkpoints for operations such as offline
or begin backup.  This statistic does not include the checkpoints
performed in the foreground, such as ALTER SYSTEM CHECKPOINT LOCAL.');

    $sysstat_map{'background checkpoints completed'} = 83;
    $pmda->add_metric(pmda_pmid(0,83), PM_TYPE_U32, $sysstat_indom,
        PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
        'oracle.sysstat.bgchkpts.completed',
        'Total background checkpoints completed',
'The "background checkpoints completed" statistic from the V$SYSSTAT
view.  This is the number of checkpoints completed by the background.
This statistic is incremented when the background successfully advances
the thread checkpoint.');

    $sysstat_map{'transaction lock foreground requests'} = 84;
    $pmda->add_metric(pmda_pmid(0,84), PM_TYPE_U32, $sysstat_indom,
        PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
        'oracle.sysstat.tranlock.fgreqs',
        'Total transaction lock foreground requests',
'The "transaction lock foreground requests" statistic from the V$SYSSTAT
view.  For parallel server this is incremented on each call to ktugil()
"Kernel Transaction Get Instance Lock".  For single instance this has
no meaning.');

    $sysstat_map{'transaction lock foreground wait time'} = 85;
    $pmda->add_metric(pmda_pmid(0,85), PM_TYPE_U32, $sysstat_indom,
        PM_SEM_COUNTER, pmda_units(0,1,0,0,PM_TIME_MSEC,0),
        'oracle.sysstat.tranlock.fgwaittime',
        'Total transaction lock foreground wait time',
'The "transaction lock foreground wait time" statistic from the
V$SYSSTAT view.  This is the total time spent waiting for a transaction
instance lock.');

    $sysstat_map{'transaction lock background gets'} = 86;
    $pmda->add_metric(pmda_pmid(0,86), PM_TYPE_U32, $sysstat_indom,
        PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
        'oracle.sysstat.tranlock.bggets',
        'Total transaction lock background gets',
'The "transaction lock background gets" statistic from the V$SYSSTAT
view.  For parallel server this is incremented on each call to ktuglb()
"Kernel Transaction Get lock in Background".');

    $sysstat_map{'transaction lock background get time'} = 87;
    $pmda->add_metric(pmda_pmid(0,87), PM_TYPE_U32, $sysstat_indom,
        PM_SEM_COUNTER, pmda_units(0,1,0,0,PM_TIME_MSEC,0),
        'oracle.sysstat.tranlock.bggettime',
        'Total transaction lock background get time',
'The "transaction lock background get time" statistic from the V$SYSSTAT
view.  Total time spent waiting for a transaction instance lock in
Background.');

    $sysstat_map{'table scans (short tables)'} = 88;
    $pmda->add_metric(pmda_pmid(0,88), PM_TYPE_U32, $sysstat_indom,
        PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
        'oracle.sysstat.table.shortscans', 'Total table scans (short tables)',
'The "table scans (short tables)" statistic from the V$SYSSTAT view.
Long (or conversely short) tables can be defined by optimizer hints
coming down into the row source access layer of Oracle.  The table must
have the CACHE option set.');

    $sysstat_map{'table scans (long tables)'} = 89;
    $pmda->add_metric(pmda_pmid(0,89), PM_TYPE_U32, $sysstat_indom,
        PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
        'oracle.sysstat.table.longscans', 'Total table scans (long tables)',
'The "table scans (long tables)" statistic from the V$SYSSTAT view.
Long (or conversely short) tables can be defined as tables that do not
meet the short table criteria described in the help text for the
oracle.sysstat.table.shortscans metric.');

    $sysstat_map{'table scan rows gotten'} = 90;
    $pmda->add_metric(pmda_pmid(0,90), PM_TYPE_U32, $sysstat_indom,
        PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
        'oracle.sysstat.table.scanrows', 'Total table scan rows gotten',
'The "table scan rows gotten" statistic from the V$SYSSTAT view.  This
is collected during a scan operation, but instead of counting the
number of database blocks (see oracle.sysstat.table.scanblocks),
it counts the rows being processed.');

    $sysstat_map{'table scan blocks gotten'} = 91;
    $pmda->add_metric(pmda_pmid(0,91), PM_TYPE_U32, $sysstat_indom,
        PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
        'oracle.sysstat.table.scanblocks', 'Total table scan blocks gotten',
'The "table scan blocks gotten" statistic from the V$SYSSTAT view.
During scanning operations, each row is retrieved sequentially by
Oracle.  This metric is incremented for each block encountered during
the scan.
This informs you of the number of database blocks that you had to get
from the buffer cache for the purpose of scanning.  Compare the value
of this parameter to the value of oracle.sysstat.consgets
(consistent gets) to get a feel for how much of the consistent read
activity can be attributed to scanning.');

    $sysstat_map{'table fetch by rowid'} = 92;
    $pmda->add_metric(pmda_pmid(0,92), PM_TYPE_U32, $sysstat_indom,
        PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
        'oracle.sysstat.table.rowidfetches', 'Total table fetch by rowid',
'The "table fetch by rowid" statistic from the V$SYSSTAT view.  When
rows are fetched using a ROWID (usually from an index), each row
returned increments this counter.
This metric is an indication of row fetch operations being performed
with the aid of an index.  Because doing table scans usually indicates
either non-optimal queries or tables without indices, this metric
should increase as the above issues have been addressed in the
application.');

    $sysstat_map{'table fetch continued row'} = 93;
    $pmda->add_metric(pmda_pmid(0,93), PM_TYPE_U32, $sysstat_indom,
        PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
        'oracle.sysstat.table.contfetches', 'Total table fetch continued row',
'The "table fetch continued row" statistic from the V$SYSSTAT view.
This metric is incremented when a row that spans more than one block is
encountered during a fetch.
Retrieving rows that span more than one block increases the logical I/O
by a factor that corresponds to the number of blocks that need to be
accessed.  Exporting and re-importing may eliminate this problem.  Also
take a closer look at the STORAGE parameters PCT_FREE and PCT_USED.
This problem cannot be fixed if rows are larger than database blocks
(for example, if the LONG datatype is used and the rows are extremely
large).');

    $sysstat_map{'cluster key scans'} = 94;
    $pmda->add_metric(pmda_pmid(0,94), PM_TYPE_U32, $sysstat_indom,
        PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
        'oracle.sysstat.clustkey.scans', 'Total cluster key scans',
'The "cluster key scans" statistic from the V$SYSSTAT view.
This is the number of cluster scans that were started.');

    $sysstat_map{'cluster key scan block gets'} = 95;
    $pmda->add_metric(pmda_pmid(0,95), PM_TYPE_U32, $sysstat_indom,
        PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
        'oracle.sysstat.clustkey.scanblocks',
        'Total cluster key scan block gets',
'The "cluster key scan block gets" statistic from the V$SYSSTAT view.
This is the number of blocks obtained in a cluster scan.');

    $sysstat_map{'parse time cpu'} = 96;
    $pmda->add_metric(pmda_pmid(0,96), PM_TYPE_U32, $sysstat_indom,
        PM_SEM_COUNTER, pmda_units(0,1,0,0,PM_TIME_MSEC,0),
        'oracle.sysstat.sql.parsecpu', 'Total parse time cpu',
'The "parse time cpu" statistic from the V$SYSSTAT view.  This is the
total CPU time used for parsing (hard and soft parsing).  Units are
milliseconds of CPU time.');

    $sysstat_map{'parse time elapsed'} = 97;
    $pmda->add_metric(pmda_pmid(0,97), PM_TYPE_U32, $sysstat_indom,
        PM_SEM_COUNTER, pmda_units(0,1,0,0,PM_TIME_MSEC,0),
        'oracle.sysstat.sql.parsereal', 'Total parse time elapsed',
'The "parse time elapsed" statistic from the V$SYSSTAT view.
This is the total elapsed time for parsing.  Subtracting
oracle.sysstat.sql.parsecpu from this metric gives the total
waiting time for parse resources.  Units are milliseconds.');

    $sysstat_map{'parse count (total)'} = 98;
    $pmda->add_metric(pmda_pmid(0,98), PM_TYPE_U32, $sysstat_indom,
        PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
        'oracle.sysstat.sql.parsed', 'Total parse count',
'The "parse count (total)" statistic from the V$SYSSTAT view.  This is
the total number of parse calls (hard and soft).  A soft parse is a
check to make sure that the permissions on the underlying objects have
not changed.');

    $sysstat_map{'execute count'} = 99;
    $pmda->add_metric(pmda_pmid(0,99), PM_TYPE_U32, $sysstat_indom,
        PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
        'oracle.sysstat.sql.executed', 'Total execute count',
'The "execute count" statistic from the V$SYSSTAT view.
This is the total number of calls (user and recursive) that
execute SQL statements.');

    $sysstat_map{'sorts (memory)'} = 100;
    $pmda->add_metric(pmda_pmid(0,100), PM_TYPE_U32, $sysstat_indom,
        PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
        'oracle.sysstat.sql.memsorts', 'Total sorts (memory)',
'The "sorts (memory)" statistic from the V$SYSSTAT view.  If the number
of disk writes is zero, then the sort was performed completely in
memory and this metric is incremented.
This is more an indication of sorting activity in the application
workload.  You cannot do much better than memory sorts, except for no
sorts at all.  Sorting is usually caused by selection criteria
specifications within table join SQL operations.');

    $sysstat_map{'sorts (disk)'} = 101;
    $pmda->add_metric(pmda_pmid(0,101), PM_TYPE_U32, $sysstat_indom,
        PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
        'oracle.sysstat.sql.disksorts', 'Total sorts (disk)',
'The "sorts (disk)" statistic from the V$SYSSTAT view.  If the number
of disk writes is non-zero for a given sort operation, then this metric
is incremented.
Sorts that require I/O to disk are quite resource intensive.
Try increasing the size of the Oracle initialization parameter
SORT_AREA_SIZE.');

    $sysstat_map{'sorts (rows)'} = 102;
    $pmda->add_metric(pmda_pmid(0,102), PM_TYPE_U32, $sysstat_indom,
        PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
        'oracle.sysstat.sql.rowsorts', 'Total sorts (rows)',
'The "sorts (rows)" statistic from the V$SYSSTAT view.
This is the total number of rows sorted.');

    $sysstat_map{'session cursor cache hits'} = 103;
    $pmda->add_metric(pmda_pmid(0,103), PM_TYPE_U32, $sysstat_indom,
        PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
        'oracle.sysstat.sccachehits', 'Total session cursor cache hits',
'The "session cursor cache hits" statistic from the V$SYSSTAT view.
This is the count of the number of hits in the session cursor cache.
A hit means that the SQL statement did not have to be reparsed.
By subtracting this metric from oracle.sysstat.sql.parsed one can
determine the real number of parses that have been performed.');

    $sysstat_map{'cursor authentications'} = 104;
    $pmda->add_metric(pmda_pmid(0,104), PM_TYPE_U32, $sysstat_indom,
        PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
        'oracle.sysstat.cursauths', 'Total cursor authentications',
'The "cursor authentications" statistic from the V$SYSSTAT view.  This
is the total number of cursor authentications.  The number of times
that cursor privileges have been verified, either for a SELECT or
because privileges were revoked from an object, causing all users of
the cursor to be re-authenticated.');

    $sysstat_map{'recovery blocks read'} = 105;
    $pmda->add_metric(pmda_pmid(0,105), PM_TYPE_U32, $sysstat_indom,
        PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
        'oracle.sysstat.recovery.breads', 'Total recovery blocks read',
'The "recovery blocks read" statistic from the V$SYSSTAT view.
This is the number of blocks read during recovery.');

    $sysstat_map{'recovery array reads'} = 106;
    $pmda->add_metric(pmda_pmid(0,106), PM_TYPE_U32, $sysstat_indom,
        PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
        'oracle.sysstat.recovery.areads', 'Total recovery array reads',
'The "recovery array reads" statistic from the V$SYSSTAT view.  This is
the number of reads performed during recovery.');

    $sysstat_map{'recovery array read time'} = 107;
    $pmda->add_metric(pmda_pmid(0,107), PM_TYPE_U32, $sysstat_indom,
        PM_SEM_COUNTER, pmda_units(0,1,0,0,PM_TIME_MSEC,0),
        'oracle.sysstat.recovery.areadtime', 'Total recovery array read time',
'The "recovery array read time" statistic from the V$SYSSTAT view.
This is the elapsed time of I/O while doing recovery.');

    $sysstat_map{'table scans (rowid ranges)'} = 108;
    $pmda->add_metric(pmda_pmid(0,108), PM_TYPE_U32, $sysstat_indom,
        PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
        'oracle.sysstat.table.rowidrngscans',
        'Total table scans (rowid ranges)',
'The "table scans (rowid ranges)" statistic from the V$SYSSTAT view.
This is a count of the table scans with specified ROWID endpoints.
These scans are performed for Parallel Query.');

    $sysstat_map{'table scans (cache partitions)'} = 109;
    $pmda->add_metric(pmda_pmid(0,109), PM_TYPE_U32, $sysstat_indom,
        PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
        'oracle.sysstat.table.cachepartscans',
        'Total table scans (cache partitions)',
'The "table scans (cache partitions)" statistic from the V$SYSSTAT
view.  This is a count of range scans on tables that have the CACHE
option enabled.');

    $sysstat_map{'CR blocks created'} = 110;
    $pmda->add_metric(pmda_pmid(0,110), PM_TYPE_U32, $sysstat_indom,
        PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
        'oracle.sysstat.cr.createblk', 'Total CR blocks created',
'The "CR blocks created" statistic from the V$SYSSTAT view.
A buffer in the buffer cache was cloned.  The most common reason
for cloning is that the buffer is held in an incompatible mode.');

    $sysstat_map{'current blocks converted for CR'} = 111;
    $pmda->add_metric(pmda_pmid(0,111), PM_TYPE_U32, $sysstat_indom,
        PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
        'oracle.sysstat.cr.convcurrblk',
        'Total Current blocks converted for CR',
'The "Current blocks converted for CR" statistic from the V$SYSSTAT
view.  A CURRENT buffer (shared or exclusive) is made CR before it can
be used.');

    $sysstat_map{'Unnecessary process cleanup for SCN batching'} = 112;
    $pmda->add_metric(pmda_pmid(0,112), PM_TYPE_U32, $sysstat_indom,
        PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
        'oracle.sysstat.unnecprocclnscn',
        'Total Unnecessary process cleanup for SCN batching',
'The "Unnecessary process cleanup for SCN batching" statistic from the
V$SYSSTAT view.  This is the total number of times that the process
cleanup was performed unnecessarily because the session/process did not
get the next batched SCN (System Commit Number).  The next batched SCN
went to another session instead.');

    $sysstat_map{'transaction tables consistent reads - undo records applied'} = 113;
    $pmda->add_metric(pmda_pmid(0,113), PM_TYPE_U32, $sysstat_indom,
        PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
        'oracle.sysstat.consread.transtable.undo',
        'Total transaction tables consistent reads - undo records applied',
'The "transaction tables consistent reads - undo records applied"
statistic from the V$SYSSTAT view.  This is the number of UNDO records
applied to get CR images of data blocks.');

    $sysstat_map{'transaction tables consistent read rollbacks'} = 114;
    $pmda->add_metric(pmda_pmid(0,114), PM_TYPE_U32, $sysstat_indom,
        PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
        'oracle.sysstat.consread.transtable.rollback',
        'Total transaction tables consistent read rollbacks',
'The "transaction tables consistent read rollbacks" statistic from the
V$SYSSTAT view.  This is the total number of times transaction tables
are CR rolled back.');

    $sysstat_map{'data blocks consistent reads - undo records applied'} = 115;
    $pmda->add_metric(pmda_pmid(0,115), PM_TYPE_U32, $sysstat_indom,
        PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
        'oracle.sysstat.datablkundo',
        'Total data blocks consistent reads - undo records applied',
'The "data blocks consistent reads - undo records applied" statistic
from the V$SYSSTAT view.  This is the total number of UNDO records
applied to get CR images of data blocks.');

    $sysstat_map{'no work - consistent read gets'} = 116;
    $pmda->add_metric(pmda_pmid(0,116), PM_TYPE_U32, $sysstat_indom,
        PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
        'oracle.sysstat.noworkgets', 'Total no work - consistent read gets',
'The "no work - consistent read gets" statistic from the V$SYSSTAT
view.  This metric is not documented by Oracle.');

    $sysstat_map{'cleanouts only - consistent read gets'} = 117;
    $pmda->add_metric(pmda_pmid(0,117), PM_TYPE_U32, $sysstat_indom,
        PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
        'oracle.sysstat.consread.cleangets',
        'Total cleanouts only - consistent read gets',
'The "cleanouts only - consistent read gets" statistic from the
V$SYSSTAT view.  The number of times a CR get required a block
cleanout ONLY and no application of undo.');

    $sysstat_map{'rollbacks only - consistent read gets'} = 118;
    $pmda->add_metric(pmda_pmid(0,118), PM_TYPE_U32, $sysstat_indom,
        PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
        'oracle.sysstat.consread.rollbackgets',
        'Total rollbacks only - consistent read gets',
'The "rollbacks only - consistent read gets" statistic from the
V$SYSSTAT view.  This is the total number of CR operations requiring
UNDO to be applied but no block cleanout.');

    $sysstat_map{'cleanouts and rollbacks - consistent read gets'} = 119;
    $pmda->add_metric(pmda_pmid(0,119), PM_TYPE_U32, $sysstat_indom,
        PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
        'oracle.sysstat.consread.cleanrollbackgets',
        'Total cleanouts and rollbacks - consistent read gets',
'The "cleanouts and rollbacks - consistent read gets" statistic from the
V$SYSSTAT view.  This is the total number of CR gets requiring BOTH
block cleanout and subsequent rollback to get to the required snapshot
time.');

    $sysstat_map{'rollback changes - undo records applied'} = 120;
    $pmda->add_metric(pmda_pmid(0,120), PM_TYPE_U32, $sysstat_indom,
        PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
        'oracle.sysstat.rollbackchangeundo',
        'Total rollback changes - undo records applied',
'The "rollback changes - undo records applied" statistic from the
V$SYSSTAT view.  This is the total number of undo records applied to
blocks to rollback real changes.  Eg: as a result of a rollback command
and *NOT* in the process of getting a CR block image.
Eg:      commit;
         insert into mytab values (10);
         insert into mytab values (20);
         rollback;
should increase this statistic by 2 (assuming no recursive operations).');

    $sysstat_map{'transaction rollbacks'} = 121;
    $pmda->add_metric(pmda_pmid(0,121), PM_TYPE_U32, $sysstat_indom,
        PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
        'oracle.sysstat.transrollbacks', 'Total transaction rollbacks',
'The "transaction rollbacks" statistic from the V$SYSSTAT view.  This is
the actual transaction rollbacks that involve undoing real changes.
Contrast with oracle.sysstat.user_rollbacks which only indicates the
number of ROLLBACK statements received.');

    $sysstat_map{'immediate (CURRENT) block cleanout applications'} = 122;
    $pmda->add_metric(pmda_pmid(0,122), PM_TYPE_U32, $sysstat_indom,
        PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
        'oracle.sysstat.cleanout.immedcurr',
        'Total immediate (CURRENT) block cleanout applications',
'The "immediate (CURRENT) block cleanout applications" statistic from
the V$SYSSTAT view.  This metric is not documented by Oracle.');

    $sysstat_map{'immediate (CR) block cleanout applications'} = 123;
    $pmda->add_metric(pmda_pmid(0,123), PM_TYPE_U32, $sysstat_indom,
        PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
        'oracle.sysstat.cleanout.immedcr',
        'Total immediate (CR) block cleanout applications',
'The "immediate (CR) block cleanout applications" statistic from the
V$SYSSTAT view.  This metric is not documented by Oracle.');

    $sysstat_map{'deferred (CURRENT) block cleanout applications'} = 124;
    $pmda->add_metric(pmda_pmid(0,124), PM_TYPE_U32, $sysstat_indom,
        PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
        'oracle.sysstat.cleanout.defercurr',
        'Total deferred (CURRENT) block cleanout applications',
'The "deferred (CURRENT) block cleanout applications" statistic from
the V$SYSSTAT view.  This is the number of times cleanout records are
deferred.  Deferred changes are piggybacked with real changes.');

    $sysstat_map{'table scans (direct read)'} = 125;
    $pmda->add_metric(pmda_pmid(0,125), PM_TYPE_U32, $sysstat_indom,
        PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
        'oracle.sysstat.table.dirreadscans', 'Total table scans (direct read)',
'The "table scans (direct read)" statistic from the V$SYSSTAT view.
This is a count of table scans performed with direct read (bypassing
the buffer cache).');

    $sysstat_map{'session cursor cache count'} = 126;
    $pmda->add_metric(pmda_pmid(0,126), PM_TYPE_U32, $sysstat_indom,
        PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
        'oracle.sysstat.sccachecount', 'Total session cursor cache count',
'The "session cursor cache count" statistic from the V$SYSSTAT view.
This is the total number of cursors cached.  This is only incremented
if SESSION_CACHED_CURSORS is greater than zero.  This metric is the
most useful in V$SESSTAT.  If the value for this statistic is close to
the setting of the initialization parameter SESSION_CACHED_CURSORS, the
value of the initialization parameter should be increased.');

    $sysstat_map{'total file opens'} = 127;
    $pmda->add_metric(pmda_pmid(0,127), PM_TYPE_U32, $sysstat_indom,
        PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
        'oracle.sysstat.totalfileopens', 'Total file opens',
'The "total file opens" statistic from the V$SYSSTAT view.  This is the
total number of file opens being performed by the instance.  Each
process needs a number of files (control file, log file, database file)
in order to work against the database.');

    $sysstat_map{'opens requiring cache replacement'} = 128;
    $pmda->add_metric(pmda_pmid(0,128), PM_TYPE_U32, $sysstat_indom,
        PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
        'oracle.sysstat.cachereplaceopens',
        'Opens requiring cache replacement',
'The "opens requiring cache replacement" statistic from the V$SYSSTAT
view.  This is the total number of file opens that caused a current
file to be closed in the process file cache.');

    $sysstat_map{'opens of replaced files'} = 129;
    $pmda->add_metric(pmda_pmid(0,129), PM_TYPE_U32, $sysstat_indom,
        PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
        'oracle.sysstat.replacedfileopens', 'Opens of replaced files',
'The "opens of replaced files" statistic from the V$SYSSTAT view.  This
is the total number of files that needed to be reopened because they
were no longer in the process file cache.');

    $sysstat_map{'commit cleanouts'} = 130;
    $pmda->add_metric(pmda_pmid(0,130), PM_TYPE_U32, $sysstat_indom,
        PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
        'oracle.sysstat.commitcleanouts.total', 'Total commit cleanout calls',
'The "commit cleanouts" statistic from the V$SYSSTAT view.  This is the
number of times that the cleanout block at commit time function was
performed.');

    $sysstat_map{'commit cleanouts successfully completed'} = 131;
    $pmda->add_metric(pmda_pmid(0,131), PM_TYPE_U32, $sysstat_indom,
        PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
        'oracle.sysstat.commitcleanouts.completed',
        'Successful commit cleanouts',
'The "commit cleanouts successfully completed" metric from the V$SYSSTAT
view.  This is the number of times the cleanout block at commit time
function successfully completed.');

    $sysstat_map{'commit cleanout failures: write disabled'} = 132;
    $pmda->add_metric(pmda_pmid(0,132), PM_TYPE_U32, $sysstat_indom,
        PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
        'oracle.sysstat.commitcleanouts.failures.writedisabled',
        'Commits when writes disabled',
'The "commit cleanout failures: write disabled" statistic from the
V$SYSSTAT view.  This is the number of times that a cleanout at commit
time was performed but the writes to the database had been temporarily
disabled.');

    $sysstat_map{'commit cleanout failures: hot backup in progress'} = 133;
    $pmda->add_metric(pmda_pmid(0,133), PM_TYPE_U32, $sysstat_indom,
        PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
        'oracle.sysstat.commitcleanouts.failures.hotbackup',
        'Commit attempts during hot backup',
'The "commit cleanout failures: hot backup in progress" statistic
from the V$SYSSTAT view.  This is the number of times that cleanout
at commit was attempted during hot backup.');

    $sysstat_map{'commit cleanout failures: buffer being written'} = 134;
    $pmda->add_metric(pmda_pmid(0,134), PM_TYPE_U32, $sysstat_indom,
        PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
        'oracle.sysstat.commitcleanouts.failures.bufferwrite',
        'Commits while buffer being written',
'The "commit cleanout failures: buffer being written" statistic from the
V$SYSSTAT view.  This is the number of times that a cleanout at commit
time was attempted but the buffer was being written at the time.');

    $sysstat_map{'commit cleanout failures: callback failure '} = 135;
    $pmda->add_metric(pmda_pmid(0,135), PM_TYPE_U32, $sysstat_indom,
        PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
        'oracle.sysstat.commitcleanouts.failures.callbackfail',
        'Commit callback fails',
'The "commit cleanout failures: callback failure" statistic from the
V$SYSSTAT view.  This is the number of times that the cleanout callback
function returned FALSE (failed).');

    $sysstat_map{'commit cleanout failures: block lost'} = 136;
    $pmda->add_metric(pmda_pmid(0,136), PM_TYPE_U32, $sysstat_indom,
        PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
        'oracle.sysstat.commitcleanouts.failures.blocklost',
        'Commit fails due to lost block',
'The "commit cleanout failures: block lost" statistic from the V$SYSSTAT
view.  This is the number of times that a cleanout at commit was
attempted but could not find the correct block due to forced write,
replacement, or switch CURRENT.');

    $sysstat_map{'commit cleanout failures: cannot pin'} = 137;
    $pmda->add_metric(pmda_pmid(0,137), PM_TYPE_U32, $sysstat_indom,
        PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
        'oracle.sysstat.commitcleanouts.failures.cannotpin',
        'Commit fails due to block pinning',
'The "commit cleanout failures: cannot pin" statistic from the V$SYSSTAT
view.  This is the number of times that a commit cleanout was performed
but failed because the block could not be pinned.');

    $sysstat_map{'DBWR skip hot writes'} = 138;
    $pmda->add_metric(pmda_pmid(0,138), PM_TYPE_U32, $sysstat_indom,
        PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
        'oracle.sysstat.dbwr.skiphotwrites', 'Total DBWR hot writes skipped',
'The "DBWR skip hot writes" statistic from the V$SYSSTAT view.
This metric is not documented by Oracle.');

    $sysstat_map{'DBWR checkpoint buffers written'} = 139;
    $pmda->add_metric(pmda_pmid(0,139), PM_TYPE_U32, $sysstat_indom,
        PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
        'oracle.sysstat.dbwr.ckptbufwrites',
        'Total DBWR checkpoint buffers written',
'The "DBWR checkpoint buffers written" statistic from the V$SYSSTAT
view.  This is the number of times the DBWR was asked to scan the cache
and write all blocks marked for checkpoint.');

    $sysstat_map{'DBWR transaction table writes'} = 140;
    $pmda->add_metric(pmda_pmid(0,140), PM_TYPE_U32, $sysstat_indom,
        PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
        'oracle.sysstat.dbwr.transwrites',
        'Total DBWR transaction table writes',
'The "DBWR transaction table writes" statistic from the V$SYSSTAT view.
This metric is not documented by Oracle.');

    $sysstat_map{'DBWR undo block writes'} = 141;
    $pmda->add_metric(pmda_pmid(0,141), PM_TYPE_U32, $sysstat_indom,
        PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
        'oracle.sysstat.dbwr.undoblockwrites', 'Total DBWR undo block writes',
'The "DBWR undo block writes" statistic from the V$SYSSTAT view.
This metric is not documented by Oracle.');

    $sysstat_map{'DBWR checkpoint write requests'} = 142;
    $pmda->add_metric(pmda_pmid(0,142), PM_TYPE_U32, $sysstat_indom,
        PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
        'oracle.sysstat.dbwr.ckptwritereq',
        'Total DBWR checkpoint write requests',
'The "DBWR checkpoint write requests" statistic from the V$SYSSTAT
view.  This is the number of times the DBWR was asked to scan the cache
and write all blocks.');

    $sysstat_map{'DBWR incr. ckpt. write requests'} = 143;
    $pmda->add_metric(pmda_pmid(0,143), PM_TYPE_U32, $sysstat_indom,
        PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
        'oracle.sysstat.dbwr.incrckptwritereq',
        'Total DBWR incr checkpoint write requests',
'The "DBWR incr. ckpt. write requests" statistic from the V$SYSSTAT
view.  This metric is not documented by Oracle.');

    $sysstat_map{'DBWR revisited being-written buffer'} = 144;
    $pmda->add_metric(pmda_pmid(0,144), PM_TYPE_U32, $sysstat_indom,
        PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
        'oracle.sysstat.dbwr.revisitbuf',
        'Total DBWR being-written buffer revisits',
'The "DBWR revisited being-written buffer" statistic from the V$SYSSTAT
view.  This metric is not documented by Oracle.');

    $sysstat_map{'DBWR Flush object cross instance calls'} = 145;
    $pmda->add_metric(pmda_pmid(0,145), PM_TYPE_U32, $sysstat_indom,
        PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
        'oracle.sysstat.dbwr.xinstflushcalls',
        'Total DBWR cross instance flush calls',
'The "DBWR Flush object cross instance calls" statistic from the
V$SYSSTAT view.  This is the number of times DBWR received a flush by
object number cross instance call (from a remote instance).  This
includes both checkpoint and invalidate object.');

    $sysstat_map{'DBWR Flush object call found no dirty buffers'} = 146;
    $pmda->add_metric(pmda_pmid(0,146), PM_TYPE_U32, $sysstat_indom,
        PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
        'oracle.sysstat.dbwr.nodirtybufs',
        'DBWR flush calls finding no dirty buffers',
'The "DBWR Flush object call found no dirty buffers" statistic from the
V$SYSSTAT view.  DBWR didn\'t find any dirty buffers for an object that
was flushed from the cache.');

    $sysstat_map{'remote instance undo block writes'} = 147;
    $pmda->add_metric(pmda_pmid(0,147), PM_TYPE_U32, $sysstat_indom,
        PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
        'oracle.sysstat.remote.instundoblockwr',
        'Remote instance undo block writes',
'The "remote instance undo block writes" statistic from the V$SYSSTAT
view.  This is the number of times this instance wrote a dirty undo
block so that another instance could read it.');

    $sysstat_map{'remote instance undo header writes'} = 148;
    $pmda->add_metric(pmda_pmid(0,148), PM_TYPE_U32, $sysstat_indom,
        PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
        'oracle.sysstat.remote.instundoheaderwr',
        'Remote instance undo header writes',
'The "remote instance undo header writes" statistic from the V$SYSSTAT
view.  This is the number of times this instance wrote a dirty undo
header block so that another instance could read it.');

    $sysstat_map{'calls to get snapshot scn: kcmgss'} = 149;
    $pmda->add_metric(pmda_pmid(0,149), PM_TYPE_U32, $sysstat_indom,
        PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
        'oracle.sysstat.kcmgss_snapshotscn',
        'Total calls to get snapshot SCN: kcmgss',
'The "calls to get snapshot scn: kcmgss" statistic from the V$SYSSTAT
view.  This is the number of times a snap System Commit Number (SCN)
was allocated.  The SCN is allocated at the start of a transaction.');

    $sysstat_map{'kcmgss waited for batching'} = 150;
    $pmda->add_metric(pmda_pmid(0,150), PM_TYPE_U32, $sysstat_indom,
        PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
        'oracle.sysstat.kcmgss_batchwait', 'Total kcmgss waits for batching',
'The "kcmgss waited for batching" statistic from the V$SYSSTAT view.
This is the number of times the kernel waited on a snapshot System
Commit Number (SCN).');

    $sysstat_map{'kcmgss read scn without going to DLM'} = 151;
    $pmda->add_metric(pmda_pmid(0,151), PM_TYPE_U32, $sysstat_indom,
        PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
        'oracle.sysstat.kcmgss_nodlmscnread',
        'Total kcmgss SCN reads with using DLM',
'The "kcmgss read scn without going to DLM" statistic from the V$SYSSTAT
view.  This is the number of times the kernel casually confirmed the
System Commit Number (SCN) without using the Distributed Lock Manager
(DLM).');

    $sysstat_map{'kcmccs called get current scn'} = 152;
    $pmda->add_metric(pmda_pmid(0,152), PM_TYPE_U32, $sysstat_indom,
        PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
        'oracle.sysstat.kcmccs_currentscn',
        'Total kcmccs calls to get current SCN',
'The "kcmccs called get current scn" statistic from the V$SYSSTAT view.
This is the number of times the kernel got the CURRENT SCN (System
Commit Number) when there was a need to casually confirm the SCN.');

    $sysstat_map{'serializable aborts'} = 153;
    $pmda->add_metric(pmda_pmid(0,153), PM_TYPE_U32, $sysstat_indom,
        PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
        'oracle.sysstat.serializableaborts', 'Total serializable aborts',
'The "serializable aborts" statistic from the V$SYSSTAT view.  This is
the number of times a SQL statement in serializable isolation level had
to abort.');

    $sysstat_map{'global cache hash latch waits'} = 154;
    $pmda->add_metric(pmda_pmid(0,154), PM_TYPE_U32, $sysstat_indom,
        PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
        'oracle.sysstat.globalcache.hashlatchwaits',
        'Global cache hash latch waits',
'The "global cache hash latch waits" statistic from the V$SYSSTAT view.
This is the number of times that the buffer cache hash chain latch
couldn\'t be acquired immediately, when processing a lock element.');

    $sysstat_map{'global cache freelist waits'} = 155;
    $pmda->add_metric(pmda_pmid(0,155), PM_TYPE_U32, $sysstat_indom,
        PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
        'oracle.sysstat.globalcache.freelistwaits',
        'Global cache freelist waits',
'The "global cache freelist waits" statistic from the V$SYSSTAT view.
This is the number of pings for free lock elements (when all release
locks are in use).');

    $sysstat_map{'global cache defers'} = 156;
    $pmda->add_metric(pmda_pmid(0,156), PM_TYPE_U32, $sysstat_indom,
        PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
        'oracle.sysstat.globalcache.defers',
        'Global cache ping request defers',
'The "global cache defers" statistic from the V$SYSSTAT view.
This is the number of times a ping request was deferred until later.');

    $sysstat_map{'instance recovery database freeze count'} = 157;
    $pmda->add_metric(pmda_pmid(0,157), PM_TYPE_U32, $sysstat_indom,
        PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
        'oracle.sysstat.instrecoverdbfreeze',
        'Instance recovery database freezes',
'The "instance recovery database freeze count" statistic from the
V$SYSSTAT view.  This metric is not documented by Oracle.');

    $sysstat_map{'Commit SCN cached'} = 158;
    $pmda->add_metric(pmda_pmid(0,158), PM_TYPE_U32, $sysstat_indom,
        PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
        'oracle.sysstat.commitscncached', 'Commit SCN cached',
'The "Commit SCN cached" statistic from the V$SYSSTAT view.  The System
Commit Number (SCN) is used to serialize time within a single instance,
and across all instances.  This lock resource caches the current value
of the SCN - the value is incremented in response to many database
events, but most notably COMMIT WORK.  Access to the SCN lock value to
get and store the SCN is batched on most cluster implementations, so
that every process that needs a new SCN gets one and stores a new value
back on one instance, before the SCN lock is released so that it may be
granted to another instance.  Processes get the SC lock once and then
use conversion operations to manipulate the lock value.');

    $sysstat_map{'Cached Commit SCN referenced'} = 159;
    $pmda->add_metric(pmda_pmid(0,159), PM_TYPE_U32, $sysstat_indom,
        PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
        'oracle.sysstat.cachedscnreferenced', 'Cached Commit SCN referenced',
'The "Cached Commit SCN referenced" statistic from the V$SYSSTAT view.
The SCN (System Commit Number), is generally a timing mechanism Oracle
uses to guarantee ordering of transactions and to enable correct
recovery from failure.  They are used for guaranteeing
read-consistency, and checkpointing.');

    $sysstat_map{'parse count (hard)'} = 160;
    $pmda->add_metric(pmda_pmid(0,160), PM_TYPE_U32, $sysstat_indom,
        PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
        'oracle.sysstat.hardparsed', 'Total number of hard parses performed',
'The "parse count (hard)" statistic from the V$SYSSTAT view.  This is
the total number of parse calls (real parses).  A hard parse means
allocating a workheap and other memory structures, and then building a
parse tree.  A hard parse is a very expensive operation in terms of
memory use.');

    $sysstat_map{'bytes received via SQL*Net from client'} = 161;
    $pmda->add_metric(pmda_pmid(0,161), PM_TYPE_U32, $sysstat_indom,
        PM_SEM_COUNTER, pmda_units(1,0,0,PM_SPACE_BYTE,0,0),
        'oracle.sysstat.sqlnet.clientrecvs',
        'Total bytes from client via SQL*Net',
'The "bytes received via SQL*Net from client" statistic from the
V$SYSSTAT view.  This is the total number of bytes received from the
client over SQL*Net.');

    $sysstat_map{'bytes sent via SQL*Net to client'} = 162;
    $pmda->add_metric(pmda_pmid(0,162), PM_TYPE_U32, $sysstat_indom,
        PM_SEM_COUNTER, pmda_units(1,0,0,PM_SPACE_BYTE,0,0),
        'oracle.sysstat.sqlnet.clientsends',
        'Total bytes to client via SQL*Net',
'The "bytes sent via SQL*Net to client" statistic from the V$SYSSTAT
view.  This is the total number of bytes sent to the client over
SQL*Net.');

    $sysstat_map{'SQL*Net roundtrips to/from client'} = 163;
    $pmda->add_metric(pmda_pmid(0,163), PM_TYPE_U32, $sysstat_indom,
        PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
        'oracle.sysstat.sqlnet.clientroundtrips',
        'Total client SQL*Net roundtrips',
'The "SQL*Net roundtrips to/from client" statistic from the V$SYSSTAT
view.  This is the total number of network messages sent to and
received from the client.');

    $sysstat_map{'bytes received via SQL*Net from dblink'} = 164;
    $pmda->add_metric(pmda_pmid(0,164), PM_TYPE_U32, $sysstat_indom,
        PM_SEM_COUNTER, pmda_units(1,0,0,PM_SPACE_BYTE,0,0),
        'oracle.sysstat.sqlnet.dblinkrecvs',
        'Total bytes from dblink via SQL*Net',
'The "bytes received via SQL*Net from dblink" statistic from the
V$SYSSTAT view.  This is the total number of bytes received from
the database link over SQL*Net.');

    $sysstat_map{'bytes sent via SQL*Net to dblink'} = 165;
    $pmda->add_metric(pmda_pmid(0,165), PM_TYPE_U32, $sysstat_indom,
        PM_SEM_COUNTER, pmda_units(1,0,0,PM_SPACE_BYTE,0,0),
        'oracle.sysstat.sqlnet.dblinksends',
        'Total bytes to dblink via SQL*Net',
'The "bytes sent via SQL*Net to dblink" statistic from the V$SYSSTAT
view.  This is the total number of bytes sent over a database link.');

    $sysstat_map{'SQL*Net roundtrips to/from dblink'} = 166;
    $pmda->add_metric(pmda_pmid(0,166), PM_TYPE_U32, $sysstat_indom,
        PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
        'oracle.sysstat.sqlnet.dblinkroundtrips',
        'Total dblink SQL*Net roundtrips',
'The "SQL*Net roundtrips to/from dblink" statistic from the V$SYSSTAT
view.  This is the total number of network messages sent to and
received from a database link.');

    $sysstat_map{'queries parallelized'} = 167;
    $pmda->add_metric(pmda_pmid(0,167), PM_TYPE_U32, $sysstat_indom,
        PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
        'oracle.sysstat.parallel.queries', 'Total queries parallelized',
'The "queries parallelized" statistic from the V$SYSSTAT view.  This is
the number of SELECT statements which have been parallelized.');

    $sysstat_map{'DML statements parallelized'} = 168;
    $pmda->add_metric(pmda_pmid(0,168), PM_TYPE_U32, $sysstat_indom,
        PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
        'oracle.sysstat.parallel.DMLstatements',
        'Total DML statements parallelized',
'The "DML statements parallelized" statistic from the V$SYSSTAT view.
This is the number of Data Manipulation Language (DML) statements which
have been parallelized.');

    $sysstat_map{'DDL statements parallelized'} = 169;
    $pmda->add_metric(pmda_pmid(0,169), PM_TYPE_U32, $sysstat_indom,
        PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
        'oracle.sysstat.parallel.DDLstatements',
        'Total DDL statements parallelized',
'The "DDL statements parallelized" statistic from the V$SYSSTAT view.
This is the number of Data Definition Language (DDL) statements which
have been parallelized.');

    $sysstat_map{'PX local messages sent'} = 170;
    $pmda->add_metric(pmda_pmid(0,170), PM_TYPE_U32, $sysstat_indom,
        PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
        'oracle.sysstat.PX.localsends', 'PX local messages sent',
'The "PX local messages sent" statistic from the V$SYSSTAT view.
This is the number of local messages sent for Parallel Execution.');

    $sysstat_map{'PX local messages recv\'d'} = 171;
    $pmda->add_metric(pmda_pmid(0,171), PM_TYPE_U32, $sysstat_indom,
        PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
        'oracle.sysstat.PX.localrecvs', 'PX local messages received',
'The "PX local messages recv\'d" statistic from the V$SYSSTAT view.
This is the number of local messages received for Parallel Execution.');

    $sysstat_map{'PX remote messages sent'} = 172;
    $pmda->add_metric(pmda_pmid(0,172), PM_TYPE_U32, $sysstat_indom,
        PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
        'oracle.sysstat.PX.remotesends', 'PX remote messages sent',
'The "PX remote messages sent" statistic from the V$SYSSTAT view.
This is the number of remote messages sent for Parallel Execution.');

    $sysstat_map{'PX remote messages recv\'d'} = 173;
    $pmda->add_metric(pmda_pmid(0,173), PM_TYPE_U32, $sysstat_indom,
        PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
        'oracle.sysstat.PX.remoterecvs', 'PX remote messages received',
'The "PX remote messages recv\'d" statistic from the V$SYSSTAT view.
This is the number of remote messages received for Parallel Execution.');

    $sysstat_map{'buffer is pinned count'} = 174;
    $pmda->add_metric(pmda_pmid(0,174), PM_TYPE_U32, $sysstat_indom,
        PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
        'oracle.sysstat.buffer.pinned', 'Total pinned buffers',
'The "buffer is pinned count" statistic from the V$SYSSTAT view.
This metric is not documented by Oracle.');

    $sysstat_map{'buffer is not pinned count'} = 175;
    $pmda->add_metric(pmda_pmid(0,175), PM_TYPE_U32, $sysstat_indom,
        PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
        'oracle.sysstat.buffer.notpinned', 'Total not pinned buffers',
'The "buffer is not pinned count" statistic from the V$SYSSTAT view.
This metric is not documented by Oracle.');

    $sysstat_map{'no buffer to keep pinned count'} = 176;
    $pmda->add_metric(pmda_pmid(0,176), PM_TYPE_U32, $sysstat_indom,
        PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
        'oracle.sysstat.buffer.nonetopin', 'No buffer to keep pinned count',
'The "no buffer to keep pinned count" statistic from the V$SYSSTAT
view.  This metric is not documented by Oracle.');

    $sysstat_map{'OS User time used'} = 177;
    $pmda->add_metric(pmda_pmid(0,177), PM_TYPE_U32, $sysstat_indom,
        PM_SEM_COUNTER, pmda_units(0,1,0,0,PM_TIME_MSEC,0),
        'oracle.sysstat.OS.utime', 'OS User time used',
'The "OS User time used" statistic from the V$SYSSTAT view.
Units are milliseconds of CPU user time.');

    $sysstat_map{'OS System time used'} = 178;
    $pmda->add_metric(pmda_pmid(0,178), PM_TYPE_U32, $sysstat_indom,
        PM_SEM_COUNTER, pmda_units(0,1,0,0,PM_TIME_MSEC,0),
        'oracle.sysstat.OS.stime', 'OS System time used',
'The "OS System time used" statistic from the V$SYSSTAT view.
Units are milliseconds of CPU system time.');

    $sysstat_map{'IM parallel scan rows'} = 179;
    $pmda->add_metric(pmda_pmid(0,179), PM_TYPE_U32, $sysstat_indom,
        PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
        'oracle.sysstat.IM.parallel.scanrows', '',
'The "IM parallel scan rows" statistic from the V$SYSSTAT view.');

    $sysstat_map{'IM parallel scans'} = 180;
    $pmda->add_metric(pmda_pmid(0,180), PM_TYPE_U32, $sysstat_indom,
        PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
        'oracle.sysstat.IM.parallel.scans', '',
'The "IM parallel scans" statistic from the V$SYSSTAT view.');

    $sysstat_map{'IM parallel scan degree'} = 181;
    $pmda->add_metric(pmda_pmid(0,181), PM_TYPE_U32, $sysstat_indom,
        PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
        'oracle.sysstat.IM.parallel.scandegree', '',
'The "IM parallel scan degree" statistic from the V$SYSSTAT view.');

    $sysstat_map{'IM parallel scan tasks by thread'} = 182;
    $pmda->add_metric(pmda_pmid(0,182), PM_TYPE_U32, $sysstat_indom,
        PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
        'oracle.sysstat.IM.parallel.thread', '',
'The "IM parallel scan tasks by thread" statistic from the V$SYSSTAT view.');

    $sysstat_map{'IM parallel scan tasks by parent'} = 183;
    $pmda->add_metric(pmda_pmid(0,183), PM_TYPE_U32, $sysstat_indom,
        PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
        'oracle.sysstat.IM.parallel.parent', '',
'The "IM parallel scan tasks by parent" statistic from the V$SYSSTAT view.');

    $sysstat_map{'IM parallel rowset 2 sets'} = 184;
    $pmda->add_metric(pmda_pmid(0,184), PM_TYPE_U32, $sysstat_indom,
        PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
        'oracle.sysstat.IM.parallel.rowset', '',
'The "IM parallel rowset 2 sets" statistic from the V$SYSSTAT view.');

    $sysstat_map{'IM scan rows'} = 185;
    $pmda->add_metric(pmda_pmid(0,185), PM_TYPE_U32, $sysstat_indom,
        PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
        'oracle.sysstat.IM.scan.rows', 'Number of rows scanned in all IMCUs',
'The "IM scan rows" statistic from the V$SYSSTAT view.');

    $sysstat_map{'IM scan CUs columns accessed'} = 186;
    $pmda->add_metric(pmda_pmid(0,186), PM_TYPE_U32, $sysstat_indom,
        PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
        'oracle.sysstat.IM.scan.colaccess', 'Number of columns accessed by a scan',
'The "IM scan CUs columns accessed" statistic from the V$SYSSTAT view.');

    $sysstat_map{'IM scan CUs pruned'} = 187;
    $pmda->add_metric(pmda_pmid(0,187), PM_TYPE_U32, $sysstat_indom,
        PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
        'oracle.sysstat.IM.scan.CUpruned', 'Number of IMCUs with no rows passing min/max',
'The "IM scan CUs pruned" statistic from the V$SYSSTAT view.');

    $sysstat_map{'IM scan segments minmax eligible'} = 188;
    $pmda->add_metric(pmda_pmid(0,188), PM_TYPE_U32, $sysstat_indom,
        PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
        'oracle.sysstat.IM.scan.segmentminmax', 'Number of IMCUs eligible for min/max pruning',
'The "IM scan segments minmax eligible" statistic from the V$SYSSTAT view.');

    $sysstat_map{'IM scan segments disk'} = 189;
    $pmda->add_metric(pmda_pmid(0,189), PM_TYPE_U32, $sysstat_indom,
        PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
        'oracle.sysstat.IM.scan.segmentdisk', 'In-memory scans accessed from buffer cache/direct read',
'Number of times a segment marked for in-memory was accessed entirely
from the buffer cache/direct read.
The "IM scan segments disk" statistic from the V$SYSSTAT view.');

    $sysstat_map{'IM scan rows optimized'} = 190;
    $pmda->add_metric(pmda_pmid(0,190), PM_TYPE_U32, $sysstat_indom,
        PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
        'oracle.sysstat.IM.scan.rowsoptimized', 'Number of rows skipped by optimization',
'Number of rows that were skipped (because of storage index pruning) or
that were not accessed due to aggregations with predicate push downs.
The "IM scan rows optimized" statistic from the V$SYSSTAT view.');
}

sub setup_rowcache	## row cache statistics from v$rowcache
{
    $pmda->add_metric(pmda_pmid(7,0), PM_TYPE_U32, $rowcache_indom,
	PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
	'oracle.rowcache.count',
	'Number of entries in this data dictionary cache',
'The total number of data dictionary cache entries, broken down by data
type.  This is extracted from the COUNT column of the V$ROWCACHE view.');

    $pmda->add_metric(pmda_pmid(7,1), PM_TYPE_U32, $rowcache_indom,
	PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
	'oracle.rowcache.gets',
	'Number of requests for cached information on data dictionary objects',
'The total number of valid data dictionary cache entries, broken down by
data type.  This is extracted from the GETS column of the V$ROWCACHE
view.');

    $pmda->add_metric(pmda_pmid(7,2), PM_TYPE_U32, $rowcache_indom,
	PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
	'oracle.rowcache.getmisses',
	'Number of data requests resulting in cache misses',
'The total number of data dictionary requests that resulted in cache
misses, broken down by data type.  This is extracted from the GETMISSES
column of the V$ROWCACHE view.');

    $pmda->add_metric(pmda_pmid(7,3), PM_TYPE_U32, $rowcache_indom,
	PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
	'oracle.rowcache.scans', 'Number of scan requests',
'The total number of data dictionary cache scans, broken down by data
type.  This is extracted from the SCANS column of the V$ROWCACHE view.');

    $pmda->add_metric(pmda_pmid(7,4), PM_TYPE_U32, $rowcache_indom,
	PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
	'oracle.rowcache.scanmisses',
	'Number of data dictionary cache misses',
'The total number of times data dictionary cache scans failed to find
data in the cache, broken down by data type.  This is extracted from
the SCANMISSES column of the V$ROWCACHE view.');
}

sub setup_rollstat	## rollback I/O statistics from v$rollstat
{
    $pmda->add_metric(pmda_pmid(4,0), PM_TYPE_U32, $rollstat_indom,
	PM_SEM_INSTANT, pmda_units(1,0,0,PM_SPACE_BYTE,0,0),
	'oracle.rollback.rssize', 'Size of rollback segment',
'Size in bytes of the rollback segment.  This value is obtained from the
RSSIZE column in the V$ROLLSTAT view.');

    $pmda->add_metric(pmda_pmid(4,1), PM_TYPE_U32, $rollstat_indom,
	PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
	'oracle.rollback.writes',
	'Number of bytes written to rollback segment',
'The total number of bytes written to rollback segment.  This value is
obtained from the WRITES column of the V$ROLLSTAT view.');

    $pmda->add_metric(pmda_pmid(4,2), PM_TYPE_U32, $rollstat_indom,
	PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
	'oracle.rollback.xacts', 'Number of active transactions',
'The number of active transactions.  This value is obtained from the
XACTS column of the V$ROLLSTAT view.');

    $pmda->add_metric(pmda_pmid(4,3), PM_TYPE_U32, $rollstat_indom,
	PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
	'oracle.rollback.gets',
	'Number of header gets for rollback segment',
'The number of header gets for the rollback segment.  This value is
obtained from the GETS column of the V$ROLLSTAT view.');

    $pmda->add_metric(pmda_pmid(4,4), PM_TYPE_U32, $rollstat_indom,
	PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
	'oracle.rollback.waits',
	'Number of header waits for rollback segment',
'The number of header gets for the rollback segment.  This value is
obtained from the WAIT column of the V$ROLLSTAT view.');

    $pmda->add_metric(pmda_pmid(4,5), PM_TYPE_U32, $rollstat_indom,
	PM_SEM_INSTANT, pmda_units(1,0,0,PM_SPACE_BYTE,0,0),
	'oracle.rollback.hwmsize',
	'High water mark of rollback segment size',
'High water mark of rollback segment size.  This value is obtained from
the HWMSIZE column of the V$ROLLSTAT view.');

    $pmda->add_metric(pmda_pmid(4,6), PM_TYPE_U32, $rollstat_indom,
	PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
	'oracle.rollback.shrinks',
	'Number of times rollback segment shrank',
'The number of times the size of the rollback segment decreased,
eliminating additional extents.  This value is obtained from the
SHRINKS column of the V$ROLLSTAT view.');

    $pmda->add_metric(pmda_pmid(4,7), PM_TYPE_U32, $rollstat_indom,
	PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
	'oracle.rollback.wraps',
	'Number of times rollback segment wrapped',
'The number of times the rollback segment wrapped from one extent
to another.  This value is obtained from the WRAPS column of the
V$ROLLSTAT view.');

    $pmda->add_metric(pmda_pmid(4,8), PM_TYPE_U32, $rollstat_indom,
	PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
	'oracle.rollback.extends',
	'Number of times rollback segment size extended',
'The number of times the size of the rollback segment grew to include
another extent.  This value is obtained from the EXTENDS column of the
V$ROLLSTAT view.');

    $pmda->add_metric(pmda_pmid(4,9), PM_TYPE_U32, $rollstat_indom,
	PM_SEM_INSTANT, pmda_units(1,0,0,PM_SPACE_BYTE,0,0),
	'oracle.rollback.avshrink', 'Average shrink size',
'Average of freed extent size for rollback segment.  This value is
obtained from the AVESHRINK column of the V$ROLLSTAT view.');

    $pmda->add_metric(pmda_pmid(4,10), PM_TYPE_U32, $rollstat_indom,
	PM_SEM_INSTANT, pmda_units(1,0,0,PM_SPACE_BYTE,0,0),
	'oracle.rollback.avactive',
	'Current size of active entents averaged over time',
'Current average size of extents with uncommitted transaction data.
This value is obtained from the AVEACTIVE column from the V$ROLLSTAT
view.');
}

sub setup_reqdist	## request time histogram from v$reqdist
{
    $pmda->add_metric(pmda_pmid(5,0), PM_TYPE_U32, $reqdist_indom,
	PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
	'oracle.reqdist', 'Histogram of database operation request times',
'A histogram of database request times divided into twelve buckets (time
ranges).  This is extracted from the V$REQDIST table.
The time ranges grow exponentially as a function of the bucket number,
such that the maximum time for each bucket is (4 * 2^N) / 100 seconds,
where N is the bucket number (and also the instance identifier).
NOTE:
    The TIMED_STATISTICS database parameter must be TRUE or this metric
    will not return any values.');
}

sub setup_object_cache	## cache statistics from v$db_object_cache
{
    $pmda->add_metric(pmda_pmid(9,0), PM_TYPE_U32, $object_cache_indom,
	PM_SEM_INSTANT, pmda_units(1,0,0,PM_SPACE_KBYTE,0,0),
	'oracle.object_cache.sharemem',
	'Sharable memory usage in database cache pool by object types',
'The amount of sharable memory in the shared pool consumed by various
objects, divided into object types.  The valid object types are:
INDEX, TABLE, CLUSTER, VIEW, SET, SYNONYM, SEQUENCE, PROCEDURE,
FUNCTION, PACKAGE, PACKAGE BODY, TRIGGER, CLASS, OBJECT, USER, DBLINK,
NON_EXISTENT, NOT LOADED and OTHER.
The values for each of these object types are obtained from the
SHARABLE_MEM column of the V$DB_OBJECT_CACHE view.');

    $pmda->add_metric(pmda_pmid(9,1), PM_TYPE_U32, $object_cache_indom,
	PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
	'oracle.object_cache.loads', 'Number of times object loaded',
'The number of times the object has been loaded.  This count also
increases when and object has been invalidated.  These values are
obtained from the LOADS column of the V$DB_OBJECT_CACHE view.');

    $pmda->add_metric(pmda_pmid(9,2), PM_TYPE_U32, $object_cache_indom,
	PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
	'oracle.object_cache.locks',
	'Number of users currently locking this object',
'The number of users currently locking this object.  These values are
obtained from the LOCKS column of the V$DB_OBJECT_CACHE view.');

    $pmda->add_metric(pmda_pmid(9,3), PM_TYPE_U32, $object_cache_indom,
	PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
	'oracle.object_cache.pins',
	'Number of users currently pinning this object',
'The number of users currently pinning this object.  These values are
obtained from the PINS column of the V$DB_OBJECT_CACHE view.');
}

sub setup_license	## licence data from v$license
{
    $pmda->add_metric(pmda_pmid(1,0), PM_TYPE_U32, $license_indom,
	PM_SEM_INSTANT, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
	'oracle.license.maxsess',
	'Maximum number of concurrent user sessions',
'The maximum number of concurrent user sessions permitted for the
instance.  This value is obtained from the SESSIONS_MAX column of
the V$LICENSE view.');

    $pmda->add_metric(
	pmda_pmid(1,1), PM_TYPE_U32, $license_indom,
	PM_SEM_INSTANT, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
	'oracle.license.cursess',
	'Current number of concurrent user sessions',
'The current number of concurrent user sessions for the instance.
This value is obtained from the SESSIONS_CURRENT column of the
V$LICENSE view.');

    $pmda->add_metric(
	pmda_pmid(1,2), PM_TYPE_U32, $license_indom,
	PM_SEM_INSTANT, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
	'oracle.license.warnsess',
	'Warning limit for concurrent user sessions',
'The warning limit for concurrent user sessions for this instance.
This value is obtained from the SESSIONS_WARNING column of the
V$LICENSE view.');

    $pmda->add_metric(
	pmda_pmid(1,3), PM_TYPE_U32, $license_indom,
	PM_SEM_INSTANT, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
	'oracle.license.highsess',
	'Highest number of concurrent user sessions since instance started',
'The highest number of concurrent user sessions since the instance
started.  This value is obtained from the SESSIONS_HIGHWATER column of
the V$LICENSE view.');

    $pmda->add_metric(pmda_pmid(1,4), PM_TYPE_U32, $license_indom,
	PM_SEM_INSTANT, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
	'oracle.license.maxusers',
	'Maximum number of named users permitted',
'The maximum number of named users allowed for the database.  This value
is obtained from the USERS_MAX column of the V$LICENSE view.');
}

sub setup_librarycache	## statistics from v$librarycache
{
    $pmda->add_metric(pmda_pmid(12,0), PM_TYPE_U32, $librarycache_indom,
	PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
	'oracle.librarycache.gets',
	'Number of lock requests for each namespace object',
'The number of times a lock was requested for objects of this
namespace.  This value is obtained from the GETS column of the
V$LIBRARYCACHE view.');

    $pmda->add_metric(pmda_pmid(12,1), PM_TYPE_U32, $librarycache_indom,
	PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
	'oracle.librarycache.gethits',
	'Number of times objects handle found in memory',
'The number of times an object\'s handle was found in memory.  This value
is obtained from the GETHITS column of the V$LIBRARYCACHE view.');

    $pmda->add_metric(pmda_pmid(12,2), PM_TYPE_FLOAT, $librarycache_indom,
	PM_SEM_INSTANT, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
	'oracle.librarycache.gethitratio',
	'Ratio of gethits to hits',
'The ratio of GETHITS to HITS.  This value is obtained from the
GETHITRATIO column of the V$LIBRARYCACHE view.');

    $pmda->add_metric(pmda_pmid(12,3), PM_TYPE_U32, $librarycache_indom,
	PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
	'oracle.librarycache.pins',
	'Number of times a pin was requested for each namespace object',
'The number of times a PIN was requested for each object of the library
cache namespace.  This value is obtained from the PINS column of the
V$LIBRARYCACHE view.');

    $pmda->add_metric(pmda_pmid(12,4), PM_TYPE_U32, $librarycache_indom,
	PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
	'oracle.librarycache.pinhits',
	'Number of times all metadata found in memory',
'The number of times that all of the meta data pieces of the library
object were found in memory.  This value is obtained from the PINHITS
column of the V$LIBRARYCACHE view.');

    $pmda->add_metric(pmda_pmid(12,5), PM_TYPE_FLOAT, $librarycache_indom,
	PM_SEM_INSTANT, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
	'oracle.librarycache.pinhitratio', 'Ratio of pins to pinhits',
'The ratio of PINS to PINHITS.  This value is obtained from the
PINHITRATIO column of the V$LIBRARYCACHE view.');

    $pmda->add_metric(pmda_pmid(12,6), PM_TYPE_U32, $librarycache_indom,
	PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
	'oracle.librarycache.reloads', 'Number of disk reloads required',
'Any PIN of an object that is not the first PIN performed since the
object handle was created, and which requires loading the object from
the disk.  This value is obtained from the RELOADS column of the
V$LIBRARYCACHE view.');

    $pmda->add_metric(pmda_pmid(12,7), PM_TYPE_U32, $librarycache_indom,
	PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
	'oracle.librarycache.invalidations',
	'Invalidations due to dependent object modifications',
'The total number of times objects in the library cache namespace were
marked invalid due to a dependent object having been modified.  This
value is obtained from the INVALIDATIONS column of the V$LIBRARYCACHE
view.');
}

sub setup_latch		## latch statistics from v$latch
{
    $pmda->add_metric(pmda_pmid(2,0), PM_TYPE_U32, $latch_indom,
	PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
	'oracle.latch.gets',
	'Number of times obtained a wait',
'The number of times latch obtained a wait.  These values are obtained
from the GETS column of the V$LATCH view.');

    $pmda->add_metric(pmda_pmid(2,1), PM_TYPE_U32, $latch_indom,
	PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
	'oracle.latch.misses',
	'Number of times obtained a wait but failed on first try',
'The number of times obtained a wait but failed on the first try.  These
values are obtained from the MISSES column of the V$LATCH view.');

    $pmda->add_metric(pmda_pmid(2,2), PM_TYPE_U32, $latch_indom,
	PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
	'oracle.latch.sleeps',
	'Number of times slept when wanted a wait',
'The number of times slept when wanted a wait.  These values are
obtained from the SLEEPS column of the V$LATCH view.');

    $pmda->add_metric(pmda_pmid(2,3), PM_TYPE_U32, $latch_indom,
	PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
	'oracle.latch.imgets',
	'Number of times obtained without a wait',
'The number of times latch obtained without a wait.  These values are
obtained from the IMMEDIATE_GETS column of the V$LATCH view.');

    $pmda->add_metric(pmda_pmid(2,4), PM_TYPE_U32, $latch_indom,
	PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
	'oracle.latch.immisses',
	'Number of times failed to get latch without a wait',
'The number of times failed to get latch without a wait.  These values
are obtained from the IMMEDIATE_MISSES column of the V$LATCH view.');

    $pmda->add_metric(pmda_pmid(2,5), PM_TYPE_U32, $latch_indom,
	PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
	'oracle.latch.wakes',
	'Number of times a wait was awakened',
'The number of times a wait was awakened.  These values are obtained
from the WAITERS_WOKEN column of the V$LATCH view.');

    $pmda->add_metric(pmda_pmid(2,6), PM_TYPE_U32, $latch_indom,
	PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
	'oracle.latch.holds',
	'Number of waits while holding a different latch',
'The number of waits while holding a different latch.  These values are
obtained from the WAITS_HOLDING_LATCH column of the V$LATCH view.');

    $pmda->add_metric(pmda_pmid(2,7), PM_TYPE_U32, $latch_indom,
	PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
	'oracle.latch.spingets',
	'Gets that missed first try but succeeded on spin',
'Gets that missed first try but succeeded on spin.  These values are
obtained from the SPIN_GETS column of the V$LATCH view.');
}

sub setup_backup	## file backup status from v$backup
{
     $pmda->add_metric(pmda_pmid(6,0), PM_TYPE_STRING, $backup_indom,
	PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
	'oracle.backup.status',
	'Backup status of online datafiles',
'The backup status of online datafiles.
This value is extracted from the STATUS column of the V$BACKUP view.');

    $pmda->add_metric(pmda_pmid(6,1), PM_TYPE_U32, $backup_indom,
	PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
	'oracle.backup.status_code',
	'Backup status of online datafiles',
'The Backup status of online datafiles.  The status is encoded as an
ASCII character:
	not active      -  ( 45)
	active          +  ( 43)
	offline         o  (111)
	normal          n  (110)
	error           E  ( 69)
	unknown         ?  ( 63)
This value is extracted from the STATUS column of the V$BACKUP view.');
}

sub setup_filestat	## file I/O statistics from v$filestat
{
    $pmda->add_metric(pmda_pmid(3,0), PM_TYPE_U32, $filestat_indom,
	PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
	'oracle.file.phyrds',
	'Physical reads from database files',
'The number of physical reads from each database file.  These values
are obtained from the PHYRDS column in the V$FILESTAT view.');

    $pmda->add_metric(pmda_pmid(3,1), PM_TYPE_U32, $filestat_indom,
	PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
	'oracle.file.phywrts',
	'Physical writes to database files',
'The number of times the DBWR process is required to write to each of
the database files.  These values are obtained from the PHYWRTS column
in the V$FILESTAT view.');

    $pmda->add_metric(pmda_pmid(3,2), PM_TYPE_U32, $filestat_indom,
	PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
	'oracle.file.phyblkrd',
	'Physical blocks read from database files',
'The number of physical blocks read from each database file.  These
values are obtained from the PHYBLKRDS column in the V$FILESTAT view.');

    $pmda->add_metric(pmda_pmid(3,3), PM_TYPE_U32, $filestat_indom,
	PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
	'oracle.file.phyblkwrt',
	'Physical blocks written to database files',
'The number of physical blocks written to each database file.  These
values are obtained from the PHYBLKWRT column in the V$FILESTAT view.');

    $pmda->add_metric(pmda_pmid(3,4), PM_TYPE_U32, $filestat_indom,
	PM_SEM_COUNTER, pmda_units(0,0,0,0,0,0),
	'oracle.file.readtim',
	'Time spent reading from database files',
'The number of milliseconds spent doing reads if the TIMED_STATISTICS
database parameter is TRUE.  If this parameter is false, then the
metric will have a value of zero.  This value is obtained from the
READTIM column of the V$FILESTAT view.');

    $pmda->add_metric(pmda_pmid(3,5), PM_TYPE_U32, $filestat_indom,
	PM_SEM_COUNTER, pmda_units(0,0,0,0,0,0),
	'oracle.file.writetim',
	'Time spent writing to database files',
'The number of milliseconds spent doing writes if the TIMED_STATISTICS
database parameter is TRUE.  If this parameter is false, then the
metric will have a value of zero.  This value is obtained from the
WRITETIM column of the V$FILESTAT view.');
}

# $ENV{PCP_PERL_DEBUG} = 'LIBPMDA';
$pmda = PCP::PMDA->new('oracle', 32);
$pmda->connect_pmcd;
$pmda->set_user($os_user);

oracle_control_setup();
oracle_metrics_setup();
oracle_indoms_setup();

$pmda->set_fetch_callback(\&oracle_fetch_callback);
$pmda->set_store_callback(\&oracle_store_callback);
$pmda->set_fetch(\&oracle_fetch);
$pmda->set_instance(\&oracle_instance);
$pmda->set_refresh(\&oracle_refresh);
$pmda->run;
