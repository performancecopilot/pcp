#
# Copyright (c) 2015 Martins Innus.  All Rights Reserved.
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

our $noval = PM_ERR_APPVERSION;

use vars qw( $pmda );

our  $lustre_llite_indom = 0;

# llite proc root
our $LLITE_PATH = "/proc/fs/lustre/llite/";

# lnet proc root
our $LNET_PATH = "/proc/sys/lnet/";

# Configuration files for overriding the location of LLITE_PATH, etc, mostly for testing purposes
for my $file (pmda_config('PCP_PMDAS_DIR') . '/lustre/lustre.conf', 'luster.conf') {
        eval `cat $file` unless ! -f $file;
}

# Check env variable for llite path to use.  Should be a dir with all the files
if ( defined $ENV{"LUSTRE_LLITE_PATH"} ) {
	$LLITE_PATH = $ENV{"LUSTRE_LLITE_PATH"}
}

# Check env variable for lnet path to use.  Should be a dir with all the files
if ( defined $ENV{"LUSTRE_LNET_PATH"} ) {
	$LNET_PATH = $ENV{"LUSTRE_LNET_PATH"}
}

# List of metrics we care about

# llite
# Bytes have: min, max, count, total
our @llite_byte_stats = ('read_bytes', 'write_bytes', 'osc_read', 'osc_write');

# Pages have: min, max, count, total
our @llite_page_stats = ('brw_read', 'brw_write');

# Reg have: count
our @llite_reg_stats = ('dirty_pages_hits', 'dirty_pages_misses', 'ioctl', 'open', 'close',
	   'mmap', 'seek', 'fsync', 'readdir', 'setattr',
	   'truncate', 'flock', 'getattr', 'create', 'link', 'unlink',
	   'symlink', 'mkdir', 'rmdir', 'mknod', 'rename',
	   'statfs', 'alloc_inode', 'setxattr', 'getxattr', 'getxattr_hits', 'listxattr',
	   'removexattr', 'inode_permission');



# The stats hash is keyed on base name of the entires in /proc/fs/lustre/llite/ e.g.
# 'lustre' from lustre-ffff880378305c00.  The value is a hash ref(a mount and can have multiple per host), 
# and that hash contains all of the stats data keyed on pmid_name.
our %h_llite = ();

# Only one set of lnet metrics per host
# Still use a ref so its clear theres no indom
our $h_lnet = {};

#
# Find all the lustre file systems on the host
# Devices are of the form "foo-<16_hex_chars>"
#
# And parse the stats file underneath

