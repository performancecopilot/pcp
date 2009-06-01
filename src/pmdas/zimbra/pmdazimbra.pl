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

my $pmda = PCP::PMDA->new('zimbra', 98);
my $stats = '/opt/zimbra/zmstat/';

# Zimbra instrumentation is exported through a series of CSV files.
# Several of these are system-level files that we already have PMDAs
# for, extracted more accurately and efficiently, so we ignore those
# (cpu,vm,mysql).
# For the rest, we use the PMDA.pm file "tail" mechanism to monitor
# appends to each of the CSV files.  For all data received, we parse
# the line and extract values.  These we store in fixed-size arrays,
# one per metric cluster, then the fetch callback just passes out the
# most recently seen value.

my ( $imap_domain, $soap_domain ) = ( 0, 1 );
my @imap_indom = (
	0 => 'CAPABILITY',	1 => 'UID',		2 => 'STATUS',
	3 => 'SUBSCRIBE',	4 => 'LIST',		5 => 'SELECT',
	6 => 'LOGIN',		7 => 'NAMESPACE',	8 => 'APPEND',
	9 => 'OTHER' );
my @soap_indom = (
	0 => 'AuthRequest',		1 => 'DelegateAuthRequest',
	2 => 'GetAccountInfoRequest',	3 => 'GetAllServersRequest',
	4 => 'GetDomainRequest',	5 => 'GetDomainInfoRequest',
	6 => 'GetCosRequest',		7 => 'GetServiceStatusRequest',
	8 => 'GetVersionInfoRequest',	9 => 'GetAccountMembershipRequest',
	10 => 'GetLDAPEntriesRequest',	11 => 'GetAdminSavedSearchesRequest',
	12 => 'GetAdminExtensionZimletsRequest', 13 => 'GetAllConfigRequest',
	14 => 'GetLicenseRequest',	15 => 'SearchDirectoryRequest',
	16 => 'Other' );

sub zimbra_array_lookup
{
    my ( $indomref, $name ) = @_;
    my @indom = @$indomref;
    my $index;

    for ($index = 0; $index < $#indom; $index++) {
	return $index unless ($indom[$index] != $name);
    }
    return $#indom - 1;		# return the "other" bucket by default
}

use vars qw( $fd_timestamp $imap_timestamp $soap_timestamp
	     $mailbox_timestamp $mtaqueue_timestamp $proc_timestamp
	     $threads_timestamp );
use vars qw( @fd_values @imap_time_values @imap_count_values @soap_time_values 
	     @soap_count_values @mailbox_values @mtaqueue_values @proc_values
	     @threads_values );

sub zimbra_fd_parser
{
    if (s|^(\d\d/\d\d/\d\d\d\d \d\d:\d\d:\d\d), ||) {
	chomp;
	$fd_timestamp = $1;
	@fd_values = split /,/;
    }
}

sub zimbra_imap_parser
{
    if (s|^(\d\d/\d\d/\d\d\d\d \d\d:\d\d:\d\d), ||) {
	chomp;
	my $timestamp = $1;
	my @values = split /,/;

	if ($timestamp != $imap_timestamp) {
	    $imap_timestamp = $timestamp;
	    @imap_count_values = ();
	    @imap_time_values = ();
	}

	my $index = zimbra_array_lookup(\@imap_indom, $1);
	$imap_count_values[$index] = $values[0];
	$imap_time_values[$index] = $values[1];
    }
}

sub zimbra_soap_parser
{
    if (s|^(\d\d/\d\d/\d\d\d\d \d\d:\d\d:\d\d), ||) {
	chomp;
	my $timestamp = $1;
	my @values = split /,/;

	if ($timestamp != $soap_timestamp) {
	    $soap_timestamp = $timestamp;
	    @soap_count_values = ();
	    @soap_time_values = ();
	}

	my $index = zimbra_array_lookup(\@soap_indom, $1);
	$soap_count_values[$index] = $values[0];
	$soap_time_values[$index] = $values[1];
    }
}

sub zimbra_mailbox_parser
{
    if (s|^(\d\d/\d\d/\d\d\d\d \d\d:\d\d:\d\d), ||) {
	chomp;
	$mailbox_timestamp = $1;
	@mailbox_values = split /,/;
    }
}

sub zimbra_mtaqueue_parser
{
    if (s|^(\d\d/\d\d/\d\d\d\d \d\d:\d\d:\d\d), ||) {
	chomp;
	$mtaqueue_timestamp = $1;
	@mtaqueue_values = split /,/;
    }
}

sub zimbra_proc_parser
{
    if (s|^(\d\d/\d\d/\d\d\d\d \d\d:\d\d:\d\d), ||) {
	chomp;
	$mtaqueue_timestamp = $1;
	@mtaqueue_values = split /,/;
    }
}

sub zimbra_threads_parser
{
    if (s|^(\d\d/\d\d/\d\d\d\d \d\d:\d\d:\d\d), ||) {
	chomp;
	$threads_timestamp = $1;
	@threads_values = split /,/;
    }
}

