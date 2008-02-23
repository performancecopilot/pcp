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

my @memcache_instances = ( 0 => '127.0.0.1:11211' );
                         # 1 => '132.67.22.1:11211'

use vars qw( $pmda $sock $id $n @socks );
my $memcache_indom = 0;	# one for each memcached
my $query = "stats\r\nstats slabs\r\nstats malloc\r\nstats items\r\n";
# stats
my ( $pid, $uptime, $current_items, $total_items, $bytes,
	$current_connections, $total_connections, $connection_structures,
	$connection_structs, $gets, $sets, $hits, $misses,
	$bytes_read, $bytes_written, $limit_maxbytes );
# stats slabs (6-17)
my ( @chunk_size, @chunks_per_page, @total_pages,
	@total_chunks, @used_chunks, @free_chunks, @free_chunks_end,
	@chunk_size, @chunks_per_page, @total_pages, @total_chunks,
	@used_chunks, @free_chunks, @free_chunks_end );
my ( $active_slabs, $total_malloced );
# stats malloc
my ( $arena_size, $free_chunks, $fastbin_blocks, $mmapped_regions,
	$mmapped_space, $max_total_alloc, $fastbin_space, $total_alloc,
	$total_free, $releasable_space );
# stats items (6-17)
my ( @item_number, @item_age );

sub memcache_stats_callback
{
    ( $_ ) = @_;
    # stats
    if    (/^STAT pid (\d+)$/)			{ $pid = $1; }
    elsif (/^STAT uptime (\d+)$/)		{ $uptime = $1; }
    elsif (/^STAT curr_items (\d+)$/)		{ $current_items = $1; }
    elsif (/^STAT total_items (\d+)$/)		{ $total_items = $1; }
    elsif (/^STAT bytes (\d+)$/)		{ $bytes = $1; }
    elsif (/^STAT curr_connections (\d+)$/)	{ $current_conns = $1; }
    elsif (/^STAT total_connections (\d+)$/)	{ $total_conns = $1; }
    elsif (/^STAT conn_structs (\d+)$/)		{ $conn_structs = $1; }
    elsif (/^STAT cmd_get (\d+)$/)		{ $gets = $1; }
    elsif (/^STAT cmd_set (\d+)$/)		{ $sets = $1; }
    elsif (/^STAT get_hits (\d+)$/)		{ $get_hits = $1; }
    elsif (/^STAT get_misses (\d+)$/)		{ $get_misses = $1; }
    elsif (/^STAT bytes_read (\d+)$/)		{ $bytes_read = $1; }
    elsif (/^STAT bytes_written (\d+)$/)	{ $bytes_written = $1; }
    elsif (/^STAT limit_maxbytes (\d+)$/)	{ $limit_maxbytes = $1; }
    # stats slabs
    elsif (/^STAT (\d+):chunk_size (\d+)$/)	{ $chunk_size[$1] = $2; }
    elsif (/^STAT (\d+):chunks_per_page (\d+)$/) { $chunks_per_page{$1} = $2; }
    elsif (/^STAT (\d+):total_pages (\d+)$/)	{ $total_pages{$1} = $2; }
    elsif (/^STAT (\d+):total_chunks (\d+)$/)	{ $total_chunks{$1} = $2; }
    elsif (/^STAT (\d+):used_chunks (\d+)$/)	{ $used_chunks{$1} = $2; }
    elsif (/^STAT (\d+):free_chunks (\d+)$/)	{ $free_chunks{$1} = $2; }
    elsif (/^STAT (\d+):free_chunks_end (\d+)$/) { $free_chunks_end{$1} = $2; }
    elsif (/^STAT active_slabs (\d+)$/)		{ $active_slabs = $1; }
    elsif (/^STAT total_malloced (\d+)$/)	{ $total_malloced = $1; }
    # stats malloc
    elsif (/^STAT arena_size (\d+)$/)		{ $arena_size = $1; }
    elsif (/^STAT free_chunks (\d+)$/)		{ $free_chunks = $1; }
    elsif (/^STAT fastbin_blocks (\d+)$/)	{ $fastbin_blocks = $1; }
    elsif (/^STAT mmapped_regions (\d+)$/)	{ $mmapped_regions = $1; }
    elsif (/^STAT mmapped_space (\d+)$/)	{ $mmapped_space = $1; }
    elsif (/^STAT max_total_alloc (\d+)$/)	{ $max_total_alloc = $1; }
    elsif (/^STAT fastbin_space (\d+)$/)	{ $fastbin_space = $1; }
    elsif (/^STAT total_alloc (\d+)$/)		{ $total_alloc = $1; }
    elsif (/^STAT total_free (\d+)$/)		{ $total_free = $1; }
    elsif (/^STAT releasable_space (\d+)$/)	{ $releasable_space = $1; }
    # stats items
    elsif (/^STAT items:(\d+):number (\d+)$/)	{ $item_number[$1] = $2; }
    elsif (/^STAT items:(\d+):age (\d+)$/)	{ $item_age{$1} = $2; }
}

