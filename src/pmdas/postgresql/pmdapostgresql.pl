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
my $rel_indom = 0; my @rel_instances;
my $func_indom = 1; my @func_instances;
my $database_indom = 2; my @database_instances;
my $index_rel_indom = 3; my @index_rel_instances;

# TODO:
# "does not exist"? pg_stat_database_conflicts, pg_stat_replication,
#		    pg_stat_xact_all_tables, pg_stat_xact_sys_tables,
#		    pg_stat_xact_user_tables, pg_stat_xact_user_functions

# hash of hashes holding DB handle and last values, indexed by table name
my %tables_by_name = (
    pg_stat_activity		=> { handle => undef, values => {} },
    pg_stat_bgwriter		=> { handle => undef, values => {} },
    pg_stat_database		=> { handle => undef, values => {} },
    pg_stat_all_tables		=> { handle => undef, values => {} },
    pg_stat_sys_tables		=> { handle => undef, values => {} },
    pg_stat_user_tables		=> { handle => undef, values => {} },
    pg_stat_all_indexes		=> { handle => undef, values => {} },
    pg_stat_sys_indexes		=> { handle => undef, values => {} },
    pg_stat_user_indexes	=> { handle => undef, values => {} },
    pg_statio_all_tables	=> { handle => undef, values => {} },
    pg_statio_sys_tables	=> { handle => undef, values => {} },
    pg_statio_user_tables	=> { handle => undef, values => {} },
    pg_statio_all_indexes	=> { handle => undef, values => {} },
    pg_statio_sys_indexes	=> { handle => undef, values => {} },
    pg_statio_user_indexes	=> { handle => undef, values => {} },
    pg_statio_all_sequences	=> { handle => undef, values => {} },
    pg_statio_sys_sequences	=> { handle => undef, values => {} },
    pg_statio_user_sequences	=> { handle => undef, values => {} },
    pg_stat_user_functions	=> { handle => undef, values => {} },
);

# hash of hashes holding setup and refresh function, indexed by PMID cluster
my %tables_by_cluster = (
    '0'	 => {
	name	=> 'pg_stat_activity',
	setup	=> \&setup_activity,
	refresh => \&refresh_activity },
    '1'	 => {
	name	=> 'pg_stat_bgwriter',
	setup	=> \&setup_bgwriter,
	refresh	=> \&refresh_bgwriter },
    '2'	 => {
	name	=> 'pg_stat_database',
	setup	=> \&setup_database,
	refresh => \&refresh_database },
    '3'	 => {
	name	=> 'pg_stat_user_functions',
	setup	=> \&setup_user_functions,
	refresh => \&refresh_user_functions },
    '10' => {
	name	=> 'pg_stat_all_tables',
	setup	=> \&setup_stat_tables,
	params	=> 'all_tables',
	refresh => \&refresh_all_tables },
    '11' => {
	name	=> 'pg_stat_sys_tables',
	setup	=> \&setup_stat_tables,
	params	=> 'sys_tables',
	refresh => \&refresh_sys_tables },
    '12' => {
	name	=> 'pg_stat_user_tables',
	setup	=> \&setup_stat_tables,
	params	=> 'user_tables',
	refresh => \&refresh_user_tables },
    '13' => {
	name	=> 'pg_stat_all_indexes',
	setup	=> \&setup_stat_indexes,
	params	=> 'all_indexes',
	refresh => \&refresh_all_indexes },
    '14' => {
	name	=> 'pg_stat_sys_indexes',
	setup	=> \&setup_stat_indexes,
	params	=> 'sys_indexes',
	refresh => \&refresh_sys_indexes },
    '15' => {
	name	=> 'pg_stat_user_indexes',
	setup	=> \&setup_stat_indexes,
	params	=> 'user_indexes',
	refresh => \&postgresql_user_indexes },
    '30' => {
	name	=> 'pg_statio_all_tables',
	setup	=> \&setup_statio_tables,
	params	=> 'all_tables',
	refresh => \&refresh_io_all_tables },
    '31' => {
	name	=> 'pg_statio_sys_tables',
	setup	=> \&setup_statio_tables,
	params	=> 'sys_tables',
	refresh => \&refresh_io_sys_tables },
    '32' => {
	name	=> 'pg_statio_user_tables',
	setup	=> \&setup_statio_tables,
	params	=> 'user_tables',
	refresh => \&refresh_io_user_tables },
    '33' => {
	name	=> 'pg_statio_all_indexes',
	setup	=> \&setup_statio_indexes,
	params	=> 'all_indexes',
	refresh => \&refresh_io_all_indexes },
    '34' => {
	name	=> 'pg_statio_sys_indexes',
	setup	=> \&setup_statio_indexes,
	params	=> 'sys_indexes',
	refresh => \&refresh_io_all_indexes },
    '35' => {
	name	=> 'pg_statio_user_indexes',
	setup	=> \&setup_statio_indexes,
	params	=> 'user_indexes',
	refresh => \&refresh_io_all_indexes },
    '36' => {
	name	=> 'pg_statio_all_sequences',
	setup	=> \&setup_statio_sequences,
	params	=> 'all_sequences',
	refresh => \&refresh_io_all_sequences },
    '37' => {
	name	=> 'pg_statio_sys_sequences',
	setup	=> \&setup_statio_sequences,
	params	=> 'sys_sequences',
	refresh => \&refresh_io_sys_sequences },
    '38' => {
	name	=> 'pg_statio_user_sequences',
	setup	=> \&setup_statio_sequences,
	params	=> 'user_sequences',
	refresh => \&refresh_io_user_sequences },
);