sub lustre_get_llite_stats{
	opendir(LLITEDIR, $LLITE_PATH) || die "Can't open directory: $LLITE_PATH\n";

	while(my $ldev = readdir(LLITEDIR) ){
		if( $ldev =~ /^(\w+)-([a-f0-9]{16})$/ ){

			# Get the device information

			my $mtroot = $1;
			my $hexsuper = $2;
			$h_llite{$mtroot} = {}; # {} is an anonymous hash ref
			$h_llite{$mtroot}->{'lustre.llite.volume'} = $mtroot;
			$h_llite{$mtroot}->{'lustre.llite.superblock'} = $hexsuper;

			# Parse the actual stats file
			# From: lustre-2.5.3/lustre/llite/lproc_llite.c
			# Stats do not exists unless the counter has been incremented. Need to initialize to 0
			
			for my $stat_name (@llite_byte_stats) {
				$h_llite{$mtroot}->{"lustre.llite.$stat_name.count"} = 0;
				$h_llite{$mtroot}->{"lustre.llite.$stat_name.min"} = 0;
				$h_llite{$mtroot}->{"lustre.llite.$stat_name.max"} = 0;
				$h_llite{$mtroot}->{"lustre.llite.$stat_name.total"} = 0;
			}
			for my $stat_name (@llite_page_stats) {
				$h_llite{$mtroot}->{"lustre.llite.$stat_name.count"} = 0;
				$h_llite{$mtroot}->{"lustre.llite.$stat_name.min"} = 0;
				$h_llite{$mtroot}->{"lustre.llite.$stat_name.max"} = 0;
				$h_llite{$mtroot}->{"lustre.llite.$stat_name.total"} = 0;
			}
			for my $stat_name (@llite_reg_stats) {
				$h_llite{$mtroot}->{"lustre.llite.$stat_name.count"} = 0;
			}
			
			my $statspath = $LLITE_PATH . $ldev . '/stats';

			if( ! open STATS, '<', $statspath ) {
                		$pmda->err("pmdalustre failed to open $statspath: $!");
                		die "Can't open $statspath: $!\n";
			}

			while (<STATS>) {
                		my $line = $_;
				# Byte types first
				my $byte_types = join '|', @llite_byte_stats;
				my $pages_types = join '|', @llite_page_stats;
				my $reg_types = join '|', @llite_reg_stats;
				if( $line =~ /^($byte_types)\s+(\d+) samples \[bytes\] (\d+) (\d+) (\d+)$/){
					my $name = $1;
					my $count = $2;
					my $min = $3;
					my $max = $4;
					my $total = $5;
					$h_llite{$mtroot}->{"lustre.llite.$name.count"} = $count;
					$h_llite{$mtroot}->{"lustre.llite.$name.min"} = $min;
					$h_llite{$mtroot}->{"lustre.llite.$name.max"} = $max;
					$h_llite{$mtroot}->{"lustre.llite.$name.total"} = $total;
				# Pages types
				# I have not seen these, so can't test
				} elsif ( $line =~ /^($pages_types)\s+(\d+) samples \[pages\] (\d+) (\d+) (\d+)$/){
					my $name = $1;
                                        my $count = $2;
                                        my $min = $3;
                                        my $max = $4;
                                        my $total = $5;
                                        $h_llite{$mtroot}->{"lustre.llite.$name.count"} = $count;
                                        $h_llite{$mtroot}->{"lustre.llite.$name.min"} = $min;
                                        $h_llite{$mtroot}->{"lustre.llite.$name.max"} = $max;
                                        $h_llite{$mtroot}->{"lustre.llite.$name.total"} = $total;
				# Regs types
				} elsif ( $line =~ /^($reg_types)\s+(\d+) samples \[regs\]$/ ){
					my $name = $1;
                                        my $count = $2;
					$h_llite{$mtroot}->{"lustre.llite.$name.count"} = $count;
				}
			}
			close( STATS );
		}
	}
	closedir( LLITEDIR );
}

# lnet stats. One set per host

sub lustre_get_lnet_stats{

	my $statspath = $LNET_PATH . 'stats';

	if( ! open STATS, '<', $statspath ){
		$pmda->err ("pmdalustre failed to open $statspath: $!");
		die "Can't open $statspath: $!\n";
	}

	while (<STATS>) {
		my $line = $_;
		# From: lustre-2.5.3/lnet/lnet/router_proc.c
		# which claims to be lnet_proc.c
		if( $line =~ /^(\d+)\s(\d+)\s(\d+)\s(\d+)\s(\d+)\s(\d+)\s(\d+)\s(\d+)\s(\d+)\s(\d+)\s(\d+)$/){
			my $msgs_alloc = $1;
			my $msgs_max = $2;
			my $errors = $3;
			my $send_count = $4;
			my $recv_count = $5;
			my $route_count = $6;
			my $drop_count = $7;
			my $send_length = $8;
			my $recv_length = $9;
			my $route_length = $10;
			my $drop_length = $11;
			$h_lnet->{'lustre.lnet.msgs_alloc'} = $msgs_alloc;
			$h_lnet->{'lustre.lnet.msgs_max'} = $msgs_max;
			$h_lnet->{'lustre.lnet.errors'} = $errors;
			$h_lnet->{'lustre.lnet.send_count'} = $send_count;
			$h_lnet->{'lustre.lnet.recv_count'} = $recv_count;
			$h_lnet->{'lustre.lnet.route_count'} = $route_count;
			$h_lnet->{'lustre.lnet.drop_count'} = $drop_count;
			$h_lnet->{'lustre.lnet.send_length'} = $send_length;
			$h_lnet->{'lustre.lnet.recv_length'} = $recv_length;
			$h_lnet->{'lustre.lnet.route_length'} = $route_length;
			$h_lnet->{'lustre.lnet.drop_length'} = $drop_length;
		}
	}
	close( STATS );
}

