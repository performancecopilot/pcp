#!/usr/bin/perl -w
#
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
# You should have received a copy of the GNU General Public License along
# with this program; if not, write to the Free Software Foundation, Inc.,
# 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
# 

use strict;
use PCP::PMDA;
use vars qw( $pmda $id $n %caches );

my @memcache_instances = ( 0 => '127.0.0.1:11211',
			 # 1 => '127.0.0.1:11212',
			 # 2 => '192.168.5.76:11211',
			 );

my $memcache_indom = 0;	# one for each memcached
my $query = "stats\r\nstats slabs\r\nstats items\r\n"; # sent to memcached

sub memcache_stats_callback
{
    $id = shift;

    if (/^STAT items:(\d+):(\w+) (\d+)$/) {	# stats items
	$caches{$id}{"item$1"}{$2} = $3;
    }
    elsif (/^STAT (\d+):(\w+) (\d+)$/) {	# stats slabs
	$caches{$id}{"slab$1"}{$2} = $3;
    }
    elsif (/^STAT (\w+) (\d+)$/) {		# stats
	$caches{$id}{$1} = $2;
    }
    elsif (!(/^END$/)) {			# unknown
	$pmda->log("Eh?: $_");
    }
}

sub memcache_connect
{
    for ($id = 1; $id < $#memcache_instances; $id += 2) {
	my ($host, $port) = split(/:/, $memcache_instances[$id]);
	$pmda->add_sock($host, $port, \&memcache_stats_callback, $id);
    }
}

sub memcache_timer_callback
{
    for ($id = 1; $id < $#memcache_instances; $id += 2) {
	$pmda->put_sock($id, $query);
    }
}

