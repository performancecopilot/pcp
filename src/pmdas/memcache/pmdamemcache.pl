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
# The memcached PMDA
# NOTE: Not all metrics are exported at the moment, in particular,
# the per-slab and per-item statistics are not.  It may be better to
# manage instances differently as these values are dynamic - perhaps
# have the monitored memcaches (the current indom) in the namespace,
# like the old DBMS PMDAs.
# 
use strict;
use warnings;
use PCP::PMDA;
use vars qw( $pmda $id $n %caches );

my $memcache_delay = 5;	# refresh rate in seconds
my @memcache_instances = ( 0 => '127.0.0.1:11211',
			 # 1 => '127.0.0.1:11212',
			 # 2 => '192.168.5.76:11211',
			 );
# Configuration files for overriding the above settings
for my $file (	pmda_config('PCP_PMDAS_DIR') . '/memcache/memcache.conf',
		'./memcache.conf' ) {
    eval `cat $file` unless ! -f $file;
}

my $memcache_indom = 0;	# one for each memcached
my $query = "stats\r\nstats slabs\r\nstats items\r\n"; # sent to memcached

sub memcache_stats_callback
{
    ( $id, $_ ) = @_;
    # $pmda->log("memcache_stats_callback: id $id");

    if (/^STAT items:(\d+):(\w+) (\d+)/) {	# stats items
	# $caches{$id}{"item$1"}{$2} = $3;
    }
    elsif (/^STAT (\d+):(\w+) (\d+)/) {		# stats slabs
	# $caches{$id}{"slab$1"}{$2} = $3;
    }
    elsif (/^STAT (\w+) (\d+)/) {		# stats
	$caches{$id}{$1} = $2;
    }
    elsif (!(/^END/)) {				# unknown
	$pmda->log("Eh?: $_");
    }
}

sub memcache_connect
{
    # $pmda->log("memcache_connect: $#memcache_instances");

    for ($id = 0; $id < $#memcache_instances; $id += 2) {
	my ($host, $port) = split(/:/, $memcache_instances[$id+1]);
	$pmda->add_sock($host, $port, \&memcache_stats_callback, $id);
    }
}

sub memcache_timer_callback
{
    # $pmda->log("memcache_timer_callback");

    for ($id = 0; $id < $#memcache_instances; $id += 2) {
	$pmda->put_sock($id, $query);
    }
}