#
# fetch is called once by pcp for each refresh and then the fetch callback is
# called to query each statistic individually
#
sub lustre_fetch {
	lustre_get_llite_stats();
	lustre_get_lnet_stats();

	our $pmda->replace_indom($lustre_llite_indom, \%h_llite);
}

sub lustre_fetch_callback {
	my ($cluster, $item, $inst) = @_; 

	if( $cluster == 0 ){
		# llite
		my $lookup = pmda_inst_lookup($lustre_llite_indom, $inst);
		return (PM_ERR_INST, 0) unless defined($lookup);

		my $pmid_name = pmda_pmid_name($cluster, $item)
			or die "Unknown metric name: cluster $cluster item $item\n";

		return ($lookup->{$pmid_name}, 1);
	}
	elsif( $cluster == 1 ){
		# lnet
		my $pmid_name = pmda_pmid_name($cluster, $item)
			or die "Unknown metric name: cluster $cluster item $item\n";

		return ($h_lnet->{$pmid_name}, 1);
	}
	else{
		return (PM_ERR_PMID, 0);
	}
}

# the PCP::PMDA->new line is parsed by the check_domain rule of the PMDA build
# process, so there are special requirements:  no comments, the domain has to
# be a bare number.
#
our $pmda = PCP::PMDA->new('lustre', 134);

# Metrics

# llite stats - cluster 0
# lnet stats - cluster 1

# llite

$pmda->add_metric(pmda_pmid(0,1), PM_TYPE_STRING, $lustre_llite_indom,
		PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		'lustre.llite.volume',
		'Volume Name',
		'');
$pmda->add_metric(pmda_pmid(0,2), PM_TYPE_STRING, $lustre_llite_indom,
		PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		'lustre.llite.superblock',
		'Superblock Identifier',
		'');

my $item = 3;
for my $stat_name (@llite_byte_stats) {

	$pmda->add_metric(pmda_pmid(0, $item++), PM_TYPE_U64, $lustre_llite_indom,
			  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
			  "lustre.llite.$stat_name.count",
			  'number of calls',
			  '');

	$pmda->add_metric(pmda_pmid(0, $item++), PM_TYPE_U64, $lustre_llite_indom,
			  PM_SEM_INSTANT, pmda_units(1,0,0,PM_SPACE_BYTE,0,0),
			  "lustre.llite.$stat_name.min",
			  'minimum byte value seen in a call',
			  '');

	$pmda->add_metric(pmda_pmid(0, $item++), PM_TYPE_U64, $lustre_llite_indom,
			  PM_SEM_INSTANT, pmda_units(1,0,0,PM_SPACE_BYTE,0,0),
			  "lustre.llite.$stat_name.max",
			  'maximum byte value seen in a call',
			  '');

	$pmda->add_metric(pmda_pmid(0, $item++), PM_TYPE_U64, $lustre_llite_indom,
			  PM_SEM_COUNTER, pmda_units(1,0,0,PM_SPACE_BYTE,0,0),
			  "lustre.llite.$stat_name.total",
			  'total byte count',
			  '');
}

