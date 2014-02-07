#
# Copyright (c) 2011 SGI.
# All rights reserved.
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

# The instance domain is 0 for all nfs client stats
our  $nfsclient_indom = 0;

# The instances hash is converted to an array and passed to add_indom (and
# replace_indom) in the second arg.  It is keyed on the instance number which
# starts at 1 and is incremented for each nfs mount in the mountstats file.
# The value is the server export string, e.g. 'hostname:/export', which happens
# to be the key of the stats hash.
our %instances = ();

# The stats hash is keyed on the 'device' string (the export string), e.g.
# 'hostname:/export'.  The value is a hash ref, and that hash contains all of
# the stats data keyed on pmid_name
our %h = ();

#
# Parse /proc/self/mountstats and store the stats, taking one pass through
# the file.
#
sub nfsclient_parse_proc_mountstats {
	# mountstats output has a section for each mounted filesystems on the
	# system.  Each section starts with a line like:  'device X mounted on
	# Y with fstype Z'.  Some filesystems (like nfs) print additional
	# output that we need to capture.
	open STATS, "< /proc/self/mountstats"
		or die "Can't open /proc/self/mountstats: $!\n";

	my $i = 0;	# instance number
	my $export;
	while (<STATS>) {
		my $line = $_;

		# does this line represent a mount?
		if ($line =~ /^device (\S*) mounted on (\S*) with fstype.*/) {
			$export = $1;
			my $mtpt = $2;

			# is it an nfs mount?
			if ($line =~ /.*nfs statvers=1\.0$/) {
				$instances{$i++} = $export; # a new instance
				$h{$export} = {}; # {} is an anonymous hash ref
				$h{$export}->{'nfsclient.export'} = $export;
				$h{$export}->{'nfsclient.mtpt'} = $mtpt; 
			} else {
				# the following lines aren't nfs so undef
				# $export so that they are ignored below
				undef $export;
				next;
			}
		}

		# skip lines that do not belong to nfs mounts
		if (not defined $export) {
			next;
		}

		# TODO parse nfs stats and put them in $h
	}

	close STATS;
}

#
# fetch is called once by pcp for each refresh and then the fetch callback is
# called to query each statistic individually
#
sub nfsclient_fetch {
	nfsclient_parse_proc_mountstats();

	our $pmda->replace_indom($nfsclient_indom, [%instances]);
}

#
# the fetch callback returns is passed the cluster and item number (which
# identifies each statistic and is passed to pcp in arg 1 of add_metric) as
# well as the instance id.  Based on this, look up the export name and
# pmid_name, lookup the stat data in the hashes and return it in arg0 of an
# array.
#
sub nfsclient_fetch_callback {
	my ($cluster, $item, $inst) = @_; 

	my $export = $instances{$inst};
	my $pmid_name = pmda_pmid_name($cluster, $item)
		or die "Unknown metric name: cluster $cluster item $item\n";

	return ($h{$export}->{$pmid_name}, 1); # 1 indicates the stat is valid
}

# the PCP::PMDA->new line is parsed by the check_domain rule of the PMDA build
# process, so there are special requirements:  no comments, the domain has to
# be a bare number.
#
# domain 111 was chosen since it doesn't exist in ../pmns/stdpmid
our $pmda = PCP::PMDA->new('nfsclient', 111);

# metrics go here, with full descriptions
$pmda->add_metric(pmda_pmid(0,1), PM_TYPE_STRING, $nfsclient_indom,
		PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		'nfsclient.export',
		'Export',
		'');
$pmda->add_metric(pmda_pmid(0,2), PM_TYPE_STRING, $nfsclient_indom,
		PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		'nfsclient.mtpt',
		'Mount Point',
		'');

&nfsclient_parse_proc_mountstats;
$pmda->add_indom($nfsclient_indom, [%instances], 'NFS mounts', '');

$pmda->set_fetch(\&nfsclient_fetch);
$pmda->set_fetch_callback(\&nfsclient_fetch_callback);

$pmda->run;