sub memcache_connect
{
    for ($n = 0; $n < $memcache_instances; $n++) {
	my ($host, $port) = split($memcache_instances[$n], ":");
	$socks[$n] = $pmda->add_sock($host, $port, \&memcache_input_callback);
    }
}

sub memcache_fetch	# called once per "fetch" PDU, before callbacks
{
    for ($n = 0; $n < $memcache_instances; $n++) {
	$pmda->put_sock($socks[$n], $query);
    }
}

sub memcache_fetch_callback	# must return array of value,status
{
    my ($cluster, $item, $inst) = @_;

    return (PM_ERR_NOTREADY, 0) unless ($connected);
    return (PM_ERR_INST, 0) unless ($inst > 0 && $inst < $ncaches);

    my $cache = $caches[$inst];
    if ($cluster == 0) {
	if ($item == 0)     { return ($cache->pid, 1); }
	elsif ($item == 1)  { return ($cache->$uptime, 1); }
	elsif ($item == 2)  { return ($cache->$current_items, 1); }
	elsif ($item == 3)  { return ($cache->$total_items = $1); }
	elsif ($item == 4)  { return ($cache->$bytes = $1); }
	elsif ($item == 5)  { return ($cache->$current_conns = $1); }
	elsif ($item == 6)  { return ($cache->$total_conns = $1); }
	elsif ($item == 7)  { return ($cache->$conn_structs = $1); }
	elsif ($item == 8)  { return ($cache->$gets = $1); }
	elsif ($item == 9)  { return ($cache->$sets = $1); }
	elsif ($item == 10) { return ($cache->$get_hits = $1); }
	elsif ($item == 11) { return ($cache->$get_misses = $1); }
	elsif ($item == 12) { return ($cache->$bytes_read = $1); }
	elsif ($item == 13) { return ($cache->$bytes_written = $1); }
	elsif ($item == 14) { return ($cache->$limit_maxbytes = $1); }
    }
    elsif ($cluster == 1) {
	my $slab = $item / 7;	# 7 metrics in this cluster
	$item %= 7;
	if ($item == 0)     { return ($cache->$chunk_size[$slab], 1); }
	elsif ($item == 1)  { return ($cache->$chunks_per_page[$slab], 1); }
	elsif ($item == 1)  { return ($cache->$total_pages[$slab], 1); }
	elsif ($item == 1)  { return ($cache->$total_chunks[$slab], 1); }
	elsif ($item == 1)  { return ($cache->$used_chunks[$slab], 1); }
	elsif ($item == 1)  { return ($cache->$free_chunks[$slab], 1); }
	elsif ($item == 1)  { return ($cache->$free_chunks_end[$slab], 1); }
    }
    elsif ($cluster == 2) {
	if ($item == 0)     { return ($cache->active_slabs, 1); }
	elsif ($item == 1)  { return ($cache->total_malloced, 1); }
    }
    elsif ($cluster == 3) {
	my $memitem = $item / 2; # 2 metrics in this cluster
	$item %= 2;
	if ($item == 0)     { return ($cache->$item_count[$memitem], 1); }
	elsif ($item == 1)  { return ($cache->$item_age[$memitem], 1); }
    }
    return (PM_ERR_PMID, 0);
}

$pmda = PCP::PMDA->new('pmdamemcache', 88, 'memcache.log');

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

$id = 0;
foreach $n in (6 .. 17) {	# stats slabs (N=6-17)
    $pmda->add_metric(pmda_pmid(1,$id++), PM_TYPE_U32, $memcache_indom,
		      PM_SEM_INSTANT, pmda_units(1,0,0,PM_SPACE_BYTE,0,0),
		      "memcache.slabs.slab$n.chunk_size", '', '');
    $pmda->add_metric(pmda_pmid(1,$id++), PM_TYPE_U32, $memcache_indom,
		      PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		      "memcache.slabs.slab$n.chunks_per_page", '', '');
    $pmda->add_metric(pmda_pmid(1,$id++), PM_TYPE_U32, $memcache_indom,
		      PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		      "memcache.slabs.slab$n.total_pages", '', '');
    $pmda->add_metric(pmda_pmid(1,$id++), PM_TYPE_U32, $memcache_indom,
		      PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		      "memcache.slabs.slab$n.total_chunks", '', '');
    $pmda->add_metric(pmda_pmid(1,$id++), PM_TYPE_U32, $memcache_indom,
		      PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		      "memcache.slabs.slab$n.used_chunks", '', '');
    $pmda->add_metric(pmda_pmid(1,$id++), PM_TYPE_U32, $memcache_indom,
		      PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		      "memcache.slabs.slab$n.free_chunks", '', '');
    $pmda->add_metric(pmda_pmid(1,$id++), PM_TYPE_U32, $memcache_indom,
		      PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		      "memcache.slabs.slab$n.free_chunks_end", '', '');
}