sub memcache_fetch_callback
{
    my ($cluster, $item, $inst) = @_;

    return (PM_ERR_INST, 0) unless ($inst > 0 && $inst < $#memcache_instances);
    $id = $memcache_instances[$inst];

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
    elsif ($cluster == 1) {
	# 11 different slabs (6..17), and 7 metrics in this cluster
	if ($item > 7 * 11) { return (PM_ERR_PMID, 0); }
	$id = $item / 7;
	$item %= 7;
	my $slab = "slab$id";

	if ($item == 0)     { return ($caches{$id}{$slab}{'chunk_size'}, 1); }
	elsif ($item == 1)  { return ($caches{$id}{$slab}{'chunks_per_page'}, 1); }
	elsif ($item == 2)  { return ($caches{$id}{$slab}{'total_pages'}, 1); }
	elsif ($item == 3)  { return ($caches{$id}{$slab}{'total_chunks'}, 1); }
	elsif ($item == 4)  { return ($caches{$id}{$slab}{'used_chunks'}, 1); }
	elsif ($item == 5)  { return ($caches{$id}{$slab}{'free_chunks'}, 1); }
	elsif ($item == 6)  { return ($caches{$id}{$slab}{'free_chunks_end'}, 1); }
    }
    elsif ($cluster == 2) {
	if ($item == 0)     { return ($caches{$id}{'active_slabs'}, 1); }
	elsif ($item == 1)  { return ($caches{$id}{'total_malloced'}, 1); }
    }
    elsif ($cluster == 3) {
	# 11 different slabs (6..17), and 2 metrics in this cluster
	if ($item > 2 * 11) { return (PM_ERR_PMID, 0); }
	$id = $item / 2;
	$item %= 2;
	my $itemid = "item$id";

	if ($item == 0)     { return ($caches{$id}{$itemid}{'count'}, 1); }
	elsif ($item == 1)  { return ($caches{$id}{$itemid}{'age'}, 1); }
    }
    return (PM_ERR_PMID, 0);
}

$pmda = PCP::PMDA->new('memcache', 89);

$pmda->add_metric(pmda_pmid(0,0), PM_TYPE_U32, $memcache_indom,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'memcache.pid', undef, undef);
$pmda->add_metric(pmda_pmid(0,1), PM_TYPE_U32, $memcache_indom,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'memcache.uptime', undef, undef);
$pmda->add_metric(pmda_pmid(0,2), PM_TYPE_U32, $memcache_indom,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'memcache.current_items', undef, undef);
$pmda->add_metric(pmda_pmid(0,3), PM_TYPE_U32, $memcache_indom,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'memcache.total_items', undef, undef);
$pmda->add_metric(pmda_pmid(0,4), PM_TYPE_U64, $memcache_indom,
		  PM_SEM_INSTANT, pmda_units(1,0,0,PM_SPACE_BYTE,0,0),
		  'memcache.bytes', undef, undef);
$pmda->add_metric(pmda_pmid(0,5), PM_TYPE_U32, $memcache_indom,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'memcache.current_connections', undef, undef);
$pmda->add_metric(pmda_pmid(0,6), PM_TYPE_U32, $memcache_indom,
		  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  'memcache.total_connections', undef, undef);
$pmda->add_metric(pmda_pmid(0,7), PM_TYPE_U32, $memcache_indom,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'memcache.connection_structures', undef, undef);
$pmda->add_metric(pmda_pmid(0,8), PM_TYPE_U32, $memcache_indom,
		  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  'memcache.gets', undef, undef);
$pmda->add_metric(pmda_pmid(0,9), PM_TYPE_U32, $memcache_indom,
		  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  'memcache.sets', undef, undef);
$pmda->add_metric(pmda_pmid(0,10), PM_TYPE_U32, $memcache_indom,
		  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  'memcache.hits', undef, undef);
$pmda->add_metric(pmda_pmid(0,11), PM_TYPE_U32, $memcache_indom,
		  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  'memcache.misses', undef, undef);
$pmda->add_metric(pmda_pmid(0,12), PM_TYPE_U64, $memcache_indom,
		  PM_SEM_COUNTER, pmda_units(1,0,0,PM_SPACE_BYTE,0,0),
		  'memcache.bytes_read', undef, undef);
$pmda->add_metric(pmda_pmid(0,13), PM_TYPE_U64, $memcache_indom,
		  PM_SEM_COUNTER, pmda_units(1,0,0,PM_SPACE_BYTE,0,0),
		  'memcache.bytes_written', undef, undef);
$pmda->add_metric(pmda_pmid(0,14), PM_TYPE_U32, $memcache_indom,
		  PM_SEM_INSTANT, pmda_units(1,0,0,PM_SPACE_BYTE,0,0),
		  'memcache.limit_maxbytes', undef, undef);

$id = 0;
foreach $n (6 .. 17) {	# stats slabs (N=6-17)
    $pmda->add_metric(pmda_pmid(1,$id++), PM_TYPE_U32, $memcache_indom,
		      PM_SEM_INSTANT, pmda_units(1,0,0,PM_SPACE_BYTE,0,0),
		      "memcache.slabs.slab$n.chunk_size", undef, undef);
    $pmda->add_metric(pmda_pmid(1,$id++), PM_TYPE_U32, $memcache_indom,
		      PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		      "memcache.slabs.slab$n.chunks_per_page", undef, undef);
    $pmda->add_metric(pmda_pmid(1,$id++), PM_TYPE_U32, $memcache_indom,
		      PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		      "memcache.slabs.slab$n.total_pages", undef, undef);
    $pmda->add_metric(pmda_pmid(1,$id++), PM_TYPE_U32, $memcache_indom,
		      PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		      "memcache.slabs.slab$n.total_chunks", undef, undef);
    $pmda->add_metric(pmda_pmid(1,$id++), PM_TYPE_U32, $memcache_indom,
		      PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		      "memcache.slabs.slab$n.used_chunks", undef, undef);
    $pmda->add_metric(pmda_pmid(1,$id++), PM_TYPE_U32, $memcache_indom,
		      PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		      "memcache.slabs.slab$n.free_chunks", undef, undef);
    $pmda->add_metric(pmda_pmid(1,$id++), PM_TYPE_U32, $memcache_indom,
		      PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		      "memcache.slabs.slab$n.free_chunks_end", undef, undef);
}

$pmda->add_metric(pmda_pmid(2,0), PM_TYPE_U32, $memcache_indom,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'memcache.active_slabs', undef, undef);
$pmda->add_metric(pmda_pmid(2,1), PM_TYPE_U32, $memcache_indom,
		  PM_SEM_INSTANT, pmda_units(1,0,0,PM_SPACE_BYTE,0,0),
		  'memcache.total_malloced', undef, undef);

$id = 0;
foreach $n (6 .. 17) {	# stats items (N=6-17)
    $pmda->add_metric(pmda_pmid(3,$id++), PM_TYPE_U32, $memcache_indom,
		      PM_SEM_INSTANT, pmda_units(1,0,0,PM_SPACE_BYTE,0,0),
		      "memcache.items.item$n.count", undef, undef);
    $pmda->add_metric(pmda_pmid(3,$id++), PM_TYPE_U32, $memcache_indom,
		      PM_SEM_INSTANT, pmda_units(0,1,0,0,PM_TIME_SEC,0),
		      "memcache.items.item$n.age", undef, undef);
}

$pmda->add_indom( $memcache_indom, \@memcache_instances, '', '' );
$pmda->add_timer( 5.0, \&memcache_fetch_callback, 0 );
$pmda->set_fetch_callback( \&memcache_fetch_callback );

&memcache_connect;
$pmda->run;