sub memcache_fetch_callback
{
    my ($cluster, $item, $inst) = @_;

    # $pmda->log("memcache_fetch_callback: $cluster:$item ($inst)");

    return (PM_ERR_INST, 0) unless ($inst != PM_IN_NULL);
    return (PM_ERR_INST, 0) unless ($inst < $#memcache_instances);
    $id = $memcache_instances[$inst];
    return (PM_ERR_AGAIN, 0) unless defined($caches{$id});

    if ($cluster == 0) {
	if ($item == 0)     { return ($caches{$id}{'pid'}, 1); }
	elsif ($item == 1)  { return ($caches{$id}{'uptime'}, 1); }
	elsif ($item == 2)  { return ($caches{$id}{'curr_items'}, 1); }
	elsif ($item == 3)  { return ($caches{$id}{'total_items'}, 1); }
	elsif ($item == 4)  { return ($caches{$id}{'bytes'}, 1); }
	elsif ($item == 5)  { return ($caches{$id}{'curr_connections'}, 1); }
	elsif ($item == 6)  { return ($caches{$id}{'total_connections'}, 1); }
	elsif ($item == 7)  { return ($caches{$id}{'connection_structures'}, 1); }
	elsif ($item == 8)  { return ($caches{$id}{'cmd_get'}, 1); }
	elsif ($item == 9)  { return ($caches{$id}{'cmd_set'}, 1); }
	elsif ($item == 10) { return ($caches{$id}{'get_hits'}, 1); }
	elsif ($item == 11) { return ($caches{$id}{'get_misses'}, 1); }
	elsif ($item == 12) { return ($caches{$id}{'bytes_read'}, 1); }
	elsif ($item == 13) { return ($caches{$id}{'bytes_written'}, 1); }
	elsif ($item == 14) { return ($caches{$id}{'limit_maxbytes'}, 1); }
    }
#   elsif ($cluster == 1) {
#	# many different slabs (X..Y), and 7 metrics in this cluster
#	if ($item > 7 * 11) { return (PM_ERR_PMID, 0); }
#	$id = int($item / 7) + 6;
#	$item %= 7;
#	my $slab = "slab$id";
#
#	return (PM_ERR_AGAIN, 0) unless defined($caches{$id}{$slab});
#	if ($item == 0)     { return ($caches{$id}{$slab}{'chunk_size'}, 1); }
#	elsif ($item == 1)  { return ($caches{$id}{$slab}{'chunks_per_page'}, 1); }
#	elsif ($item == 2)  { return ($caches{$id}{$slab}{'total_pages'}, 1); }
#	elsif ($item == 3)  { return ($caches{$id}{$slab}{'total_chunks'}, 1); }
#	elsif ($item == 4)  { return ($caches{$id}{$slab}{'used_chunks'}, 1); }
#	elsif ($item == 5)  { return ($caches{$id}{$slab}{'free_chunks'}, 1); }
#	elsif ($item == 6)  { return ($caches{$id}{$slab}{'free_chunks_end'}, 1); }
#   }
    elsif ($cluster == 2) {
	if ($item == 0)     { return ($caches{$id}{'active_slabs'}, 1); }
	elsif ($item == 1)  { return ($caches{$id}{'total_malloced'}, 1); }
    }
#   elsif ($cluster == 3) {
#	# many different slabs (X..Y), and 2 metrics in this cluster
#	if ($item > 2 * [Y]) { return (PM_ERR_PMID, 0); }
#	$id = int($item / 2) + [X];
#	$item %= 2;
#	my $itemid = "item$id";
#
#	return (PM_ERR_AGAIN, 0) unless defined($caches{$id}{$itemid});
#	if ($item == 0)     { return ($caches{$id}{$itemid}{'count'}, 1); }
#	elsif ($item == 1)  { return ($caches{$id}{$itemid}{'age'}, 1); }
#   }
    return (PM_ERR_PMID, 0);
}

$pmda = PCP::PMDA->new('memcache', 89);

$pmda->add_metric(pmda_pmid(0,0), PM_TYPE_U32, $memcache_indom,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'memcache.pid', '', '');
$pmda->add_metric(pmda_pmid(0,1), PM_TYPE_U32, $memcache_indom,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'memcache.uptime', '', '');
$pmda->add_metric(pmda_pmid(0,2), PM_TYPE_U32, $memcache_indom,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'memcache.current_items', '', '');
$pmda->add_metric(pmda_pmid(0,3), PM_TYPE_U32, $memcache_indom,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'memcache.total_items', '', '');
$pmda->add_metric(pmda_pmid(0,4), PM_TYPE_U64, $memcache_indom,
		  PM_SEM_INSTANT, pmda_units(1,0,0,PM_SPACE_BYTE,0,0),
		  'memcache.bytes', '', '');
$pmda->add_metric(pmda_pmid(0,5), PM_TYPE_U32, $memcache_indom,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'memcache.current_connections', '', '');
$pmda->add_metric(pmda_pmid(0,6), PM_TYPE_U32, $memcache_indom,
		  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  'memcache.total_connections', '', '');
$pmda->add_metric(pmda_pmid(0,7), PM_TYPE_U32, $memcache_indom,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'memcache.connection_structures', '', '');
$pmda->add_metric(pmda_pmid(0,8), PM_TYPE_U32, $memcache_indom,
		  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  'memcache.gets', '', '');
$pmda->add_metric(pmda_pmid(0,9), PM_TYPE_U32, $memcache_indom,
		  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  'memcache.sets', '', '');
$pmda->add_metric(pmda_pmid(0,10), PM_TYPE_U32, $memcache_indom,
		  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  'memcache.hits', '', '');
$pmda->add_metric(pmda_pmid(0,11), PM_TYPE_U32, $memcache_indom,
		  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  'memcache.misses', '', '');
$pmda->add_metric(pmda_pmid(0,12), PM_TYPE_U64, $memcache_indom,
		  PM_SEM_COUNTER, pmda_units(1,0,0,PM_SPACE_BYTE,0,0),
		  'memcache.bytes_read', '', '');
$pmda->add_metric(pmda_pmid(0,13), PM_TYPE_U64, $memcache_indom,
		  PM_SEM_COUNTER, pmda_units(1,0,0,PM_SPACE_BYTE,0,0),
		  'memcache.bytes_written', '', '');
$pmda->add_metric(pmda_pmid(0,14), PM_TYPE_U32, $memcache_indom,
		  PM_SEM_INSTANT, pmda_units(1,0,0,PM_SPACE_BYTE,0,0),
		  'memcache.limit_maxbytes', '', '');
# $id = 0;
# foreach $n (6 .. 17) {	# stats slabs (N=6-17)
#     $pmda->add_metric(pmda_pmid(1,$id++), PM_TYPE_U32, $memcache_indom,
# 		      PM_SEM_INSTANT, pmda_units(1,0,0,PM_SPACE_BYTE,0,0),
# 		      "memcache.slabs.slab$n.chunk_size", '', '');
#     $pmda->add_metric(pmda_pmid(1,$id++), PM_TYPE_U32, $memcache_indom,
# 		      PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
# 		      "memcache.slabs.slab$n.chunks_per_page", '', '');
#     $pmda->add_metric(pmda_pmid(1,$id++), PM_TYPE_U32, $memcache_indom,
# 		      PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
# 		      "memcache.slabs.slab$n.total_pages", '', '');
#     $pmda->add_metric(pmda_pmid(1,$id++), PM_TYPE_U32, $memcache_indom,
# 		      PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
# 		      "memcache.slabs.slab$n.total_chunks", '', '');
#     $pmda->add_metric(pmda_pmid(1,$id++), PM_TYPE_U32, $memcache_indom,
# 		      PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
# 		      "memcache.slabs.slab$n.used_chunks", '', '');
#     $pmda->add_metric(pmda_pmid(1,$id++), PM_TYPE_U32, $memcache_indom,
# 		      PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
# 		      "memcache.slabs.slab$n.free_chunks", '', '');
#     $pmda->add_metric(pmda_pmid(1,$id++), PM_TYPE_U32, $memcache_indom,
# 		      PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
# 		      "memcache.slabs.slab$n.free_chunks_end", '', '');
# }

$pmda->add_metric(pmda_pmid(2,0), PM_TYPE_U32, $memcache_indom,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'memcache.active_slabs', '', '');
$pmda->add_metric(pmda_pmid(2,1), PM_TYPE_U32, $memcache_indom,
		  PM_SEM_INSTANT, pmda_units(1,0,0,PM_SPACE_BYTE,0,0),
		  'memcache.total_malloced', '', '');

# $id = 0;
# foreach $n (6 .. 17) {	# stats items (N=6-17)
#     $pmda->add_metric(pmda_pmid(3,$id++), PM_TYPE_U32, $memcache_indom,
# 		      PM_SEM_INSTANT, pmda_units(1,0,0,PM_SPACE_BYTE,0,0),
# 		      "memcache.items.item$n.count", '', '');
#     $pmda->add_metric(pmda_pmid(3,$id++), PM_TYPE_U32, $memcache_indom,
# 		      PM_SEM_INSTANT, pmda_units(0,1,0,0,PM_TIME_SEC,0),
# 		      "memcache.items.item$n.age", '', '');
# }

$pmda->add_indom($memcache_indom, \@memcache_instances,
		 'Instance domain exporting each memcache daemon', '');

$pmda->add_timer($memcache_delay, \&memcache_timer_callback, 0);
$pmda->set_fetch_callback(\&memcache_fetch_callback);

&memcache_connect;

$pmda->set_user('pcp');
$pmda->run;

=pod

=head1 NAME

pmdamemcache - memcache performance metrics domain agent (PMDA)

=head1 DESCRIPTION

This PMDA extracts performance data from memcached, a distributed memory
caching daemon commonly used to improve web serving performance.  A farm
of memcached processes over multiple servers can be utilised by a single
web application, increasing the total available object cache size, and
decreasing the database load associated with smaller cache sizes.  This
system is described in detail at http://www.danga.com/memcached.

=head1 INSTALLATION

Configure B<pmdamemcache> to extract the values from set of hosts
used in the memcache farm.  These hosts can be listed in the
$PCP_PMDAS_DIR/memcache/memcache.conf file, in the format (i.e.
Perl array) described at the top of pmdamemcache.pl.  A custom
refresh rate can also be configured using this mechanism.

	# cd $PCP_PMDAS_DIR/memcache
	# [ edit memcache.conf ]

Once this is setup, you can access the names and values for the
memcache performance metrics by doing the following as root:

	# cd $PCP_PMDAS_DIR/memcache
	# ./Install

If you want to undo the installation, do the following as root:

	# cd $PCP_PMDAS_DIR/memcache
	# ./Remove

B<pmdamemcache> is launched by pmcd(1) and should never be executed
directly.  The Install and Remove scripts notify pmcd(1) when
the agent is installed or removed.

=head1 FILES

=over

=item $PCP_PMDAS_DIR/memcache/memcache.conf

configuration file listing monitored memcache instances

=item $PCP_PMDAS_DIR/memcache/Install

installation script for the B<pmdamemcache> agent

=item $PCP_PMDAS_DIR/memcache/Remove

undo installation script for the B<pmdamemcache> agent

=item $PCP_LOG_DIR/pmcd/memcache.log

default log file for error messages from B<pmdamemcache>

=back

=head1 SEE ALSO

pmcd(1) and memcached(1).
