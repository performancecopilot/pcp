#
# Copyright (c) 2012-2013 Chandana De Silva.
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
use DBI;

my $database = 'DBI:mysql:mysql';
my $username = 'dbmonitor';
my $password = 'dbmonitor';

# Configuration files for overriding the above settings
for my $file (	'/etc/pcpdbi.conf',	# system defaults (lowest priority)
		pmda_config('PCP_PMDAS_DIR') . '/mysql/mysql.conf',
		'./mysql.conf' ) {	# current directory (high priority)
    eval `cat $file` unless ! -f $file;
}

use vars qw( $pmda %status %variables @processes %slave_status );
use vars qw( $dbh $sth_variables $sth_status $sth_processes $sth_slave_status );
my $process_indom = 0;
my @process_instances;

# translate yes/no true/false on/off to 1/0
sub mysql_txt2num {
    my ($value) = lc($_[0]);

    if (!defined($value)) {
	    return (PM_ERR_AGAIN, 0);
    }
    elsif ($value eq "yes" || $value eq "true" || $value eq "on") {
        return 1;
    }
    elsif ($value eq "no" || $value eq "false" || $value eq "off") {
        return 0;
    }
    else {
        return -1;
    }
}

sub mysql_connection_setup
{
    # $pmda->log("mysql_connection_setup\n");

    if (!defined($dbh)) {
        $dbh = DBI->connect($database, $username, $password);
        if (defined($dbh)) {
            # set the db handle to undef in case of any failure
            # this will force a database reconnect
            $dbh->{HandleError} = sub { $dbh = undef; };
            $pmda->log("MySQL connection established\n");
            $sth_variables = $dbh->prepare('show variables');
            $sth_status = $dbh->prepare('show global status');
            $sth_processes = $dbh->prepare('show processlist');
            $sth_slave_status = $dbh->prepare('show slave status');
        }
    }
}

sub mysql_variables_refresh
{
    # $pmda->log("mysql_variables_refresh\n");

    %variables = ();	# clear any previous contents
    if (defined($dbh)) {
        $sth_variables->execute();
        my $result = $sth_variables->fetchall_arrayref();
        for my $i (0 .. $#{$result}) {
            $variables{$result->[$i][0]} = $result->[$i][1];
        }
    }
}

sub mysql_status_refresh
{
    # $pmda->log("mysql_status_refresh\n");

    %status = ();	# clear any previous contents
    if (defined($dbh)) {
        $sth_status->execute();
        my $result = $sth_status->fetchall_arrayref();
        my $txtnum;
        my $txtnumvar;
        for my $i (0 .. $#{$result}) {
            my $key = lcfirst $result->[$i][0];
            $status{$key} = $result->[$i][1];
            # if this status value has a yes/no type value, get it translated
            $txtnum = mysql_txt2num($result->[$i][1]);
            if ($txtnum ge 0) {
                $txtnumvar=$key . "_num";
                $status{$txtnumvar} = $txtnum;
            }
        }
    }
}

sub mysql_process_refresh
{
    # $pmda->log("mysql_process_refresh\n");

    @processes = ();	# clear any previous contents
    @process_instances = ();	# refresh indom too

    if (defined($dbh)) {
        $sth_processes->execute();
        my $result = $sth_processes->fetchall_arrayref();
        for my $i (0 .. $#{$result}) {
            $process_instances[($i*2)] = $i;
            $process_instances[($i*2)+1] = "$result->[$i][0]";
            $processes[$i] = $result->[$i];
        }
    }

    $pmda->replace_indom($process_indom, \@process_instances);
}

sub mysql_slave_status_refresh
{
    # $pmda->log("mysql_slave_status_refresh\n");

    %slave_status = ();	# clear any previous contents
    if (defined($dbh)) {
        $sth_slave_status->execute();
        my $result = $sth_slave_status->fetchrow_hashref();
        my $txtnum;
        my $txtnumvar;
        while ( my ($key, $value) = each(%$result) ) {
            $slave_status{lc $key} = $value;
            # if this status value has a yes/no type value, get it translated
            $txtnum = mysql_txt2num($value);
            if ($txtnum ge 0) {
                $txtnumvar=lc($key) . "_num";
                $slave_status{$txtnumvar} = $txtnum;
            }
        }
    }
}

sub mysql_refresh
{
    my ($cluster) = @_;

    # $pmda->log("mysql_refresh $cluster\n");
    if ($cluster == 0)		{ mysql_status_refresh; }
    elsif ($cluster == 1)	{ mysql_variables_refresh; }
    elsif ($cluster == 2)	{ mysql_process_refresh; }
    elsif ($cluster == 3)	{ mysql_slave_status_refresh; }
}

sub mysql_fetch_callback
{
    my ($cluster, $item, $inst) = @_;
    my $metric_name = pmda_pmid_name($cluster, $item);
    my ($mysql_name, $value, @procs);

    # $pmda->log("mysql_fetch_callback $metric_name $cluster:$item ($inst)\n");

    if (!defined($metric_name))	{ return (PM_ERR_PMID, 0); }
    $mysql_name = $metric_name;

    if ($cluster == 2) {
	if ($inst < 0)		{ return (PM_ERR_INST, 0); }
	if ($inst > @process_instances)	{ return (PM_ERR_INST, 0); }
	$value = $processes[$inst];
	if (!defined($value))	{ return (PM_ERR_INST, 0); }
	@procs = @$value;
	if (!defined($procs[$item]) && $item == 6) { return ("?", 1); }
	if (!defined($procs[$item])) { return (PM_ERR_APPVERSION, 0); }
    	return ($procs[$item], 1);
    }
    if ($inst != PM_IN_NULL)		{ return (PM_ERR_INST, 0); }
    if ($cluster == 0) {
        $mysql_name =~ s/^mysql\.status\.//;
        $value = $status{$mysql_name};
        if (!defined($value))	{ return (PM_ERR_APPVERSION, 0); }
        return ($value, 1);
    }
    elsif ($cluster == 1) {
        $mysql_name =~ s/^mysql\.variables\.//;
        $value = $variables{$mysql_name};
        if (!defined($value))	{ return (PM_ERR_APPVERSION, 0); }
        return ($value, 1);
    }
    elsif ($cluster == 3) {
        $mysql_name =~ s/^mysql\.slave_status\.//;
        $value = $slave_status{$mysql_name};
        if (!defined($value))	{ return (PM_ERR_APPVERSION, 0); }
        return ($value, 1);
    }
    return (PM_ERR_PMID, 0);
}

$pmda = PCP::PMDA->new('mysql', 66);

$pmda->add_metric(pmda_pmid(0,0), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  'mysql.status.aborted_clients', '', '');
$pmda->add_metric(pmda_pmid(0,1), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  'mysql.status.aborted_connects', '', '');
$pmda->add_metric(pmda_pmid(0,2), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_COUNTER, pmda_units(1,0,0,PM_SPACE_BYTE,0,0),
		  'mysql.status.binlog_cache_disk_use', '', '');
$pmda->add_metric(pmda_pmid(0,3), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_COUNTER, pmda_units(1,0,0,PM_SPACE_BYTE,0,0),
		  'mysql.status.binlog_cache_use', '', '');
$pmda->add_metric(pmda_pmid(0,4), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_COUNTER, pmda_units(1,0,0,PM_SPACE_BYTE,0,0),
		  'mysql.status.bytes_received', '', '');
$pmda->add_metric(pmda_pmid(0,5), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_COUNTER, pmda_units(1,0,0,PM_SPACE_BYTE,0,0),
		  'mysql.status.bytes_sent', '', '');
