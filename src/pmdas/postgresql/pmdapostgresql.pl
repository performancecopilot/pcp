
# Copyright (c) 2011 Nathan Scott.  All Rights Reserved.
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

my $database = 'dbi:Pg:dbname=postgres';
my $username = 'postgres';	# DB username for DB login
my $password = '';		# DBI parameter, typically unused for postgres
my $os_user = '';		# O/S user to run the PMDA (defaults to $username)
my $version;			# DB version

my $max_version = '9.4';	# Highest DB version PMDA has been tested with

# Note on $max_version
#	Testing is complete up to Postgresql Version 9.4.
#	There are some references to 9.5 in the code below, although
#	these are based on currently available documentation for the
#	forthcoming 9.5 release, and as such should be treated as
#	experimental until production versions of 9.5 are available
#	for testing.
#	Ken McDonell - Jul 2015

# Configuration files for overriding the above settings
# Note: each .conf file may override a setting from a previous .conf
#       file ... so the order of evaluation is important to determine
#       who "wins", namely app defaults, then system defaults, then
#       PMDA defaults, then current directory
#
for my $file (	'/etc/pcpdbi.conf',	# system defaults (lowest priority)
		pmda_config('PCP_PMDAS_DIR') . '/postgresql/postgresql.conf',
		'./postgresql.conf' ) {	# current directory (high priority)
    eval `cat $file` unless ! -f $file;
}

use vars qw( $pmda $dbh );
my $pm_in_null = PM_IN_NULL;
my $all_rel_indom = 0; my @all_rel_instances;
my $sys_rel_indom = 1; my @sys_rel_instances;
my $user_rel_indom = 2; my @user_rel_instances;
my $process_indom = 3; my @process_instances;
my $function_indom = 4; my @function_instances;
my $database_indom = 5; my @database_instances;
my $all_index_indom = 6; my @all_index_instances;
my $sys_index_indom = 7; my @sys_index_instances;
my $user_index_indom = 8; my @user_index_instances;
my $all_seq_indom = 9; my @all_seq_instances;
my $sys_seq_indom = 10; my @sys_seq_instances;
my $user_seq_indom = 11; my @user_seq_instances;
my $replicant_indom = 12; my @replicant_instances;

# hash of hashes holding DB handle, last values, and min version indexed by table name
my %tables_by_name = (
    pg_stat_activity		=> { handle => undef, values => {}, version => undef },
    pg_stat_bgwriter		=> { handle => undef, values => {}, version => undef },
    pg_stat_database		=> { handle => undef, values => {}, version => undef },
    pg_stat_database_conflicts	=> { handle => undef, values => {}, version => 9.1 },
    pg_stat_replication		=> { handle => undef, values => {}, version => 9.1 },
    pg_stat_all_tables		=> { handle => undef, values => {}, version => undef },
    pg_stat_sys_tables		=> { handle => undef, values => {}, version => undef },
    pg_stat_user_tables		=> { handle => undef, values => {}, version => undef },
    pg_stat_all_indexes		=> { handle => undef, values => {}, version => undef },
    pg_stat_sys_indexes		=> { handle => undef, values => {}, version => undef },
    pg_stat_user_indexes	=> { handle => undef, values => {}, version => undef },
    pg_statio_all_tables	=> { handle => undef, values => {}, version => undef },
    pg_statio_sys_tables	=> { handle => undef, values => {}, version => undef },
    pg_statio_user_tables	=> { handle => undef, values => {}, version => undef },
    pg_statio_all_indexes	=> { handle => undef, values => {}, version => undef },
    pg_statio_sys_indexes	=> { handle => undef, values => {}, version => undef },
    pg_statio_user_indexes	=> { handle => undef, values => {}, version => undef },
    pg_statio_all_sequences	=> { handle => undef, values => {}, version => undef },
    pg_statio_sys_sequences	=> { handle => undef, values => {}, version => undef },
    pg_statio_user_sequences	=> { handle => undef, values => {}, version => undef },
    pg_stat_user_functions	=> { handle => undef, values => {}, version => undef },
    pg_stat_xact_user_functions	=> { handle => undef, values => {}, version => 9.1 },
    pg_stat_xact_all_tables	=> { handle => undef, values => {}, version => 9.1 },
    pg_stat_xact_sys_tables	=> { handle => undef, values => {}, version => 9.1 },
    pg_stat_xact_user_tables	=> { handle => undef, values => {}, version => 9.1 },
    pg_active			=> { handle => undef, values => {}, version => undef, sql => 'select pg_is_in_recovery(), pg_current_xlog_location()' },
    pg_recovery			=> { handle => undef, values => {}, version => undef, sql => 'select pg_is_in_recovery(), pg_last_xlog_receive_location(), pg_last_xlog_replay_location()' },
);

# hash of hashes holding setup and refresh function, indexed by PMID cluster
my %tables_by_cluster = (
    '0'	 => {
	name	=> 'pg_stat_activity',
	setup	=> \&setup_activity,
	indom	=> $process_indom,
	refresh => \&refresh_activity },
    '1'	 => {
	name	=> 'pg_stat_bgwriter',
	setup	=> \&setup_bgwriter,
	indom	=> PM_INDOM_NULL,
	refresh	=> \&refresh_bgwriter },
    '2'	 => {
	name	=> 'pg_stat_database',
	setup	=> \&setup_database,
	indom	=> $database_indom,
	refresh => \&refresh_database },
    '3'	 => {
	name	=> 'pg_stat_user_functions',
	setup	=> \&setup_user_functions,
	indom	=> $function_indom,
	refresh => \&refresh_user_functions },
    '4'	 => {
	name	=> 'pg_stat_xact_user_functions',
	setup	=> \&setup_xact_user_functions,
	indom	=> $function_indom,
	refresh => \&refresh_xact_user_functions },
    '5'	 => {
	name	=> 'pg_stat_database_conflicts',
	setup	=> \&setup_database_conflicts,
	indom	=> $database_indom,
	refresh => \&refresh_database_conflicts },
    '6'	 => {
	name	=> 'pg_stat_replication',
	setup	=> \&setup_replication,
	indom	=> $replicant_indom,
	refresh => \&refresh_replication },
    '7'	 => {
	name	=> 'pg_active',
	setup	=> \&setup_active_functions,
	indom	=> PM_INDOM_NULL,
	refresh	=> \&refresh_active_functions },
    '8'	 => {
	name	=> 'pg_recovery',
	setup	=> \&setup_recovery_functions,
	indom	=> PM_INDOM_NULL,
	refresh	=> \&refresh_recovery_functions },
    '10' => {
	name	=> 'pg_stat_all_tables',
	setup	=> \&setup_stat_tables,
	indom	=> $all_rel_indom,
	params	=> 'all_tables',
	refresh => \&refresh_all_tables },
    '11' => {
	name	=> 'pg_stat_sys_tables',
	setup	=> \&setup_stat_tables,
	indom	=> $sys_rel_indom,
	params	=> 'sys_tables',
	refresh => \&refresh_sys_tables },
    '12' => {
	name	=> 'pg_stat_user_tables',
	setup	=> \&setup_stat_tables,
	indom	=> $user_rel_indom,
	params	=> 'user_tables',
	refresh => \&refresh_user_tables },
    '13' => {
	name	=> 'pg_stat_all_indexes',
	setup	=> \&setup_stat_indexes,
	indom	=> $all_index_indom,
	params	=> 'all_indexes',
	refresh => \&refresh_all_indexes },
    '14' => {
	name	=> 'pg_stat_sys_indexes',
	setup	=> \&setup_stat_indexes,
	params	=> 'sys_indexes',
	indom	=> $sys_index_indom,
	refresh => \&refresh_sys_indexes },
    '15' => {
	name	=> 'pg_stat_user_indexes',
	setup	=> \&setup_stat_indexes,
	indom	=> $user_index_indom,
	params	=> 'user_indexes',
	refresh => \&refresh_user_indexes },
    '16' => {
	name	=> 'pg_stat_xact_all_tables',
	setup	=> \&setup_stat_xact_tables,
	indom	=> $all_rel_indom,
	params	=> 'all_tables',
	refresh => \&refresh_xact_all_tables },
    '17' => {
	name	=> 'pg_stat_xact_sys_tables',
	setup	=> \&setup_stat_xact_tables,
	indom	=> $sys_rel_indom,
	params	=> 'sys_tables',
	refresh => \&refresh_xact_sys_tables },
    '18' => {
	name	=> 'pg_stat_xact_user_tables',
	setup	=> \&setup_stat_xact_tables,
	indom	=> $user_rel_indom,
	params	=> 'user_tables',
	refresh => \&refresh_xact_user_tables },
    '30' => {
	name	=> 'pg_statio_all_tables',
	setup	=> \&setup_statio_tables,
	indom	=> $all_rel_indom,
	params	=> 'all_tables',
	refresh => \&refresh_io_all_tables },
    '31' => {
	name	=> 'pg_statio_sys_tables',
	setup	=> \&setup_statio_tables,
	indom	=> $sys_rel_indom,
	params	=> 'sys_tables',
	refresh => \&refresh_io_sys_tables },
    '32' => {
	name	=> 'pg_statio_user_tables',
	setup	=> \&setup_statio_tables,
	indom	=> $user_rel_indom,
	params	=> 'user_tables',
	refresh => \&refresh_io_user_tables },
    '33' => {
	name	=> 'pg_statio_all_indexes',
	setup	=> \&setup_statio_indexes,
	indom	=> $all_index_indom,
	params	=> 'all_indexes',
	refresh => \&refresh_io_all_indexes },
    '34' => {
	name	=> 'pg_statio_sys_indexes',
	setup	=> \&setup_statio_indexes,
	indom	=> $sys_index_indom,
	params	=> 'sys_indexes',
	refresh => \&refresh_io_all_indexes },
    '35' => {
	name	=> 'pg_statio_user_indexes',
	setup	=> \&setup_statio_indexes,
	indom	=> $user_index_indom,
	params	=> 'user_indexes',
	refresh => \&refresh_io_all_indexes },
    '36' => {
	name	=> 'pg_statio_all_sequences',
	setup	=> \&setup_statio_sequences,
	indom	=> $all_seq_indom,
	params	=> 'all_sequences',
	refresh => \&refresh_io_all_sequences },
    '37' => {
	name	=> 'pg_statio_sys_sequences',
	setup	=> \&setup_statio_sequences,
	indom	=> $sys_seq_indom,
	params	=> 'sys_sequences',
	refresh => \&refresh_io_sys_sequences },
    '38' => {
	name	=> 'pg_statio_user_sequences',
	setup	=> \&setup_statio_sequences,
	params	=> 'user_sequences',
	indom	=> $user_seq_indom,
	refresh => \&refresh_io_user_sequences },
);