sub zimbra_fetch_callback
{
    my ($cluster, $item, $inst) = @_;

    if ($inst != PM_IN_NULL && $cluster != 1 && $cluster != 2) {
	return (PM_ERR_INST, 0);
    }

    if ($cluster == 0) {			# fd.csv
	if ($item >= 0 && $item < $#fd_values) {
	    return ($fd_values[$item], 1);
	}
    }
    elsif ($cluster == 1 && $item == 0) {	# imap.csv
	if ($inst >= 0 && $inst < $#imap_count_values) {
	    return ($imap_count_values[$inst], 1);
	}
    }
    elsif ($cluster == 1 && $item == 1) {	# imap.csv
	if ($inst >= 0 && $inst < $#imap_count_values) {
	    return ($imap_count_values[$item], 1);
	}
    }
    elsif ($cluster == 2 && $item == 0) {	# soap.csv
	if ($inst >= 0 && $inst < $#soap_count_values) {
	    return ($soap_count_values[$item], 1);
	}
    }
    elsif ($cluster == 2 && $item == 1) {	# soap.csv
	if ($inst >= 0 && $inst < $#soap_time_values) {
	    return ($soap_time_values[$item], 1);
	}
    }
    elsif ($cluster == 3) {			# mailboxd.csv
	if ($item >= 0 && $item < $#mailbox_values) {
	    return ($mailbox_values[$item], 1);
	}
    }
    elsif ($cluster == 4) {			# mtaqueue.csv
	if ($item >= 0 && $item < $#mtaqueue_values) {
	    return ($mtaqueue_values[$item], 1);
	}
    }
    elsif ($cluster == 5) {			# proc.csv
	if ($item >= 0 && $item < $#proc_values) {
	    return ($proc_values[$item], 1);
	}
    }
    elsif ($cluster == 6) {			# threads.csv
	if ($item >= 0 && $item < $#threads_values) {
	    return ($threads_values[$item], 1);
	}
    }

    return (PM_ERR_PMID, 0);
}

$pmda->add_metric(pmda_pmid(0,0), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT,
	pmda_units(0,0,1,0,0,PM_COUNT_ONE), 'zimbra.fd_count',
	'/opt/zimbra/zmstat/fd.csv', 'Open file descriptors');

$pmda->add_metric(pmda_pmid(1,0), PM_TYPE_U32, $imap_domain, PM_SEM_INSTANT,
	pmda_units(0,0,1,0,0,PM_COUNT_ONE), 'zimbra.imap.exec_count',
	'/opt/zimbra/zmstat/imap.csv',
	'Count of Internet Message Access Protocol commands');
$pmda->add_metric(pmda_pmid(1,1), PM_TYPE_U32, $imap_domain, PM_SEM_INSTANT,
	pmda_units(0,1,0,0,PM_TIME_MSEC,0), 'zimbra.imap.exec_avgtime',
	'/opt/zimbra/zmstat/imap.csv',
	'Time spent executing Internet Message Access Protocol commands');

$pmda->add_metric(pmda_pmid(2,0), PM_TYPE_U32, $soap_domain, PM_SEM_INSTANT,
	pmda_units(0,0,1,0,0,PM_COUNT_ONE), 'zimbra.soap.exec_count',
	'/opt/zimbra/zmstat/soap.csv',
	'Count of Simple Object Access Protocol commands');
$pmda->add_metric(pmda_pmid(2,1), PM_TYPE_U32, $soap_domain, PM_SEM_INSTANT,
	pmda_units(0,1,0,0,PM_TIME_MSEC,0), 'zimbra.imap.exec_avgtime',
	'/opt/zimbra/zmstat/soap.csv',
	'Time spent executing Simple Object Access Protocol commands');

$pmda->add_metric(pmda_pmid(3,0), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT,
	pmda_units(0,0,1,0,0,PM_COUNT_ONE), 'zimbra.mailboxd.lmtp_rcvd_msgs',
	'/opt/zimbra/zmstat/mailboxd.csv', 'Received mail messages');
$pmda->add_metric(pmda_pmid(3,1), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT,
	pmda_units(1,0,0,PM_SPACE_BYTE,0,0), 'zimbra.mailboxd.lmtp_rcvd_bytes',
	'/opt/zimbra/zmstat/mailboxd.csv', 'Received bytes');
$pmda->add_metric(pmda_pmid(3,2), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT,
	pmda_units(0,0,1,0,0,PM_COUNT_ONE), 'zimbra.mailboxd.lmtp_rcvd_rcpt',
	'/opt/zimbra/zmstat/mailboxd.csv', 'Received receipts');
$pmda->add_metric(pmda_pmid(3,3), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT,
	pmda_units(0,0,1,0,0,PM_COUNT_ONE), 'zimbra.mailboxd.lmtp_dlvd_msgs',
	'/opt/zimbra/zmstat/mailboxd.csv', 'Delivered messages');
$pmda->add_metric(pmda_pmid(3,4), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT,
	pmda_units(0,0,1,0,0,PM_COUNT_ONE), 'zimbra.mailboxd.lmtp_dlvd_bytes',
	'/opt/zimbra/zmstat/mailboxd.csv', 'Delivered bytes');
$pmda->add_metric(pmda_pmid(3,5), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT,
	pmda_units(0,0,1,0,0,PM_COUNT_ONE), 'zimbra.mailboxd.db_conn_count',
	'/opt/zimbra/zmstat/mailboxd.csv', 'Database connection count');
$pmda->add_metric(pmda_pmid(3,6), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT,
	pmda_units(0,1,0,0,PM_TIME_MSEC,0), 'zimbra.mailboxd.db_conn_ms_avg',
	'/opt/zimbra/zmstat/mailboxd.csv', 'Database connection time');
$pmda->add_metric(pmda_pmid(3,7), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT,
	pmda_units(0,0,1,0,0,PM_COUNT_ONE), 'zimbra.mailboxd.ldap_dc_count',
	'/opt/zimbra/zmstat/mailboxd.csv', '');
$pmda->add_metric(pmda_pmid(3,8), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT,
	pmda_units(0,1,0,0,PM_TIME_MSEC,0), 'zimbra.mailboxd.ldap_dc_ms_avg',
	'/opt/zimbra/zmstat/mailboxd.csv', '');