$pmda->add_metric(pmda_pmid(0,6), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  'mysql.status.com_admin_commands', '', '');
$pmda->add_metric(pmda_pmid(0,7), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  'mysql.status.com_alter_db', '', '');
$pmda->add_metric(pmda_pmid(0,8), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  'mysql.status.com_alter_table', '', '');
$pmda->add_metric(pmda_pmid(0,9), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  'mysql.status.com_analyze', '', '');
$pmda->add_metric(pmda_pmid(0,10), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  'mysql.status.com_backup_table', '', '');
$pmda->add_metric(pmda_pmid(0,11), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  'mysql.status.com_begin', '', '');
$pmda->add_metric(pmda_pmid(0,12), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  'mysql.status.com_call_procedure', '', '');
$pmda->add_metric(pmda_pmid(0,13), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  'mysql.status.com_change_db', '', '');
$pmda->add_metric(pmda_pmid(0,14), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  'mysql.status.com_change_master', '', '');
$pmda->add_metric(pmda_pmid(0,15), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  'mysql.status.com_check', '', '');
$pmda->add_metric(pmda_pmid(0,16), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  'mysql.status.com_checksum', '', '');
$pmda->add_metric(pmda_pmid(0,17), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  'mysql.status.com_commit', '', '');
$pmda->add_metric(pmda_pmid(0,18), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  'mysql.status.com_create_db', '', '');
$pmda->add_metric(pmda_pmid(0,19), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  'mysql.status.com_create_function', '', '');
$pmda->add_metric(pmda_pmid(0,20), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  'mysql.status.com_create_index', '', '');
$pmda->add_metric(pmda_pmid(0,21), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  'mysql.status.com_create_table', '', '');
$pmda->add_metric(pmda_pmid(0,22), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  'mysql.status.com_create_user', '', '');
$pmda->add_metric(pmda_pmid(0,23), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  'mysql.status.com_dealloc_sql', '', '');
$pmda->add_metric(pmda_pmid(0,24), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  'mysql.status.com_delete', '', '');
$pmda->add_metric(pmda_pmid(0,25), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  'mysql.status.com_delete_multi', '', '');
$pmda->add_metric(pmda_pmid(0,26), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  'mysql.status.com_do', '', '');
$pmda->add_metric(pmda_pmid(0,27), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  'mysql.status.com_drop_db', '', '');
$pmda->add_metric(pmda_pmid(0,28), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  'mysql.status.com_drop_function', '', '');
$pmda->add_metric(pmda_pmid(0,29), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  'mysql.status.com_drop_index', '', '');
$pmda->add_metric(pmda_pmid(0,30), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  'mysql.status.com_drop_table', '', '');
$pmda->add_metric(pmda_pmid(0,31), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  'mysql.status.com_drop_user', '', '');
$pmda->add_metric(pmda_pmid(0,32), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  'mysql.status.com_execute_sql', '', '');
$pmda->add_metric(pmda_pmid(0,33), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  'mysql.status.com_flush', '', '');
$pmda->add_metric(pmda_pmid(0,34), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  'mysql.status.com_grant', '', '');
$pmda->add_metric(pmda_pmid(0,35), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  'mysql.status.com_ha_close', '', '');
$pmda->add_metric(pmda_pmid(0,36), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  'mysql.status.com_ha_open', '', '');
$pmda->add_metric(pmda_pmid(0,37), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  'mysql.status.com_ha_read', '', '');
$pmda->add_metric(pmda_pmid(0,38), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  'mysql.status.com_help', '', '');
$pmda->add_metric(pmda_pmid(0,39), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  'mysql.status.com_insert', '', '');
$pmda->add_metric(pmda_pmid(0,40), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  'mysql.status.com_insert_select', '', '');
$pmda->add_metric(pmda_pmid(0,41), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  'mysql.status.com_kill', '', '');
$pmda->add_metric(pmda_pmid(0,42), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  'mysql.status.com_load', '', '');
$pmda->add_metric(pmda_pmid(0,43), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  'mysql.status.com_load_master_data', '', '');
$pmda->add_metric(pmda_pmid(0,44), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  'mysql.status.com_load_master_table', '', '');
$pmda->add_metric(pmda_pmid(0,45), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  'mysql.status.com_lock_tables', '', '');
$pmda->add_metric(pmda_pmid(0,46), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  'mysql.status.com_optimize', '', '');
$pmda->add_metric(pmda_pmid(0,47), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  'mysql.status.com_preload_keys', '', '');
$pmda->add_metric(pmda_pmid(0,48), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  'mysql.status.com_prepare_sql', '', '');
$pmda->add_metric(pmda_pmid(0,49), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  'mysql.status.com_purge', '', '');
$pmda->add_metric(pmda_pmid(0,50), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  'mysql.status.com_purge_before_date', '', '');
$pmda->add_metric(pmda_pmid(0,51), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  'mysql.status.com_rename_table', '', '');
$pmda->add_metric(pmda_pmid(0,52), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  'mysql.status.com_repair', '', '');
$pmda->add_metric(pmda_pmid(0,53), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  'mysql.status.com_replace', '', '');
$pmda->add_metric(pmda_pmid(0,54), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  'mysql.status.com_replace_select', '', '');
$pmda->add_metric(pmda_pmid(0,55), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  'mysql.status.com_reset', '', '');
$pmda->add_metric(pmda_pmid(0,56), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  'mysql.status.com_restore_table', '', '');
$pmda->add_metric(pmda_pmid(0,57), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  'mysql.status.com_revoke', '', '');
$pmda->add_metric(pmda_pmid(0,58), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  'mysql.status.com_revoke_all', '', '');
$pmda->add_metric(pmda_pmid(0,59), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  'mysql.status.com_rollback', '', '');
$pmda->add_metric(pmda_pmid(0,60), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  'mysql.status.com_savepoint', '', '');
$pmda->add_metric(pmda_pmid(0,61), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  'mysql.status.com_select', '', '');
$pmda->add_metric(pmda_pmid(0,62), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  'mysql.status.com_set_option', '', '');
$pmda->add_metric(pmda_pmid(0,63), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  'mysql.status.com_show_binlog_events', '', '');
$pmda->add_metric(pmda_pmid(0,64), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  'mysql.status.com_show_binlogs', '', '');
$pmda->add_metric(pmda_pmid(0,65), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  'mysql.status.com_show_charsets', '', '');
$pmda->add_metric(pmda_pmid(0,66), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  'mysql.status.com_show_collations', '', '');
$pmda->add_metric(pmda_pmid(0,67), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  'mysql.status.com_show_column_types', '', '');
$pmda->add_metric(pmda_pmid(0,68), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  'mysql.status.com_show_create_db', '', '');
$pmda->add_metric(pmda_pmid(0,69), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  'mysql.status.com_show_create_table', '', '');
$pmda->add_metric(pmda_pmid(0,70), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  'mysql.status.com_show_databases', '', '');
$pmda->add_metric(pmda_pmid(0,71), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  'mysql.status.com_show_errors', '', '');
$pmda->add_metric(pmda_pmid(0,72), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  'mysql.status.com_show_fields', '', '');
$pmda->add_metric(pmda_pmid(0,73), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  'mysql.status.com_show_grants', '', '');
$pmda->add_metric(pmda_pmid(0,74), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  'mysql.status.com_show_innodb_status', '', '');
$pmda->add_metric(pmda_pmid(0,75), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  'mysql.status.com_show_keys', '', '');
$pmda->add_metric(pmda_pmid(0,76), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  'mysql.status.com_show_logs', '', '');
$pmda->add_metric(pmda_pmid(0,77), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  'mysql.status.com_show_master_status', '', '');
$pmda->add_metric(pmda_pmid(0,78), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  'mysql.status.com_show_ndb_status', '', '');
$pmda->add_metric(pmda_pmid(0,79), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  'mysql.status.com_show_new_master', '', '');
$pmda->add_metric(pmda_pmid(0,80), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  'mysql.status.com_show_open_tables', '', '');
$pmda->add_metric(pmda_pmid(0,81), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  'mysql.status.com_show_privileges', '', '');
$pmda->add_metric(pmda_pmid(0,82), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  'mysql.status.com_show_processlist', '', '');
$pmda->add_metric(pmda_pmid(0,83), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  'mysql.status.com_show_slave_hosts', '', '');
$pmda->add_metric(pmda_pmid(0,84), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  'mysql.status.com_show_slave_status', '', '');
$pmda->add_metric(pmda_pmid(0,85), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  'mysql.status.com_show_status', '', '');
$pmda->add_metric(pmda_pmid(0,86), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  'mysql.status.com_show_storage_engines', '', '');
$pmda->add_metric(pmda_pmid(0,87), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  'mysql.status.com_show_tables', '', '');
$pmda->add_metric(pmda_pmid(0,88), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  'mysql.status.com_show_triggers', '', '');
$pmda->add_metric(pmda_pmid(0,89), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  'mysql.status.com_show_variables', '', '');
$pmda->add_metric(pmda_pmid(0,90), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  'mysql.status.com_show_warnings', '', '');
$pmda->add_metric(pmda_pmid(0,91), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  'mysql.status.com_slave_start', '', '');
$pmda->add_metric(pmda_pmid(0,92), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  'mysql.status.com_slave_stop', '', '');
$pmda->add_metric(pmda_pmid(0,93), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  'mysql.status.com_stmt_close', '', '');
$pmda->add_metric(pmda_pmid(0,94), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  'mysql.status.com_stmt_execute', '', '');
$pmda->add_metric(pmda_pmid(0,95), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  'mysql.status.com_stmt_fetch', '', '');
$pmda->add_metric(pmda_pmid(0,96), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  'mysql.status.com_stmt_prepare', '', '');
$pmda->add_metric(pmda_pmid(0,97), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  'mysql.status.com_stmt_reset', '', '');
$pmda->add_metric(pmda_pmid(0,98), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  'mysql.status.com_stmt_send_long_data', '', '');
$pmda->add_metric(pmda_pmid(0,99), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  'mysql.status.com_truncate', '', '');
$pmda->add_metric(pmda_pmid(0,100), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  'mysql.status.com_unlock_tables', '', '');
$pmda->add_metric(pmda_pmid(0,101), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  'mysql.status.com_update', '', '');
$pmda->add_metric(pmda_pmid(0,102), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  'mysql.status.com_update_multi', '', '');
$pmda->add_metric(pmda_pmid(0,103), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  'mysql.status.com_xa_commit', '', '');
$pmda->add_metric(pmda_pmid(0,104), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  'mysql.status.com_xa_end', '', '');
$pmda->add_metric(pmda_pmid(0,105), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  'mysql.status.com_xa_prepare', '', '');
$pmda->add_metric(pmda_pmid(0,106), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  'mysql.status.com_xa_recover', '', '');
$pmda->add_metric(pmda_pmid(0,107), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  'mysql.status.com_xa_rollback', '', '');
$pmda->add_metric(pmda_pmid(0,108), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  'mysql.status.com_xa_start', '', '');
$pmda->add_metric(pmda_pmid(0,109), PM_TYPE_STRING, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'mysql.status.compression', '', '');
$pmda->add_metric(pmda_pmid(0,110), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  'mysql.status.connections', '', '');
$pmda->add_metric(pmda_pmid(0,111), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  'mysql.status.created_tmp_disk_tables', '', '');
$pmda->add_metric(pmda_pmid(0,112), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  'mysql.status.created_tmp_files', '', '');
$pmda->add_metric(pmda_pmid(0,113), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  'mysql.status.created_tmp_tables', '', '');
$pmda->add_metric(pmda_pmid(0,114), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  'mysql.status.delayed_errors', '', '');
$pmda->add_metric(pmda_pmid(0,115), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  'mysql.status.delayed_insert_threads', '', '');
$pmda->add_metric(pmda_pmid(0,116), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  'mysql.status.delayed_writes', '', '');
$pmda->add_metric(pmda_pmid(0,117), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  'mysql.status.flush_commands', '', '');
$pmda->add_metric(pmda_pmid(0,118), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  'mysql.status.handler_commit', '', '');
$pmda->add_metric(pmda_pmid(0,119), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  'mysql.status.handler_delete', '', '');
$pmda->add_metric(pmda_pmid(0,120), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  'mysql.status.handler_discover', '', '');
$pmda->add_metric(pmda_pmid(0,121), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  'mysql.status.handler_prepare', '', '');
$pmda->add_metric(pmda_pmid(0,122), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  'mysql.status.handler_read_first', '', '');
$pmda->add_metric(pmda_pmid(0,123), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  'mysql.status.handler_read_key', '', '');
$pmda->add_metric(pmda_pmid(0,124), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  'mysql.status.handler_read_next', '', '');
$pmda->add_metric(pmda_pmid(0,125), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  'mysql.status.handler_read_prev', '', '');
$pmda->add_metric(pmda_pmid(0,126), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  'mysql.status.handler_read_rnd', '', '');
$pmda->add_metric(pmda_pmid(0,127), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  'mysql.status.handler_read_rnd_next', '', '');
$pmda->add_metric(pmda_pmid(0,128), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  'mysql.status.handler_rollback', '', '');
$pmda->add_metric(pmda_pmid(0,129), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  'mysql.status.handler_savepoint', '', '');
$pmda->add_metric(pmda_pmid(0,130), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  'mysql.status.handler_savepoint_rollback', '', '');
$pmda->add_metric(pmda_pmid(0,131), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  'mysql.status.handler_update', '', '');
$pmda->add_metric(pmda_pmid(0,132), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  'mysql.status.handler_write', '', '');
$pmda->add_metric(pmda_pmid(0,133), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  'mysql.status.innodb_buffer_pool_pages_data', '', '');
$pmda->add_metric(pmda_pmid(0,134), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  'mysql.status.innodb_buffer_pool_pages_dirty', '', '');
$pmda->add_metric(pmda_pmid(0,135), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  'mysql.status.innodb_buffer_pool_pages_flushed', '', '');
$pmda->add_metric(pmda_pmid(0,136), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  'mysql.status.innodb_buffer_pool_pages_free', '', '');
$pmda->add_metric(pmda_pmid(0,137), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  'mysql.status.innodb_buffer_pool_pages_latched', '', '');
$pmda->add_metric(pmda_pmid(0,138), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  'mysql.status.innodb_buffer_pool_pages_misc', '', '');
$pmda->add_metric(pmda_pmid(0,139), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  'mysql.status.innodb_buffer_pool_pages_total', '', '');
$pmda->add_metric(pmda_pmid(0,140), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  'mysql.status.innodb_buffer_pool_read_ahead_rnd', '', '');
$pmda->add_metric(pmda_pmid(0,141), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  'mysql.status.innodb_buffer_pool_read_ahead_seq', '', '');
$pmda->add_metric(pmda_pmid(0,142), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  'mysql.status.innodb_buffer_pool_read_requests', '', '');
$pmda->add_metric(pmda_pmid(0,143), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  'mysql.status.innodb_buffer_pool_reads', '', '');
$pmda->add_metric(pmda_pmid(0,144), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  'mysql.status.innodb_buffer_pool_wait_free', '', '');
$pmda->add_metric(pmda_pmid(0,145), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  'mysql.status.innodb_buffer_pool_write_requests', '', '');
$pmda->add_metric(pmda_pmid(0,146), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  'mysql.status.innodb_data_fsyncs', '', '');
$pmda->add_metric(pmda_pmid(0,147), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  'mysql.status.innodb_data_pending_fsyncs', '', '');
$pmda->add_metric(pmda_pmid(0,148), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  'mysql.status.innodb_data_pending_reads', '', '');
$pmda->add_metric(pmda_pmid(0,149), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  'mysql.status.innodb_data_pending_writes', '', '');
$pmda->add_metric(pmda_pmid(0,150), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_COUNTER, pmda_units(1,0,0,PM_SPACE_BYTE,0,0),
		  'mysql.status.innodb_data_read', '', '');
