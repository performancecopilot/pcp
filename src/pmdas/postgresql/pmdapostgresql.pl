#
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
my $username = 'postgres';
my $password = '';	# DBI parameter, typically unused for postgres

# Configuration files for overriding the above settings
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
	refresh => \&refresh_user_functions }, # identical refresh routine
    '5'	 => {
	name	=> 'pg_stat_database_conflicts',
	setup	=> \&setup_database_conflicts,
	indom	=> $database_indom,
	refresh => \&refresh_database }, # identical refresh routine
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
	$dbh = DBI->connect($database, $username, $password,
			    {AutoCommit => 1, pg_bool_tf => 0});
	if (defined($dbh)) {
	    $pmda->log("PostgreSQL connection established");
	    my $version = postgresql_version_query();

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

    # $pmda->log("fetch_cb $metric_name $cluster:$item ($inst) - $key");

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
	    $process_instances[($i*2)+1] = "$tableref->[2] $tableref->[5]";
	    # 9.0 does not have client_hostname, deal with that first
	    splice @$tableref, 7, 0, (undef) unless (@$tableref > 13);
	    # special case needed for 'client_*' columns (6 -> 8), may be null
	    for my $j (6 .. 8) {	# for each special case column
		$tableref->[$j] = '' unless (defined($tableref->[$j]));
	    }
	    $table{values}{$instid} = $tableref;
	}
    }
    $pmda->replace_indom($process_indom, \@process_instances);
}