$pmda->add_metric(pmda_pmid(3,9), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT,
	pmda_units(0,0,1,0,0,PM_COUNT_ONE), 'zimbra.mailboxd.mbox_add_msg_count',
	'/opt/zimbra/zmstat/mailboxd.csv', 'Mailbox message add count');
$pmda->add_metric(pmda_pmid(3,10), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT,
	pmda_units(0,1,0,0,PM_TIME_MSEC,0), 'zimbra.mailboxd.mbox_add_msg_ms_avg',
	'/opt/zimbra/zmstat/mailboxd.csv', 'Mailbox message add average time');
$pmda->add_metric(pmda_pmid(3,11), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT,
	pmda_units(0,0,1,0,0,PM_COUNT_ONE), 'zimbra.mailboxd.mbox_get_count',
	'/opt/zimbra/zmstat/mailboxd.csv', 'Mailbox get count');
$pmda->add_metric(pmda_pmid(3,12), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT,
	pmda_units(0,1,0,0,PM_TIME_MSEC,0), 'zimbra.mailboxd.mbox_get_ms_avg',
	'/opt/zimbra/zmstat/mailboxd.csv', 'Mailbox average get time');
$pmda->add_metric(pmda_pmid(3,13), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT,
	pmda_units(0,0,0,0,0,0), 'zimbra.mailboxd.mbox_cache',
	'/opt/zimbra/zmstat/mailboxd.csv', 'Mailbox cache');
$pmda->add_metric(pmda_pmid(3,14), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT,
	pmda_units(0,0,0,0,0,0), 'zimbra.mailboxd.mbox_msg_cache',
	'/opt/zimbra/zmstat/mailboxd.csv', 'Mailbox message cache');
$pmda->add_metric(pmda_pmid(3,15), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT,
	pmda_units(0,0,0,0,0,0), 'zimbra.mailboxd.mbox_item_cache',
	'/opt/zimbra/zmstat/mailboxd.csv', 'Mailbox item cache');
$pmda->add_metric(pmda_pmid(3,16), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT,
	pmda_units(0,0,1,0,0,PM_COUNT_ONE), 'zimbra.mailboxd.soap_count',
	'/opt/zimbra/zmstat/mailboxd.csv', 'Simple Object Access Protocol count');
$pmda->add_metric(pmda_pmid(3,17), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT,
	pmda_units(0,1,0,0,PM_TIME_MSEC,0), 'zimbra.mailboxd.soap_ms_avg',
	'/opt/zimbra/zmstat/mailboxd.csv', 'Simple Object Access Protocol time');
$pmda->add_metric(pmda_pmid(3,18), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT,
	pmda_units(0,0,1,0,0,PM_COUNT_ONE), 'zimbra.mailboxd.imap_count',
	'/opt/zimbra/zmstat/mailboxd.csv', 'Internet Message Access Protocol count');
$pmda->add_metric(pmda_pmid(3,19), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT,
	pmda_units(0,1,0,0,PM_TIME_MSEC,0), 'zimbra.mailboxd.imap_ms_avg',
	'/opt/zimbra/zmstat/mailboxd.csv', 'Internet Message Access Protocol time');
$pmda->add_metric(pmda_pmid(3,20), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT,
	pmda_units(0,0,1,0,0,PM_COUNT_ONE), 'zimbra.mailboxd.pop_count',
	'/opt/zimbra/zmstat/mailboxd.csv', 'Post Office Protocol count');
$pmda->add_metric(pmda_pmid(3,21), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT,
	pmda_units(0,1,0,0,PM_TIME_MSEC,0), 'zimbra.mailboxd.pop_ms_avg',
	'/opt/zimbra/zmstat/mailboxd.csv', 'Post Office Protocol time');
$pmda->add_metric(pmda_pmid(3,22), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT,
	pmda_units(0,0,0,0,0,0), 'zimbra.mailboxd.idx.wrt_avg',
	'/opt/zimbra/zmstat/mailboxd.csv', 'Lucene Index Writer count');
$pmda->add_metric(pmda_pmid(3,23), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT,
	pmda_units(0,0,0,0,0,0), 'zimbra.mailboxd.idx.wrt_opened',
	'/opt/zimbra/zmstat/mailboxd.csv', '');
$pmda->add_metric(pmda_pmid(3,24), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT,
	pmda_units(0,0,1,0,0,PM_COUNT_ONE), 'zimbra.mailboxd.idx.wrt_opened_cache_hit',
	'/opt/zimbra/zmstat/mailboxd.csv', '');
$pmda->add_metric(pmda_pmid(3,25), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT,
	pmda_units(0,0,1,0,0,PM_COUNT_ONE), 'zimbra.mailboxd.calcache_hit',
	'/opt/zimbra/zmstat/mailboxd.csv', '');
$pmda->add_metric(pmda_pmid(3,26), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT,
	pmda_units(0,0,1,0,0,PM_COUNT_ONE), 'zimbra.mailboxd.calcache_mem_hit',
	'/opt/zimbra/zmstat/mailboxd.csv', '');
$pmda->add_metric(pmda_pmid(3,27), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT,
	pmda_units(0,0,1,0,0,PM_COUNT_ONE), 'zimbra.mailboxd.calcache_lru_size',
	'/opt/zimbra/zmstat/mailboxd.csv', '');
$pmda->add_metric(pmda_pmid(3,28), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT,
	pmda_units(1,0,0,PM_SPACE_BYTE,0,0), 'zimbra.mailboxd.idx.bytes_written',
	'/opt/zimbra/zmstat/mailboxd.csv', '');