$pmda->add_metric(pmda_pmid(0,151), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  'mysql.status.innodb_data_reads', '', '');
$pmda->add_metric(pmda_pmid(0,152), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  'mysql.status.innodb_data_writes', '', '');
$pmda->add_metric(pmda_pmid(0,153), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_COUNTER, pmda_units(1,0,0,PM_SPACE_BYTE,0,0),
		  'mysql.status.innodb_data_written', '', '');
$pmda->add_metric(pmda_pmid(0,154), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  'mysql.status.innodb_dblwr_pages_written', '', '');
$pmda->add_metric(pmda_pmid(0,155), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  'mysql.status.innodb_dblwr_writes', '', '');
$pmda->add_metric(pmda_pmid(0,156), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  'mysql.status.innodb_log_waits', '', '');
$pmda->add_metric(pmda_pmid(0,157), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  'mysql.status.innodb_log_write_requests', '', '');
$pmda->add_metric(pmda_pmid(0,158), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  'mysql.status.innodb_log_writes', '', '');
$pmda->add_metric(pmda_pmid(0,159), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  'mysql.status.innodb_os_log_fsyncs', '', '');
$pmda->add_metric(pmda_pmid(0,160), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  'mysql.status.innodb_os_log_pending_fsyncs', '', '');
$pmda->add_metric(pmda_pmid(0,161), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  'mysql.status.innodb_os_log_pending_writes', '', '');
$pmda->add_metric(pmda_pmid(0,162), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_COUNTER, pmda_units(1,0,0,PM_SPACE_BYTE,0,0),
		  'mysql.status.innodb_os_log_written', '', '');
$pmda->add_metric(pmda_pmid(0,163), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(1,0,0,PM_SPACE_BYTE,0,0),
		  'mysql.status.innodb_page_size', '', '');
$pmda->add_metric(pmda_pmid(0,164), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  'mysql.status.innodb_pages_created', '', '');
$pmda->add_metric(pmda_pmid(0,165), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  'mysql.status.innodb_pages_read', '', '');
$pmda->add_metric(pmda_pmid(0,166), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  'mysql.status.innodb_pages_written', '', '');
$pmda->add_metric(pmda_pmid(0,167), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  'mysql.status.innodb_row_lock_current_waits', '', '');
$pmda->add_metric(pmda_pmid(0,168), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_COUNTER, pmda_units(0,1,0,0,PM_TIME_MSEC,0),
		  'mysql.status.innodb_row_lock_time', '', '');
$pmda->add_metric(pmda_pmid(0,169), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,1,0,0,PM_TIME_MSEC,0),
		  'mysql.status.innodb_row_lock_time_avg', '', '');
$pmda->add_metric(pmda_pmid(0,170), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,1,0,0,PM_TIME_MSEC,0),
		  'mysql.status.innodb_row_lock_time_max', '', '');
$pmda->add_metric(pmda_pmid(0,171), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  'mysql.status.innodb_row_lock_waits', '', '');
$pmda->add_metric(pmda_pmid(0,172), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  'mysql.status.innodb_rows_deleted', '', '');
$pmda->add_metric(pmda_pmid(0,173), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  'mysql.status.innodb_rows_inserted', '', '');
$pmda->add_metric(pmda_pmid(0,174), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  'mysql.status.innodb_rows_read', '', '');
$pmda->add_metric(pmda_pmid(0,175), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  'mysql.status.innodb_rows_updated', '', '');
$pmda->add_metric(pmda_pmid(0,176), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  'mysql.status.key_blocks_not_flushed', '', '');
$pmda->add_metric(pmda_pmid(0,177), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  'mysql.status.key_blocks_unused', '', '');
$pmda->add_metric(pmda_pmid(0,178), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  'mysql.status.key_blocks_used', '', '');
$pmda->add_metric(pmda_pmid(0,179), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  'mysql.status.key_read_requests', '', '');
$pmda->add_metric(pmda_pmid(0,180), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  'mysql.status.key_reads', '', '');
$pmda->add_metric(pmda_pmid(0,181), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  'mysql.status.key_write_requests', '', '');
$pmda->add_metric(pmda_pmid(0,182), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  'mysql.status.key_writes', '', '');
$pmda->add_metric(pmda_pmid(0,183), PM_TYPE_DOUBLE, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'mysql.status.last_query_cost', '', '');
$pmda->add_metric(pmda_pmid(0,184), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'mysql.status.max_used_connections', '', ''); 
$pmda->add_metric(pmda_pmid(0,185), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'mysql.status.ndb_cluster_node_id', '', ''); 
$pmda->add_metric(pmda_pmid(0,186), PM_TYPE_STRING, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'mysql.status.ndb_config_from_host', '', ''); 
$pmda->add_metric(pmda_pmid(0,187), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'mysql.status.ndb_config_from_port', '', ''); 
$pmda->add_metric(pmda_pmid(0,188), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'mysql.status.ndb_number_of_data_nodes', '', ''); 
$pmda->add_metric(pmda_pmid(0,189), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'mysql.status.not_flushed_delayed_rows', '', '');
$pmda->add_metric(pmda_pmid(0,190), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'mysql.status.open_files', '', '');
$pmda->add_metric(pmda_pmid(0,191), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'mysql.status.open_streams', '', '');
$pmda->add_metric(pmda_pmid(0,192), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'mysql.status.open_tables', '', '');
$pmda->add_metric(pmda_pmid(0,193), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  'mysql.status.opened_tables', '', '');
$pmda->add_metric(pmda_pmid(0,194), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'mysql.status.prepared_stmt_count', '', '');
$pmda->add_metric(pmda_pmid(0,195), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'mysql.status.qcache_free_blocks', '', '');
$pmda->add_metric(pmda_pmid(0,196), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(1,0,0,PM_SPACE_BYTE,0,0),
		  'mysql.status.qcache_free_memory', '', '');
