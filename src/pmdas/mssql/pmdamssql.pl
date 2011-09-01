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
# You should have received a copy of the GNU General Public License along
# with this program; if not, write to the Free Software Foundation, Inc.,
# 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
#

use strict;
use warnings;
use PCP::PMDA;
use DBI;

my $server = 'localhost';
my $database = 'PCP';
my $username = 'dbmonitor';
my $password = 'dbmonitor';
# my $bufferpoolused = "MEMORYCLERK_SQLBUFFERPOOL";

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
my $database_indom = 0;
my @database_instances;

sub mssql_connection_setup
{
    $pmda->log("mssql_connection_setup\n");

    if (!defined($dbh)) {
    	$dbh = DBI->connect("DBI:ODBC:Driver={SQL Server};Server=$server;Database=$database;UID=$username;PWD=$password");
    	if (defined($dbh)) {
	        $pmda->log("MSSQL connection established\n");
	        $sth_virtual_file_stats = $dbh->prepare(
        	    "select num_of_reads, num_of_bytes_read, io_stall_read_ms, num_of_writes, " .
        	    "num_of_bytes_written, io_stall_write_ms, size_on_disk_bytes " .
        	    "from sys.dm_io_virtual_file_stats(DB_ID('$database'),1)");
	        $sth_os_memory_clerks = $dbh->prepare(
	             "SELECT SUM(multi_pages_kb + virtual_memory_committed_kb + shared_memory_committed_kb + awe_allocated_kb)" .
	             " from sys.dm_os_memory_clerks WHERE type IN " .
	             "('MEMORYCLERK_SQLBUFFERPOOL', 'MEMORYCLERK_SQLQUERYCOMPILE'," .
	             " 'MEMORYCLERK_SQLQUERYPLAN', 'MEMORYCLERK_SQLQUERYEXEC')");
	        $sth_total_running_user_processes = $dbh->prepare(
	             "SELECT count(*) as total_running_user_processes FROM sys.dm_exec_requests " .
	             "WHERE session_id >= 51 AND status = 'running')";
	    }
    }
}

sub mssql_os_memory_clerks_refresh
{
    $pmda->log("mssql_os_memory_clerks_refresh\n");

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
    $pmda->log("mssql_virtual_file_stats_refresh\n");

    @virtual_file_stats = ();	# clear any previous contents
    if (defined($dbh)) {
    	$sth_virtual_file_stats->execute();
	    @virtual_file_stats = $sth_virtual_file_stats->fetchrow_array();
    }
}

sub mssql_total_running_user_processes
{
    $pmda->log("mssql_total_running_user_processes\n");

    @total_running_user_processes = ();	# clear any previous contents
    if (defined($dbh)) {
    	$sth_total_running_user_processes->execute();
	    @total_running_user_processes = $sth_total_running_user_processes->fetchrow_array();
    }
}

sub mssql_refresh
{
    my ($cluster) = @_;

    $pmda->log("mysql_refresh $cluster\n");
    if ($cluster == 0)		{ mssql_virtual_file_stats_refresh; }
    elsif ($cluster == 1)	{ mssql_os_memory_clerks_refresh; }
    elsif ($cluster == 2)	{ mssql_total_running_user_processes; }
}

sub mssql_fetch_callback
{
    my ($cluster, $item, $inst) = @_;

    $pmda->log("mssql_fetch_callback $metric_name $cluster:$item ($inst)\n");

    if ($inst != PM_IN_NULL)		{ return (PM_ERR_INST, 0); }
    if ($cluster == 0) {
        if ($item > 6)              { return (PM_ERR_PMID, 0); }
        if (!defined($virtual_file_stats[$item])) { return (PM_ERR_AGAIN, 0); }
        return ($virtual_file_stats[$item], 1);
    }
    if ($cluster == 1) {
        if ($item > 4)              { return (PM_ERR_PMID, 0); }
        if (!defined($os_memory_clerks[$item])) { return (PM_ERR_AGAIN, 0); }
        return ($os_memory_clerks[$item], 1);
    }
    if ($cluster == 2) {
        if ($item > 1)              { return (PM_ERR_PMID, 0); }
        if (!defined($total_running_user_processes[$item])) { return (PM_ERR_AGAIN, 0); }
        return ($total_running_user_processes[$item], 1);
    }
    return (PM_ERR_PMID, 0);
#	if ($inst < 0)		{ return (PM_ERR_INST, 0); }
#	if ($inst > @process_instances)	{ return (PM_ERR_INST, 0); }
}

$pmda = PCP::PMDA->new('mssql', 109);

$pmda->add_metric(pmda_pmid(0,0), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  'mssql.virtual_file.read', '', '');
$pmda->add_metric(pmda_pmid(0,1), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_COUNTER, pmda_units(1,0,0,PM_SPACE_BYTE,0,0),
		  'mssql.virtual_file.read_bytes', '', '');
$pmda->add_metric(pmda_pmid(0,2), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_COUNTER, pmda_units(0,1,0,0,PM_TIME_MSEC,0),
		  'mssql.virtual_file.read_io_stall_time', '', '');
$pmda->add_metric(pmda_pmid(0,3), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  'mssql.virtual_file.write', '', '');
$pmda->add_metric(pmda_pmid(0,4), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_COUNTER, pmda_units(1,0,0,PM_SPACE_BYTE,0,0),
		  'mssql.virtual_file.write_bytes', '', '');
$pmda->add_metric(pmda_pmid(0,5), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_COUNTER, pmda_units(0,1,0,0,PM_TIME_MSEC,0),
		  'mssql.virtual_file.write_io_stall_time', '', '');
$pmda->add_metric(pmda_pmid(0,6), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(1,0,0,PM_SPACE_BYTE,0,0),
		  'mssql.virtual_file.size', '', '');

$pmda->add_metric(pmda_pmid(1,0), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(1,0,0,PM_SPACE_KBYTE,0,0),
		  'mssql.os_memory_clerks.bufferpool', '', '');
$pmda->add_metric(pmda_pmid(1,1), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(1,0,0,PM_SPACE_KBYTE,0,0),
		  'mssql.os_memory_clerks.querycompile', '', '');
$pmda->add_metric(pmda_pmid(1,2), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(1,0,0,PM_SPACE_KBYTE,0,0),
		  'mssql.os_memory_clerks.queryplan', '', '');
$pmda->add_metric(pmda_pmid(1,3), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(1,0,0,PM_SPACE_KBYTE,0,0),
		  'mssql.os_memory_clerks.queryexec', '', '');

$pmda->add_metric(pmda_pmid(2,0), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  'mssql.running_user_process.total', '', '');
		  
#$pmda->add_indom($process_indom, \@process_instances,
#	 'Instance domain exporting each MySQL process', '');

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