$pmda->add_metric(pmda_pmid(2,0), PM_TYPE_U32, $memcache_indom,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'memcache.active_slabs', '', '');
$pmda->add_metric(pmda_pmid(2,1), PM_TYPE_U32, $memcache_indom,
		  PM_SEM_INSTANT, pmda_units(1,0,0,PM_SPACE_BYTE,0,0),
		  'memcache.total_malloced', '', '');
$pmda->add_metric(pmda_pmid(3,0), PM_TYPE_32, $memcache_indom,
		  PM_SEM_INSTANT, pmda_units(1,0,0,PM_SPACE_BYTE,0,0),
		  'memcache.malloc.arena_size', '', '');
$pmda->add_metric(pmda_pmid(3,1), PM_TYPE_32, $memcache_indom,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'memcache.malloc.free_chunks', '', '');
$pmda->add_metric(pmda_pmid(3,2), PM_TYPE_32, $memcache_indom,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'memcache.malloc.fastbin_blocks', '', '');
$pmda->add_metric(pmda_pmid(3,3), PM_TYPE_32, $memcache_indom,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'memcache.malloc.mmapped_regions', '', '');
$pmda->add_metric(pmda_pmid(3,4), PM_TYPE_32, $memcache_indom,
		  PM_SEM_INSTANT, pmda_units(1,0,0,PM_SPACE_BYTE,0,0),
		  'memcache.malloc.mmapped_space', '', '');
$pmda->add_metric(pmda_pmid(3,5), PM_TYPE_32, $memcache_indom,
		  PM_SEM_INSTANT, pmda_units(1,0,0,PM_SPACE_BYTE,0,0),
		  'memcache.malloc.max_total_alloc', '', '');
$pmda->add_metric(pmda_pmid(3,6), PM_TYPE_32, $memcache_indom,
		  PM_SEM_INSTANT, pmda_units(1,0,0,PM_SPACE_BYTE,0,0),
		  'memcache.malloc.fastbin_space', '', '');
$pmda->add_metric(pmda_pmid(3,7), PM_TYPE_32, $memcache_indom,
		  PM_SEM_INSTANT, pmda_units(1,0,0,PM_SPACE_BYTE,0,0),
		  'memcache.malloc.total_alloc', '', '');
$pmda->add_metric(pmda_pmid(3,8), PM_TYPE_32, $memcache_indom,
		  PM_SEM_INSTANT, pmda_units(1,0,0,PM_SPACE_BYTE,0,0),
		  'memcache.malloc.total_free', '', '');
$pmda->add_metric(pmda_pmid(3,9), PM_TYPE_32, $memcache_indom,
		  PM_SEM_INSTANT, pmda_units(1,0,0,PM_SPACE_BYTE,0,0),
		  'memcache.malloc.releasable_space', '', '');

$id = 0;
foreach $n in (6 .. 17) {	# stats items (N=6-17)
    $pmda->add_metric(pmda_pmid(4,$id++), PM_TYPE_U32, $memcache_indom,
		      PM_SEM_INSTANT, pmda_units(1,0,0,PM_SPACE_BYTE,0,0),
		      "memcache.items.item$n.count", '', '');
    $pmda->add_metric(pmda_pmid(4,$id++), PM_TYPE_U32, $memcache_indom,
		      PM_SEM_INSTANT, pmda_units(0,1,0,0,PM_TIME_SEC,0),
		      "memcache.items.item$n.age", '', '');
}

$pmda->add_indom( $memcache_indom, \@memcache_instances, '', '' );
$pmda->set_fetch_callback( \&memcache_fetch_callback );

&memcache_connect;
$pmda->run;

__END__

=head1 NAME
pmdamemcache - memcached distributed memory cache daemon PMDA

=head1 SYNOPSIS

=head1 DESCRIPTION

The B<pmdamemcache> PMDA...

=head1 SEE ALSO

L<PMDA> - the Performance Metrics Domain Agent's documentation

=cut
