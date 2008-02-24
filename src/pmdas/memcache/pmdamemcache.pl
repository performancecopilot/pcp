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
			 # 2 => '132.67.22.1:11211',
			 );

my $memcache_indom = 0;	# one for each memcached
my $query = "stats\r\nstats slabs\r\nstats items\r\n"; # sent to memcached

sub memcache_stats_callback
{
    my ( %cache, %item, %slab );
    ( $id, $_ ) = @_;

    %cache = $caches{$id};

    # stats
    if (/^STAT pid (\d+)$/)			{ $cache{'pid'} = $1; }
    elsif (/^STAT uptime (\d+)$/)		{ $cache{'uptime'} = $1; }
    elsif (/^STAT curr_items (\d+)$/)		{ $cache{'curr_items'} = $1; }
    elsif (/^STAT total_items (\d+)$/)		{ $cache{'total_items'} = $1; }
    elsif (/^STAT bytes (\d+)$/)		{ $cache{'bytes'} = $1; }
    elsif (/^STAT curr_connections (\d+)$/)	{ $cache{'curr_conns'} = $1; }
    elsif (/^STAT total_connections (\d+)$/)	{ $cache{'total_conns'} = $1; }
    elsif (/^STAT conn_structs (\d+)$/)		{ $cache{'conn_structs'} = $1; }
    elsif (/^STAT cmd_get (\d+)$/)		{ $cache{'gets'} = $1; }
    elsif (/^STAT cmd_set (\d+)$/)		{ $cache{'sets'} = $1; }
    elsif (/^STAT get_hits (\d+)$/)		{ $cache{'get_hits'} = $1; }
    elsif (/^STAT get_misses (\d+)$/)		{ $cache{'get_misses'} = $1; }
    elsif (/^STAT bytes_read (\d+)$/)		{ $cache{'bytes_read'} = $1; }
    elsif (/^STAT bytes_written (\d+)$/)   { $cache{'bytes_written'} = $1; }
    elsif (/^STAT limit_maxbytes (\d+)$/)  { $cache{'limit_maxbytes'} = $1; }

    # stats slabs
    elsif (/^STAT (\d+):chunk_size (\d+)$/) {
	%slab = $cache{"slab$1"};
	$slab{'chunk_size'} = $2;
    }
    elsif (/^STAT (\d+):chunks_per_page (\d+)$/) {
	%slab = $cache{"slab$1"};
	$slab{'chunks_per_page'} = $2;
    }
    elsif (/^STAT (\d+):total_pages (\d+)$/) {
	%slab = $cache{"slab$1"};
	$slab{'total_pages'} = $2;
    }
    elsif (/^STAT (\d+):total_chunks (\d+)$/) {
	%slab = $cache{"slab$1"};
	$slab{'total_chunks'} = $2;
    }
    elsif (/^STAT (\d+):used_chunks (\d+)$/) {
	%slab = $cache{"slab$1"};
	$slab{'used_chunks'} = $2;
    }
    elsif (/^STAT (\d+):free_chunks (\d+)$/) {
	%slab = $cache{"slab$1"};
	$slab{'free_chunks'} = $2;
    }
    elsif (/^STAT (\d+):free_chunks_end (\d+)$/) {
	%slab = $cache{"slab$1"};
	$slab{'free_chunks_end'} = $2;
    }
    elsif (/^STAT active_slabs (\d+)$/)    { $cache{'active_slabs'} = $1; }
    elsif (/^STAT total_malloced (\d+)$/)  { $cache{'total_malloced'} = $1; }

    # stats items
    elsif (/^STAT items:(\d+):number (\d+)$/) {
	%item = $cache{"item$1"};
	$item{'number'} = $2;
    }
    elsif (/^STAT items:(\d+):age (\d+)$/) {
	%item = $cache{"item$1"};
	$item{'age'} = $2;
    }
    elsif (/^END$/) { }
    else {
	$pmda->log("Eh?: $_");
    }
}