$pmda->add_metric(pmda_pmid(0,197), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  'mysql.status.qcache_hits', '', '');
$pmda->add_metric(pmda_pmid(0,198), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  'mysql.status.qcache_inserts', '', '');
$pmda->add_metric(pmda_pmid(0,199), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  'mysql.status.qcache_lowmem_prunes', '', '');
$pmda->add_metric(pmda_pmid(0,200), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  'mysql.status.qcache_not_cached', '', '');
$pmda->add_metric(pmda_pmid(0,201), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  'mysql.status.qcache_queries_in_cache', '', '');
$pmda->add_metric(pmda_pmid(0,202), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  'mysql.status.qcache_total_blocks', '', '');
$pmda->add_metric(pmda_pmid(0,203), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  'mysql.status.questions', '', '');
$pmda->add_metric(pmda_pmid(0,204), PM_TYPE_STRING, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'mysql.status.rpl_status', '', '');
$pmda->add_metric(pmda_pmid(0,205), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  'mysql.status.select_full_join', '', '');
$pmda->add_metric(pmda_pmid(0,206), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  'mysql.status.select_full_range_join', '', '');
$pmda->add_metric(pmda_pmid(0,207), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  'mysql.status.select_range', '', '');
$pmda->add_metric(pmda_pmid(0,208), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  'mysql.status.select_range_check', '', '');
$pmda->add_metric(pmda_pmid(0,209), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  'mysql.status.select_scan', '', '');
$pmda->add_metric(pmda_pmid(0,210), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  'mysql.status.slave_open_temp_tables', '', '');
$pmda->add_metric(pmda_pmid(0,211), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  'mysql.status.slave_retried_transactions', '', '');
$pmda->add_metric(pmda_pmid(0,212), PM_TYPE_STRING, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'mysql.status.slave_running', '', '');
$pmda->add_metric(pmda_pmid(0,213), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  'mysql.status.slow_launch_threads', '', '');
$pmda->add_metric(pmda_pmid(0,214), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  'mysql.status.slow_queries', '', '');
$pmda->add_metric(pmda_pmid(0,215), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  'mysql.status.sort_merge_passes', '', '');
$pmda->add_metric(pmda_pmid(0,216), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  'mysql.status.sort_range', '', '');
$pmda->add_metric(pmda_pmid(0,217), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  'mysql.status.sort_rows', '', '');
$pmda->add_metric(pmda_pmid(0,218), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  'mysql.status.sort_scan', '', '');
$pmda->add_metric(pmda_pmid(0,219), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'mysql.status.ssl_accept_renegotiates', '', '');
$pmda->add_metric(pmda_pmid(0,220), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  'mysql.status.ssl_accepts', '', '');
$pmda->add_metric(pmda_pmid(0,221), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  'mysql.status.ssl_callback_cache_hits', '', '');
$pmda->add_metric(pmda_pmid(0,222), PM_TYPE_STRING, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'mysql.status.ssl_cipher', '', '');
$pmda->add_metric(pmda_pmid(0,223), PM_TYPE_STRING, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'mysql.status.ssl_cipher_list', '', '');
$pmda->add_metric(pmda_pmid(0,224), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  'mysql.status.ssl_client_connects', '', '');
$pmda->add_metric(pmda_pmid(0,225), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  'mysql.status.ssl_connect_renegotiates', '', '');
$pmda->add_metric(pmda_pmid(0,226), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'mysql.status.ssl_ctx_verify_depth', '', '');
$pmda->add_metric(pmda_pmid(0,227), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'mysql.status.ssl_ctx_verify_mode', '', '');
$pmda->add_metric(pmda_pmid(0,228), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,1,0,0,PM_TIME_SEC,0),
		  'mysql.status.ssl_default_timeout', '', '');
$pmda->add_metric(pmda_pmid(0,229), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  'mysql.status.ssl_finished_accepts', '', '');
$pmda->add_metric(pmda_pmid(0,230), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  'mysql.status.ssl_finished_connects', '', '');
$pmda->add_metric(pmda_pmid(0,231), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  'mysql.status.ssl_session_cache_hits', '', '');
$pmda->add_metric(pmda_pmid(0,232), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  'mysql.status.ssl_session_cache_misses', '', '');
$pmda->add_metric(pmda_pmid(0,233), PM_TYPE_STRING, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'mysql.status.ssl_session_cache_mode', '', '');
$pmda->add_metric(pmda_pmid(0,234), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  'mysql.status.ssl_session_cache_overflows', '', '');
$pmda->add_metric(pmda_pmid(0,235), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'mysql.status.ssl_session_cache_size', '', '');
$pmda->add_metric(pmda_pmid(0,236), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  'mysql.status.ssl_session_cache_timeouts', '', '');
$pmda->add_metric(pmda_pmid(0,237), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  'mysql.status.ssl_sessions_reused', '', '');
$pmda->add_metric(pmda_pmid(0,238), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'mysql.status.ssl_used_session_cache_entries', '', '');
$pmda->add_metric(pmda_pmid(0,239), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'mysql.status.ssl_verify_depth', '', '');
$pmda->add_metric(pmda_pmid(0,240), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'mysql.status.ssl_verify_mode', '', '');
$pmda->add_metric(pmda_pmid(0,241), PM_TYPE_STRING, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'mysql.status.ssl_version', '', '');
$pmda->add_metric(pmda_pmid(0,242), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  'mysql.status.table_locks_immediate', '', '');
$pmda->add_metric(pmda_pmid(0,243), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  'mysql.status.table_locks_waited', '', '');
$pmda->add_metric(pmda_pmid(0,244), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'mysql.status.tc_log_max_pages_used', '', '');
$pmda->add_metric(pmda_pmid(0,245), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'mysql.status.tc_log_page_size', '', '');
$pmda->add_metric(pmda_pmid(0,246), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  'mysql.status.tc_log_page_waits', '', '');
$pmda->add_metric(pmda_pmid(0,247), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'mysql.status.threads_cached', '', '');
$pmda->add_metric(pmda_pmid(0,248), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'mysql.status.threads_connected', '', '');
$pmda->add_metric(pmda_pmid(0,249), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'mysql.status.threads_created', '', '');
$pmda->add_metric(pmda_pmid(0,250), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'mysql.status.threads_running', '', '');
$pmda->add_metric(pmda_pmid(0,251), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,1,0,0,PM_TIME_SEC,0),
		  'mysql.status.uptime', '', '');
$pmda->add_metric(pmda_pmid(0,252), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,1,0,0,PM_TIME_SEC,0),
		  'mysql.status.uptime_since_flush_status', '', '');
$pmda->add_metric(pmda_pmid(0,253), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'mysql.status.slave_running_num', '', '');
$pmda->add_metric(pmda_pmid(0,254), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'mysql.status.compression_num', '', '');

$pmda->add_metric(pmda_pmid(1,0), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'mysql.variables.auto_increment_increment', '', '');
$pmda->add_metric(pmda_pmid(1,1), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'mysql.variables.auto_increment_offset', '', '');
$pmda->add_metric(pmda_pmid(1,2), PM_TYPE_STRING, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'mysql.variables.automatic_sp_privileges', '', '');
$pmda->add_metric(pmda_pmid(1,3), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'mysql.variables.back_log', '', '');
$pmda->add_metric(pmda_pmid(1,4), PM_TYPE_STRING, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'mysql.variables.basedir', '', '');
$pmda->add_metric(pmda_pmid(1,5), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'mysql.variables.binlog_cache_size', '', '');
$pmda->add_metric(pmda_pmid(1,5), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'mysql.variables.bulk_insert_buffer_size', '', '');
$pmda->add_metric(pmda_pmid(1,6), PM_TYPE_STRING, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'mysql.variables.character_set_client', '', '');
$pmda->add_metric(pmda_pmid(1,7), PM_TYPE_STRING, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'mysql.variables.character_set_connection', '', '');
$pmda->add_metric(pmda_pmid(1,8), PM_TYPE_STRING, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'mysql.variables.character_set_database', '', '');
$pmda->add_metric(pmda_pmid(1,9), PM_TYPE_STRING, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'mysql.variables.character_set_filesystem', '', '');
$pmda->add_metric(pmda_pmid(1,10), PM_TYPE_STRING, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'mysql.variables.character_set_results', '', '');
$pmda->add_metric(pmda_pmid(1,11), PM_TYPE_STRING, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'mysql.variables.character_set_server', '', '');
$pmda->add_metric(pmda_pmid(1,12), PM_TYPE_STRING, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'mysql.variables.character_set_system', '', '');
$pmda->add_metric(pmda_pmid(1,13), PM_TYPE_STRING, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'mysql.variables.character_sets_dir', '', '');
$pmda->add_metric(pmda_pmid(1,14), PM_TYPE_STRING, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'mysql.variables.collation_connection', '', '');
$pmda->add_metric(pmda_pmid(1,15), PM_TYPE_STRING, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'mysql.variables.collation_database', '', '');
$pmda->add_metric(pmda_pmid(1,16), PM_TYPE_STRING, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'mysql.variables.collation_server', '', '');
$pmda->add_metric(pmda_pmid(1,17), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'mysql.variables.completion_type', '', '');
$pmda->add_metric(pmda_pmid(1,18), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'mysql.variables.concurrent_insert', '', '');
$pmda->add_metric(pmda_pmid(1,19), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,1,0,0,PM_TIME_SEC,0),
		  'mysql.variables.connect_timeout', '', '');
