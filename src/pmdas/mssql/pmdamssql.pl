#
# Copyright (c) 2011 Aconex.  All Rights Reserved.
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

my $server = 'localhost';
my $database = 'PCP';
my $username = 'dbmonitor';
my $password = 'dbmonitor';

# Configuration files for overriding the above settings
for my $file (	'/etc/pcpdbi.conf',	# system defaults (lowest priority)
		pmda_config('PCP_PMDAS_DIR') . '/mssql/mssql.conf',
		'./mssql.conf' ) {	# current directory (high priority)
    eval `cat $file` unless ! -f $file;
}

use vars qw( $pmda $dbh );
use vars qw( $sth_os_memory_clerks  );
use vars qw( $sth_virtual_file_stats @virtual_file_stats );
use vars qw( $sth_total_running_user_processes @total_running_user_processes );
use vars qw( $sth_os_memory_clerks @os_memory_clerks );
use vars qw( $sth_os_workers_waiting_cpu @os_workers_waiting_cpu );
my $database_indom = 0;
my @database_instances;

sub mssql_connection_setup
{
    #$pmda->log("mssql_connection_setup\n");

    if (!defined($dbh)) {
    	$dbh = DBI->connect("DBI:Sybase:server=$server", $username, $password);
    	if (defined($dbh)) {
	        $pmda->log("MSSQL connection established\n");
	        $sth_virtual_file_stats = $dbh->prepare(
        	    "select db_name(database_id), cast(num_of_reads as numeric), cast(num_of_bytes_read as numeric)," .
        	    " cast(io_stall_read_ms as numeric), cast(num_of_writes as numeric)," .
        	    " cast(num_of_bytes_written as numeric), cast(io_stall_write_ms as numeric)," .
        	    " cast(size_on_disk_bytes as numeric) " .
        	    "from sys.dm_io_virtual_file_stats(DB_ID(''),1)");
	        $sth_os_memory_clerks = $dbh->prepare(
	             "SELECT SUM(multi_pages_kb + virtual_memory_committed_kb + shared_memory_committed_kb + awe_allocated_kb)" .
	             " from sys.dm_os_memory_clerks WHERE type IN " .
	             "('MEMORYCLERK_SQLBUFFERPOOL', 'MEMORYCLERK_SQLQUERYCOMPILE'," .
	             " 'MEMORYCLERK_SQLQUERYEXEC', 'MEMORYCLERK_SQLQUERYPLAN')" .
	             " group by type order by type");
	        $sth_total_running_user_processes = $dbh->prepare(
	             "SELECT count(*) FROM sys.dm_exec_requests " .
	             "WHERE session_id >= 51 AND status = 'running'");
            $sth_os_workers_waiting_cpu = $dbh->prepare(
	             "SELECT ISNULL(COUNT(*),0) FROM sys.dm_os_workers AS workers " .
	             "INNER JOIN sys.dm_os_schedulers AS schedulers " .
	             "ON workers.scheduler_address = schedulers.scheduler_address " .
	             "WHERE workers.state = 'RUNNABLE' AND schedulers.scheduler_id < 255"); 
	    }
    }
}

sub mssql_os_memory_clerks_refresh
{
    #$pmda->log("mssql_os_memory_clerks_refresh\n");

    @os_memory_clerks = ();	# clear any previous contents
    if (defined($dbh)) {
    	$sth_os_memory_clerks->execute();
        my $result = $sth_os_memory_clerks->fetchall_arrayref();
	    for my $i (0 .. $#{$result}) {	    
	        $os_memory_clerks[$i] = $result->[$i][0];
	    }
	}
}

sub mssql_virtual_file_stats_refresh
{
    #$pmda->log("mssql_virtual_file_stats_refresh\n");

    @virtual_file_stats = ();	# clear any previous contents
    @database_instances = ();

    if (defined($dbh)) {
    	$sth_virtual_file_stats->execute();
	    my $result = $sth_virtual_file_stats->fetchall_arrayref();

	    for my $i (0 .. $#{$result}) {
	        $database_instances[($i*2)] = $i;
	        $database_instances[($i*2)+1] = "$result->[$i][0]";
	        $virtual_file_stats[$i] = $result->[$i];
	    }

	    $pmda->replace_indom( $database_indom, \@database_instances );
    }
}