sub postgresql_connection_setup
{
    if (!defined($dbh)) {
	$dbh = DBI->connect($database, $username, $password, {AutoCommit => 0});
	if (defined($dbh)) {
	    $pmda->log("PostgreSQL connection established");
	    foreach my $key (keys %tables_by_name) {
		my $query = $dbh->prepare("select * from $key");
		$tables_by_name{$key}{handle} = $query;
	    }
	}
    }
}

sub postgresql_metrics_setup
{
    foreach my $cluster (sort (keys %tables_by_cluster)) {
	my $setup = $tables_by_cluster{"$cluster"}{setup};
	&$setup($cluster, $tables_by_cluster{"$cluster"}{params});
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

    if (!defined($metric_name))	{ return (PM_ERR_PMID, 0); }
    $key = $metric_name;
    $key =~ s/^postgresql\./pg_/;
    $key =~ s/\.[a-z0-9_]*$//;
    $key =~ s/\./_/g;

    # $pmda->log("fetch_cb $metric_name $cluster:$item ($inst) - $key");

    $valueref = $tables_by_name{$key}{values}{"$inst"};
    if (!defined($valueref))	{ return (PM_ERR_INST, 0); }
    @columns = @$valueref;
    $value = $columns[$item];
    if (!defined($value))	{ return (PM_ERR_APPVERSION, 0); }
    return ($value, 1);
}

#
# Refresh routines - one per table (cluster) - format database query
# result set for later use by the generic fetch callback routine.
# 
sub refresh_activity { }

sub refresh_bgwriter
{
    my $tableref = shift;
    my %table = %$tableref;
    my ( $handle, $valuesref ) = ( $table{handle}, $table{values} );
    my %values = %$valuesref;

    %values = ();	# clear any previous values
    if (defined($dbh) && defined($handle)) {
	$handle->execute();
	my $result = $handle->fetchall_arrayref();
	if (defined($result)) {
	    $table{values}{"$pm_in_null"} = \@{$result->[0]};
	}
    }
}

sub refresh_database
{
    my $tableref = shift;
    my %table = %$tableref;
    my ( $handle, $valuesref ) = ( $table{handle}, $table{values} );
    my %values = %$valuesref;

    %values = ();	# clear any previous values
    @database_instances = ();	# refresh indom too

    if (defined($dbh) && defined($handle)) {
	$handle->execute();
	my $result = $handle->fetchall_arrayref();
	if (defined($result)) {
	    for my $i (0 .. $#{$result}) {	# for each row (instance) returned
		my $instid = $result->[$i][0];
		my $instname = $result->[$i][1];
		$database_instances[($i*2)] = $instid;
		$database_instances[($i*2)+1] = $instname;
	        $table{values}{$instid} = $result->[$i];
	    }
	}
    }
    $pmda->replace_indom($database_indom, \@database_instances);
}

sub refresh_user_functions { }
sub refresh_all_tables { }
sub refresh_sys_tables { }
sub refresh_user_tables { }
sub refresh_all_indexes { }
sub refresh_sys_indexes { }
sub refresh_user_indexes { }
sub refresh_io_all_tables { }
sub refresh_io_sys_tables { }
sub refresh_io_user_tables { }
sub refresh_io_all_indexes { }
sub refresh_io_sys_indexes { }
sub refresh_io_user_indexes { }
sub refresh_io_all_sequences { }
sub refresh_io_sys_sequences { }
sub refresh_io_user_sequences { }

#
# Setup routines - one per cluster, add metrics to PMDA
# 
sub setup_activity
{
    my ($cluster) = @_;

    $pmda->add_metric(pmda_pmid($cluster,0), PM_TYPE_U32, $database_indom,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'postgresql.stat.activity.procpid', '', '');
    $pmda->add_metric(pmda_pmid($cluster,1), PM_TYPE_U32, $database_indom,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'postgresql.stat.activity.usesysid', '', '');
    $pmda->add_metric(pmda_pmid($cluster,2), PM_TYPE_STRING, $database_indom,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'postgresql.stat.activity.usename', '', '');
    $pmda->add_metric(pmda_pmid($cluster,3), PM_TYPE_STRING, $database_indom,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'postgresql.stat.activity.application_name', '', '');
    $pmda->add_metric(pmda_pmid($cluster,4), PM_TYPE_32, $database_indom,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'postgresql.stat.activity.client_addr', '', '');
    $pmda->add_metric(pmda_pmid($cluster,5), PM_TYPE_STRING, $database_indom,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'postgresql.stat.activity.backend_start', '', '');
    $pmda->add_metric(pmda_pmid($cluster,6), PM_TYPE_STRING, $database_indom,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'postgresql.stat.activity.xact_start', '', '');
    $pmda->add_metric(pmda_pmid($cluster,7), PM_TYPE_STRING, $database_indom,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'postgresql.stat.activity.query_start', '', '');
    $pmda->add_metric(pmda_pmid($cluster,8), PM_TYPE_STRING, $database_indom,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'postgresql.stat.activity.waiting', '', '');
    $pmda->add_metric(pmda_pmid($cluster,9), PM_TYPE_STRING, $database_indom,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'postgresql.stat.activity.current_query', '', '');
}

sub setup_stat_tables
{
    my ($cluster, $tables) = @_;

    # indom: relid + relname
    $pmda->add_metric(pmda_pmid($cluster,0), PM_TYPE_STRING, $rel_indom,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  "postgresql.stat.$tables.schemaname", '', '');
    $pmda->add_metric(pmda_pmid($cluster,1), PM_TYPE_U32, $rel_indom,
		  PM_SEM_COUNTER, pmda_units(0,0,0,0,0,0),
		  "postgresql.stat.$tables.seq_scan", '', '');
    $pmda->add_metric(pmda_pmid($cluster,2), PM_TYPE_U32, $rel_indom,
		  PM_SEM_COUNTER, pmda_units(0,0,0,0,0,0),
		  "postgresql.stat.$tables.seq_tup_read", '', '');
    $pmda->add_metric(pmda_pmid($cluster,3), PM_TYPE_U32, $rel_indom,
		  PM_SEM_COUNTER, pmda_units(0,0,0,0,0,0),
		  "postgresql.stat.$tables.idx_scan", '', '');
    $pmda->add_metric(pmda_pmid($cluster,4), PM_TYPE_U32, $rel_indom,
		  PM_SEM_COUNTER, pmda_units(0,0,0,0,0,0),
		  "postgresql.stat.$tables.idx_tup_fetch", '', '');
    $pmda->add_metric(pmda_pmid($cluster,5), PM_TYPE_U32, $rel_indom,
		  PM_SEM_COUNTER, pmda_units(0,0,0,0,0,0),
		  "postgresql.stat.$tables.n_tup_ins", '', '');
    $pmda->add_metric(pmda_pmid($cluster,6), PM_TYPE_U32, $rel_indom,
		  PM_SEM_COUNTER, pmda_units(0,0,0,0,0,0),
		  "postgresql.stat.$tables.n_tup_upd", '', '');
    $pmda->add_metric(pmda_pmid($cluster,7), PM_TYPE_U32, $rel_indom,
		  PM_SEM_COUNTER, pmda_units(0,0,0,0,0,0),
		  "postgresql.stat.$tables.n_tup_del", '', '');
    $pmda->add_metric(pmda_pmid($cluster,8), PM_TYPE_U32, $rel_indom,
		  PM_SEM_COUNTER, pmda_units(0,0,0,0,0,0),
		  "postgresql.stat.$tables.n_tup_hot_upd", '', '');
    $pmda->add_metric(pmda_pmid($cluster,9), PM_TYPE_U32, $rel_indom,
		  PM_SEM_COUNTER, pmda_units(0,0,0,0,0,0),
		  "postgresql.stat.$tables.n_live_tup", '', '');
    $pmda->add_metric(pmda_pmid($cluster,10), PM_TYPE_U32, $rel_indom,
		  PM_SEM_COUNTER, pmda_units(0,0,0,0,0,0),
		  "postgresql.stat.$tables.n_dead_tup", '', '');
    $pmda->add_metric(pmda_pmid($cluster,11), PM_TYPE_STRING, $rel_indom,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  "postgresql.stat.$tables.last_vacuum", '', '');
    $pmda->add_metric(pmda_pmid($cluster,12), PM_TYPE_STRING, $rel_indom,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  "postgresql.stat.$tables.last_autovacuum", '', '');
    $pmda->add_metric(pmda_pmid($cluster,13), PM_TYPE_STRING, $rel_indom,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  "postgresql.stat.$tables.last_analyze", '', '');
    $pmda->add_metric(pmda_pmid($cluster,14), PM_TYPE_STRING, $rel_indom,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  "postgresql.stat.$tables.last_autoanalyze", '', '');
}

sub setup_bgwriter
{
    my ($cluster) = @_;

    $pmda->add_metric(pmda_pmid($cluster,0), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  'postgresql.stat.bgwriter.checkpoints_timed', '', '');
    $pmda->add_metric(pmda_pmid($cluster,1), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  'postgresql.stat.bgwriter.checkpoints_req', '', '');
    $pmda->add_metric(pmda_pmid($cluster,2), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  'postgresql.stat.bgwriter.buffers_checkpoints', '', '');
    $pmda->add_metric(pmda_pmid($cluster,3), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  'postgresql.stat.bgwriter.buffers_clean', '', '');
    $pmda->add_metric(pmda_pmid($cluster,4), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  'postgresql.stat.bgwriter.maxwritten_clean', '', '');
    $pmda->add_metric(pmda_pmid($cluster,5), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  'postgresql.stat.bgwriter.buffers_backend', '', '');
    $pmda->add_metric(pmda_pmid($cluster,6), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  'postgresql.stat.bgwriter.buffers_alloc', '', '');
}

sub setup_database
{
    my ($cluster) = @_;

    # indom: datid + datname
    $pmda->add_metric(pmda_pmid($cluster,2), PM_TYPE_U32, $database_indom,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'postgresql.stat.database.numbackends', '', '');
    $pmda->add_metric(pmda_pmid($cluster,3), PM_TYPE_U32, $database_indom,
		  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  'postgresql.stat.database.xact_commit', '', '');
    $pmda->add_metric(pmda_pmid($cluster,4), PM_TYPE_U32, $database_indom,
		  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  'postgresql.stat.database.xact_rollback', '', '');
    $pmda->add_metric(pmda_pmid($cluster,5), PM_TYPE_U32, $database_indom,
		  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  'postgresql.stat.database.blks_read', '', '');
    $pmda->add_metric(pmda_pmid($cluster,6), PM_TYPE_U32, $database_indom,
		  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  'postgresql.stat.database.blks_hit', '', '');
    $pmda->add_metric(pmda_pmid($cluster,7), PM_TYPE_U32, $database_indom,
		  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  'postgresql.stat.database.tup_returned', '', '');
    $pmda->add_metric(pmda_pmid($cluster,8), PM_TYPE_U32, $database_indom,
		  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  'postgresql.stat.database.tup_fetched', '', '');
    $pmda->add_metric(pmda_pmid($cluster,9), PM_TYPE_U32, $database_indom,
		  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  'postgresql.stat.database.tup_inserted', '', '');
    $pmda->add_metric(pmda_pmid($cluster,10), PM_TYPE_U32, $database_indom,
		  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  'postgresql.stat.database.tup_updated', '', '');
    $pmda->add_metric(pmda_pmid($cluster,11), PM_TYPE_U32, $database_indom,
		  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  'postgresql.stat.database.tup_deleted', '', '');
}

sub setup_stat_indexes
{
    my ($cluster, $indexes) = @_;

    # indom: indexrelid + indexrelname
    $pmda->add_metric(pmda_pmid($cluster,0), PM_TYPE_32, $index_rel_indom,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  "postgresql.stat.$indexes.relid", '', '');
    $pmda->add_metric(pmda_pmid($cluster,1), PM_TYPE_STRING, $index_rel_indom,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  "postgresql.stat.$indexes.schemaname", '', '');
    $pmda->add_metric(pmda_pmid($cluster,2), PM_TYPE_STRING, $index_rel_indom,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  "postgresql.stat.$indexes.relname", '', '');
    $pmda->add_metric(pmda_pmid($cluster,3), PM_TYPE_U32, $index_rel_indom,
		  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  "postgresql.stat.$indexes.idx_scan", '', '');
    $pmda->add_metric(pmda_pmid($cluster,4), PM_TYPE_U32, $index_rel_indom,
		  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  "postgresql.stat.$indexes.idx_tup_read", '', '');
    $pmda->add_metric(pmda_pmid($cluster,5), PM_TYPE_U32, $index_rel_indom,
		  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  "postgresql.stat.$indexes.idx_tup_fetch", '', '');
}

sub setup_statio_tables
{
    my ($cluster, $tables) = @_;

    $pmda->add_metric(pmda_pmid($cluster,0), PM_TYPE_STRING, $rel_indom,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  "postgresql.statio.$tables.schemaname", '', '');
    $pmda->add_metric(pmda_pmid($cluster,1), PM_TYPE_U32, $rel_indom,
		  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  "postgresql.statio.$tables.heap_blks_read", '', '');
    $pmda->add_metric(pmda_pmid($cluster,2), PM_TYPE_U32, $rel_indom,
		  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  "postgresql.statio.$tables.heap_blks_hit", '', '');
    $pmda->add_metric(pmda_pmid($cluster,3), PM_TYPE_U32, $rel_indom,
		  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  "postgresql.statio.$tables.idx_blks_read", '', '');
    $pmda->add_metric(pmda_pmid($cluster,4), PM_TYPE_U32, $rel_indom,
		  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  "postgresql.statio.$tables.idx_blks_hit", '', '');
    $pmda->add_metric(pmda_pmid($cluster,5), PM_TYPE_U32, $rel_indom,
		  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  "postgresql.statio.$tables.toast_blks_read", '', '');
    $pmda->add_metric(pmda_pmid($cluster,6), PM_TYPE_U32, $rel_indom,
		  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  "postgresql.statio.$tables.toast_blks_hit", '', '');
    $pmda->add_metric(pmda_pmid($cluster,7), PM_TYPE_U32, $rel_indom,
		  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  "postgresql.statio.$tables.tidx_blks_read", '', '');
    $pmda->add_metric(pmda_pmid($cluster,8), PM_TYPE_U32, $rel_indom,
		  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  "postgresql.statio.$tables.tidx_blks_hit", '', '');
}

sub setup_statio_indexes
{
    my ($cluster, $indexes) = @_;

    # indom: indexrelid + indexrelname
    $pmda->add_metric(pmda_pmid($cluster,0), PM_TYPE_32, $index_rel_indom,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  "postgresql.statio.$indexes.relid", '', '');
    $pmda->add_metric(pmda_pmid($cluster,1), PM_TYPE_STRING, $index_rel_indom,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  "postgresql.statio.$indexes.schemaname", '', '');
    $pmda->add_metric(pmda_pmid($cluster,2), PM_TYPE_STRING, $index_rel_indom,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  "postgresql.statio.$indexes.relname", '', '');
    $pmda->add_metric(pmda_pmid($cluster,3), PM_TYPE_U32, $index_rel_indom,
		  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  "postgresql.statio.$indexes.idx_blks_read", '', '');
    $pmda->add_metric(pmda_pmid($cluster,4), PM_TYPE_U32, $index_rel_indom,
		  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  "postgresql.statio.$indexes.idx_blks_hit", '', '');
}

sub setup_statio_sequences
{
    my ($cluster, $sequences) = @_;

    # indom: relid + relname
    $pmda->add_metric(pmda_pmid($cluster,0), PM_TYPE_STRING, $rel_indom,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  "postgresql.statio.$sequences.schemaname", '', '');
    $pmda->add_metric(pmda_pmid($cluster,1), PM_TYPE_U32, $rel_indom,
		  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  "postgresql.statio.$sequences.blks_read", '', '');
    $pmda->add_metric(pmda_pmid($cluster,2), PM_TYPE_U32, $rel_indom,
		  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  "postgresql.statio.$sequences.blks_hit", '', '');
}

sub setup_user_functions
{
    my ($cluster) = @_;

    # indom: funcid + funcname
    $pmda->add_metric(pmda_pmid($cluster,0), PM_TYPE_STRING, $func_indom,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'postgresql.stat.user_functions.schemaname', '', '');
    $pmda->add_metric(pmda_pmid($cluster,1), PM_TYPE_U64, $rel_indom,
		  PM_SEM_COUNTER, pmda_units(0,0,0,0,0,0),
		  'postgresql.stat.user_functions.calls', '', '');
    $pmda->add_metric(pmda_pmid($cluster,2), PM_TYPE_U64, $rel_indom,
		  PM_SEM_COUNTER, pmda_units(0,1,0,0,PM_TIME_MSEC,0),
		  'postgresql.stat.user_functions.total_time', '', '');
    $pmda->add_metric(pmda_pmid($cluster,3), PM_TYPE_U64, $rel_indom,
		  PM_SEM_COUNTER, pmda_units(0,1,0,0,PM_TIME_MSEC,0),
		  'postgresql.stat.user_functions.self_time', '', '');
}

#
# Main PMDA thread of execution starts here - setup metrics and callbacks
# drop root priveleges, then call PMDA 'run' routine to talk to pmcd.
#
$pmda = PCP::PMDA->new('postgresql', 110);
postgresql_metrics_setup();

$pmda->add_indom($rel_indom, \@rel_instances,
		 'Instance domain exporting each PostgreSQL relation', '');
$pmda->add_indom($func_indom, \@func_instances,
		 'Instance domain exporting PostgreSQL user functions', '');
$pmda->add_indom($database_indom, \@database_instances,
		 'Instance domain exporting each PostgreSQL database', '');
$pmda->add_indom($index_rel_indom, \@index_rel_instances,
		 'Instance domain exporting each PostgreSQL indexes', '');

$pmda->set_fetch_callback(\&postgresql_fetch_callback);
$pmda->set_fetch(\&postgresql_connection_setup);
$pmda->set_refresh(\&postgresql_refresh);
$pmda->set_user('postgres');
$pmda->run;

=pod

=head1 NAME

pmdapostgresql - PostgreSQL database PMDA

=head1 DESCRIPTION

B<pmdapostgresql> is a Performance Co-Pilot PMDA which extracts
live performance data from a running PostgreSQL database.

=head1 INSTALLATION

B<pmdapostgresql> uses a configuration file from (in this order):

=over

=item * /etc/pcpdbi.conf

=item * $PCP_PMDAS_DIR/postgresql/postgresql.conf

=back

This file can contain overridden values (Perl code) for the settings
listed at the start of pmdapostgresql.pl, namely:

=over

=item * database name (see DBI(3) for details)

=item * database username

=back

Once this is setup, you can access the names and values for the
postgresql performance metrics by doing the following as root:

	# cd $PCP_PMDAS_DIR/postgresql
	# ./Install

If you want to undo the installation, do the following as root:

	# cd $PCP_PMDAS_DIR/postgresql
	# ./Remove

B<pmdapostgresql> is launched by pmcd(1) and should never be executed
directly.  The Install and Remove scripts notify pmcd(1) when
the agent is installed or removed.

=head1 FILES

=over

=item /etc/pcpdbi.conf

configuration file for all PCP database monitors

=item $PCP_PMDAS_DIR/postgresql/postgresql.conf

configuration file for B<pmdapostgresql>

=item $PCP_PMDAS_DIR/postgresql/Install

installation script for the B<pmdapostgresql> agent

=item $PCP_PMDAS_DIR/postgresql/Remove

undo installation script for the B<pmdapostgresql> agent

=item $PCP_LOG_DIR/pmcd/postgresql.log

default log file for error messages from B<pmdapostgresql>

=back

=head1 SEE ALSO

pmcd(1), pmdadbping.pl(1) and DBI(3).