$pmda->add_metric(pmda_pmid(1,20), PM_TYPE_STRING, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'mysql.variables.datadir', '', '');
$pmda->add_metric(pmda_pmid(1,21), PM_TYPE_STRING, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'mysql.variables.date_format', '', '');
$pmda->add_metric(pmda_pmid(1,22), PM_TYPE_STRING, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'mysql.variables.datetime_format', '', '');
$pmda->add_metric(pmda_pmid(1,23), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'mysql.variables.default_week_format', '', '');
$pmda->add_metric(pmda_pmid(1,24), PM_TYPE_STRING, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'mysql.variables.delay_key_write', '', '');
$pmda->add_metric(pmda_pmid(1,25), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'mysql.variables.delayed_insert_limit', '', '');
$pmda->add_metric(pmda_pmid(1,26), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,1,0,0,PM_TIME_SEC,0),
		  'mysql.variables.delayed_insert_timeout', '', '');
$pmda->add_metric(pmda_pmid(1,27), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'mysql.variables.delayed_queue_size', '', '');
$pmda->add_metric(pmda_pmid(1,28), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'mysql.variables.div_precision_increment', '', '');
$pmda->add_metric(pmda_pmid(1,29), PM_TYPE_STRING, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'mysql.variables.keep_files_on_create', '', '');
$pmda->add_metric(pmda_pmid(1,30), PM_TYPE_STRING, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'mysql.variables.engine_condition_pushdown', '', '');
$pmda->add_metric(pmda_pmid(1,31), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'mysql.variables.expire_logs_days', '', '');
$pmda->add_metric(pmda_pmid(1,32), PM_TYPE_STRING, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'mysql.variables.flush', '', '');
$pmda->add_metric(pmda_pmid(1,33), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,1,0,0,PM_TIME_SEC,0),
		  'mysql.variables.flush_time', '', '');
$pmda->add_metric(pmda_pmid(1,34), PM_TYPE_STRING, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'mysql.variables.ft_boolean_syntax', '', '');
$pmda->add_metric(pmda_pmid(1,35), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'mysql.variables.ft_max_word_len', '', '');
$pmda->add_metric(pmda_pmid(1,35), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'mysql.variables.ft_min_word_len', '', '');
$pmda->add_metric(pmda_pmid(1,35), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'mysql.variables.ft_query_expansion_limit', '', '');
$pmda->add_metric(pmda_pmid(1,36), PM_TYPE_STRING, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'mysql.variables.ft_stopword_file', '', '');
$pmda->add_metric(pmda_pmid(1,37), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'mysql.variables.group_concat_max_len', '', '');
$pmda->add_metric(pmda_pmid(1,38), PM_TYPE_STRING, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'mysql.variables.have_archive', '', '');
$pmda->add_metric(pmda_pmid(1,39), PM_TYPE_STRING, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'mysql.variables.have_bdb', '', '');
$pmda->add_metric(pmda_pmid(1,40), PM_TYPE_STRING, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'mysql.variables.have_blackhole_engine', '', '');
$pmda->add_metric(pmda_pmid(1,41), PM_TYPE_STRING, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'mysql.variables.have_compress', '', '');
$pmda->add_metric(pmda_pmid(1,42), PM_TYPE_STRING, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'mysql.variables.have_crypt', '', '');
$pmda->add_metric(pmda_pmid(1,43), PM_TYPE_STRING, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'mysql.variables.have_csv', '', '');
$pmda->add_metric(pmda_pmid(1,44), PM_TYPE_STRING, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'mysql.variables.have_dynamic_loading', '', '');
$pmda->add_metric(pmda_pmid(1,45), PM_TYPE_STRING, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'mysql.variables.have_example_engine', '', '');
$pmda->add_metric(pmda_pmid(1,46), PM_TYPE_STRING, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'mysql.variables.have_federated_engine', '', '');
$pmda->add_metric(pmda_pmid(1,47), PM_TYPE_STRING, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'mysql.variables.have_geometry', '', '');
$pmda->add_metric(pmda_pmid(1,48), PM_TYPE_STRING, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'mysql.variables.have_innodb', '', '');
$pmda->add_metric(pmda_pmid(1,49), PM_TYPE_STRING, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'mysql.variables.have_isam', '', '');
$pmda->add_metric(pmda_pmid(1,50), PM_TYPE_STRING, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'mysql.variables.have_merge_engine', '', '');
$pmda->add_metric(pmda_pmid(1,51), PM_TYPE_STRING, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'mysql.variables.have_ndbcluster', '', '');
$pmda->add_metric(pmda_pmid(1,52), PM_TYPE_STRING, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'mysql.variables.have_openssl', '', '');
$pmda->add_metric(pmda_pmid(1,53), PM_TYPE_STRING, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'mysql.variables.have_ssl', '', '');
$pmda->add_metric(pmda_pmid(1,54), PM_TYPE_STRING, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'mysql.variables.have_query_cache', '', '');
$pmda->add_metric(pmda_pmid(1,55), PM_TYPE_STRING, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'mysql.variables.have_raid', '', '');
$pmda->add_metric(pmda_pmid(1,56), PM_TYPE_STRING, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'mysql.variables.have_rtree_keys', '', '');
$pmda->add_metric(pmda_pmid(1,57), PM_TYPE_STRING, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'mysql.variables.have_symlink', '', '');
$pmda->add_metric(pmda_pmid(1,58), PM_TYPE_STRING, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'mysql.variables.hostname', '', '');
$pmda->add_metric(pmda_pmid(1,59), PM_TYPE_STRING, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'mysql.variables.init_connect', '', '');
$pmda->add_metric(pmda_pmid(1,60), PM_TYPE_STRING, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'mysql.variables.init_file', '', '');
$pmda->add_metric(pmda_pmid(1,61), PM_TYPE_STRING, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'mysql.variables.init_slave', '', '');
$pmda->add_metric(pmda_pmid(1,63), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'mysql.variables.innodb_additional_mem_pool_size', '', '');
$pmda->add_metric(pmda_pmid(1,64), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'mysql.variables.innodb_autoextend_increment', '', '');
$pmda->add_metric(pmda_pmid(1,65), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(1,0,0,PM_SPACE_MBYTE,0,0),
		  'mysql.variables.innodb_buffer_pool_awe_mem_mb', '', '');
$pmda->add_metric(pmda_pmid(1,66), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'mysql.variables.innodb_buffer_pool_size', '', '');
$pmda->add_metric(pmda_pmid(1,67), PM_TYPE_STRING, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'mysql.variables.innodb_checksums', '', '');
$pmda->add_metric(pmda_pmid(1,68), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'mysql.variables.innodb_commit_concurrency', '', '');
$pmda->add_metric(pmda_pmid(1,68), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'mysql.variables.innodb_concurrency_tickets', '', '');
$pmda->add_metric(pmda_pmid(1,69), PM_TYPE_STRING, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'mysql.variables.innodb_data_file_path', '', '');
$pmda->add_metric(pmda_pmid(1,70), PM_TYPE_STRING, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'mysql.variables.innodb_data_home_dir', '', '');
$pmda->add_metric(pmda_pmid(1,71), PM_TYPE_STRING, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'mysql.variables.innodb_doublewrite', '', '');
$pmda->add_metric(pmda_pmid(1,72), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'mysql.variables.innodb_fast_shutdown', '', '');
$pmda->add_metric(pmda_pmid(1,73), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'mysql.variables.innodb_file_io_threads', '', '');
$pmda->add_metric(pmda_pmid(1,74), PM_TYPE_STRING, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'mysql.variables.innodb_file_per_table', '', '');
$pmda->add_metric(pmda_pmid(1,75), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'mysql.variables.innodb_flush_log_at_trx_commit', '', '');
$pmda->add_metric(pmda_pmid(1,76), PM_TYPE_STRING, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'mysql.variables.innodb_flush_method', '', '');
$pmda->add_metric(pmda_pmid(1,77), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'mysql.variables.innodb_force_recovery', '', '');
$pmda->add_metric(pmda_pmid(1,78), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'mysql.variables.innodb_lock_wait_timeout', '', '');
$pmda->add_metric(pmda_pmid(1,79), PM_TYPE_STRING, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'mysql.variables.innodb_locks_unsafe_for_binlog', '', '');
$pmda->add_metric(pmda_pmid(1,80), PM_TYPE_STRING, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'mysql.variables.innodb_log_arch_dir', '', '');
$pmda->add_metric(pmda_pmid(1,81), PM_TYPE_STRING, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'mysql.variables.innodb_log_archive', '', '');
$pmda->add_metric(pmda_pmid(1,82), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'mysql.variables.innodb_log_buffer_size', '', '');
$pmda->add_metric(pmda_pmid(1,83), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'mysql.variables.innodb_log_file_size', '', '');
$pmda->add_metric(pmda_pmid(1,84), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'mysql.variables.innodb_log_files_in_group', '', '');
$pmda->add_metric(pmda_pmid(1,85), PM_TYPE_STRING, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'mysql.variables.innodb_log_group_home_dir', '', '');
$pmda->add_metric(pmda_pmid(1,86), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'mysql.variables.innodb_max_dirty_pages_pct', '', '');
$pmda->add_metric(pmda_pmid(1,87), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'mysql.variables.innodb_max_purge_lag', '', '');
$pmda->add_metric(pmda_pmid(1,88), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'mysql.variables.innodb_mirrored_log_groups', '', '');
$pmda->add_metric(pmda_pmid(1,89), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'mysql.variables.innodb_open_files', '', '');
$pmda->add_metric(pmda_pmid(1,90), PM_TYPE_STRING, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'mysql.variables.innodb_rollback_on_timeout', '', '');
$pmda->add_metric(pmda_pmid(1,91), PM_TYPE_STRING, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'mysql.variables.innodb_support_xa', '', '');
$pmda->add_metric(pmda_pmid(1,92), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'mysql.variables.innodb_sync_spin_loops', '', '');
$pmda->add_metric(pmda_pmid(1,93), PM_TYPE_STRING, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'mysql.variables.innodb_table_locks', '', '');
$pmda->add_metric(pmda_pmid(1,94), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'mysql.variables.innodb_thread_concurrency', '', '');
$pmda->add_metric(pmda_pmid(1,95), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'mysql.variables.innodb_thread_sleep_delay', '', '');
$pmda->add_metric(pmda_pmid(1,96), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'mysql.variables.interactive_timeout', '', '');
$pmda->add_metric(pmda_pmid(1,97), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(1,0,0,PM_SPACE_BYTE,0,0),
		  'mysql.variables.join_buffer_size', '', '');
