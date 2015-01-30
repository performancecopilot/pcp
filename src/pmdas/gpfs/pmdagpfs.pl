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

# FS Indom.  Will use cluster:fs as instance
our  $gpfs_fs_indom = 0;

our $MMPMON_CMD = "echo \"fs_io_s\" | /usr/lpp/mmfs/bin/mmpmon -s -p";

# Configuration files for overriding the location of MMPMON_CMD, etc, mostly for testing purposes
for my $file (pmda_config('PCP_PMDAS_DIR') . '/gpfs/gpfs.conf', 'gpfs.conf') {
        eval `cat $file` unless ! -f $file;
}

# Check env variable for MMPMON_CMD to use.  Should be a cmd.  Easiest to use "echo test_file" for QA
if ( defined $ENV{"GPFS_MMPMON_CMD"} ) {
	$MMPMON_CMD = $ENV{"GPFS_MMPMON_CMD"};
}

# The stats hash is keyed on cluster:filesystem
our %h_gpfs = ();

# Parse the client io stats
# Looks like this:
#
# _fs_io_s_ _n_ 10.10.1.1 _nn_ myhost _rc_ 0 _t_ 1421869154 _tu_ 766776 _cl_ gpfs.foobar.com _fs_ gpfs0 _d_ 107 _br_ 3592793024 _bw_ 2552776479 _oc_ 6758 _cc_ 6542 _rdc_ 2923 _wc_ 176495 _dir_ 1367 _iu_ 1418
#
# which is defined like:
#
# mmpmon node 10.10.1.1 name myhost fs_io_s OK
# cluster:        gpfs.foobar.com
# filesystem:     gpfs0
# disks:                 107
# timestamp:      1421869103/978437
# bytes read:     3592793024
# bytes written:  2552776479
# opens:                6758
# closes:               6542
# reads:                2923
# writes:             176495
# readdir:              1367
# inode updates:        1418

sub gpfs_get_io_stats{
	open(MMPMON, "$MMPMON_CMD |") || die "Can't open pipe: $MMPMON_CMD\n";
	while (<MMPMON>){
		my $line = $_;

		# Split on whitesapce and hope they only tack onto the end if they add fields
		my @stats = split(/\s+/,$line);

		my $cluster = $stats[12];
		my $fs = $stats[14];
		my $disks = $stats[16];
		my $bread = $stats[18];
		my $bwrite = $stats[20];
		my $opens = $stats[22];
		my $closes = $stats[24];
		my $reads = $stats[26];
		my $writes = $stats[28];
		my $readdir = $stats[30];
		my $iupdates = $stats[32];

		my $id = "$cluster:$fs";

		$h_gpfs{$id} = {}; # {} is an anonymous hash ref
		$h_gpfs{$id}->{'gpfs.fsios.cluster'} = $cluster;
		$h_gpfs{$id}->{'gpfs.fsios.filesystem'} = $fs;
		$h_gpfs{$id}->{'gpfs.fsios.disks'} = $disks;
		$h_gpfs{$id}->{'gpfs.fsios.read_bytes'} = $bread;
		$h_gpfs{$id}->{'gpfs.fsios.write_bytes'} = $bwrite;
		$h_gpfs{$id}->{'gpfs.fsios.opens'} = $opens;
		$h_gpfs{$id}->{'gpfs.fsios.closes'} = $closes;
		$h_gpfs{$id}->{'gpfs.fsios.reads'} = $reads;
		$h_gpfs{$id}->{'gpfs.fsios.writes'} = $writes;
		$h_gpfs{$id}->{'gpfs.fsios.readdir'} = $readdir;
		$h_gpfs{$id}->{'gpfs.fsios.inode_updates'} = $iupdates;
	}
	close( MMPMON );
}

#
# fetch is called once by pcp for each refresh and then the fetch callback is
# called to query each statistic individually
#
sub gpfs_fetch {
	gpfs_get_io_stats();

	our $pmda->replace_indom($gpfs_fs_indom, \%h_gpfs);
}

sub gpfs_fetch_callback {
	my ($cluster, $item, $inst) = @_; 

	if( $cluster == 0 ){
		# gpfs fs_io_s client stats
		my $lookup = pmda_inst_lookup($gpfs_fs_indom, $inst);
		return (PM_ERR_INST, 0) unless defined($lookup);

		my $pmid_name = pmda_pmid_name($cluster, $item)
			or die "Unknown metric name: cluster $cluster item $item\n";

		return ($lookup->{$pmid_name}, 1);
	}
	else{
		return (PM_ERR_PMID, 0);
	}
}

# the PCP::PMDA->new line is parsed by the check_domain rule of the PMDA build
# process, so there are special requirements:  no comments, the domain has to
# be a bare number.
#
our $pmda = PCP::PMDA->new('gpfs', 135);

# Metrics

#  gpfs fs_io_s client stats - cluster 0

# fs_io_s

$pmda->add_metric(pmda_pmid(0,1), PM_TYPE_STRING, $gpfs_fs_indom,
		PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		'gpfs.fsios.cluster',
		'Cluster name',
		'');

$pmda->add_metric(pmda_pmid(0,2), PM_TYPE_STRING, $gpfs_fs_indom,
		PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		'gpfs.fsios.filesystem',
		'Filesystem name',
		'');

$pmda->add_metric(pmda_pmid(0, 3), PM_TYPE_U64, $gpfs_fs_indom,
		PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		"gpfs.fsios.disks",
		'Number of disks in this filesystem',
		'');

$pmda->add_metric(pmda_pmid(0, 4), PM_TYPE_U64, $gpfs_fs_indom,
		PM_SEM_COUNTER, pmda_units(1,0,0,PM_SPACE_BYTE,0,0),
		"gpfs.fsios.read_bytes",
		'Read bytes counter',
		'');

$pmda->add_metric(pmda_pmid(0, 5), PM_TYPE_U64, $gpfs_fs_indom,
		PM_SEM_COUNTER, pmda_units(1,0,0,PM_SPACE_BYTE,0,0),
		"gpfs.fsios.write_bytes",
		'Write bytes counter',
		'');

$pmda->add_metric(pmda_pmid(0, 6), PM_TYPE_U64, $gpfs_fs_indom,
		PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		"gpfs.fsios.opens",
		'Number of open calls',
		'');

$pmda->add_metric(pmda_pmid(0, 7), PM_TYPE_U64, $gpfs_fs_indom,
		PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		"gpfs.fsios.closes",
		'Number of close calls',
		'');

$pmda->add_metric(pmda_pmid(0, 8), PM_TYPE_U64, $gpfs_fs_indom,
		PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		"gpfs.fsios.reads",
		'Number of read calls',
		'');

$pmda->add_metric(pmda_pmid(0, 9), PM_TYPE_U64, $gpfs_fs_indom,
		PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		"gpfs.fsios.writes",
		'Number of write calls',
		'');

$pmda->add_metric(pmda_pmid(0, 10), PM_TYPE_U64, $gpfs_fs_indom,
		PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		"gpfs.fsios.readdir",
		'Number of readdir calls',
		'');

$pmda->add_metric(pmda_pmid(0, 11), PM_TYPE_U64, $gpfs_fs_indom,
		PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		"gpfs.fsios.inode_updates",
		'Number of inode update calls',
		'');

&gpfs_get_io_stats;

$gpfs_fs_indom = $pmda->add_indom($gpfs_fs_indom, {}, '', '');

$pmda->set_fetch(\&gpfs_fetch);
$pmda->set_fetch_callback(\&gpfs_fetch_callback);

$pmda->run;