$pmda->add_metric(pmda_pmid(3,29), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT,
	pmda_units(1,0,0,PM_SPACE_BYTE,0,0), 'zimbra.mailboxd.idx.bytes_written_avg',
	'/opt/zimbra/zmstat/mailboxd.csv', '');
$pmda->add_metric(pmda_pmid(3,30), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT,
	pmda_units(1,0,0,PM_SPACE_BYTE,0,0), 'zimbra.mailboxd.idx.bytes_read',
	'/opt/zimbra/zmstat/mailboxd.csv', '');
$pmda->add_metric(pmda_pmid(3,31), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT,
	pmda_units(1,0,0,PM_SPACE_BYTE,0,0), 'zimbra.mailboxd.idx.bytes_read_avg',
	'/opt/zimbra/zmstat/mailboxd.csv', '');
$pmda->add_metric(pmda_pmid(3,32), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT,
	pmda_units(0,0,1,0,0,PM_COUNT_ONE), 'zimbra.mailboxd.bis.read',
	'/opt/zimbra/zmstat/mailboxd.csv', '');
$pmda->add_metric(pmda_pmid(3,33), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT,
	pmda_units(0,0,0,0,0,0), 'zimbra.mailboxd.bis.seek_rate',
	'/opt/zimbra/zmstat/mailboxd.csv', '');
$pmda->add_metric(pmda_pmid(3,34), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT,
	pmda_units(0,0,0,0,0,0), 'zimbra.mailboxd.db_pool_size',
	'/opt/zimbra/zmstat/mailboxd.csv', '');
$pmda->add_metric(pmda_pmid(3,35), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT,
	pmda_units(0,0,1,0,0,PM_COUNT_ONE), 'zimbra.mailboxd.innodb_bp_hit_rate',
	'/opt/zimbra/zmstat/mailboxd.csv', '');
$pmda->add_metric(pmda_pmid(3,36), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT,
	pmda_units(0,0,1,0,0,PM_COUNT_ONE), 'zimbra.mailboxd.pop_conn',
	'/opt/zimbra/zmstat/mailboxd.csv', '');
$pmda->add_metric(pmda_pmid(3,37), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT,
	pmda_units(0,0,1,0,0,PM_COUNT_ONE), 'zimbra.mailboxd.pop_ssl_conn',
	'/opt/zimbra/zmstat/mailboxd.csv', '');
$pmda->add_metric(pmda_pmid(3,38), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT,
	pmda_units(0,0,1,0,0,PM_COUNT_ONE), 'zimbra.mailboxd.imap_conn',
	'/opt/zimbra/zmstat/mailboxd.csv', '');
$pmda->add_metric(pmda_pmid(3,39), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT,
	pmda_units(0,0,1,0,0,PM_COUNT_ONE), 'zimbra.mailboxd.imap_ssl_conn',
	'/opt/zimbra/zmstat/mailboxd.csv', '');
$pmda->add_metric(pmda_pmid(3,40), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT,
	pmda_units(0,0,1,0,0,PM_COUNT_ONE), 'zimbra.mailboxd.soap_sessions',
	'/opt/zimbra/zmstat/mailboxd.csv', 'Simple Object Access Protocol session count');
# Zimbra duplicates four metrics here, so skip four for direct indexing
$pmda->add_metric(pmda_pmid(3,45), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	pmda_units(0,0,1,0,0,PM_COUNT_ONE), 'zimbra.mailboxd.gc.minor_count',
	'/opt/zimbra/zmstat/mailboxd.csv', 'Java minor garbage collection count');
$pmda->add_metric(pmda_pmid(3,46), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	pmda_units(0,1,0,0,PM_TIME_MSEC,0), 'zimbra.mailboxd.gc.minor_time',
	'/opt/zimbra/zmstat/mailboxd.csv', 'Java minor garbage collection time');
$pmda->add_metric(pmda_pmid(3,47), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	pmda_units(0,0,1,0,0,PM_COUNT_ONE), 'zimbra.mailboxd.gc.major_count',
	'/opt/zimbra/zmstat/mailboxd.csv', 'Java major garbage collection count');
$pmda->add_metric(pmda_pmid(3,48), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	pmda_units(0,1,0,0,PM_TIME_MSEC,0), 'zimbra.mailboxd.gc.major_time',
	'/opt/zimbra/zmstat/mailboxd.csv', 'Java major garbage collection time');
$pmda->add_metric(pmda_pmid(3,49), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT,
	pmda_units(1,0,0,PM_SPACE_BYTE,0,0), 'zimbra.mailboxd.mempool.code_cache_used',
	'/opt/zimbra/zmstat/mailboxd.csv', 'Java code cache space used');
$pmda->add_metric(pmda_pmid(3,50), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT,
	pmda_units(1,0,0,PM_SPACE_BYTE,0,0), 'zimbra.mailboxd.mempool.code_cache_free',
	'/opt/zimbra/zmstat/mailboxd.csv', 'Java code cache space free');
$pmda->add_metric(pmda_pmid(3,51), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT,
	pmda_units(1,0,0,PM_SPACE_BYTE,0,0), 'zimbra.mailboxd.mempool.eden_space_used',
	'/opt/zimbra/zmstat/mailboxd.csv', 'Java Eden area space used');
$pmda->add_metric(pmda_pmid(3,52), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT,
	pmda_units(1,0,0,PM_SPACE_BYTE,0,0), 'zimbra.mailboxd.mempool.eden_space_free',
	'/opt/zimbra/zmstat/mailboxd.csv', 'Java Eden area space free');