$pmda->add_metric(pmda_pmid(1,98), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(1,0,0,PM_SPACE_BYTE,0,0),
		  'mysql.variables.key_buffer_size', '', '');
$pmda->add_metric(pmda_pmid(1,99), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'mysql.variables.key_cache_age_threshold', '', '');
$pmda->add_metric(pmda_pmid(1,100), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'mysql.variables.key_cache_block_size', '', '');
$pmda->add_metric(pmda_pmid(1,101), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'mysql.variables.key_cache_division_limit', '', '');
$pmda->add_metric(pmda_pmid(1,102), PM_TYPE_STRING, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'mysql.variables.language', '', '');
$pmda->add_metric(pmda_pmid(1,103), PM_TYPE_STRING, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'mysql.variables.large_files_support', '', '');
$pmda->add_metric(pmda_pmid(1,104), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(1,0,0,PM_SPACE_BYTE,0,0),
		  'mysql.variables.large_page_size', '', '');
$pmda->add_metric(pmda_pmid(1,105), PM_TYPE_STRING, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'mysql.variables.large_pages', '', '');
$pmda->add_metric(pmda_pmid(1,106), PM_TYPE_STRING, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'mysql.variables.lc_time_names', '', '');
$pmda->add_metric(pmda_pmid(1,107), PM_TYPE_STRING, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'mysql.variables.license', '', '');
$pmda->add_metric(pmda_pmid(1,108), PM_TYPE_STRING, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'mysql.variables.local_infile', '', '');
$pmda->add_metric(pmda_pmid(1,109), PM_TYPE_STRING, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'mysql.variables.locked_in_memory', '', '');
$pmda->add_metric(pmda_pmid(1,110), PM_TYPE_STRING, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'mysql.variables.log', '', '');
$pmda->add_metric(pmda_pmid(1,111), PM_TYPE_STRING, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'mysql.variables.log_bin', '', '');
$pmda->add_metric(pmda_pmid(1,112), PM_TYPE_STRING, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'mysql.variables.log_bin_trust_function_creators', '', '');
$pmda->add_metric(pmda_pmid(1,113), PM_TYPE_STRING, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'mysql.variables.log_error', '', '');
$pmda->add_metric(pmda_pmid(1,114), PM_TYPE_STRING, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'mysql.variables.log_queries_not_using_indexes', '', '');
$pmda->add_metric(pmda_pmid(1,115), PM_TYPE_STRING, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'mysql.variables.log_slave_updates', '', '');
$pmda->add_metric(pmda_pmid(1,116), PM_TYPE_STRING, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'mysql.variables.log_slow_queries', '', '');
$pmda->add_metric(pmda_pmid(1,117), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'mysql.variables.log_warnings', '', '');
$pmda->add_metric(pmda_pmid(1,118), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,1,0,0,PM_TIME_SEC,0),
		  'mysql.variables.long_query_time', '', '');
$pmda->add_metric(pmda_pmid(1,119), PM_TYPE_STRING, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'mysql.variables.low_priority_updates', '', '');
$pmda->add_metric(pmda_pmid(1,120), PM_TYPE_STRING, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'mysql.variables.lower_case_file_system', '', '');
$pmda->add_metric(pmda_pmid(1,121), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'mysql.variables.lower_case_table_names', '', '');
$pmda->add_metric(pmda_pmid(1,122), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(1,0,0,PM_SPACE_BYTE,0,0),
		  'mysql.variables.max_allowed_packet', '', '');
$pmda->add_metric(pmda_pmid(1,124), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(1,0,0,PM_SPACE_BYTE,0,0),
		  'mysql.variables.max_binlog_cache_size', '', '');
$pmda->add_metric(pmda_pmid(1,125), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(1,0,0,PM_SPACE_BYTE,0,0),
		  'mysql.variables.max_binlog_size', '', '');
$pmda->add_metric(pmda_pmid(1,126), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'mysql.variables.max_connect_errors', '', '');
$pmda->add_metric(pmda_pmid(1,127), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'mysql.variables.max_connections', '', '');
$pmda->add_metric(pmda_pmid(1,128), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'mysql.variables.max_delayed_threads', '', '');
$pmda->add_metric(pmda_pmid(1,129), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'mysql.variables.max_error_count', '', '');
$pmda->add_metric(pmda_pmid(1,130), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'mysql.variables.max_heap_table_size', '', '');
$pmda->add_metric(pmda_pmid(1,131), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'mysql.variables.max_insert_delayed_threads', '', '');
$pmda->add_metric(pmda_pmid(1,132), PM_TYPE_U64, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'mysql.variables.max_join_size', '', '');
$pmda->add_metric(pmda_pmid(1,133), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'mysql.variables.max_length_for_sort_data', '', '');
$pmda->add_metric(pmda_pmid(1,134), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'mysql.variables.max_prepared_stmt_count', '', '');
$pmda->add_metric(pmda_pmid(1,135), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'mysql.variables.max_relay_log_size', '', '');
$pmda->add_metric(pmda_pmid(1,136), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'mysql.variables.max_seeks_for_key', '', '');
$pmda->add_metric(pmda_pmid(1,137), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'mysql.variables.max_sort_length', '', '');
$pmda->add_metric(pmda_pmid(1,138), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'mysql.variables.max_sp_recursion_depth', '', '');
$pmda->add_metric(pmda_pmid(1,139), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'mysql.variables.max_tmp_tables', '', '');
$pmda->add_metric(pmda_pmid(1,140), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'mysql.variables.max_user_connections', '', '');
$pmda->add_metric(pmda_pmid(1,141), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'mysql.variables.max_write_lock_count', '', '');
$pmda->add_metric(pmda_pmid(1,142), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'mysql.variables.multi_range_count', '', '');
$pmda->add_metric(pmda_pmid(1,143), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(1,0,0,PM_SPACE_BYTE,0,0),
		  'mysql.variables.myisam_data_pointer_size', '', '');
$pmda->add_metric(pmda_pmid(1,144), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(1,0,0,PM_SPACE_BYTE,0,0),
		  'mysql.variables.myisam_max_sort_file_size', '', '');
$pmda->add_metric(pmda_pmid(1,145), PM_TYPE_STRING, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'mysql.variables.myisam_recover_options', '', '');
$pmda->add_metric(pmda_pmid(1,146), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'mysql.variables.myisam_repair_threads', '', '');
$pmda->add_metric(pmda_pmid(1,147), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(1,0,0,PM_SPACE_BYTE,0,0),
		  'mysql.variables.myisam_sort_buffer_size', '', '');
$pmda->add_metric(pmda_pmid(1,148), PM_TYPE_STRING, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'mysql.variables.myisam_stats_method', '', '');
$pmda->add_metric(pmda_pmid(1,149), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(1,0,0,PM_SPACE_BYTE,0,0),
		  'mysql.variables.ndb_autoincrement_prefetch_sz', '', '');
$pmda->add_metric(pmda_pmid(1,150), PM_TYPE_STRING, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'mysql.variables.ndb_force_send', '', '');
$pmda->add_metric(pmda_pmid(1,151), PM_TYPE_STRING, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'mysql.variables.ndb_use_exact_count', '', '');
$pmda->add_metric(pmda_pmid(1,152), PM_TYPE_STRING, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'mysql.variables.ndb_use_transactions', '', '');
$pmda->add_metric(pmda_pmid(1,153), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,1,0,0,PM_TIME_SEC,0),
		  'mysql.variables.ndb_cache_check_time', '', '');
$pmda->add_metric(pmda_pmid(1,154), PM_TYPE_STRING, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'mysql.variables.ndb_connectstring', '', '');
$pmda->add_metric(pmda_pmid(1,155), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(1,0,0,PM_SPACE_BYTE,0,0),
		  'mysql.variables.net_buffer_length', '', '');
