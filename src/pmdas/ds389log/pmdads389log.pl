#
# Copyright (C) 2014-2015 Marko Myllynen <myllynen@redhat.com>
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
use Date::Manip;
use POSIX;

# 389 DS is not locale aware
setlocale(LC_ALL, "C");

our $lc_opts = '-D /dev/shm -s all';
our $lc_ival = 30; # minimal query interval in seconds, must be >= 30
our $ds_alog = ''; # empty - guess; ok if only one DS instance in use
our $ds_logd = '/var/log/dirsrv';
our $ds_user = 'nobody'; # empty - use root

# Configuration files for overriding the above settings
for my $file (pmda_config('PCP_PMDAS_DIR') . '/ds389log/ds389log.conf', './ds389log.conf') {
	eval `cat $file` unless ! -f $file;
}

my %data = (
	# logconv.pl string - name - subtree - cluster - id - type
	# type : 0 - cumulative, 1 - peak
	'Total Connections:'		=> [ 'totalconns', 'conns', 0, 0, 0 ],
	'Peak Concurrent Connections:'	=> [ 'peakconns', 'conns', 0, 1, 1 ],
	'U1'				=> [ 'cleanclose', 'conns', 0, 2, 0 ],
	'B1'				=> [ 'badclose', 'conns', 0, 3, 0 ],
	'Total Operations:'		=> [ 'totalops', 'ops', 1, 0, 0 ],
	'Total Results:'		=> [ 'totalres', 'ops', 1, 1, 0 ],
	'Searches:'			=> [ 'searches', 'ops', 1, 2, 0 ],
	'Modifications:'		=> [ 'mods', 'ops', 1, 3, 0 ],
	'Adds:'				=> [ 'adds', 'ops', 1, 4, 0 ],
	'Deletes:'			=> [ 'dels', 'ops', 1, 5, 0 ],
	'Mod RDNs:'			=> [ 'modrdns', 'ops', 1, 6, 0 ],
	'Compares:'			=> [ 'comps', 'ops', 1, 7, 0 ],
	'Binds:'			=> [ 'binds', 'ops', 1, 8, 0 ],
	'Paged Searches:'		=> [ 'pagedsearches', 'searches', 2, 0, 0 ],
	'Unindexed Searches:'		=> [ 'unindexedsearches', 'searches', 2, 1, 0 ],
	'err=0'				=> [ 'noerror', 'errors', 3, 0, 0 ],
	'err=X'				=> [ 'error', 'errors', 3, 1, 0 ], # custom
	'Highest FD Taken:'		=> [ 'fdhigh', 'fd', 4, 0, 1 ],
);

use vars qw( $pmda %metrics );

# Timestamps
my @lc_prev = localtime();
my @lc_curr;

sub ds389log_set_ds_access_log {
	$ds_alog = `ls -1 $ds_logd/slapd-*/access 2>/dev/null | tail -n 1`;
	my $un = `id -un`;
	chomp($ds_alog); chomp($un);
	die "$un can't read access log file \"$ds_logd/slapd-*/access\"" unless -f $ds_alog;
	$pmda->log("Using access log file $ds_alog");
}

sub ds389log_fetch {
	ds389log_set_ds_access_log() if $ds_alog eq '';
	return if $ds_alog eq '';

	# Server might not have written entries for operations during
	# the past few seconds yet so we will collect them next round.
	@lc_curr = localtime();
	$lc_curr[0] -= 30; # secs

	if ((strftime("%s", @lc_curr) - strftime("%s", @lc_prev)) < $lc_ival) {
		return;
	}

	# Don't include anything twice
	$lc_prev[0] += 1; # secs

	# Include the previous rotated log only if needed
	my $prev_log = `ls -1rt $ds_alog.2* 2>/dev/null | tail -n 1`;
	if ($prev_log ne '') {
		my $lastline = `tail -n 1 $prev_log`;
		$lastline =~ s/\[//;
		$lastline =~ s/\].*//;
		$lastline =~ s/:/ /;
		my $log_ts = UnixDate($lastline, "%s");
		if (strftime("%s", @lc_prev) > $log_ts) {
			$prev_log = '';
		}
	}

	my $lc_start = strftime("[%d/%b/%Y:%H:%M:%S %z]", @lc_prev);
	my $lc_end   = strftime("[%d/%b/%Y:%H:%M:%S %z]", @lc_curr);
	@lc_prev = @lc_curr;

	my $ds_stats = "logconv.pl -cpe $lc_opts -S $lc_start -E $lc_end $ds_alog $prev_log 2>/dev/null";
	open(STATS, "$ds_stats |") or
		die $pmda->err("pmda389log failed to open $ds_stats pipe: $!");
	my @stats = <STATS>;
	close(STATS);

	my $errors = 0; # combined
	my $nobind = 0; # no dupes
	foreach my $line (@stats) {
		my $key;

		if ($line =~ /^.*:/ || $line =~ /^U1/ || $line =~ /^B1/) {
			$key = $&;
		}

		if ($key eq "Binds:") {
			next if $nobind;
			$nobind = 1;
		}
		if ($line =~ /^err=.?/) {
			$key = 'err=X';
		}
		if ($line =~ /^err=0/) {
			$key = 'err=0';
		}

		if (defined($key) && defined $data{$key}) {
			$key = 'err=' if $key eq 'err=X';
			if ($line =~ /($key)\s+(\d+)/ || $line =~ /($key\d+)\s+(\d+)/) {
				my $value = $2;

				if ($key eq 'err=') {
					$key = 'err=X';
					$errors += $value;
					$value = $errors;
				}

				my $id = 'ds389log.' . $data{$key}[1] . '.' . $data{$key}[0];

				if ($data{$key}[4] eq 1) {
					my $prev = $metrics{$id};
					$value = $prev if $prev > $value;
				} else {
					$value = $metrics{$id} + $value;
				}

				$metrics{$id} = $value;
			}
		}
	}
}

sub ds389log_fetch_callback {
	my ($cluster, $item, $inst) = @_;

	if ($inst != PM_INDOM_NULL)	{ return (PM_ERR_INST, 0); }

	my $pmnm = pmda_pmid_name($cluster, $item);
	my $value = $metrics{$pmnm};

	if (!defined($value))		{ return (PM_ERR_APPVERSION, 0); }

	return ($value, 1);
}

$pmda = PCP::PMDA->new('ds389log', 131);

# Add and zero metrics
foreach my $key (keys %data) {
	my $name = 'ds389log.' . $data{$key}->[1] . '.' . $data{$key}->[0];
	$pmda->add_metric(pmda_pmid($data{$key}->[2], $data{$key}->[3]),
			PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
			pmda_units(0,0,1,0,0,PM_COUNT_ONE),
			$name, '', '');
	$metrics{$name} = 0;
}

$pmda->set_refresh(\&ds389log_fetch);
$pmda->set_fetch_callback(\&ds389log_fetch_callback);
# NB: needs to run as root or as a user having read access to the logs
$pmda->set_user($ds_user) if $ds_user ne '';
$pmda->run;