sub refresh_replication
{
    my $tableref = shift;
    my $result = refresh_results($tableref);
    my %table = %$tableref;

    @replicant_instances = ();		# refresh indom too
    if (defined($result)) {
	for my $i (0 .. $#{$result}) {	# for each row (instance) returned
	    my $instid = $replicant_instances[($i*2)] = "$result->[$i][2]";
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

sub refresh_bgwriter
{
    my $tableref = shift;
    my $result = refresh_results($tableref);
    my %table = %$tableref;

    $table{values}{"$pm_in_null"} = \@{$result->[0]} unless (!defined($result));
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

sub refresh_database
{
    my $tableref = shift;
    my $result = refresh_results($tableref);
    my %table = %$tableref;

    @database_instances = ();		# refresh indom too
    if (defined($result)) {
	for my $i (0 .. $#{$result}) {	# for each row (instance) returned
	    my $instid = $database_instances[($i*2)] = $result->[$i][0];
	    $database_instances[($i*2)+1] = $result->[$i][1];
	    $table{values}{$instid} = $result->[$i];
	}
    }
    $pmda->replace_indom($database_indom, \@database_instances);
}

sub refresh_user_functions
{
    my $tableref = shift;
    my $result = refresh_results($tableref);
    my %table = %$tableref;

    @function_instances = ();		# refresh indom too
    if (defined($result)) {
	for my $i (0 .. $#{$result}) {	# for each row (instance) returned
	    my $instid = $function_instances[($i*2)] = $result->[$i][0];
	    $function_instances[($i*2)+1] = $result->[$i][2];
	    $table{values}{$instid} = $result->[$i];
	}
    }
    $pmda->replace_indom($function_indom, \@function_instances);
}

sub refresh_all_tables
{
    my $tableref = shift;
    my $result = refresh_results($tableref);
    my %table = %$tableref;

    @all_rel_instances = ();		# refresh indom too
    if (defined($result)) {
	for my $i (0 .. $#{$result}) {	# for each row (instance) returned
	    my $instid = $all_rel_instances[($i*2)] = $result->[$i][0];
	    $all_rel_instances[($i*2)+1] = $result->[$i][2];
	    # special case needed for 'last_*' columns (13 -> 16)
	    for my $j (13 .. 16) {	# for each special case column
		$result->[$i][$j] = '' unless (defined($result->[$i][$j]));
	    }
	    $table{values}{$instid} = $result->[$i];
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
	    $sys_rel_instances[($i*2)+1] = $result->[$i][2];
	    # special case needed for 'last_*' columns (13 -> 16)
	    for my $j (13 .. 16) {	# for each special case column
		$result->[$i][$j] = '' unless (defined($result->[$i][$j]));
	    }
	    $table{values}{$instid} = $result->[$i];
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
	    $user_rel_instances[($i*2)+1] = $result->[$i][2];
	    # special case needed for 'last_*' columns (13 -> 16)
	    for my $j (13 .. 16) {	# for each special case column
		$result->[$i][$j] = '' unless (defined($result->[$i][$j]));
	    }
	    $table{values}{$instid} = $result->[$i];
	}
    }
    $pmda->replace_indom($user_rel_indom, \@user_rel_instances);
}

sub refresh_xact_all_tables
{
    my $tableref = shift;
    my $result = refresh_results($tableref);
    my %table = %$tableref;

    @all_rel_instances = ();		# refresh indom too
    if (defined($result)) {
	for my $i (0 .. $#{$result}) {	# for each row (instance) returned
	    my $instid = $all_rel_instances[($i*2)] = $result->[$i][0];
	    $all_rel_instances[($i*2)+1] = $result->[$i][2];
	    $table{values}{$instid} = $result->[$i];
	}
    }
    $pmda->replace_indom($all_rel_indom, \@all_rel_instances);
}

sub refresh_xact_sys_tables
{
    my $tableref = shift;
    my $result = refresh_results($tableref);
    my %table = %$tableref;

    @sys_rel_instances = ();		# refresh indom too
    if (defined($result)) {
	for my $i (0 .. $#{$result}) {	# for each row (instance) returned
	    my $instid = $sys_rel_instances[($i*2)] = $result->[$i][0];
	    $sys_rel_instances[($i*2)+1] = $result->[$i][2];
	    $table{values}{$instid} = $result->[$i];
	}
    }
    $pmda->replace_indom($sys_rel_indom, \@sys_rel_instances);
}

sub refresh_xact_user_tables
{
    my $tableref = shift;
    my $result = refresh_results($tableref);
    my %table = %$tableref;

    @user_rel_instances = ();		# refresh indom too
    if (defined($result)) {
	for my $i (0 .. $#{$result}) {	# for each row (instance) returned
	    my $instid = $user_rel_instances[($i*2)] = $result->[$i][0];
	    $user_rel_instances[($i*2)+1] = $result->[$i][2];
	    $table{values}{$instid} = $result->[$i];
	}
    }
    $pmda->replace_indom($user_rel_indom, \@user_rel_instances);
}

sub refresh_all_indexes
{
    my $tableref = shift;
    my $result = refresh_results($tableref);
    my %table = %$tableref;

    @all_index_instances = ();		# refresh indom too
    if (defined($result)) {
	for my $i (0 .. $#{$result}) {	# for each row (instance) returned
	    my $instid = $all_index_instances[($i*2)] = $result->[$i][1];
	    $all_index_instances[($i*2)+1] = $result->[$i][4];
	    $table{values}{$instid} = $result->[$i];
	}
    }
    $pmda->replace_indom($all_index_indom, \@all_index_instances);
}

sub refresh_sys_indexes
{
    my $tableref = shift;
    my $result = refresh_results($tableref);
    my %table = %$tableref;

    @sys_index_instances = ();		# refresh indom too
    if (defined($result)) {
	for my $i (0 .. $#{$result}) {	# for each row (instance) returned
	    my $instid = $sys_index_instances[($i*2)] = $result->[$i][1];
	    $sys_index_instances[($i*2)+1] = $result->[$i][4];
	    $table{values}{$instid} = $result->[$i];
	}
    }
    $pmda->replace_indom($sys_index_indom, \@sys_index_instances);
}

sub refresh_user_indexes
{
    my $tableref = shift;
    my $result = refresh_results($tableref);
    my %table = %$tableref;

    @user_index_instances = ();		# refresh indom too
    if (defined($result)) {
	for my $i (0 .. $#{$result}) {	# for each row (instance) returned
	    my $instid = $user_index_instances[($i*2)] = $result->[$i][1];
	    $user_index_instances[($i*2)+1] = $result->[$i][4];
	    $table{values}{$instid} = $result->[$i];
	}
    }
    $pmda->replace_indom($user_index_indom, \@user_index_instances);
}

sub refresh_io_all_tables
{
    my $tableref = shift;
    my $result = refresh_results($tableref);
    my %table = %$tableref;

    @all_rel_instances = ();		# refresh indom too
    if (defined($result)) {
	for my $i (0 .. $#{$result}) {	# for each row (instance) returned
	    my $instid = $all_rel_instances[($i*2)] = $result->[$i][0];
	    $all_rel_instances[($i*2)+1] = $result->[$i][2];
	    $table{values}{$instid} = $result->[$i];
	}
    }
    $pmda->replace_indom($all_rel_indom, \@all_rel_instances);
}

sub refresh_io_sys_tables
{
    my $tableref = shift;
    my $result = refresh_results($tableref);
    my %table = %$tableref;

    @sys_rel_instances = ();		# refresh indom too
    if (defined($result)) {
	for my $i (0 .. $#{$result}) {	# for each row (instance) returned
	    my $instid = $sys_rel_instances[($i*2)] = $result->[$i][0];
	    $sys_rel_instances[($i*2)+1] = $result->[$i][2];
	    $table{values}{$instid} = $result->[$i];
	}
    }
    $pmda->replace_indom($sys_rel_indom, \@sys_rel_instances);
}

sub refresh_io_user_tables
{
    my $tableref = shift;
    my $result = refresh_results($tableref);
    my %table = %$tableref;

    @user_rel_instances = ();		# refresh indom too
    if (defined($result)) {
	for my $i (0 .. $#{$result}) {	# for each row (instance) returned
	    my $instid = $user_rel_instances[($i*2)] = $result->[$i][0];
	    $user_rel_instances[($i*2)+1] = $result->[$i][2];
	    $table{values}{$instid} = $result->[$i];
	}
    }
    $pmda->replace_indom($user_rel_indom, \@user_rel_instances);
}

sub refresh_io_all_indexes
{
    my $tableref = shift;
    my $result = refresh_results($tableref);
    my %table = %$tableref;

    @all_index_instances = ();		# refresh indom too
    if (defined($result)) {
	for my $i (0 .. $#{$result}) {	# for each row (instance) returned
	    my $instid = $all_index_instances[($i*2)] = $result->[$i][1];
	    $all_index_instances[($i*2)+1] = $result->[$i][4];
	    $table{values}{$instid} = $result->[$i];
	}
    }
    $pmda->replace_indom($all_index_indom, \@all_index_instances);
}

sub refresh_io_sys_indexes
{
    my $tableref = shift;
    my $result = refresh_results($tableref);
    my %table = %$tableref;

    @sys_index_instances = ();		# refresh indom too
    if (defined($result)) {
	for my $i (0 .. $#{$result}) {	# for each row (instance) returned
	    my $instid = $sys_index_instances[($i*2)] = $result->[$i][1];
	    $sys_index_instances[($i*2)+1] = $result->[$i][4];
	    $table{values}{$instid} = $result->[$i];
	}
    }
    $pmda->replace_indom($sys_index_indom, \@sys_index_instances);
}

sub refresh_io_user_indexes
{
    my $tableref = shift;
    my $result = refresh_results($tableref);
    my %table = %$tableref;

    @user_index_instances = ();		# refresh indom too
    if (defined($result)) {
	for my $i (0 .. $#{$result}) {	# for each row (instance) returned
	    my $instid = $user_index_instances[($i*2)] = $result->[$i][1];
	    $user_index_instances[($i*2)+1] = $result->[$i][4];
	    $table{values}{$instid} = $result->[$i];
	}
    }
    $pmda->replace_indom($user_index_indom, \@user_index_instances);
}

sub refresh_io_all_sequences
{
    my $tableref = shift;
    my $result = refresh_results($tableref);
    my %table = %$tableref;

    @all_seq_instances = ();		# refresh indom too
    if (defined($result)) {
	for my $i (0 .. $#{$result}) {	# for each row (instance) returned
	    my $instid = $all_seq_instances[($i*2)] = $result->[$i][0];
	    $all_seq_instances[($i*2)+1] = $result->[$i][2];
	    $table{values}{$instid} = $result->[$i];
	}
    }
    $pmda->replace_indom($all_seq_indom, \@all_seq_instances);
}

sub refresh_io_sys_sequences
{
    my $tableref = shift;
    my $result = refresh_results($tableref);
    my %table = %$tableref;

    @sys_seq_instances = ();		# refresh indom too
    if (defined($result)) {
	for my $i (0 .. $#{$result}) {	# for each row (instance) returned
	    my $instid = $sys_seq_instances[($i*2)] = $result->[$i][0];
	    $sys_seq_instances[($i*2)+1] = $result->[$i][2];
	    $table{values}{$instid} = $result->[$i];
	}
    }
    $pmda->replace_indom($sys_seq_indom, \@sys_seq_instances);
}

sub refresh_io_user_sequences
{
    my $tableref = shift;
    my $result = refresh_results($tableref);
    my %table = %$tableref;

    @user_seq_instances = ();		# refresh indom too
    if (defined($result)) {
	for my $i (0 .. $#{$result}) {	# for each row (instance) returned
	    my $instid = $user_seq_instances[($i*2)] = $result->[$i][0];
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
    $pmda->add_metric(pmda_pmid($cluster,8), PM_TYPE_U32, $indom,
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
    $pmda->add_metric(pmda_pmid($cluster,6), PM_TYPE_U32, $indom,
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
# drop root priveleges, then call PMDA 'run' routine to talk to pmcd.
#
$pmda = PCP::PMDA->new('postgresql', 110);
postgresql_metrics_setup();
postgresql_indoms_setup();

$pmda->set_fetch_callback(\&postgresql_fetch_callback);
$pmda->set_fetch(\&postgresql_connection_setup);
$pmda->set_refresh(\&postgresql_refresh);
$pmda->set_user('postgres');
$pmda->run;