$pmda->add_metric(pmda_pmid(3,53), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT,
	pmda_units(1,0,0,PM_SPACE_BYTE,0,0), 'zimbra.mailboxd.mempool.survivor_space_used',
	'/opt/zimbra/zmstat/mailboxd.csv', 'Java survivor area space used');
$pmda->add_metric(pmda_pmid(3,54), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT,
	pmda_units(1,0,0,PM_SPACE_BYTE,0,0), 'zimbra.mailboxd.mempool.survivor_space_free',
	'/opt/zimbra/zmstat/mailboxd.csv', 'Java survivor area space free');
$pmda->add_metric(pmda_pmid(3,55), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT,
	pmda_units(1,0,0,PM_SPACE_BYTE,0,0), 'zimbra.mailboxd.mempool.old_gen_space_used',
	'/opt/zimbra/zmstat/mailboxd.csv', 'Java old generation space used');
$pmda->add_metric(pmda_pmid(3,56), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT,
	pmda_units(1,0,0,PM_SPACE_BYTE,0,0), 'zimbra.mailboxd.mempool.old_gen_space_free',
	'/opt/zimbra/zmstat/mailboxd.csv', 'Java old generation space free');
$pmda->add_metric(pmda_pmid(3,57), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT,
	pmda_units(1,0,0,PM_SPACE_BYTE,0,0), 'zimbra.mailboxd.mempool.perm_gen_space_used',
	'/opt/zimbra/zmstat/mailboxd.csv', 'Java permanent generation space used');
$pmda->add_metric(pmda_pmid(3,58), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT,
	pmda_units(1,0,0,PM_SPACE_BYTE,0,0), 'zimbra.mailboxd.mempool.perm_gen_space_free',
	'/opt/zimbra/zmstat/mailboxd.csv', 'Java permanent generation space free');
$pmda->add_metric(pmda_pmid(3,59), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT,
	pmda_units(1,0,0,PM_SPACE_BYTE,0,0), 'zimbra.mailboxd.heap.used',
	'/opt/zimbra/zmstat/mailboxd.csv', 'Java heap space used');
$pmda->add_metric(pmda_pmid(3,60), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT,
	pmda_units(1,0,0,PM_SPACE_BYTE,0,0), 'zimbra.mailboxd.heap.free',
	'/opt/zimbra/zmstat/mailboxd.csv', 'Java heap space free');

$pmda->add_metric(pmda_pmid(4,0), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT,
	pmda_units(1,0,0,PM_SPACE_KBYTE,0,0), 'zimbra.mtaqueue.bytes',
	'/opt/zimbra/zmstat/mtaqueue.csv',
	'Number of kilobytes queued to the Mail Transfer Agent');
$pmda->add_metric(pmda_pmid(4,1), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT,
	pmda_units(0,1,0,0,PM_TIME_MSEC,0), 'zimbra.mtaqueue.requests',
	'/opt/zimbra/zmstat/mtaqueue.csv',
	'Number of requests queued to the Mail Transfer Agent');

$pmda->add_metric(pmda_pmid(5,0), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT,
	pmda_units(0,0,0,0,0,0), 'zimbra.proc.mailbox.cputime',
	'/opt/zimbra/zmstat/proc.csv',
	'Total time as a percentage spent executing the mailbox process');
$pmda->add_metric(pmda_pmid(5,1), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT,
	pmda_units(0,0,0,0,0,0), 'zimbra.proc.mailbox.utime',
	'/opt/zimbra/zmstat/proc.csv',
	'Total user time as a percentage spent executing the mailbox process');
$pmda->add_metric(pmda_pmid(5,2), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT,
	pmda_units(0,0,0,0,0,0), 'zimbra.proc.mailbox.stime',
	'/opt/zimbra/zmstat/proc.csv',
	'Total systime as a percentage spent executing the mailbox process');
$pmda->add_metric(pmda_pmid(5,3), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT,
	pmda_units(1,0,0,PM_SPACE_MBYTE,0,0), 'zimbra.proc.mailbox.total',
	'/opt/zimbra/zmstat/proc.csv',
	'Total virtual memory footprint of the mailbox processes');
$pmda->add_metric(pmda_pmid(5,4), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT,
	pmda_units(1,0,0,PM_SPACE_MBYTE,0,0), 'zimbra.proc.mailbox.rss',
	'/opt/zimbra/zmstat/proc.csv',
	'Resident set size of the mailbox processes');
$pmda->add_metric(pmda_pmid(5,5), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT,
	pmda_units(1,0,0,PM_SPACE_MBYTE,0,0), 'zimbra.proc.mailbox.shared',
	'/opt/zimbra/zmstat/proc.csv',
	'Shared memory space used by the mailbox processes');
$pmda->add_metric(pmda_pmid(5,6), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT,
	pmda_units(0,0,1,0,0,PM_COUNT_ONE), 'zimbra.proc.mailbox.count',
	'/opt/zimbra/zmstat/proc.csv', 'Total number of mailbox processes');

$pmda->add_metric(pmda_pmid(5,7), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT,
	pmda_units(0,0,0,0,0,0), 'zimbra.proc.mysql.cputime',
	'/opt/zimbra/zmstat/proc.csv',
	'Total time as a percentage spent executing the MySQL process');
$pmda->add_metric(pmda_pmid(5,8), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT,
	pmda_units(0,0,0,0,0,0), 'zimbra.proc.mysql.utime',
	'/opt/zimbra/zmstat/proc.csv',
	'Total user time as a percentage spent executing the MySQL process');