sub mssql_total_running_user_processes
{
    #$pmda->log("mssql_total_running_user_processes\n");

    @total_running_user_processes = ();	# clear any previous contents
    if (defined($dbh)) {
    	$sth_total_running_user_processes->execute();
	my $result = $sth_total_running_user_processes->fetchall_arrayref();
	@total_running_user_processes = ( $result->[0][0] );
    }
}

sub mssql_os_workers_waiting_cpu_refresh
{
    #$pmda->log("mssql_os_workers_refresh\n");

    @os_workers_waiting_cpu = ();	# clear any previous contents
    if (defined($dbh)) {
    	$sth_os_workers_waiting_cpu->execute();
        my $result = $sth_os_workers_waiting_cpu->fetchall_arrayref();
        @os_workers_waiting_cpu = ( $result->[0][0] );
	}
}

sub mssql_refresh
{
    my ($cluster) = @_;

    #$pmda->log("mssql_refresh $cluster\n");

    if ($cluster == 0)		{ mssql_virtual_file_stats_refresh; }
    elsif ($cluster == 1)	{ mssql_os_memory_clerks_refresh; }
    elsif ($cluster == 2)	{ mssql_total_running_user_processes; }
    elsif ($cluster == 3)	{ mssql_os_workers_waiting_cpu_refresh; }
}

sub mssql_fetch_callback
{
    my ($cluster, $item, $inst) = @_;
    my ($value, @vfstats);

    #$pmda->log("mssql_fetch_callback $cluster:$item ($inst)\n");

    if ($cluster == 0) {
        if ($item > 6)              { return (PM_ERR_PMID, 0); }
    	if ($inst < 0)		        { return (PM_ERR_INST, 0); }
    	if ($inst > @database_instances)	{ return (PM_ERR_INST, 0); }
       	$value = $virtual_file_stats[$inst];
       	if (!defined($value))	    { return (PM_ERR_INST, 0); }
       	@vfstats = @$value;
        if (!defined($vfstats[$item+1])) { return (PM_ERR_AGAIN, 0); }
        return ($vfstats[$item+1], 1);
    }
    if ($inst != PM_IN_NULL)		{ return (PM_ERR_INST, 0); }
    if ($cluster == 1) {
        if ($item > 3)              { return (PM_ERR_PMID, 0); }
        if (!defined($os_memory_clerks[$item])) { return (PM_ERR_AGAIN, 0); }
        return ($os_memory_clerks[$item], 1);
    }
    if ($cluster == 2) {
        if ($item > 0)              { return (PM_ERR_PMID, 0); }
        if (!defined($total_running_user_processes[$item])) { return (PM_ERR_AGAIN, 0); }
        return ($total_running_user_processes[$item], 1);
    }
    if ($cluster == 3) {
        if ($item > 0)              { return (PM_ERR_PMID, 0); }
        if (!defined($os_workers_waiting_cpu[$item])) { return (PM_ERR_AGAIN, 0); }
        return ($os_workers_waiting_cpu[$item], 1);
    }
    return (PM_ERR_PMID, 0);
}

$pmda = PCP::PMDA->new('mssql', 109);

$pmda->add_metric(pmda_pmid(0,0), PM_TYPE_U64, $database_indom,
		  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  'mssql.virtual_file.read', 'Number of bytes reads issued on data file', '');
$pmda->add_metric(pmda_pmid(0,1), PM_TYPE_U64, $database_indom,
		  PM_SEM_COUNTER, pmda_units(1,0,0,PM_SPACE_BYTE,0,0),
		  'mssql.virtual_file.read_bytes', 'Total number of bytes read on the data file', '');
$pmda->add_metric(pmda_pmid(0,2), PM_TYPE_U64, $database_indom,
		  PM_SEM_COUNTER, pmda_units(0,1,0,0,PM_TIME_MSEC,0),
		  'mssql.virtual_file.read_io_stall_time', 'Total time in ms that the users waited for reads issued on the file', '');
$pmda->add_metric(pmda_pmid(0,3), PM_TYPE_U64, $database_indom,
		  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  'mssql.virtual_file.write', 'Number of writes made on the data file', '');