$pmda->add_metric(pmda_pmid(1,156), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,1,0,0,PM_TIME_SEC,0),
		  'mysql.variables.net_read_timeout', '', '');
$pmda->add_metric(pmda_pmid(1,157), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,1,0,0,PM_TIME_SEC,0),
		  'mysql.variables.net_retry_count', '', '');
$pmda->add_metric(pmda_pmid(1,158), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,1,0,0,PM_TIME_SEC,0),
		  'mysql.variables.net_write_timeout', '', '');
$pmda->add_metric(pmda_pmid(1,159), PM_TYPE_STRING, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'mysql.variables.new', '', '');
$pmda->add_metric(pmda_pmid(1,159), PM_TYPE_STRING, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'mysql.variables.old_passwords', '', '');
$pmda->add_metric(pmda_pmid(1,160), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'mysql.variables.open_files_limit', '', '');
$pmda->add_metric(pmda_pmid(1,161), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'mysql.variables.optimizer_prune_level', '', '');
$pmda->add_metric(pmda_pmid(1,162), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'mysql.variables.optimizer_search_depth', '', '');
$pmda->add_metric(pmda_pmid(1,163), PM_TYPE_STRING, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'mysql.variables.pid_file', '', '');
$pmda->add_metric(pmda_pmid(1,164), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'mysql.variables.port', '', '');
$pmda->add_metric(pmda_pmid(1,165), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'mysql.variables.preload_buffer_size', '', '');
$pmda->add_metric(pmda_pmid(1,166), PM_TYPE_STRING, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'mysql.variables.profiling', '', '');
$pmda->add_metric(pmda_pmid(1,167), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'mysql.variables.profiling_history_size', '', '');
$pmda->add_metric(pmda_pmid(1,168), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'mysql.variables.protocol_version', '', '');
$pmda->add_metric(pmda_pmid(1,169), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(1,0,0,PM_SPACE_BYTE,0,0),
		  'mysql.variables.query_alloc_block_size', '', '');
$pmda->add_metric(pmda_pmid(1,170), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(1,0,0,PM_SPACE_BYTE,0,0),
		  'mysql.variables.query_cache_limit', '', '');
$pmda->add_metric(pmda_pmid(1,171), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(1,0,0,PM_SPACE_BYTE,0,0),
		  'mysql.variables.query_cache_min_res_unit', '', '');
$pmda->add_metric(pmda_pmid(1,172), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(1,0,0,PM_SPACE_BYTE,0,0),
		  'mysql.variables.query_cache_size', '', '');
$pmda->add_metric(pmda_pmid(1,173), PM_TYPE_STRING, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'mysql.variables.query_cache_type', '', '');
$pmda->add_metric(pmda_pmid(1,174), PM_TYPE_STRING, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'mysql.variables.query_cache_wlock_invalidate', '', '');
$pmda->add_metric(pmda_pmid(1,175), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(1,0,0,PM_SPACE_BYTE,0,0),
		  'mysql.variables.query_prealloc_size', '', '');
$pmda->add_metric(pmda_pmid(1,176), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(1,0,0,PM_SPACE_BYTE,0,0),
		  'mysql.variables.range_alloc_block_size', '', '');
$pmda->add_metric(pmda_pmid(1,177), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(1,0,0,PM_SPACE_BYTE,0,0),
		  'mysql.variables.read_buffer_size', '', '');
$pmda->add_metric(pmda_pmid(1,178), PM_TYPE_STRING, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'mysql.variables.read_only', '', '');
$pmda->add_metric(pmda_pmid(1,179), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(1,0,0,PM_SPACE_BYTE,0,0),
		  'mysql.variables.read_rnd_buffer_size', '', '');
$pmda->add_metric(pmda_pmid(1,180), PM_TYPE_STRING, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'mysql.variables.relay_log_purge', '', '');
$pmda->add_metric(pmda_pmid(1,181), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(1,0,0,PM_SPACE_BYTE,0,0),
		  'mysql.variables.relay_log_space_limit', '', '');
$pmda->add_metric(pmda_pmid(1,182), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'mysql.variables.rpl_recovery_rank', '', '');
$pmda->add_metric(pmda_pmid(1,183), PM_TYPE_STRING, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'mysql.variables.secure_auth', '', '');
$pmda->add_metric(pmda_pmid(1,184), PM_TYPE_STRING, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'mysql.variables.secure_file_priv', '', '');
$pmda->add_metric(pmda_pmid(1,185), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'mysql.variables.server_id', '', '');
$pmda->add_metric(pmda_pmid(1,186), PM_TYPE_STRING, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'mysql.variables.skip_external_locking', '', '');
$pmda->add_metric(pmda_pmid(1,187), PM_TYPE_STRING, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'mysql.variables.skip_networking', '', '');
$pmda->add_metric(pmda_pmid(1,188), PM_TYPE_STRING, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'mysql.variables.skip_show_database', '', '');
$pmda->add_metric(pmda_pmid(1,189), PM_TYPE_STRING, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'mysql.variables.slave_compressed_protocol', '', '');
$pmda->add_metric(pmda_pmid(1,190), PM_TYPE_STRING, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'mysql.variables.slave_load_tmpdir', '', '');
$pmda->add_metric(pmda_pmid(1,191), PM_TYPE_STRING, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'mysql.variables.slave_skip_errors', '', '');
$pmda->add_metric(pmda_pmid(1,192), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,1,0,0,PM_TIME_SEC,0),
		  'mysql.variables.slave_net_timeout', '', '');
$pmda->add_metric(pmda_pmid(1,193), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'mysql.variables.slave_transaction_retries', '', '');
$pmda->add_metric(pmda_pmid(1,194), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,1,0,0,PM_TIME_SEC,0),
		  'mysql.variables.slow_launch_time', '', '');
$pmda->add_metric(pmda_pmid(1,195), PM_TYPE_STRING, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'mysql.variables.socket', '', '');
$pmda->add_metric(pmda_pmid(1,196), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(1,0,0,PM_SPACE_BYTE,0,0),
		  'mysql.variables.sort_buffer_size', '', '');
$pmda->add_metric(pmda_pmid(1,197), PM_TYPE_STRING, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'mysql.variables.sql_big_selects', '', '');
$pmda->add_metric(pmda_pmid(1,198), PM_TYPE_STRING, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'mysql.variables.sql_mode', '', '');
$pmda->add_metric(pmda_pmid(1,199), PM_TYPE_STRING, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'mysql.variables.sql_notes', '', '');
$pmda->add_metric(pmda_pmid(1,200), PM_TYPE_STRING, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'mysql.variables.sql_warnings', '', '');
$pmda->add_metric(pmda_pmid(1,201), PM_TYPE_STRING, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'mysql.variables.ssl_ca', '', '');
$pmda->add_metric(pmda_pmid(1,202), PM_TYPE_STRING, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'mysql.variables.ssl_capath', '', '');
$pmda->add_metric(pmda_pmid(1,203), PM_TYPE_STRING, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'mysql.variables.ssl_cert', '', '');
$pmda->add_metric(pmda_pmid(1,204), PM_TYPE_STRING, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'mysql.variables.ssl_cipher', '', '');
$pmda->add_metric(pmda_pmid(1,205), PM_TYPE_STRING, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'mysql.variables.ssl_key', '', '');
$pmda->add_metric(pmda_pmid(1,206), PM_TYPE_STRING, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'mysql.variables.storage_engine', '', '');
$pmda->add_metric(pmda_pmid(1,207), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_COUNTER, pmda_units(0,0,0,0,0,0),
		  'mysql.variables.sync_binlog', '', '');
$pmda->add_metric(pmda_pmid(1,208), PM_TYPE_STRING, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'mysql.variables.sync_frm', '', '');
$pmda->add_metric(pmda_pmid(1,209), PM_TYPE_STRING, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'mysql.variables.system_time_zone', '', '');
$pmda->add_metric(pmda_pmid(1,210), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'mysql.variables.table_cache', '', '');
$pmda->add_metric(pmda_pmid(1,211), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,1,0,0,PM_TIME_SEC,0),
		  'mysql.variables.table_lock_wait_timeout', '', '');
$pmda->add_metric(pmda_pmid(1,212), PM_TYPE_STRING, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'mysql.variables.table_type', '', '');
$pmda->add_metric(pmda_pmid(1,211), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'mysql.variables.thread_cache_size', '', '');
$pmda->add_metric(pmda_pmid(1,212), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(1,0,0,PM_SPACE_BYTE,0,0),
		  'mysql.variables.thread_stack', '', '');
$pmda->add_metric(pmda_pmid(1,213), PM_TYPE_STRING, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'mysql.variables.time_format', '', '');
$pmda->add_metric(pmda_pmid(1,214), PM_TYPE_STRING, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'mysql.variables.time_zone', '', '');
$pmda->add_metric(pmda_pmid(1,215), PM_TYPE_STRING, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'mysql.variables.timed_mutexes', '', '');
$pmda->add_metric(pmda_pmid(1,216), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  'mysql.variables.tmp_table_size', '', '');
$pmda->add_metric(pmda_pmid(1,217), PM_TYPE_STRING, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'mysql.variables.tmpdir', '', '');
$pmda->add_metric(pmda_pmid(1,218), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(1,0,0,PM_SPACE_BYTE,0,0),
		  'mysql.variables.transaction_alloc_block_size', '', '');
$pmda->add_metric(pmda_pmid(1,219), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(1,0,0,PM_SPACE_BYTE,0,0),
		  'mysql.variables.transaction_prealloc_size', '', '');