sub postgresql_version_query
{
    my $handle = $dbh->prepare("select VERSION()");

    if (defined($handle->execute())) {
	my $result = $handle->fetchall_arrayref();
	return 0 unless defined($result);
	my $version = $result->[0][0];
	$version =~ s/^PostgreSQL (\d+\.\d+)\.\d+ .*/$1/g;
	return $version;
    }
    return 0;
}

sub postgresql_connection_setup
{
    if (!defined($dbh)) {
	$pmda->log("connect to DB $database as user $username");
	$dbh = DBI->connect($database, $username, $password,
			    {AutoCommit => 1, pg_bool_tf => 0});
	if (defined($dbh)) {
	    $pmda->log("PostgreSQL connection established");
	    my $raw_version = postgresql_version_query();
	    $version = $raw_version;
	    # extract NN.NN version number, potentially from a much
	    # longer string, e.g. pick 9.4 from:
	    #     EnterpriseDB 9.4.1.4 on x86_64-unknown-linux-gnu ...
	    #
	    $version =~ s/^[^0-9]*([0-9][0-9]*\.[0-9][0-9]*)\..*/$1/;
	    $pmda->log("Version: $version [from DB $raw_version]");

	    foreach my $key (keys %tables_by_name) {
		my $minversion = $tables_by_name{$key}{version};
		my $sqlquery = $tables_by_name{$key}{sql};
		my $query;

		$minversion = 0 unless defined($minversion);
		if ($minversion > $version) {
		    $pmda->log("Skipping table $key, not supported on $version");
		} elsif (defined($sqlquery)) {
		    $query = $dbh->prepare($sqlquery);
		} else {
		    $query = $dbh->prepare("select * from $key");
		}
		$tables_by_name{$key}{handle} = $query unless !defined($query);
	    }
	}
    }
}

sub postgresql_indoms_setup
{
    $all_rel_indom = $pmda->add_indom($all_rel_indom, \@all_rel_instances,
		'Instance domain for PostgreSQL relations, all tables', '');
    $sys_rel_indom = $pmda->add_indom($sys_rel_indom, \@sys_rel_instances,
		'Instance domain for PostgreSQL relations, system tables', '');
    $user_rel_indom = $pmda->add_indom($user_rel_indom, \@user_rel_instances,
		'Instance domain for PostgreSQL relations, user tables', '');

    $function_indom = $pmda->add_indom($function_indom, \@function_instances,
		'Instance domain exporting PostgreSQL user functions', '');

    $process_indom = $pmda->add_indom($process_indom, \@process_instances,
		'Instance domain exporting each PostgreSQL client process', '');

    $database_indom = $pmda->add_indom($database_indom, \@database_instances,
		'Instance domain exporting each PostgreSQL database', '');

    $replicant_indom = $pmda->add_indom($replicant_indom, \@replicant_instances,
		'Instance domain exporting PostgreSQL replication processes', '');

    $all_index_indom = $pmda->add_indom($all_index_indom, \@all_index_instances,
		'Instance domain for PostgreSQL indexes, all tables', '');
    $sys_index_indom = $pmda->add_indom($sys_index_indom, \@sys_index_instances,
		'Instance domain for PostgreSQL indexes, system tables', '');
    $user_index_indom = $pmda->add_indom($user_index_indom, \@user_index_instances,
		'Instance domain for PostgreSQL indexes, user tables', '');

    $all_seq_indom = $pmda->add_indom($all_seq_indom, \@all_seq_instances,
		'Instance domain for PostgreSQL sequences, all tables', '');
    $sys_seq_indom = $pmda->add_indom($sys_seq_indom, \@sys_seq_instances,
		'Instance domain for PostgreSQL sequences, system tables', '');
    $user_seq_indom = $pmda->add_indom($user_seq_indom, \@user_seq_instances,
		'Instance domain for PostgreSQL sequences, user tables', '');
}

sub postgresql_metrics_setup
{
    foreach my $cluster (sort (keys %tables_by_cluster)) {
	my $setup = $tables_by_cluster{"$cluster"}{setup};
	my $indom = $tables_by_cluster{"$cluster"}{indom};
	&$setup($cluster, $indom, $tables_by_cluster{"$cluster"}{params});
    }
}

sub postgresql_refresh
{
    my ($cluster) = @_;

    my $name = $tables_by_cluster{"$cluster"}{name};
    my $refresh = $tables_by_cluster{"$cluster"}{refresh};
    &$refresh($tables_by_name{$name});
}

sub postgresql_fetch_callback
{
    my ($cluster, $item, $inst) = @_;
    my $metric_name = pmda_pmid_name($cluster, $item);
    my ($key, $value, $valueref, @columns );

    return (PM_ERR_PMID, 0) unless defined($metric_name);
    $key = $metric_name;
    $key =~ s/^postgresql\./pg_/;
    $key =~ s/\.[a-zA-Z0-9_]*$//;
    $key =~ s/\./_/g;

    #debug# $pmda->log("fetch_cb $metric_name $cluster:$item ($inst) - $key");

    $valueref = $tables_by_name{$key}{values}{"$inst"};
    if (!defined($valueref)) {
	return (PM_ERR_INST, 0) unless ($inst == PM_IN_NULL);
	return (PM_ERR_VALUE, 0);
    }

    @columns = @$valueref;
    $value = $columns[$item];
    return (PM_ERR_APPVERSION, 0) unless defined($value);

    return ($value, 1);
}

#
# Refresh routines - one per table (cluster) - format database query
# result set for later use by the generic fetch callback routine.
# 
sub refresh_results
{
    my $tableref = shift;
    my %table = %$tableref;
    my ( $handle, $valuesref ) = ( $table{handle}, $table{values} );
    my %values = %$valuesref;

    %values = ();	# clear any previous values

    if (defined($dbh) && defined($handle)) {
	if (defined($handle->execute())) {
	    return $handle->fetchall_arrayref();
	}
	$table{handle} = undef;
    }
    return undef;
}

my %warn_version;
my %warn_ncol;