$pmda->add_metric(pmda_pmid(5,9), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT,
	pmda_units(0,0,0,0,0,0), 'zimbra.proc.mysql.stime',
	'/opt/zimbra/zmstat/proc.csv',
	'Total systime as a percentage spent executing the MySQL process');
$pmda->add_metric(pmda_pmid(5,10), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT,
	pmda_units(1,0,0,PM_SPACE_MBYTE,0,0), 'zimbra.proc.mysql.total',
	'/opt/zimbra/zmstat/proc.csv',
	'Total virtual memory footprint of the MySQL processes');
$pmda->add_metric(pmda_pmid(5,11), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT,
	pmda_units(1,0,0,PM_SPACE_MBYTE,0,0), 'zimbra.proc.mysql.rss',
	'/opt/zimbra/zmstat/proc.csv',
	'Resident set size of the MySQL processes');
$pmda->add_metric(pmda_pmid(5,12), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT,
	pmda_units(1,0,0,PM_SPACE_MBYTE,0,0), 'zimbra.proc.mysql.shared',
	'/opt/zimbra/zmstat/proc.csv',
	'Shared memory space used by the MySQL processes');
$pmda->add_metric(pmda_pmid(5,13), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT,
	pmda_units(0,0,1,0,0,PM_COUNT_ONE), 'zimbra.proc.mysql.count',
	'/opt/zimbra/zmstat/proc.csv', 'Total number of MySQL processes');

$pmda->add_metric(pmda_pmid(5,14), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT,
	pmda_units(0,0,0,0,0,0), 'zimbra.proc.convertd.cputime',
	'/opt/zimbra/zmstat/proc.csv',
	'Total time as a percentage spent executing the convertd process');
$pmda->add_metric(pmda_pmid(5,15), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT,
	pmda_units(0,0,0,0,0,0), 'zimbra.proc.convertd.utime',
	'/opt/zimbra/zmstat/proc.csv',
	'Total user time as a percentage spent executing the convertd process');
$pmda->add_metric(pmda_pmid(5,16), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT,
	pmda_units(0,0,0,0,0,0), 'zimbra.proc.convertd.stime',
	'/opt/zimbra/zmstat/proc.csv',
	'Total systime as a percentage spent executing the convertd process');
$pmda->add_metric(pmda_pmid(5,17), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT,
	pmda_units(1,0,0,PM_SPACE_MBYTE,0,0), 'zimbra.proc.convertd.total',
	'/opt/zimbra/zmstat/proc.csv',
	'Total virtual memory footprint of the convertd processes');
$pmda->add_metric(pmda_pmid(5,18), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT,
	pmda_units(1,0,0,PM_SPACE_MBYTE,0,0), 'zimbra.proc.convertd.rss',
	'/opt/zimbra/zmstat/proc.csv',
	'Resident set size of the convertd processes');
$pmda->add_metric(pmda_pmid(5,19), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT,
	pmda_units(1,0,0,PM_SPACE_MBYTE,0,0), 'zimbra.proc.convertd.shared',
	'/opt/zimbra/zmstat/proc.csv',
	'Shared memory space used by the convertd processes');
$pmda->add_metric(pmda_pmid(5,20), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT,
	pmda_units(0,0,1,0,0,PM_COUNT_ONE), 'zimbra.proc.convertd.count',
	'/opt/zimbra/zmstat/proc.csv', 'Total number of convertd processes');

$pmda->add_metric(pmda_pmid(5,21), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT,
	pmda_units(0,0,0,0,0,0), 'zimbra.proc.ldap.cputime',
	'/opt/zimbra/zmstat/proc.csv',
	'Total time as a percentage spent executing the LDAP process');
$pmda->add_metric(pmda_pmid(5,22), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT,
	pmda_units(0,0,0,0,0,0), 'zimbra.proc.ldap.utime',
	'/opt/zimbra/zmstat/proc.csv',
	'Total user time as a percentage spent executing the LDAP process');
$pmda->add_metric(pmda_pmid(5,23), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT,
	pmda_units(0,0,0,0,0,0), 'zimbra.proc.ldap.stime',
	'/opt/zimbra/zmstat/proc.csv',
	'Total systime as a percentage spent executing the LDAP process');
$pmda->add_metric(pmda_pmid(5,24), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT,
	pmda_units(1,0,0,PM_SPACE_MBYTE,0,0), 'zimbra.proc.ldap.total',
	'/opt/zimbra/zmstat/proc.csv',
	'Total virtual memory footprint of the LDAP processes');
$pmda->add_metric(pmda_pmid(5,25), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT,
	pmda_units(1,0,0,PM_SPACE_MBYTE,0,0), 'zimbra.proc.ldap.rss',
	'/opt/zimbra/zmstat/proc.csv',
	'Resident set size of the LDAP processes');
$pmda->add_metric(pmda_pmid(5,26), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT,
	pmda_units(1,0,0,PM_SPACE_MBYTE,0,0), 'zimbra.proc.ldap.shared',
	'/opt/zimbra/zmstat/proc.csv',
	'Shared memory space used by the LDAP processes');
$pmda->add_metric(pmda_pmid(5,27), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT,
	pmda_units(0,0,1,0,0,PM_COUNT_ONE), 'zimbra.proc.ldap.count',
	'/opt/zimbra/zmstat/proc.csv', 'Total number of LDAP processes');

$pmda->add_metric(pmda_pmid(5,28), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT,
	pmda_units(0,0,0,0,0,0), 'zimbra.proc.postfix.cputime',
	'/opt/zimbra/zmstat/proc.csv',
	'Total time as a percentage spent executing the postfix process');