$pmda->add_metric(pmda_pmid(1,220), PM_TYPE_STRING, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'mysql.variables.tx_isolation', '', '');
$pmda->add_metric(pmda_pmid(1,221), PM_TYPE_STRING, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'mysql.variables.updatable_views_with_limit', '', '');
$pmda->add_metric(pmda_pmid(1,222), PM_TYPE_STRING, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'mysql.variables.version', '', '');
$pmda->add_metric(pmda_pmid(1,223), PM_TYPE_STRING, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'mysql.variables.version_comment', '', '');
$pmda->add_metric(pmda_pmid(1,224), PM_TYPE_STRING, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'mysql.variables.version_compile_machine', '', '');
$pmda->add_metric(pmda_pmid(1,225), PM_TYPE_STRING, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'mysql.variables.version_compile_os', '', '');
$pmda->add_metric(pmda_pmid(1,226), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,1,0,0,PM_TIME_SEC,0),
		  'mysql.variables.wait_timeout', '', '');

$pmda->add_metric(pmda_pmid(2,0), PM_TYPE_U32, $process_indom,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'mysql.processlist.id', '', '');
$pmda->add_metric(pmda_pmid(2,1), PM_TYPE_STRING, $process_indom,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'mysql.processlist.user', '', '');
$pmda->add_metric(pmda_pmid(2,2), PM_TYPE_STRING, $process_indom,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'mysql.processlist.host', '', '');
$pmda->add_metric(pmda_pmid(2,3), PM_TYPE_STRING, $process_indom,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'mysql.processlist.db', '', '');
$pmda->add_metric(pmda_pmid(2,4), PM_TYPE_STRING, $process_indom,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'mysql.processlist.command', '', '');
$pmda->add_metric(pmda_pmid(2,5), PM_TYPE_U32, $process_indom,
		  PM_SEM_INSTANT, pmda_units(0,1,0,0,PM_TIME_SEC,0),
		  'mysql.processlist.time', '', '');
$pmda->add_metric(pmda_pmid(2,6), PM_TYPE_STRING, $process_indom,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'mysql.processlist.state', '', '');
$pmda->add_metric(pmda_pmid(2,7), PM_TYPE_STRING, $process_indom,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'mysql.processlist.info', '', '');

$pmda->add_metric(pmda_pmid(3,0), PM_TYPE_STRING, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'mysql.slave_status.slave_io_state', '', '');
$pmda->add_metric(pmda_pmid(3,1), PM_TYPE_STRING, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'mysql.slave_status.slave_io_running', '', '');
$pmda->add_metric(pmda_pmid(3,2), PM_TYPE_STRING, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'mysql.slave_status.slave_sql_running', '', '');
$pmda->add_metric(pmda_pmid(3,3), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,1,0,0,PM_TIME_SEC,0),
		  'mysql.slave_status.seconds_behind_master', '', '');
$pmda->add_metric(pmda_pmid(3,4), PM_TYPE_STRING, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'mysql.slave_status.master_log_file', '', '');
$pmda->add_metric(pmda_pmid(3,5), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,0,1,0,0,PM_COUNT_ONE), 
		  'mysql.slave_status.read_master_log_pos', '', '');
$pmda->add_metric(pmda_pmid(3,6), PM_TYPE_STRING, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'mysql.slave_status.relay_master_log_file', '', '');
$pmda->add_metric(pmda_pmid(3,7), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  'mysql.slave_status.exec_master_log_pos', '', '');
$pmda->add_metric(pmda_pmid(3,8),  PM_TYPE_STRING, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'mysql.slave_status.relay_log_file', '', '');
$pmda->add_metric(pmda_pmid(3,9), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  'mysql.slave_status.relay_log_pos', '', '');
$pmda->add_metric(pmda_pmid(3,10), PM_TYPE_STRING, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'mysql.slave_status.until_log_file', '', '');
$pmda->add_metric(pmda_pmid(3,11), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  'mysql.slave_status.until_log_pos', '', ''); 
$pmda->add_metric(pmda_pmid(3,12), PM_TYPE_STRING, PM_INDOM_NULL,
		  PM_SEM_DISCRETE, pmda_units(0,0,0,0,0,0),
		  'mysql.slave_status.master_ssl_cipher', '', '');
$pmda->add_metric(pmda_pmid(3,13), PM_TYPE_STRING, PM_INDOM_NULL,
		  PM_SEM_DISCRETE, pmda_units(0,0,0,0,0,0),
		  'mysql.slave_status.master_ssl_ca_file', '', '');
$pmda->add_metric(pmda_pmid(3,14), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  'mysql.slave_status.skip_counter', '', '');
$pmda->add_metric(pmda_pmid(3,15), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(1,0,0,PM_SPACE_BYTE,0,0),
		  'mysql.slave_status.relay_log_space', '', '');
$pmda->add_metric(pmda_pmid(3,16), PM_TYPE_STRING, PM_INDOM_NULL,
		  PM_SEM_DISCRETE, pmda_units(0,0,0,0,0,0),
		  'mysql.slave_status.until_condition', '', '');
$pmda->add_metric(pmda_pmid(3,17), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_DISCRETE, pmda_units(0,1,0,0,PM_TIME_SEC,0),
		  'mysql.slave_status.connect_retry', '', '');
$pmda->add_metric(pmda_pmid(3,18), PM_TYPE_STRING, PM_INDOM_NULL,
		  PM_SEM_DISCRETE, pmda_units(0,0,0,0,0,0),
		  'mysql.slave_status.master_host', '', '');
$pmda->add_metric(pmda_pmid(3,19), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'mysql.slave_status.last_errno', '', '');
$pmda->add_metric(pmda_pmid(3,20), PM_TYPE_STRING, PM_INDOM_NULL,
		  PM_SEM_DISCRETE, pmda_units(0,0,0,0,0,0),
		  'mysql.slave_status.master_ssl_cert', '', '');
$pmda->add_metric(pmda_pmid(3,21), PM_TYPE_STRING, PM_INDOM_NULL,
		  PM_SEM_DISCRETE, pmda_units(0,0,0,0,0,0),
		  'mysql.slave_status.replicate_do_db', '', '');
$pmda->add_metric(pmda_pmid(3,22), PM_TYPE_STRING, PM_INDOM_NULL,
		  PM_SEM_DISCRETE, pmda_units(0,0,0,0,0,0),
		  'mysql.slave_status.replicate_ignore_db', '', '');
$pmda->add_metric(pmda_pmid(3,23), PM_TYPE_STRING, PM_INDOM_NULL,
		  PM_SEM_DISCRETE, pmda_units(0,0,0,0,0,0),
		  'mysql.slave_status.master_user', '', '');
$pmda->add_metric(pmda_pmid(3,24), PM_TYPE_STRING, PM_INDOM_NULL,
		  PM_SEM_DISCRETE, pmda_units(0,0,0,0,0,0),
		  'mysql.slave_status.replicate_do_table', '', '');
$pmda->add_metric(pmda_pmid(3,25), PM_TYPE_STRING, PM_INDOM_NULL,
		  PM_SEM_DISCRETE, pmda_units(0,0,0,0,0,0),
		  'mysql.slave_status.replicate_wild_do_table', '', '');
$pmda->add_metric(pmda_pmid(3,26), PM_TYPE_STRING, PM_INDOM_NULL,
		  PM_SEM_DISCRETE, pmda_units(0,0,0,0,0,0),
		  'mysql.slave_status.replicate_wild_ignore_table', '', '');
$pmda->add_metric(pmda_pmid(3,27), PM_TYPE_STRING, PM_INDOM_NULL,
		  PM_SEM_DISCRETE, pmda_units(0,0,0,0,0,0),
		  'mysql.slave_status.replicate_ignore_table', '', '');
$pmda->add_metric(pmda_pmid(3,28), PM_TYPE_STRING, PM_INDOM_NULL,
		  PM_SEM_DISCRETE, pmda_units(0,0,0,0,0,0),
		  'mysql.slave_status.master_ssl_allowed', '', '');
$pmda->add_metric(pmda_pmid(3,29), PM_TYPE_STRING, PM_INDOM_NULL,
		  PM_SEM_DISCRETE, pmda_units(0,0,0,0,0,0),
		  'mysql.slave_status.master_ssl_ca_path', '', '');
$pmda->add_metric(pmda_pmid(3,30), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_DISCRETE, pmda_units(0,0,0,0,0,0),
		  'mysql.slave_status.master_port', '', '');
$pmda->add_metric(pmda_pmid(3,31), PM_TYPE_STRING, PM_INDOM_NULL,
		  PM_SEM_DISCRETE, pmda_units(0,0,0,0,0,0),
		  'mysql.slave_status.master_ssl_key', '', '');
$pmda->add_metric(pmda_pmid(3,32), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'mysql.slave_status.slave_io_running_num', '', '');
$pmda->add_metric(pmda_pmid(3,33), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'mysql.slave_status.slave_sql_running_num', '', '');
$pmda->add_metric(pmda_pmid(3,34), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_DISCRETE, pmda_units(0,0,0,0,0,0),
		  'mysql.slave_status.master_ssl_allowed_num', '','');
		  
$pmda->add_indom($process_indom, \@process_instances,
		 'Instance domain exporting each MySQL process', '');

$pmda->set_fetch_callback(\&mysql_fetch_callback);
$pmda->set_fetch(\&mysql_connection_setup);
$pmda->set_refresh(\&mysql_refresh);
$pmda->set_user('pcp');
$pmda->run;