# handle fields one at a time, cherry picking to accommodate column
# re-ordering in the columns of the performance views across
# Postgresql releases
# Call as ...
#     cherrypick(tablename, ref_to_ctl, ref_to_vlist)
#
sub cherrypick {
    my $table = shift;
    my $ctlp = shift;
    my $vlistp = shift;
    my @vlist = @{$vlistp};
    my %ctl = %{$ctlp};
    my $map;
    my $ncol;
    my @ret = ();

    if (defined($ctl{$version})) {
	$map = $ctl{$version}{map};
	$ncol = $ctl{$version}{ncol};
    } else {
	if (!defined($warn_version{$table})) {
	    $pmda->log("$table: no map for version $version, assuming $max_version");
	    $warn_version{$table} = 'y';
	}
	$map = $ctl{$max_version}{map};
	$ncol = $ctl{$max_version}{ncol};
    }

    if ($#vlist != $ncol-1 && !defined($warn_ncol{$table})) {
	$pmda->log("$table: version $version: fetched" . $#vlist+1 . " columns, expecting $ncol");
	$warn_ncol{$table} = 'y';
    }
    for my $j (0 .. $#{$map}) { # for each metric (item field)
	my $pick = ${$map}[$j];
	if ($pick == -1) {
	    # no matching column
	    $ret[$j] = ''
	}
	else {
	    # even if $j == $pick, we still do the conditional copy
	    # to map null values (e.g. the client_* columns) into
	    # an empty string
	    $ret[$j] = defined($vlist[$pick]) ? $vlist[$pick] : '';
	}
    }
    return \@ret;
}

# Need mapping here because the pg_stat_activity schema changed across
# releases.
#
# Metric        item field  Columns in known releases (base 0)
#			    9.0  9.1  9.2  9.4
# ...datid		 0    0    0    0    0
# ...datname		 1    1    1    1    1
# n/a (pid)		 -    2    2    2    2
# ...usesysid		 3    3    3    3    3
# ...usename		 4    4    4    4    4
# ...application_name	 5    5    5    5    5
# ...client_addr	 6    6    6    6    6
# ...client_hostname	 7    -    7    7    7
# ...client_port	 8    7    8    8    8
# ...backend_start	 9    8    9    9    9
# ...xact_start		10    9   10   10   10
# ...query_start	11   10   11   11   11
# n/a (state_change)	 -    -    -   12   12
# ...waiting		12   11   12   13   13
# n/a (state)		 -    -    -   14   14
# n/a (backend_xid)	 -    -    -    -   15
# n/a (backend_xmin)	 -    -    -    -   16
# ...current_query	13   12   13   15   17
#
# 9.3 is the same as 9.2
# 9.5 is the same as 9.4

# one map per version with a different schema
# one map entry for each item in a PMID for the activity cluster, so
# 0 .. 13
# each map entry specifies the corresponding column from the
# pg_stat_activity table
#
# 9.0 does not have client_hostname
my @activity_map_9_0 = ( 0, 1, -1, 3, 4, 5, 6, -1, 7, 8, 9, 10, 11, 12 );
# 9.1 appears to have been the baseline for the PMDA implementation
my @activity_map_9_1 = ( 0, 1, -1, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13 );
# 9.2 and 9.3 are the same
my @activity_map_9_2 = ( 0, 1, -1, 3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15 );
# 9.4 and 9.5 are the same
my @activity_map_9_4 = ( 0, 1, -1, 3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 17 );
my %activity_ctl = (
    '9.0' => { ncol => 13, map => \@activity_map_9_0 },
    '9.1' => { ncol => 14, map => \@activity_map_9_1 },
    '9.2' => { ncol => 16, map => \@activity_map_9_2 },
    '9.3' => { ncol => 16, map => \@activity_map_9_2 },
    '9.4' => { ncol => 18, map => \@activity_map_9_4 },
    '9.5' => { ncol => 18, map => \@activity_map_9_4 },
);

sub refresh_activity
{
    my $tableref = shift;
    my $result = refresh_results($tableref);
    my %table = %$tableref;

    @process_instances = ();		# refresh indom too
    if (defined($result)) {
	for my $i (0 .. $#{$result}) {	# for each row (instance) returned
	    my $instid = $process_instances[($i*2)] = "$result->[$i][2]";
	    $tableref = $result->[$i];
	    $tableref = cherrypick('pg_stat_activity', \%activity_ctl, $tableref);
	    $process_instances[($i*2)+1] = "$tableref->[2] $tableref->[5]";
	    $table{values}{$instid} = $tableref;
	}
    }
    $pmda->replace_indom($process_indom, \@process_instances);
}

my $replication_report_bad = 1;	# report unexpected number of values only once

sub refresh_replication
{
    my $tableref = shift;
    my $result = refresh_results($tableref);
    my %table = %$tableref;

    @replicant_instances = ();		# refresh indom too
    if (defined($result)) {
	for my $i (0 .. $#{$result}) {	# for each row (instance) returned
	    my $instid = $replicant_instances[($i*2)] = "$result->[$i][2]";
	    if ($#{$result->[$i]}+1 != 16 && $replication_report_bad == 1) {
		my $ncol = $#{$result->[$i]}+1;
		$pmda->log("pg_stat_replication: version $version: fetched $ncol columns, expecting 16");
		$replication_report_bad = 0;
	    }
	    $replicant_instances[($i*2)+1] = "$result->[$i][2] $result->[$i][5]";
	    # special case needed for 'client_*' columns (4 -> 5)
	    for my $j (4 .. 5) {	# for each special case column
		$result->[$i][$j] = '' unless (defined($result->[$i][$j]));
	    }
	    $table{values}{$instid} = $result->[$i];
	}
    }
    $pmda->replace_indom($replicant_indom, \@replicant_instances);
}

# Need mapping here because the pg_stat_bgwriter schema changed across
# releases.
#
# Metric        	item field  Columns in known releases (base 0)
#				    9.0  9.1  9.2
# ...checkpoints_timed		 0    0    0    0
# ...checkpoints_req		 1    1    1    1
# n/a (checkpoint_write_time)	 -    -    -    2
# n/a (checkpoint_sync_time)	 -    -    -    3
# ...buffers_checkpoint		 2    2    2    4
# ...buffers_clean		 3    3    3    5
# ...maxwritten_clean		 4    4    4    6
# ...buffers_backend		 5    -    5    7
# n/a (buffers_backend_fsync)	 -    -    6    8
# ...buffers_alloc		 6    8    7    9
# n/a (stats_reset)		 -    -    8   10
#
# 9.3, 9.4 and 9.5 are the same as 9.2

# one map per version with a different schema
# one map entry for each item in a PMID for the bgwriter cluster, so
# 0 .. 6
# each map entry specifies the corresponding column from the
# pg_stat_bgwriter table
#
my @bgwriter_map_9_0 = ( 0, 1, 2, 3, 4, -1, 8 );
# 9.1 appears to have been the baseline for the PMDA implementation
my @bgwriter_map_9_1 = ( 0, 1, 2, 3, 4, 5, 7 );
my @bgwriter_map_9_2 = ( 0, 1, 4, 5, 6, 7, 9 );
my %bgwriter_ctl = (
    '9.0' => { ncol => 7, map => \@bgwriter_map_9_0 },
    '9.1' => { ncol => 9, map => \@bgwriter_map_9_1 },
    '9.2' => { ncol => 11, map => \@bgwriter_map_9_2 },
    '9.3' => { ncol => 11, map => \@bgwriter_map_9_2 },
    '9.4' => { ncol => 11, map => \@bgwriter_map_9_2 },
    '9.5' => { ncol => 11, map => \@bgwriter_map_9_2 },
);

sub refresh_bgwriter
{
    my $tableref = shift;
    my $result = refresh_results($tableref);
    my %table = %$tableref;

    if (defined($result)) {
	# only one row returned
	$tableref = $result->[0];
	$tableref = cherrypick('pg_stat_bgwriter', \%bgwriter_ctl, $tableref);
	$table{values}{"$pm_in_null"} = $tableref;
    }
}

sub refresh_active_functions
{
    my $tableref = shift;
    my $result = refresh_results($tableref);
    my %table = %$tableref;

    # Results from SQL (2 values; 2nd may well contain undef):
    # select pg_is_in_recovery(), pg_current_xlog_location(),
    # We need to split out the log identifier and log record offset in the
    # final value, and save separately into the cached result table.
    #
    if (defined($result)) {
	my $arrayref = $result->[0];
	my @values = @$arrayref;
	my @massaged = ( $values[0], undef,undef );
	my $moffset = 1;		# index for @massaged array

	if (defined($values[1])) {
	    my @pieces = split /\//, $values[1];
	    $massaged[$moffset++] = hex($pieces[0]);
	    $massaged[$moffset++] = hex($pieces[1]);
	}
	$table{values}{"$pm_in_null"} = \@massaged;
    }
}

sub refresh_recovery_functions
{
    my $tableref = shift;
    my $result = refresh_results($tableref);
    my %table = %$tableref;

    # Results from SQL (3 values; 3rd,4th may well contain undef):
    # select pg_is_in_recovery(),
    #        pg_last_xlog_receive_location(), pg_last_xlog_replay_location()
    # We need to split out the log identifier and log record offset in the
    # final values, and save separately into the cached result table.
    #
    if (defined($result)) {
	my $arrayref = $result->[0];
	my @values = @$arrayref;
	my @massaged = ( $values[0], undef,undef, undef,undef );
	my $moffset = 1;		# index for @massaged array

	foreach my $voffset (1..2) {	# index for @values array
	    if (defined($values[$voffset])) {
		my @pieces = split /\//, $values[$voffset];
		$massaged[$moffset++] = hex($pieces[0]);
		$massaged[$moffset++] = hex($pieces[1]);
	    }
	}
	$table{values}{"$pm_in_null"} = \@massaged;
    }
}

# Need mapping here because the pg_stat_database schema changed across
# releases.
#
# Metric        item field  Columns in known releases (base 0)
#			    9.0  9.1  9.2
# n/a (datid)		 -    0    0    0
# n/a (datname)		 -    1    1    1	- instance id
# ...numbackends	 2    2    2    2
# ...xact_commit	 3    3    3    3
# ...xact_rollback	 4    4    4    4
# ...blks_read		 5    5    5    4
# ...blks_hit		 6    6    6    4
# ...tup_returned	 7    7    7    4
# ...tup_fetched	 8    8    8    4
# ...tup_inserted	 9    9    9    4
# ...tup_updated	10   10   10   10
# ...tup_deleted	11   11   11   11
# ...conflicts		12    -   12   12
# n/a (temp_files)	 -    -    -   13
# n/a (temp_bytes)	 -    -    -   14
# n/a (deadlocks)	 -    -    -   15
# n/a (blk_read_time)	 -    -    -   16
# n/a (blk_write_time)	 -    -    -   17
# ...stats_reset	13    -   13   18

#
# 9.3, 9.4 and 9.5 all the same as 9.2

# one map per version with a different schema
# one map entry for each item in a PMID for the database cluster, so
# 0 .. 13
# each map entry specifies the corresponding column from the
# pg_stat_database table
#
my @database_map_9_0 = ( 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, -1, -1 );
# 9.1 appears to have been the baseline for the PMDA implementation
my @database_map_9_1 = ( 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13 );
my @database_map_9_2 = ( 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 18 );
my %database_ctl = (
    '9.0' => { ncol => 12, map => \@database_map_9_0 },
    '9.1' => { ncol => 14, map => \@database_map_9_1 },
    '9.2' => { ncol => 19, map => \@database_map_9_2 },
    '9.3' => { ncol => 19, map => \@database_map_9_2 },
    '9.4' => { ncol => 19, map => \@database_map_9_2 },
    '9.5' => { ncol => 19, map => \@database_map_9_2 },
);

sub refresh_database
{
    my $tableref = shift;
    my $result = refresh_results($tableref);
    my %table = %$tableref;

    @database_instances = ();		# refresh indom too
    if (defined($result)) {
	for my $i (0 .. $#{$result}) {	# for each row (instance) returned
	    my $instid = $database_instances[($i*2)] = $result->[$i][0];
	    $tableref = $result->[$i];
	    $tableref = cherrypick('pg_stat_database', \%database_ctl, $tableref);
	    $database_instances[($i*2)+1] = $result->[$i][1];
	    $table{values}{$instid} = $tableref;
	}
    }
    $pmda->replace_indom($database_indom, \@database_instances);
}

my $database_conflicts_report_bad = 1;	# report unexpected number of values only once

sub refresh_database_conflicts
{
    my $tableref = shift;
    my $result = refresh_results($tableref);
    my %table = %$tableref;

    @database_instances = ();		# refresh indom too
    if (defined($result)) {
	for my $i (0 .. $#{$result}) {	# for each row (instance) returned
	    my $instid = $database_instances[($i*2)] = $result->[$i][0];
	    if ($#{$result->[$i]}+1 != 7 && $database_conflicts_report_bad == 1) {
		my $ncol = $#{$result->[$i]}+1;
		$pmda->log("pg_stat_database_conflicts: version $version: fetched $ncol columns, expecting 7");
		$database_conflicts_report_bad = 0;
	    }
	    $database_instances[($i*2)+1] = $result->[$i][1];
	    $table{values}{$instid} = $result->[$i];
	}
    }
    $pmda->replace_indom($database_indom, \@database_instances);
}

my $user_functions_report_bad = 1;	# report unexpected number of values only once

sub refresh_user_functions
{
    my $tableref = shift;
    my $result = refresh_results($tableref);
    my %table = %$tableref;

    @function_instances = ();		# refresh indom too
    if (defined($result)) {
	for my $i (0 .. $#{$result}) {	# for each row (instance) returned
	    my $instid = $function_instances[($i*2)] = $result->[$i][0];
	    if ($#{$result->[$i]}+1 != 13 && $user_functions_report_bad == 1) {
		my $ncol = $#{$result->[$i]}+1;
		$pmda->log("pg_stat_user_functions: version $version: fetched $ncol columns, expecting 13");
		$user_functions_report_bad = 0;
	    }
	    $function_instances[($i*2)+1] = $result->[$i][2];
	    $table{values}{$instid} = $result->[$i];
	}
    }
    $pmda->replace_indom($function_indom, \@function_instances);
}

my $xact_user_functions_report_bad = 1;	# report unexpected number of values only once

sub refresh_xact_user_functions
{
    my $tableref = shift;
    my $result = refresh_results($tableref);
    my %table = %$tableref;

    @function_instances = ();		# refresh indom too
    if (defined($result)) {
	for my $i (0 .. $#{$result}) {	# for each row (instance) returned
	    my $instid = $function_instances[($i*2)] = $result->[$i][0];
	    if ($#{$result->[$i]}+1 != 13 && $xact_user_functions_report_bad == 1) {
		my $ncol = $#{$result->[$i]}+1;
		$pmda->log("pg_stat_xact_user_functions: version $version: fetched $ncol columns, expecting 13");
		$xact_user_functions_report_bad = 0;
	    }
	    $function_instances[($i*2)+1] = $result->[$i][2];
	    $table{values}{$instid} = $result->[$i];
	}
    }
    $pmda->replace_indom($function_indom, \@function_instances);
}

# Need mapping here because the pg_stat_all_tables schema changed across
# releases.
#
# Metric        item field  Columns in known releases (base 0)
#			    9.0  9.1  9.4
# n/a (relid)		 -    0    0    0	- instance id
# ...schemaname 	 1    1    1    1
# n/a (relname)		 -    2    2    2	- instance name
# ...seq_scan   	 3    3    3    3
# ...seq_tup_read        4    4    4    4
# ...idx_scan   	 5    5    5    5
# ...idx_tup_fetch       6    6    6    6
# ...n_tup_ins  	 7    7    7    7
# ...n_tup_upd  	 8    8    8    8
# ...n_tup_del  	 9    9    9    9
# ...n_tup_hot_upd      10   10   10   10
# ...n_live_tup 	11   11   11   11
# ...n_dead_tup 	12   12   12   12
# n/a (n_mod_since_analyze) - -    -   13
# ...last_vacuum        13   13   13   14
# ...last_autovacuum    14   14   14   15
# ...last_analyze       15   15   15   16
# ...last_autoanalyze   16   16   16   17
# n/a (vacuum_count)	 -    -   17   18
# n/a (autovacuum_count) -    -   18   19
# n/a (analyze_count)	 -    -   19   20
# n/a (autoanalyze_count)-    -   20   21
#
# 9.2 and 9.3 are the same as 9.1
# 9.5 is the same as 9.4

# one map per version with a different schema
# one map entry for each item in a PMID for the all_tables cluster, so
# 0 .. 16
# each map entry specifies the corresponding column from the
# pg_stat_all_tables table
#
my @all_tables_map_9_0 = ( 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16 );
my @all_tables_map_9_1 = ( 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16 );
my @all_tables_map_9_4 = ( 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 14, 15, 16, 17 );
my %all_tables_ctl = (
    '9.0' => { ncol => 17, map => \@all_tables_map_9_0 },
    '9.1' => { ncol => 21, map => \@all_tables_map_9_1 },
    '9.2' => { ncol => 21, map => \@all_tables_map_9_1 },
    '9.3' => { ncol => 21, map => \@all_tables_map_9_1 },
    '9.4' => { ncol => 22, map => \@all_tables_map_9_4 },
    '9.5' => { ncol => 22, map => \@all_tables_map_9_4 },
);

sub refresh_all_tables
{
    my $tableref = shift;
    my $result = refresh_results($tableref);
    my %table = %$tableref;

    @all_rel_instances = ();		# refresh indom too
    if (defined($result)) {
	for my $i (0 .. $#{$result}) {	# for each row (instance) returned
	    my $instid = $all_rel_instances[($i*2)] = $result->[$i][0];
	    $tableref = $result->[$i];
	    $tableref = cherrypick('pg_stat_all_tables', \%all_tables_ctl, $tableref);
	    # we've seen some strange null values in fields that should be
	    # integers, so translate '' to 0
	    for my $j (3 .. 16) {	# for each numeric metric
		if ($tableref->[$j] eq '') { $tableref->[$j] = 0; }
	    }
	    $all_rel_instances[($i*2)+1] = $tableref->[2];
	    $table{values}{$instid} = $tableref;
	}
    }
    $pmda->replace_indom($all_rel_indom, \@all_rel_instances);
}

sub refresh_sys_tables
{
    my $tableref = shift;
    my $result = refresh_results($tableref);
    my %table = %$tableref;

    @sys_rel_instances = ();		# refresh indom too
    if (defined($result)) {
	for my $i (0 .. $#{$result}) {	# for each row (instance) returned
	    my $instid = $sys_rel_instances[($i*2)] = $result->[$i][0];
	    $tableref = $result->[$i];
	    # Schema for pg_stat_sys_tables follows pg_stat_all_tables
	    #
	    $tableref = cherrypick('pg_stat_sys_tables', \%all_tables_ctl, $tableref);
	    # we've seen some strange null values in fields that should be
	    # integers, so translate '' to 0
	    for my $j (3 .. 16) {	# for each numeric metric
		if ($tableref->[$j] eq '') { $tableref->[$j] = 0; }
	    }
	    $sys_rel_instances[($i*2)+1] = $tableref->[2];
	    $table{values}{$instid} = $tableref;
	}
    }
    $pmda->replace_indom($sys_rel_indom, \@sys_rel_instances);
}

sub refresh_user_tables
{
    my $tableref = shift;
    my $result = refresh_results($tableref);
    my %table = %$tableref;

    @user_rel_instances = ();		# refresh indom too
    if (defined($result)) {
	for my $i (0 .. $#{$result}) {	# for each row (instance) returned
	    my $instid = $user_rel_instances[($i*2)] = $result->[$i][0];
	    $tableref = $result->[$i];
	    # Schema for pg_stat_user_tables follows pg_stat_all_tables
	    #
	    $tableref = cherrypick('pg_stat_user_tables', \%all_tables_ctl, $tableref);
	    # we've seen some strange null values in fields that should be
	    # integers, so translate '' to 0
	    for my $j (3 .. 16) {	# for each numeric metric
		if ($tableref->[$j] eq '') { $tableref->[$j] = 0; }
	    }
	    $user_rel_instances[($i*2)+1] = $tableref->[2];
	    $table{values}{$instid} = $tableref;
	}
    }
    $pmda->replace_indom($user_rel_indom, \@user_rel_instances);
}

my $xact_all_tables_report_bad = 1;	# report unexpected number of values only once

sub refresh_xact_all_tables
{
    my $tableref = shift;
    my $result = refresh_results($tableref);
    my %table = %$tableref;

    @all_rel_instances = ();		# refresh indom too
    if (defined($result)) {
	for my $i (0 .. $#{$result}) {	# for each row (instance) returned
	    my $instid = $all_rel_instances[($i*2)] = $result->[$i][0];
	    if ($#{$result->[$i]}+1 != 11 && $xact_all_tables_report_bad == 1) {
		my $ncol = $#{$result->[$i]}+1;
		$pmda->log("pg_stat_xact_all_tables: version $version: fetched $ncol columns, expecting 11");
		$xact_all_tables_report_bad = 0;
	    }
	    $all_rel_instances[($i*2)+1] = $result->[$i][2];
	    $table{values}{$instid} = $result->[$i];
	}
    }
    $pmda->replace_indom($all_rel_indom, \@all_rel_instances);
}

my $xact_sys_tables_report_bad = 1;	# report unexpected number of values only once

sub refresh_xact_sys_tables
{
    my $tableref = shift;
    my $result = refresh_results($tableref);
    my %table = %$tableref;

    @sys_rel_instances = ();		# refresh indom too
    if (defined($result)) {
	for my $i (0 .. $#{$result}) {	# for each row (instance) returned
	    my $instid = $sys_rel_instances[($i*2)] = $result->[$i][0];
	    if ($#{$result->[$i]}+1 != 11 && $xact_sys_tables_report_bad == 1) {
		my $ncol = $#{$result->[$i]}+1;
		$pmda->log("pg_stat_xact_sys_tables: version $version: fetched $ncol columns, expecting 11");
		$xact_sys_tables_report_bad = 0;
	    }
	    $sys_rel_instances[($i*2)+1] = $result->[$i][2];
	    $table{values}{$instid} = $result->[$i];
	}
    }
    $pmda->replace_indom($sys_rel_indom, \@sys_rel_instances);
}

my $xact_user_tables_report_bad = 1;	# report unexpected number of values only once

sub refresh_xact_user_tables
{
    my $tableref = shift;
    my $result = refresh_results($tableref);
    my %table = %$tableref;

    @user_rel_instances = ();		# refresh indom too
    if (defined($result)) {
	for my $i (0 .. $#{$result}) {	# for each row (instance) returned
	    my $instid = $user_rel_instances[($i*2)] = $result->[$i][0];
	    if ($#{$result->[$i]}+1 != 11 && $xact_user_tables_report_bad == 1) {
		my $ncol = $#{$result->[$i]}+1;
		$pmda->log("pg_stat_xact_user_tables: version $version: fetched $ncol columns, expecting 11");
		$xact_user_tables_report_bad = 0;
	    }
	    $user_rel_instances[($i*2)+1] = $result->[$i][2];
	    $table{values}{$instid} = $result->[$i];
	}
    }
    $pmda->replace_indom($user_rel_indom, \@user_rel_instances);
}

my $all_indexes_report_bad = 1;	# report unexpected number of values only once

# TODO 8 in 9.5, 7 in 9.1

sub refresh_all_indexes
{
    my $tableref = shift;
    my $result = refresh_results($tableref);
    my %table = %$tableref;

    @all_index_instances = ();		# refresh indom too
    if (defined($result)) {
	for my $i (0 .. $#{$result}) {	# for each row (instance) returned
	    my $instid = $all_index_instances[($i*2)] = $result->[$i][1];
	    if ($#{$result->[$i]}+1 != 8 && $all_indexes_report_bad == 1) {
		my $ncol = $#{$result->[$i]}+1;
		$pmda->log("pg_stat_all_indexes: version $version: fetched $ncol columns, expecting 8");
		$all_indexes_report_bad = 0;
	    }
	    $all_index_instances[($i*2)+1] = $result->[$i][4];
	    $table{values}{$instid} = $result->[$i];
	}
    }
    $pmda->replace_indom($all_index_indom, \@all_index_instances);
}

my $sys_indexes_report_bad = 1;	# report unexpected number of values only once

sub refresh_sys_indexes
{
    my $tableref = shift;
    my $result = refresh_results($tableref);
    my %table = %$tableref;

    @sys_index_instances = ();		# refresh indom too
    if (defined($result)) {
	for my $i (0 .. $#{$result}) {	# for each row (instance) returned
	    my $instid = $sys_index_instances[($i*2)] = $result->[$i][1];
	    if ($#{$result->[$i]}+1 != 8 && $sys_indexes_report_bad == 1) {
		my $ncol = $#{$result->[$i]}+1;
		$pmda->log("pg_stat_sys_indexes: version $version: fetched $ncol columns, expecting 8");
		$sys_indexes_report_bad = 0;
	    }
	    $sys_index_instances[($i*2)+1] = $result->[$i][4];
	    $table{values}{$instid} = $result->[$i];
	}
    }
    $pmda->replace_indom($sys_index_indom, \@sys_index_instances);
}

my $user_indexes_report_bad = 1;	# report unexpected number of values only once

sub refresh_user_indexes
{
    my $tableref = shift;
    my $result = refresh_results($tableref);
    my %table = %$tableref;

    @user_index_instances = ();		# refresh indom too
    if (defined($result)) {
	for my $i (0 .. $#{$result}) {	# for each row (instance) returned
	    my $instid = $user_index_instances[($i*2)] = $result->[$i][1];
	    if ($#{$result->[$i]}+1 != 8 && $user_indexes_report_bad == 1) {
		my $ncol = $#{$result->[$i]}+1;
		$pmda->log("pg_stat_user_indexes: version $version: fetched $ncol columns, expecting 8");
		$user_indexes_report_bad = 0;
	    }
	    $user_index_instances[($i*2)+1] = $result->[$i][4];
	    $table{values}{$instid} = $result->[$i];
	}
    }
    $pmda->replace_indom($user_index_indom, \@user_index_instances);
}

my $io_all_tables_report_bad = 1;	# report unexpected number of values only once

sub refresh_io_all_tables
{
    my $tableref = shift;
    my $result = refresh_results($tableref);
    my %table = %$tableref;

    @all_rel_instances = ();		# refresh indom too
    if (defined($result)) {
	for my $i (0 .. $#{$result}) {	# for each row (instance) returned
	    my $instid = $all_rel_instances[($i*2)] = $result->[$i][0];
	    if ($#{$result->[$i]}+1 != 11 && $io_all_tables_report_bad == 1) {
		my $ncol = $#{$result->[$i]}+1;
		$pmda->log("pg_statio_all_tables: version $version: fetched $ncol columns, expecting 11");
		$io_all_tables_report_bad = 0;
	    }
	    $all_rel_instances[($i*2)+1] = $result->[$i][2];
	    $table{values}{$instid} = $result->[$i];
	}
    }
    $pmda->replace_indom($all_rel_indom, \@all_rel_instances);
}

my $io_sys_tables_report_bad = 1;	# report unexpected number of values only once

sub refresh_io_sys_tables
{
    my $tableref = shift;
    my $result = refresh_results($tableref);
    my %table = %$tableref;

    @sys_rel_instances = ();		# refresh indom too
    if (defined($result)) {
	for my $i (0 .. $#{$result}) {	# for each row (instance) returned
	    my $instid = $sys_rel_instances[($i*2)] = $result->[$i][0];
	    if ($#{$result->[$i]}+1 != 11 && $io_sys_tables_report_bad == 1) {
		my $ncol = $#{$result->[$i]}+1;
		$pmda->log("pg_statio_sys_tables: version $version: fetched $ncol columns, expecting 11");
		$io_sys_tables_report_bad = 0;
	    }
	    $sys_rel_instances[($i*2)+1] = $result->[$i][2];
	    $table{values}{$instid} = $result->[$i];
	}
    }
    $pmda->replace_indom($sys_rel_indom, \@sys_rel_instances);
}

my $io_user_tables_report_bad = 1;	# report unexpected number of values only once

sub refresh_io_user_tables
{
    my $tableref = shift;
    my $result = refresh_results($tableref);
    my %table = %$tableref;

    @user_rel_instances = ();		# refresh indom too
    if (defined($result)) {
	for my $i (0 .. $#{$result}) {	# for each row (instance) returned
	    my $instid = $user_rel_instances[($i*2)] = $result->[$i][0];
	    if ($#{$result->[$i]}+1 != 11 && $io_user_tables_report_bad == 1) {
		my $ncol = $#{$result->[$i]}+1;
		$pmda->log("pg_statio_user_tables: version $version: fetched $ncol columns, expecting 11");
		$io_user_tables_report_bad = 0;
	    }
	    $user_rel_instances[($i*2)+1] = $result->[$i][2];
	    $table{values}{$instid} = $result->[$i];
	}
    }
    $pmda->replace_indom($user_rel_indom, \@user_rel_instances);
}

my $io_all_indexes_report_bad = 1;	# report unexpected number of values only once

sub refresh_io_all_indexes
{
    my $tableref = shift;
    my $result = refresh_results($tableref);
    my %table = %$tableref;

    @all_index_instances = ();		# refresh indom too
    if (defined($result)) {
	for my $i (0 .. $#{$result}) {	# for each row (instance) returned
	    my $instid = $all_index_instances[($i*2)] = $result->[$i][1];
	    if ($#{$result->[$i]}+1 != 7 && $io_all_indexes_report_bad == 1) {
		my $ncol = $#{$result->[$i]}+1;
		$pmda->log("pg_statio_all_indexes: version $version: fetched $ncol columns, expecting 7");
		$io_all_indexes_report_bad = 0;
	    }
	    $all_index_instances[($i*2)+1] = $result->[$i][4];
	    $table{values}{$instid} = $result->[$i];
	}
    }
    $pmda->replace_indom($all_index_indom, \@all_index_instances);
}

my $io_sys_indexes_report_bad = 1;	# report unexpected number of values only once

sub refresh_io_sys_indexes
{
    my $tableref = shift;
    my $result = refresh_results($tableref);
    my %table = %$tableref;

    @sys_index_instances = ();		# refresh indom too
    if (defined($result)) {
	for my $i (0 .. $#{$result}) {	# for each row (instance) returned
	    my $instid = $sys_index_instances[($i*2)] = $result->[$i][1];
	    if ($#{$result->[$i]}+1 != 13 && $io_sys_indexes_report_bad == 1) {
		my $ncol = $#{$result->[$i]}+1;
		$pmda->log("pg_statio_sys_indexes: version $version: fetched $ncol columns, expecting 13");
		$io_sys_indexes_report_bad = 0;
	    }
	    $sys_index_instances[($i*2)+1] = $result->[$i][4];
	    $table{values}{$instid} = $result->[$i];
	}
    }
    $pmda->replace_indom($sys_index_indom, \@sys_index_instances);
}

my $io_user_indexes_report_bad = 1;	# report unexpected number of values only once

sub refresh_io_user_indexes
{
    my $tableref = shift;
    my $result = refresh_results($tableref);
    my %table = %$tableref;

    @user_index_instances = ();		# refresh indom too
    if (defined($result)) {
	for my $i (0 .. $#{$result}) {	# for each row (instance) returned
	    my $instid = $user_index_instances[($i*2)] = $result->[$i][1];
	    if ($#{$result->[$i]}+1 != 13 && $io_user_indexes_report_bad == 1) {
		my $ncol = $#{$result->[$i]}+1;
		$pmda->log("pg_statio_user_indexes: version $version: fetched $ncol columns, expecting 13");
		$io_user_indexes_report_bad = 0;
	    }
	    $user_index_instances[($i*2)+1] = $result->[$i][4];
	    $table{values}{$instid} = $result->[$i];
	}
    }
    $pmda->replace_indom($user_index_indom, \@user_index_instances);
}

my $io_all_sequences_report_bad = 1;	# report unexpected number of values only once

sub refresh_io_all_sequences
{
    my $tableref = shift;
    my $result = refresh_results($tableref);
    my %table = %$tableref;

    @all_seq_instances = ();		# refresh indom too
    if (defined($result)) {
	for my $i (0 .. $#{$result}) {	# for each row (instance) returned
	    my $instid = $all_seq_instances[($i*2)] = $result->[$i][0];
	    if ($#{$result->[$i]}+1 != 13 && $io_all_sequences_report_bad == 1) {
		my $ncol = $#{$result->[$i]}+1;
		$pmda->log("pg_statio_all_sequences: version $version: fetched $ncol columns, expecting 13");
		$io_all_sequences_report_bad = 0;
	    }
	    $all_seq_instances[($i*2)+1] = $result->[$i][2];
	    $table{values}{$instid} = $result->[$i];
	}
    }
    $pmda->replace_indom($all_seq_indom, \@all_seq_instances);
}

my $io_sys_sequences_report_bad = 1;	# report unexpected number of values only once

sub refresh_io_sys_sequences
{
    my $tableref = shift;
    my $result = refresh_results($tableref);
    my %table = %$tableref;

    @sys_seq_instances = ();		# refresh indom too
    if (defined($result)) {
	for my $i (0 .. $#{$result}) {	# for each row (instance) returned
	    my $instid = $sys_seq_instances[($i*2)] = $result->[$i][0];
	    if ($#{$result->[$i]}+1 != 13 && $io_sys_sequences_report_bad == 1) {
		my $ncol = $#{$result->[$i]}+1;
		$pmda->log("pg_statio_sys_sequences: version $version: fetched $ncol columns, expecting 13");
		$io_sys_sequences_report_bad = 0;
	    }
	    $sys_seq_instances[($i*2)+1] = $result->[$i][2];
	    $table{values}{$instid} = $result->[$i];
	}
    }
    $pmda->replace_indom($sys_seq_indom, \@sys_seq_instances);
}

my $io_user_sequences_report_bad = 1;	# report unexpected number of values only once

sub refresh_io_user_sequences
{
    my $tableref = shift;
    my $result = refresh_results($tableref);
    my %table = %$tableref;

    @user_seq_instances = ();		# refresh indom too
    if (defined($result)) {
	for my $i (0 .. $#{$result}) {	# for each row (instance) returned
	    my $instid = $user_seq_instances[($i*2)] = $result->[$i][0];
	    if ($#{$result->[$i]}+1 != 13 && $io_user_sequences_report_bad == 1) {
		my $ncol = $#{$result->[$i]}+1;
		$pmda->log("pg_statio_user_sequences: version $version: fetched $ncol columns, expecting 13");
		$io_user_sequences_report_bad = 0;
	    }
	    $user_seq_instances[($i*2)+1] = $result->[$i][2];
	    $table{values}{$instid} = $result->[$i];
	}
    }
    $pmda->replace_indom($user_seq_indom, \@user_seq_instances);
}

#
# Setup routines - one per cluster, add metrics to PMDA
# 
# For help text, see
# http://www.postgresql.org/docs/9.2/static/monitoring-stats.html
#
# and the Postgres Licence:
# Portions Copyright (c) 1996-2012, The PostgreSQL Global Development Group
# 
# Portions Copyright (c) 1994, The Regents of the University of California
# 
# Permission to use, copy, modify, and distribute this software
# and its documentation for any purpose, without fee, and without a
# written agreement is hereby granted, provided that the above copyright
# notice and this paragraph and the following two paragraphs appear in
# all copies.
#
# IN NO EVENT SHALL THE UNIVERSITY OF CALIFORNIA BE LIABLE TO ANY PARTY
# FOR DIRECT, INDIRECT, SPECIAL, INCIDENTAL, OR CONSEQUENTIAL DAMAGES,
# INCLUDING LOST PROFITS, ARISING OUT OF THE USE OF THIS SOFTWARE AND
# ITS DOCUMENTATION, EVEN IF THE UNIVERSITY OF CALIFORNIA HAS BEEN
# ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
# 
# THE UNIVERSITY OF CALIFORNIA SPECIFICALLY DISCLAIMS ANY
# WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
# MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE. THE SOFTWARE
# PROVIDED HEREUNDER IS ON AN "AS IS" BASIS, AND THE UNIVERSITY
# OF CALIFORNIA HAS NO OBLIGATIONS TO PROVIDE MAINTENANCE, SUPPORT,
# UPDATES, ENHANCEMENTS, OR MODIFICATIONS
#

sub setup_activity
{
    my ($cluster, $indom) = @_;

    # indom: procpid + application_name
    $pmda->add_metric(pmda_pmid($cluster,0), PM_TYPE_U32, $indom,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'postgresql.stat.activity.datid',
		  'OID of the database this backend is connected to', '');
    $pmda->add_metric(pmda_pmid($cluster,1), PM_TYPE_STRING, $indom,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'postgresql.stat.activity.datname',
		  'Name of the database this backend is connected to', '');
    $pmda->add_metric(pmda_pmid($cluster,3), PM_TYPE_U32, $indom,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'postgresql.stat.activity.usesysid',
		  'OID of the user logged into this backend', '');
    $pmda->add_metric(pmda_pmid($cluster,4), PM_TYPE_STRING, $indom,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'postgresql.stat.activity.usename',
		  'Name of the user logged into this backend', '');
    $pmda->add_metric(pmda_pmid($cluster,5), PM_TYPE_STRING, $indom,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'postgresql.stat.activity.application_name',
		  'Name of the application that is connected to this backend', '');
    $pmda->add_metric(pmda_pmid($cluster,6), PM_TYPE_STRING, $indom,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'postgresql.stat.activity.client_addr',
		  'Client IP address',
'IP address of the client connected to this backend. If this field is null,
it indicates either that the client is connected via a Unix socket on the
server machine or that this is an internal process such as autovacuum.');
    $pmda->add_metric(pmda_pmid($cluster,7), PM_TYPE_STRING, $indom,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'postgresql.stat.activity.client_hostname',
		  'Host name of the connected client',
'Client host name is derived from reverse DNS lookup of
postgresql.stat.activity.client_addr. This field will only be non-null
for IP connections, and only when log_hostname is enabled.');
    $pmda->add_metric(pmda_pmid($cluster,8), PM_TYPE_32, $indom,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'postgresql.stat.activity.client_port',
		  'Client TCP port number',
'TCP port number that the client is using for communication with this
backend.  Value will be -1 if a Unix socket is used.');
    $pmda->add_metric(pmda_pmid($cluster,9), PM_TYPE_STRING, $indom,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'postgresql.stat.activity.backend_start',
		  'Time when this process was started',
'Time when this client process connected to the server.');
    $pmda->add_metric(pmda_pmid($cluster,10), PM_TYPE_STRING, $indom,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'postgresql.stat.activity.xact_start',
		  'Transaction start time',
'Time when this process' . "'" . ' current transaction was started.
The value will be null if no transaction is active. If the
current query is the first of its transaction, value is equal to
postgresql.stat.activity.query_start.');
    $pmda->add_metric(pmda_pmid($cluster,11), PM_TYPE_STRING, $indom,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'postgresql.stat.activity.query_start',
		  'Query start time',
'Time when the currently active query was started, or if state is not
active, when the last query was started');
    $pmda->add_metric(pmda_pmid($cluster,12), PM_TYPE_32, $indom,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'postgresql.stat.activity.waiting',
		  'True if this backend is currently waiting on a lock', '');
    $pmda->add_metric(pmda_pmid($cluster,13), PM_TYPE_STRING, $indom,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'postgresql.stat.activity.current_query',
		  'Most recent query',
'Text of this backend' . "'" . 's most recent query. If state is active this field
shows the currently executing query. In all other states, it shows the
last query that was executed.');
}

sub setup_replication
{
    my ($cluster, $indom) = @_;

    # indom: procpid + application_name
    $pmda->add_metric(pmda_pmid($cluster,1), PM_TYPE_U32, $indom,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'postgresql.stat.replication.usesysid',
		  'OID of the user logged into this WAL sender process', '');
    $pmda->add_metric(pmda_pmid($cluster,2), PM_TYPE_STRING, $indom,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'postgresql.stat.replication.usename',
		  'Name of the user logged into this WAL sender process', '');
    $pmda->add_metric(pmda_pmid($cluster,3), PM_TYPE_STRING, $indom,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'postgresql.stat.replication.application_name',
		  'Name of the application that is connected to this WAL sender', '');
    $pmda->add_metric(pmda_pmid($cluster,4), PM_TYPE_STRING, $indom,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'postgresql.stat.replication.client_addr',
		  'WAL client IP address',
'IP address of the client connected to this WAL sender. If this field is
null, it indicates that the client is connected via a Unix socket on the
server machine.');
    $pmda->add_metric(pmda_pmid($cluster,5), PM_TYPE_STRING, $indom,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'postgresql.stat.replication.client_hostname',
		  'WAL client host name',
'Host name of the connected client, as reported by a reverse DNS lookup of
postgresql.stat.replication.client_addr. This field will only be non-null
for IP connections, and only when log_hostname is enabled.');
    $pmda->add_metric(pmda_pmid($cluster,6), PM_TYPE_32, $indom,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'postgresql.stat.replication.client_port',
		  'WAL client TCP port',
'TCP port number that the client is using for communication with this WAL
sender, or -1 if a Unix socket is used.');
    $pmda->add_metric(pmda_pmid($cluster,7), PM_TYPE_STRING, $indom,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'postgresql.stat.replication.backend_start',
		  'Time when when the client connected to this WAL sender', '');
    $pmda->add_metric(pmda_pmid($cluster,8), PM_TYPE_STRING, $indom,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'postgresql.stat.replication.state',
		  'Current WAL sender state', '');
    $pmda->add_metric(pmda_pmid($cluster,9), PM_TYPE_STRING, $indom,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'postgresql.stat.replication.sent_location',
		  'Last transaction log position sent on this connection', '');
    $pmda->add_metric(pmda_pmid($cluster,10), PM_TYPE_STRING, $indom,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'postgresql.stat.replication.write_location',
		  'Last transaction log position written to disk by this standby server', '');
    $pmda->add_metric(pmda_pmid($cluster,11), PM_TYPE_STRING, $indom,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'postgresql.stat.replication.flush_location',
		  'Last transaction log position flushed to disk by this standby server', '');
    $pmda->add_metric(pmda_pmid($cluster,12), PM_TYPE_STRING, $indom,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'postgresql.stat.replication.replay_location',
		  'Last transaction log position replayed into the database on this standby server', '');
    $pmda->add_metric(pmda_pmid($cluster,13), PM_TYPE_STRING, $indom,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'postgresql.stat.replication.sync_priority',
		  'Priority of this standby server for being chosen as the synchronous standby', '');
    $pmda->add_metric(pmda_pmid($cluster,14), PM_TYPE_STRING, $indom,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'postgresql.stat.replication.sync_state',
		  'Synchronous state of this standby server', '');
}

sub setup_stat_xact_tables
{
    my ($cluster, $indom, $tables) = @_;

    # indom: relid + relname
    $pmda->add_metric(pmda_pmid($cluster,1), PM_TYPE_STRING, $indom,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  "postgresql.stat.xact.$tables.schemaname",
		  'Name of the schema that this table is in',
'');
    $pmda->add_metric(pmda_pmid($cluster,3), PM_TYPE_U32, $indom,
		  PM_SEM_COUNTER, pmda_units(0,0,0,0,0,0),
		  "postgresql.stat.xact.$tables.seq_scan",
		  '', '');
    $pmda->add_metric(pmda_pmid($cluster,4), PM_TYPE_U32, $indom,
		  PM_SEM_COUNTER, pmda_units(0,0,0,0,0,0),
		  "postgresql.stat.xact.$tables.seq_tup_read",
		  '', '');
    $pmda->add_metric(pmda_pmid($cluster,5), PM_TYPE_U32, $indom,
		  PM_SEM_COUNTER, pmda_units(0,0,0,0,0,0),
		  "postgresql.stat.xact.$tables.idx_scan",
		  '', '');
    $pmda->add_metric(pmda_pmid($cluster,6), PM_TYPE_U32, $indom,
		  PM_SEM_COUNTER, pmda_units(0,0,0,0,0,0),
		  "postgresql.stat.xact.$tables.idx_tup_fetch",
		  'Number of rows fetched by queries in this database', '');
    $pmda->add_metric(pmda_pmid($cluster,7), PM_TYPE_U32, $indom,
		  PM_SEM_COUNTER, pmda_units(0,0,0,0,0,0),
		  "postgresql.stat.xact.$tables.n_tup_ins",
		  '', '');
    $pmda->add_metric(pmda_pmid($cluster,8), PM_TYPE_U32, $indom,
		  PM_SEM_COUNTER, pmda_units(0,0,0,0,0,0),
		  "postgresql.stat.xact.$tables.n_tup_upd",
		  '', '');
    $pmda->add_metric(pmda_pmid($cluster,9), PM_TYPE_U32, $indom,
		  PM_SEM_COUNTER, pmda_units(0,0,0,0,0,0),
		  "postgresql.stat.xact.$tables.n_tup_del",
		  '', '');
    $pmda->add_metric(pmda_pmid($cluster,10), PM_TYPE_U32, $indom,
		  PM_SEM_COUNTER, pmda_units(0,0,0,0,0,0),
		  "postgresql.stat.xact.$tables.n_tup_hot_upd",
		  '', '');
}

sub setup_stat_tables
{
    my ($cluster, $indom, $tables) = @_;

    # indom: relid + relname
    $pmda->add_metric(pmda_pmid($cluster,1), PM_TYPE_STRING, $indom,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  "postgresql.stat.$tables.schemaname",
		  '', '');
    $pmda->add_metric(pmda_pmid($cluster,3), PM_TYPE_U32, $indom,
		  PM_SEM_COUNTER, pmda_units(0,0,0,0,0,0),
		  "postgresql.stat.$tables.seq_scan",
		  '', '');
    $pmda->add_metric(pmda_pmid($cluster,4), PM_TYPE_U32, $indom,
		  PM_SEM_COUNTER, pmda_units(0,0,0,0,0,0),
		  "postgresql.stat.$tables.seq_tup_read",
		  '', '');
    $pmda->add_metric(pmda_pmid($cluster,5), PM_TYPE_U32, $indom,
		  PM_SEM_COUNTER, pmda_units(0,0,0,0,0,0),
		  "postgresql.stat.$tables.idx_scan",
		  '', '');
    $pmda->add_metric(pmda_pmid($cluster,6), PM_TYPE_U32, $indom,
		  PM_SEM_COUNTER, pmda_units(0,0,0,0,0,0),
		  "postgresql.stat.$tables.idx_tup_fetch",
		  '', '');
    $pmda->add_metric(pmda_pmid($cluster,7), PM_TYPE_U32, $indom,
		  PM_SEM_COUNTER, pmda_units(0,0,0,0,0,0),
		  "postgresql.stat.$tables.n_tup_ins",
		  '', '');
    $pmda->add_metric(pmda_pmid($cluster,8), PM_TYPE_U32, $indom,
		  PM_SEM_COUNTER, pmda_units(0,0,0,0,0,0),
		  "postgresql.stat.$tables.n_tup_upd",
		  '', '');
    $pmda->add_metric(pmda_pmid($cluster,9), PM_TYPE_U32, $indom,
		  PM_SEM_COUNTER, pmda_units(0,0,0,0,0,0),
		  "postgresql.stat.$tables.n_tup_del",
		  '', '');
    $pmda->add_metric(pmda_pmid($cluster,10), PM_TYPE_U32, $indom,
		  PM_SEM_COUNTER, pmda_units(0,0,0,0,0,0),
		  "postgresql.stat.$tables.n_tup_hot_upd",
		  '', '');
    $pmda->add_metric(pmda_pmid($cluster,11), PM_TYPE_U32, $indom,
		  PM_SEM_COUNTER, pmda_units(0,0,0,0,0,0),
		  "postgresql.stat.$tables.n_live_tup",
		  '', '');
    $pmda->add_metric(pmda_pmid($cluster,12), PM_TYPE_U32, $indom,
		  PM_SEM_COUNTER, pmda_units(0,0,0,0,0,0),
		  "postgresql.stat.$tables.n_dead_tup",
		  '', '');
    $pmda->add_metric(pmda_pmid($cluster,13), PM_TYPE_STRING, $indom,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  "postgresql.stat.$tables.last_vacuum",
		  '', '');
    $pmda->add_metric(pmda_pmid($cluster,14), PM_TYPE_STRING, $indom,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  "postgresql.stat.$tables.last_autovacuum",
		  '', '');
    $pmda->add_metric(pmda_pmid($cluster,15), PM_TYPE_STRING, $indom,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  "postgresql.stat.$tables.last_analyze",
		  '', '');
    $pmda->add_metric(pmda_pmid($cluster,16), PM_TYPE_STRING, $indom,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  "postgresql.stat.$tables.last_autoanalyze",
		  '', '');
}

sub setup_bgwriter
{
    my ($cluster, $indom) = @_;

    $pmda->add_metric(pmda_pmid($cluster,0), PM_TYPE_U32, $indom,
		  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  'postgresql.stat.bgwriter.checkpoints_timed',
		  '', '');
    $pmda->add_metric(pmda_pmid($cluster,1), PM_TYPE_U32, $indom,
		  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  'postgresql.stat.bgwriter.checkpoints_req',
		  '', '');
    $pmda->add_metric(pmda_pmid($cluster,2), PM_TYPE_U32, $indom,
		  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  'postgresql.stat.bgwriter.buffers_checkpoints',
		  '', '');
    $pmda->add_metric(pmda_pmid($cluster,3), PM_TYPE_U32, $indom,
		  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  'postgresql.stat.bgwriter.buffers_clean',
		  '', '');
    $pmda->add_metric(pmda_pmid($cluster,4), PM_TYPE_U32, $indom,
		  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  'postgresql.stat.bgwriter.maxwritten_clean',
		  '', '');
    $pmda->add_metric(pmda_pmid($cluster,5), PM_TYPE_U32, $indom,
		  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  'postgresql.stat.bgwriter.buffers_backend',
		  '', '');
    $pmda->add_metric(pmda_pmid($cluster,6), PM_TYPE_U32, $indom,
		  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  'postgresql.stat.bgwriter.buffers_alloc',
		  '', '');
}

sub setup_active_functions
{
    my ($cluster, $indom) = @_;

    $pmda->add_metric(pmda_pmid($cluster,0), PM_TYPE_U32, $indom,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'postgresql.active.is_in_recovery',
		  '', '');
    $pmda->add_metric(pmda_pmid($cluster,1), PM_TYPE_U64, $indom,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'postgresql.active.xlog_current_location_log_id',
		  '', '');
    $pmda->add_metric(pmda_pmid($cluster,2), PM_TYPE_U64, $indom,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'postgresql.active.xlog_current_location_offset',
		  '', '');
}

sub setup_recovery_functions
{
    my ($cluster, $indom) = @_;

    $pmda->add_metric(pmda_pmid($cluster,0), PM_TYPE_U32, $indom,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'postgresql.recovery.is_in_recovery',
		  '', '');
    $pmda->add_metric(pmda_pmid($cluster,1), PM_TYPE_U64, $indom,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'postgresql.recovery.xlog_receive_location_log_id',
		  '', '');
    $pmda->add_metric(pmda_pmid($cluster,2), PM_TYPE_U64, $indom,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'postgresql.recovery.xlog_receive_location_offset',
		  '', '');
    $pmda->add_metric(pmda_pmid($cluster,3), PM_TYPE_U64, $indom,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'postgresql.recovery.xlog_replay_location_log_id',
		  '', '');
    $pmda->add_metric(pmda_pmid($cluster,4), PM_TYPE_U64, $indom,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'postgresql.recovery.xlog_replay_location_offset',
		  '', '');
}

sub setup_database_conflicts
{
    my ($cluster, $indom) = @_;

    # indom: datid + datname
    $pmda->add_metric(pmda_pmid($cluster,2), PM_TYPE_U32, $indom,
		  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  'postgresql.stat.database_conflicts.tablespace',
		  '', '');
    $pmda->add_metric(pmda_pmid($cluster,3), PM_TYPE_U32, $indom,
		  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  'postgresql.stat.database_conflicts.lock',
		  '', '');
    $pmda->add_metric(pmda_pmid($cluster,4), PM_TYPE_U32, $indom,
		  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  'postgresql.stat.database_conflicts.snapshot',
		  '', '');
    $pmda->add_metric(pmda_pmid($cluster,5), PM_TYPE_U32, $indom,
		  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  'postgresql.stat.database_conflicts.bufferpin',
		  '', '');
    $pmda->add_metric(pmda_pmid($cluster,6), PM_TYPE_U32, $indom,
		  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  'postgresql.stat.database_conflicts.deadlock',
		  '', '');

}
sub setup_database
{
    my ($cluster, $indom) = @_;

    # indom: datid + datname
    $pmda->add_metric(pmda_pmid($cluster,2), PM_TYPE_U32, $indom,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'postgresql.stat.database.numbackends',
		  '', '');
    $pmda->add_metric(pmda_pmid($cluster,3), PM_TYPE_U32, $indom,
		  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  'postgresql.stat.database.xact_commit',
		  '', '');
    $pmda->add_metric(pmda_pmid($cluster,4), PM_TYPE_U32, $indom,
		  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  'postgresql.stat.database.xact_rollback',
		  '', '');
    $pmda->add_metric(pmda_pmid($cluster,5), PM_TYPE_U32, $indom,
		  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  'postgresql.stat.database.blks_read',
		  '', '');
    $pmda->add_metric(pmda_pmid($cluster,6), PM_TYPE_U32, $indom,
		  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  'postgresql.stat.database.blks_hit',
		  '', '');
    $pmda->add_metric(pmda_pmid($cluster,7), PM_TYPE_U32, $indom,
		  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  'postgresql.stat.database.tup_returned',
		  '', '');
    $pmda->add_metric(pmda_pmid($cluster,8), PM_TYPE_U32, $indom,
		  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  'postgresql.stat.database.tup_fetched',
		  '', '');
    $pmda->add_metric(pmda_pmid($cluster,9), PM_TYPE_U32, $indom,
		  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  'postgresql.stat.database.tup_inserted',
		  '', '');
    $pmda->add_metric(pmda_pmid($cluster,10), PM_TYPE_U32, $indom,
		  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  'postgresql.stat.database.tup_updated',
		  '', '');
    $pmda->add_metric(pmda_pmid($cluster,11), PM_TYPE_U32, $indom,
		  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  'postgresql.stat.database.tup_deleted',
		  '', '');
    $pmda->add_metric(pmda_pmid($cluster,12), PM_TYPE_U32, $indom,
		  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  'postgresql.stat.database.conflicts',
		  '', '');
    $pmda->add_metric(pmda_pmid($cluster,13), PM_TYPE_STRING, $indom,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'postgresql.stat.database.stats_reset',
		  '', '');
}

sub setup_stat_indexes
{
    my ($cluster, $indom, $indexes) = @_;

    $pmda->add_metric(pmda_pmid($cluster,0), PM_TYPE_32, $indom,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  "postgresql.stat.$indexes.relid",
		  '', '');
    $pmda->add_metric(pmda_pmid($cluster,2), PM_TYPE_STRING, $indom,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  "postgresql.stat.$indexes.schemaname",
		  '', '');
    $pmda->add_metric(pmda_pmid($cluster,3), PM_TYPE_STRING, $indom,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  "postgresql.stat.$indexes.relname",
		  '', '');
    $pmda->add_metric(pmda_pmid($cluster,5), PM_TYPE_U32, $indom,
		  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  "postgresql.stat.$indexes.idx_scan",
		  '', '');
    $pmda->add_metric(pmda_pmid($cluster,6), PM_TYPE_U32, $indom,
		  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  "postgresql.stat.$indexes.idx_tup_read",
		  '', '');
    $pmda->add_metric(pmda_pmid($cluster,7), PM_TYPE_U32, $indom,
		  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  "postgresql.stat.$indexes.idx_tup_fetch",
		  '', '');
}

sub setup_statio_tables
{
    my ($cluster, $indom, $tables) = @_;

    $pmda->add_metric(pmda_pmid($cluster,1), PM_TYPE_STRING, $indom,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  "postgresql.statio.$tables.schemaname",
		  '', '');
    $pmda->add_metric(pmda_pmid($cluster,3), PM_TYPE_U32, $indom,
		  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  "postgresql.statio.$tables.heap_blks_read",
		  '', '');
    $pmda->add_metric(pmda_pmid($cluster,4), PM_TYPE_U32, $indom,
		  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  "postgresql.statio.$tables.heap_blks_hit",
		  '', '');
    $pmda->add_metric(pmda_pmid($cluster,5), PM_TYPE_U32, $indom,
		  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  "postgresql.statio.$tables.idx_blks_read",
		  '', '');
    $pmda->add_metric(pmda_pmid($cluster,6), PM_TYPE_U32, $indom,
		  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  "postgresql.statio.$tables.idx_blks_hit",
		  '', '');
    $pmda->add_metric(pmda_pmid($cluster,7), PM_TYPE_U32, $indom,
		  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  "postgresql.statio.$tables.toast_blks_read",
		  '', '');
    $pmda->add_metric(pmda_pmid($cluster,8), PM_TYPE_U32, $indom,
		  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  "postgresql.statio.$tables.toast_blks_hit",
		  '', '');
    $pmda->add_metric(pmda_pmid($cluster,9), PM_TYPE_U32, $indom,
		  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  "postgresql.statio.$tables.tidx_blks_read",
		  '', '');
    $pmda->add_metric(pmda_pmid($cluster,10), PM_TYPE_U32, $indom,
		  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  "postgresql.statio.$tables.tidx_blks_hit",
		  '', '');
}

sub setup_statio_indexes
{
    my ($cluster, $indom, $indexes) = @_;

    # indom: indexrelid + indexrelname
    $pmda->add_metric(pmda_pmid($cluster,0), PM_TYPE_32, $indom,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  "postgresql.statio.$indexes.relid",
		  '', '');
    $pmda->add_metric(pmda_pmid($cluster,2), PM_TYPE_STRING, $indom,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  "postgresql.statio.$indexes.schemaname",
		  '', '');
    $pmda->add_metric(pmda_pmid($cluster,4), PM_TYPE_STRING, $indom,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  "postgresql.statio.$indexes.relname",
		  '', '');
    $pmda->add_metric(pmda_pmid($cluster,5), PM_TYPE_U32, $indom,
		  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  "postgresql.statio.$indexes.idx_blks_read",
		  '', '');
    $pmda->add_metric(pmda_pmid($cluster,6), PM_TYPE_U32, $indom,
		  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  "postgresql.statio.$indexes.idx_blks_hit",
		  '', '');
}

sub setup_statio_sequences
{
    my ($cluster, $indom, $sequences) = @_;

    # indom: relid + relname
    $pmda->add_metric(pmda_pmid($cluster,1), PM_TYPE_STRING, $indom,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  "postgresql.statio.$sequences.schemaname",
		  '', '');
    $pmda->add_metric(pmda_pmid($cluster,3), PM_TYPE_U32, $indom,
		  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  "postgresql.statio.$sequences.blks_read",
		  '', '');
    $pmda->add_metric(pmda_pmid($cluster,4), PM_TYPE_U32, $indom,
		  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  "postgresql.statio.$sequences.blks_hit",
		  '', '');
}

sub setup_xact_user_functions
{
    my ($cluster, $indom) = @_;

    # indom: funcid + funcname
    $pmda->add_metric(pmda_pmid($cluster,1), PM_TYPE_STRING, $indom,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'postgresql.stat.xact.user_functions.schemaname',
		  '', '');
    $pmda->add_metric(pmda_pmid($cluster,3), PM_TYPE_U64, $indom,
		  PM_SEM_COUNTER, pmda_units(0,0,0,0,0,0),
		  'postgresql.stat.xact.user_functions.calls',
		  '', '');
    $pmda->add_metric(pmda_pmid($cluster,4), PM_TYPE_U64, $indom,
		  PM_SEM_COUNTER, pmda_units(0,1,0,0,PM_TIME_MSEC,0),
		  'postgresql.stat.xact.user_functions.total_time',
		  '', '');
    $pmda->add_metric(pmda_pmid($cluster,5), PM_TYPE_U64, $indom,
		  PM_SEM_COUNTER, pmda_units(0,1,0,0,PM_TIME_MSEC,0),
		  'postgresql.stat.xact.user_functions.self_time',
		  '', '');
}

sub setup_user_functions
{
    my ($cluster, $indom) = @_;

    # indom: funcid + funcname
    $pmda->add_metric(pmda_pmid($cluster,1), PM_TYPE_STRING, $indom,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'postgresql.stat.user_functions.schemaname',
		  '', '');
    $pmda->add_metric(pmda_pmid($cluster,3), PM_TYPE_U64, $indom,
		  PM_SEM_COUNTER, pmda_units(0,0,0,0,0,0),
		  'postgresql.stat.user_functions.calls',
		  '', '');
    $pmda->add_metric(pmda_pmid($cluster,4), PM_TYPE_U64, $indom,
		  PM_SEM_COUNTER, pmda_units(0,1,0,0,PM_TIME_MSEC,0),
		  'postgresql.stat.user_functions.total_time',
		  '', '');
    $pmda->add_metric(pmda_pmid($cluster,5), PM_TYPE_U64, $indom,
		  PM_SEM_COUNTER, pmda_units(0,1,0,0,PM_TIME_MSEC,0),
		  'postgresql.stat.user_functions.self_time',
		  '', '');
}

#
# Main PMDA thread of execution starts here - setup metrics and callbacks
# drop root privileges, then call PMDA 'run' routine to talk to pmcd.
#
$pmda = PCP::PMDA->new('postgresql', 110);
postgresql_metrics_setup();
postgresql_indoms_setup();

$pmda->set_fetch_callback(\&postgresql_fetch_callback);
$pmda->set_fetch(\&postgresql_connection_setup);
$pmda->set_refresh(\&postgresql_refresh);
if ($os_user eq '') {
    # default is to use $username, but could have been changed by
    # one of the configuration files
    $os_user = $username;
}
if (!defined($ENV{PCP_PERL_PMNS}) && !defined($ENV{PCP_PERL_DOMAIN})) {
    # really running as the PMDA, not setup from Install
    $pmda->log("Change to UID of user \"$os_user\"");
    $pmda->set_user($os_user);
}
$pmda->run;