for my $stat_name (@llite_page_stats) {

        $pmda->add_metric(pmda_pmid(0, $item++), PM_TYPE_U64, $lustre_llite_indom,
                          PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
                          "lustre.llite.$stat_name.count",
                          'number of calls',
                          '');

        $pmda->add_metric(pmda_pmid(0, $item++), PM_TYPE_U64, $lustre_llite_indom,
                          PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
                          "lustre.llite.$stat_name.min",
                          'minimum page value seen in a call',
                          '');

        $pmda->add_metric(pmda_pmid(0, $item++), PM_TYPE_U64, $lustre_llite_indom,
                          PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
                          "lustre.llite.$stat_name.max",
                          'maximum page value seen in a call',
                          '');

        $pmda->add_metric(pmda_pmid(0, $item++), PM_TYPE_U64, $lustre_llite_indom,
                          PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
                          "lustre.llite.$stat_name.total",
                          'total page count',
                          '');
}

for my $stat_name (@llite_reg_stats) {

        $pmda->add_metric(pmda_pmid(0, $item++), PM_TYPE_U64, $lustre_llite_indom,
                          PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
                          "lustre.llite.$stat_name.count",
                          'number of calls',
                          '');

}

# lnet
$item = 1;

# Can't loop through, units are all different
$pmda->add_metric(pmda_pmid(1, $item++), PM_TYPE_U32, PM_INDOM_NULL,
		PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		"lustre.lnet.msgs_alloc",
		'messages currently allocated',
		'');

$pmda->add_metric(pmda_pmid(1, $item++), PM_TYPE_U32, PM_INDOM_NULL,
                PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
                "lustre.lnet.msgs_max",
                'messages maximum',
                '');

$pmda->add_metric(pmda_pmid(1, $item++), PM_TYPE_U32, PM_INDOM_NULL,
		PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
                "lustre.lnet.errors",
                'errors',
                '');

$pmda->add_metric(pmda_pmid(1, $item++), PM_TYPE_U32, PM_INDOM_NULL,
		PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
                "lustre.lnet.send_count",
                'transmit count',
                '');

$pmda->add_metric(pmda_pmid(1, $item++), PM_TYPE_U32, PM_INDOM_NULL,
		PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
                "lustre.lnet.recv_count",
                'receive count',
                '');

$pmda->add_metric(pmda_pmid(1, $item++), PM_TYPE_U32, PM_INDOM_NULL,
		PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
                "lustre.lnet.route_count",
                'route count',
                '');

$pmda->add_metric(pmda_pmid(1, $item++), PM_TYPE_U32, PM_INDOM_NULL,
		PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
                "lustre.lnet.drop_count",
                'drop count',
                '');

$pmda->add_metric(pmda_pmid(1, $item++), PM_TYPE_U64, PM_INDOM_NULL,
		PM_SEM_COUNTER, pmda_units(1,0,0,PM_SPACE_BYTE,0,0),
                "lustre.lnet.send_length",
                'transmit bytes',
                '');

$pmda->add_metric(pmda_pmid(1, $item++), PM_TYPE_U64, PM_INDOM_NULL,
		PM_SEM_COUNTER, pmda_units(1,0,0,PM_SPACE_BYTE,0,0),
                "lustre.lnet.recv_length",
                'receive bytes',
                '');

$pmda->add_metric(pmda_pmid(1, $item++), PM_TYPE_U64, PM_INDOM_NULL,
		PM_SEM_COUNTER, pmda_units(1,0,0,PM_SPACE_BYTE,0,0),
                "lustre.lnet.route_length",
                'route bytes',
                '');

$pmda->add_metric(pmda_pmid(1, $item++), PM_TYPE_U64, PM_INDOM_NULL,
		PM_SEM_COUNTER, pmda_units(1,0,0,PM_SPACE_BYTE,0,0),
                "lustre.lnet.drop_length",
                'drop bytes',
                '');


&lustre_get_llite_stats;
&lustre_get_lnet_stats;

$lustre_llite_indom = $pmda->add_indom($lustre_llite_indom, {}, '', '');

$pmda->set_fetch(\&lustre_fetch);
$pmda->set_fetch_callback(\&lustre_fetch_callback);

$pmda->run;