sub memcache_connect
{
    for ($id = 0; $id < $#memcache_instances; $id++) {
	my ($host, $port) = split(/:/, $memcache_instances[$id]);
	$pmda->add_sock($host, $port, \&memcache_stats_callback, $id);
    }
}

sub memcache_fetch	# called once per "fetch" PDU, before callbacks
{
    for ($id = 0; $id < $#memcache_instances; $id++) {
	$pmda->put_sock($id, $query);
    }
}

sub memcache_fetch_callback	# must return array of value,status
{
    my ($cluster, $item, $inst) = @_;
    my ( %cache, %slabs, %items );

    return (PM_ERR_INST, 0) unless ($inst > 0 && $inst < $#memcache_instances);

    %cache = $caches{$inst};
    if ($cluster == 0) {
	if ($item == 0)     { return ($cache{'pid'}, 1); }
	elsif ($item == 1)  { return ($cache{'uptime'}, 1); }
	elsif ($item == 2)  { return ($cache{'curr_items'}, 1); }
	elsif ($item == 3)  { return ($cache{'total_items'}, 1); }
	elsif ($item == 4)  { return ($cache{'bytes'}, 1); }
	elsif ($item == 5)  { return ($cache{'curr_conns'}, 1); }
	elsif ($item == 6)  { return ($cache{'total_conns'}, 1); }
	elsif ($item == 7)  { return ($cache{'conn_structs'}, 1); }
	elsif ($item == 8)  { return ($cache{'gets'}, 1); }
	elsif ($item == 9)  { return ($cache{'sets'}, 1); }
	elsif ($item == 10) { return ($cache{'get_hits'}, 1); }
	elsif ($item == 11) { return ($cache{'get_misses'}, 1); }
	elsif ($item == 12) { return ($cache{'bytes_read'}, 1); }
	elsif ($item == 13) { return ($cache{'bytes_written'}, 1); }
	elsif ($item == 14) { return ($cache{'limit_maxbytes'}, 1); }
    }
    elsif ($cluster == 1) {
	# 11 different slabs (6..17), and 7 metrics in this cluster
	if ($item > 7 * 11) { return (PM_ERR_PMID, 0); }
	$id = $item / 7;
	$item %= 7;
	%slabs = $cache{"slab$id"};

	if ($item == 0)     { return ($slabs{'chunk_size'}, 1); }
	elsif ($item == 1)  { return ($slabs{'chunks_per_page'}, 1); }
	elsif ($item == 2)  { return ($slabs{'total_pages'}, 1); }
	elsif ($item == 3)  { return ($slabs{'total_chunks'}, 1); }
	elsif ($item == 4)  { return ($slabs{'used_chunks'}, 1); }
	elsif ($item == 5)  { return ($slabs{'free_chunks'}, 1); }
	elsif ($item == 6)  { return ($slabs{'free_chunks_end'}, 1); }
    }
    elsif ($cluster == 2) {
	if ($item == 0)     { return ($cache{'active_slabs'}, 1); }
	elsif ($item == 1)  { return ($cache{'total_malloced'}, 1); }
    }
    elsif ($cluster == 3) {
	# 11 different slabs (6..17), and 2 metrics in this cluster
	if ($item > 2 * 11) { return (PM_ERR_PMID, 0); }
	$id = $item / 2;
	$item %= 2;
	%items = $cache{"item$id"};

	if ($item == 0)     { return ($items{'count'}, 1); }
	elsif ($item == 1)  { return ($items{'age'}, 1); }
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
foreach $n (6 .. 17) {	# stats slabs (N=6-17)
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

$id = 0;
foreach $n (6 .. 17) {	# stats items (N=6-17)
    $pmda->add_metric(pmda_pmid(3,$id++), PM_TYPE_U32, $memcache_indom,
		      PM_SEM_INSTANT, pmda_units(1,0,0,PM_SPACE_BYTE,0,0),
		      "memcache.items.item$n.count", '', '');
    $pmda->add_metric(pmda_pmid(3,$id++), PM_TYPE_U32, $memcache_indom,
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