$pmda->add_metric(pmda_pmid(5,29), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT,
	pmda_units(0,0,0,0,0,0), 'zimbra.proc.postfix.utime',
	'/opt/zimbra/zmstat/proc.csv',
	'Total user time as a percentage spent executing the postfix process');
$pmda->add_metric(pmda_pmid(5,30), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT,
	pmda_units(0,0,0,0,0,0), 'zimbra.proc.postfix.stime',
	'/opt/zimbra/zmstat/proc.csv',
	'Total systime as a percentage spent executing the postfix process');
$pmda->add_metric(pmda_pmid(5,31), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT,
	pmda_units(1,0,0,PM_SPACE_MBYTE,0,0), 'zimbra.proc.postfix.total',
	'/opt/zimbra/zmstat/proc.csv',
	'Total virtual memory footprint of the postfix processes');
$pmda->add_metric(pmda_pmid(5,32), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT,
	pmda_units(1,0,0,PM_SPACE_MBYTE,0,0), 'zimbra.proc.postfix.rss',
	'/opt/zimbra/zmstat/proc.csv',
	'Resident set size of the postfix processes');
$pmda->add_metric(pmda_pmid(5,33), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT,
	pmda_units(1,0,0,PM_SPACE_MBYTE,0,0), 'zimbra.proc.postfix.shared',
	'/opt/zimbra/zmstat/proc.csv',
	'Shared memory space used by the postfix processes');
$pmda->add_metric(pmda_pmid(5,34), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT,
	pmda_units(0,0,1,0,0,PM_COUNT_ONE), 'zimbra.proc.postfix.count',
	'/opt/zimbra/zmstat/proc.csv', 'Total number of postfix processes');

$pmda->add_metric(pmda_pmid(5,35), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT,
	pmda_units(0,0,0,0,0,0), 'zimbra.proc.amavis.cputime',
	'/opt/zimbra/zmstat/proc.csv',
	'Total time as a percentage spent executing the virus scanner');
$pmda->add_metric(pmda_pmid(5,36), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT,
	pmda_units(0,0,0,0,0,0), 'zimbra.proc.amavis.utime',
	'/opt/zimbra/zmstat/proc.csv',
	'Total user time as a percentage spent executing the virus scanner');
$pmda->add_metric(pmda_pmid(5,37), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT,
	pmda_units(0,0,0,0,0,0), 'zimbra.proc.amavis.stime',
	'/opt/zimbra/zmstat/proc.csv',
	'Total systime as a percentage spent executing the virus scanner');
$pmda->add_metric(pmda_pmid(5,38), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT,
	pmda_units(1,0,0,PM_SPACE_MBYTE,0,0), 'zimbra.proc.amavis.total',
	'/opt/zimbra/zmstat/proc.csv',
	'Total virtual memory footprint of the virus scanner process');
$pmda->add_metric(pmda_pmid(5,39), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT,
	pmda_units(1,0,0,PM_SPACE_MBYTE,0,0), 'zimbra.proc.amavis.rss',
	'/opt/zimbra/zmstat/proc.csv',
	'Resident set size of the virus scanner process');
$pmda->add_metric(pmda_pmid(5,40), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT,
	pmda_units(1,0,0,PM_SPACE_MBYTE,0,0), 'zimbra.proc.amavis.shared',
	'/opt/zimbra/zmstat/proc.csv',
	'Shared memory space used by the virus scanner processes');
$pmda->add_metric(pmda_pmid(5,41), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT,
	pmda_units(0,0,1,0,0,PM_COUNT_ONE), 'zimbra.proc.amavis.count',
	'/opt/zimbra/zmstat/proc.csv', 'Total number of virus scanner processes');

$pmda->add_metric(pmda_pmid(5,42), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT,
	pmda_units(0,0,0,0,0,0), 'zimbra.proc.clam.cputime',
	'/opt/zimbra/zmstat/proc.csv',
	'Total time as a percentage spent executing the anti-virus process');
$pmda->add_metric(pmda_pmid(5,43), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT,
	pmda_units(0,0,0,0,0,0), 'zimbra.proc.clam.utime',
	'/opt/zimbra/zmstat/proc.csv',
	'Total user time as a percentage spent executing the anti-virus process');
$pmda->add_metric(pmda_pmid(5,44), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT,
	pmda_units(0,0,0,0,0,0), 'zimbra.proc.clam.stime',
	'/opt/zimbra/zmstat/proc.csv',
	'Total systime as a percentage spent executing the anti-virus process');
$pmda->add_metric(pmda_pmid(5,45), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT,
	pmda_units(1,0,0,PM_SPACE_MBYTE,0,0), 'zimbra.proc.clam.total',
	'/opt/zimbra/zmstat/proc.csv',
	'Total virtual memory footprint of the anti-virus processes');
$pmda->add_metric(pmda_pmid(5,46), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT,
	pmda_units(1,0,0,PM_SPACE_MBYTE,0,0), 'zimbra.proc.clam.rss',
	'/opt/zimbra/zmstat/proc.csv',
	'Resident set size of the clam processes');
$pmda->add_metric(pmda_pmid(5,47), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT,
	pmda_units(1,0,0,PM_SPACE_MBYTE,0,0), 'zimbra.proc.clam.shared',
	'/opt/zimbra/zmstat/proc.csv',
	'Shared memory space used by the anti-virus processes');
$pmda->add_metric(pmda_pmid(5,48), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT,
	pmda_units(0,0,1,0,0,PM_COUNT_ONE), 'zimbra.proc.clam.count',
	'/opt/zimbra/zmstat/proc.csv', 'Total number of anti-virus processes');