$pmda->add_metric(pmda_pmid(0,4), PM_TYPE_U64, $database_indom,
		  PM_SEM_COUNTER, pmda_units(1,0,0,PM_SPACE_BYTE,0,0),
		  'mssql.virtual_file.write_bytes', 'Total number of bytes written to the data file', '');
$pmda->add_metric(pmda_pmid(0,5), PM_TYPE_U64, $database_indom,
		  PM_SEM_COUNTER, pmda_units(0,1,0,0,PM_TIME_MSEC,0),
		  'mssql.virtual_file.write_io_stall_time', 'Total time in ms that users waited for writes to be completed o the file', '');
$pmda->add_metric(pmda_pmid(0,6), PM_TYPE_U64, $database_indom,
		  PM_SEM_INSTANT, pmda_units(1,0,0,PM_SPACE_BYTE,0,0),
		  'mssql.virtual_file.size', 'Number of bytes used on the disk from the data file', '');

$pmda->add_metric(pmda_pmid(1,0), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(1,0,0,PM_SPACE_KBYTE,0,0),
		  'mssql.os_memory_clerks.bufferpool', '', '');
$pmda->add_metric(pmda_pmid(1,1), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(1,0,0,PM_SPACE_KBYTE,0,0),
		  'mssql.os_memory_clerks.querycompile', '', '');
$pmda->add_metric(pmda_pmid(1,2), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(1,0,0,PM_SPACE_KBYTE,0,0),
		  'mssql.os_memory_clerks.queryexec', '', '');
$pmda->add_metric(pmda_pmid(1,3), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(1,0,0,PM_SPACE_KBYTE,0,0),
		  'mssql.os_memory_clerks.queryplan', '', '');

$pmda->add_metric(pmda_pmid(2,0), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  'mssql.running_user_process.total', 'Total number of running user process belonging to aconexsq', '');

$pmda->add_metric(pmda_pmid(3,0), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  'mssql.os_workers_waiting_cpu.count', 'Total number of queries waiting for cpu', '');

$pmda->add_indom($database_indom, \@database_instances,
    	 'Instance domain exporting each MSSQL database', '');

$pmda->set_fetch_callback(\&mssql_fetch_callback);
$pmda->set_fetch(\&mssql_connection_setup);
$pmda->set_refresh(\&mssql_refresh);
$pmda->run;

=pod

=head1 NAME

pmdamssql - Microsoft SQL database PMDA

=head1 DESCRIPTION

B<pmdamssql> is a Performance Co-Pilot PMDA which extracts
live performance data from a running SQL Server database.
These metrics are typically sourced from Dynamic Management
Views (DMVs), augmenting the SQL server metrics exported by
the Windows PMDA.

=head1 INSTALLATION

B<pmdamssql> uses a configuration file from (in this order):

=over

=item * /etc/pcpdbi.conf

=item * $PCP_PMDAS_DIR/mssql/mssql.conf

=back

This file can contain overridden values (Perl code) for the settings
listed at the start of pmdamssql.pl, namely:

=over

=item * database name (see DBI(3) for details)

=item * database user name

=item * database pass word

=back

Once this is setup, you can access the names and values for the
mysql performance metrics by doing the following as root:

	# cd $PCP_PMDAS_DIR/mssql
	# ./Install

If you want to undo the installation, do the following as root:

	# cd $PCP_PMDAS_DIR/mssql
	# ./Remove

B<pmdamssql> is launched by pmcd(1) and should never be executed
directly.  The Install and Remove scripts notify pmcd(1) when
the agent is installed or removed.

=head1 FILES

=over

=item /etc/pcpdbi.conf

configuration file for all PCP database monitors

=item $PCP_PMDAS_DIR/mssql/mssql.conf

configuration file for B<pmdamssql>

=item $PCP_PMDAS_DIR/mssql/Install

installation script for the B<pmdamssql> agent

=item $PCP_PMDAS_DIR/mssql/Remove

undo installation script for the B<pmdamssql> agent

=item $PCP_LOG_DIR/pmcd/mssql.log

default log file for error messages from B<pmdamssql>

=back

=head1 SEE ALSO

pmcd(1), pmdadbping.pl(1) and DBI(3).