$pmda->add_metric(pmda_pmid(6,0), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT,
	pmda_units(0,0,1,0,0,PM_COUNT_ONE), 'zimbra.threads.btpool',
	'/opt/zimbra/zmstat/threads.csv', '');
$pmda->add_metric(pmda_pmid(6,1), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT,
	pmda_units(0,0,1,0,0,PM_COUNT_ONE), 'zimbra.threads.pool',
	'/opt/zimbra/zmstat/threads.csv', '');
$pmda->add_metric(pmda_pmid(6,2), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT,
	pmda_units(0,0,1,0,0,PM_COUNT_ONE), 'zimbra.threads.lmtp',
	'/opt/zimbra/zmstat/threads.csv', '');
$pmda->add_metric(pmda_pmid(6,3), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT,
	pmda_units(0,0,1,0,0,PM_COUNT_ONE), 'zimbra.threads.imap',
	'/opt/zimbra/zmstat/threads.csv', '');
$pmda->add_metric(pmda_pmid(6,4), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT,
	pmda_units(0,0,1,0,0,PM_COUNT_ONE), 'zimbra.threads.pop3',
	'/opt/zimbra/zmstat/threads.csv', '');
$pmda->add_metric(pmda_pmid(6,5), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT,
	pmda_units(0,0,1,0,0,PM_COUNT_ONE), 'zimbra.threads.scheduled_tasks',
	'/opt/zimbra/zmstat/threads.csv', '');
$pmda->add_metric(pmda_pmid(6,6), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT,
	pmda_units(0,0,1,0,0,PM_COUNT_ONE), 'zimbra.threads.timer',
	'/opt/zimbra/zmstat/threads.csv', '');
$pmda->add_metric(pmda_pmid(6,7), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT,
	pmda_units(0,0,1,0,0,PM_COUNT_ONE), 'zimbra.threads.anon_io',
	'/opt/zimbra/zmstat/threads.csv', '');
$pmda->add_metric(pmda_pmid(6,8), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT,
	pmda_units(0,0,1,0,0,PM_COUNT_ONE), 'zimbra.threads.flap_processor',
	'/opt/zimbra/zmstat/threads.csv', '');
$pmda->add_metric(pmda_pmid(6,9), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT,
	pmda_units(0,0,1,0,0,PM_COUNT_ONE), 'zimbra.threads.gc',
	'/opt/zimbra/zmstat/threads.csv', '');
$pmda->add_metric(pmda_pmid(6,10), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT,
	pmda_units(0,0,1,0,0,PM_COUNT_ONE), 'zimbra.threads.socket_acceptor',
	'/opt/zimbra/zmstat/threads.csv', '');
$pmda->add_metric(pmda_pmid(6,11), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT,
	pmda_units(0,0,1,0,0,PM_COUNT_ONE), 'zimbra.threads.thread',
	'/opt/zimbra/zmstat/threads.csv', '');
$pmda->add_metric(pmda_pmid(6,12), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT,
	pmda_units(0,0,1,0,0,PM_COUNT_ONE), 'zimbra.threads.other',
	'/opt/zimbra/zmstat/threads.csv', '');
$pmda->add_metric(pmda_pmid(6,13), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT,
	pmda_units(0,0,1,0,0,PM_COUNT_ONE), 'zimbra.threads.total',
	'/opt/zimbra/zmstat/threads.csv', '');

$pmda->add_indom($imap_domain, \@imap_indom, 'IMAP operations',
		'Internet Message Access Protocol operations');
$pmda->add_indom($soap_domain, \@soap_indom, 'SOAP operation',
		'Simple Object Access Protocol operations');

$pmda->add_tail($stats . 'fd.csv', \&zimbra_fd_parser, undef);
$pmda->add_tail($stats . 'imap.csv', \&zimbra_imap_parser, undef);
$pmda->add_tail($stats . 'mailboxd.csv', \&zimbra_mailbox_parser, undef);
$pmda->add_tail($stats . 'mtaqueue.csv', \&zimbra_mtaqueue_parser, undef);
$pmda->add_tail($stats . 'proc.csv', \&zimbra_proc_parser, undef);
$pmda->add_tail($stats . 'soap.csv', \&zimbra_soap_parser, undef);
$pmda->add_tail($stats . 'threads.csv', \&zimbra_threads_parser, undef);

$pmda->set_fetch_callback(\&zimbra_fetch_callback);
$pmda->run;

=pod

=head1 NAME

pmdazimbra - Zimbra Collaboration Suite (ZCS) PMDA

=head1 DESCRIPTION

B<pmdazimbra> is a Performance Metrics Domain Agent (PMDA) which
exports metric values from several subsystems of the Zimbra Suite.
Further details on Zimbra can be found at http://www.zimbra.com/.

=head1 INSTALLATION

If you want access to the names and values for the zimbra performance
metrics, do the following as root:

	# cd $PCP_PMDAS_DIR/zimbra
	# ./Install

If you want to undo the installation, do the following as root:

	# cd $PCP_PMDAS_DIR/zimbra
	# ./Remove

B<pmdazimbra> is launched by pmcd(1) and should never be executed
directly.  The Install and Remove scripts notify pmcd(1) when
the agent is installed or removed.

=head1 FILES

=over

=item /opt/zimbra/zmstat/*

comma-separated value files containing Zimbra performance data

=item $PCP_PMDAS_DIR/zimbra/Install

installation script for the B<pmdazimbra> agent

=item $PCP_PMDAS_DIR/zimbra/Remove

undo installation script for the B<pmdazimbra> agent

=item $PCP_LOG_DIR/pmcd/zimbra.log

default log file for error messages from B<pmdazimbra>

=back

=head1 SEE ALSO

pmcd(1).
