#
# Copyright (c) 2011 SGI.  All Rights Reserved.
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

use vars qw( $pmda @all_ops @common_ops @v3only_ops @v4_ops @v41_ops);

# The instance domain is 0 for all nfs client stats
our  $nfsclient_indom = 0;

# mountstats or other file to use
our $MOUNTSTATS_PATH = "/proc/self/mountstats";

# Configuration files for overriding the location of mountstats, mostly for testing purposes
for my $file (pmda_config('PCP_PMDAS_DIR') . '/nfsclient/nfsclient.conf', 'nfsclient.conf') {
        eval `cat $file` unless ! -f $file;
}

# Check env variable for mountstats file to use
if ( defined $ENV{"NFSCLIENT_MOUNTSTATS_PATH"} ) {
	$MOUNTSTATS_PATH = $ENV{"NFSCLIENT_MOUNTSTATS_PATH"}
}

our $noval = PM_ERR_APPVERSION;

# The stats hash is keyed on the 'mount' string (the mountpoint string), e.g.
# '/home'.  The value is a hash ref, and that hash contains all of
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
	open STATS, '<', $MOUNTSTATS_PATH ||
		( $pmda->err("pmdanfsclient failed to open $MOUNTSTATS_PATH: $!") &&
		die "Can't open $MOUNTSTATS_PATH: $!\n") ;

	my $export;
	my $nfsinst;
	while (<STATS>) {
		my $line = $_;

		# does this line represent a mount?
		if ($line =~ /^device (\S*) mounted on (\S*) with fstype.*/) {
			$export = $1;
			my $mtpt = $2;
			$nfsinst = $mtpt;

			# is it an nfs mount?
			if ($line =~ /.*nfs(4)? statvers=.*/) {
				$h{$nfsinst} = {}; # {} is an anonymous hash ref
				$h{$nfsinst}->{'nfsclient.export'} = $export;
				$h{$nfsinst}->{'nfsclient.mountpoint'} = $mtpt; 
			} else {
				# the following lines aren't nfs so undef
				# $nfsinst so that they are ignored below
				undef $nfsinst;
				next;
			}
		}

		# skip lines that do not belong to nfs mounts
		if (not defined $nfsinst) {
			next;
		}

		# opts
		if ($line =~ /^\topts:\t(.*)$/) {

			# Need to differentiate between options that are not available based on nfs vers, vs options that are not set and therefore default values ?
			# Prepopulate based on this constraint
			# Try to stay away from the "no" names
			# Guess reasonable defaults based on mapping below
			#
			$h{$nfsinst}->{'nfsclient.options.readmode'} = "rw";
			$h{$nfsinst}->{'nfsclient.options.sync'} = 0;
			$h{$nfsinst}->{'nfsclient.options.atime'} = 1;
			$h{$nfsinst}->{'nfsclient.options.diratime'} = 1;
			$h{$nfsinst}->{'nfsclient.options.vers'} = "3";
			$h{$nfsinst}->{'nfsclient.options.rsize'} = 0;
			$h{$nfsinst}->{'nfsclient.options.wsize'} = 0;
			$h{$nfsinst}->{'nfsclient.options.bsize'} = 0;
			$h{$nfsinst}->{'nfsclient.options.namlen'} = 0;
			$h{$nfsinst}->{'nfsclient.options.acregmin'} = 0;
			$h{$nfsinst}->{'nfsclient.options.acregmax'} = 0;
			$h{$nfsinst}->{'nfsclient.options.acdirmin'} = 0;
			$h{$nfsinst}->{'nfsclient.options.acdirmax'} = 0;
			$h{$nfsinst}->{'nfsclient.options.recovery'} = "hard";
			$h{$nfsinst}->{'nfsclient.options.posix'} = 0;
			$h{$nfsinst}->{'nfsclient.options.cto'} = 1;
			$h{$nfsinst}->{'nfsclient.options.ac'} = 1;
			$h{$nfsinst}->{'nfsclient.options.lock'} = 1;
			$h{$nfsinst}->{'nfsclient.options.acl'} = 1;
			$h{$nfsinst}->{'nfsclient.options.rdirplus'} = 1;
			$h{$nfsinst}->{'nfsclient.options.sharecache'} = 1;
			$h{$nfsinst}->{'nfsclient.options.resvport'} = 1;
			$h{$nfsinst}->{'nfsclient.options.proto'} = "tcp";
			$h{$nfsinst}->{'nfsclient.options.port'} = 0;
			$h{$nfsinst}->{'nfsclient.options.timeo'} = 600;
			$h{$nfsinst}->{'nfsclient.options.retrans'} = 3;
			$h{$nfsinst}->{'nfsclient.options.sec'} = "sys";

			# NFS 3 only options but can be NULL also
			$h{$nfsinst}->{'nfsclient.options.mountaddr'} = "unspecified";
			$h{$nfsinst}->{'nfsclient.options.mountvers'} = 0;
			$h{$nfsinst}->{'nfsclient.options.mountport'} = 0;
			$h{$nfsinst}->{'nfsclient.options.mountproto'} = "";

			# NFS 4 only
			$h{$nfsinst}->{'nfsclient.options.clientaddr'} = "";

			# All
			$h{$nfsinst}->{'nfsclient.options.fsc'} = 0;
			$h{$nfsinst}->{'nfsclient.options.migration'} = 0;
			$h{$nfsinst}->{'nfsclient.options.lookupcache'} = "";
			$h{$nfsinst}->{'nfsclient.options.local_lock'} = "none";

			$h{$nfsinst}->{'nfsclient.options.string'} = $1; # Keep this???

			my @mountopts = split(',', $1);

			foreach my $mountopt (@mountopts) {
            			chomp $mountopt;
				# Is there a better way to do this ??
				# Started with a regex for the full line but got ugly
				#
				if( $mountopt =~ /^(ro|rw)$/ ){ 
					$h{$nfsinst}->{'nfsclient.options.readmode'} = "$1";
				} elsif ( $mountopt =~ /^(sync)$/ ){
					$h{$nfsinst}->{'nfsclient.options.sync'} = 1;
				} elsif ( $mountopt =~ /^(noatime)$/ ){
					$h{$nfsinst}->{'nfsclient.options.atime'} = 0;
				} elsif ( $mountopt =~ /^(nodirtime)$/ ){
					$h{$nfsinst}->{'nfsclient.options.diratime'} = 0;
				} elsif ( $mountopt =~ /^vers=([2-4](\.[0-9])?)/ ){
					$h{$nfsinst}->{'nfsclient.options.vers'} = "$1";
				} elsif ( $mountopt =~ /^rsize=(\d+)$/ ){
					$h{$nfsinst}->{'nfsclient.options.rsize'} = $1;
				} elsif ( $mountopt =~ /^wsize=(\d+)$/ ){
					$h{$nfsinst}->{'nfsclient.options.wsize'} = $1;
				} elsif ( $mountopt =~ /^bsize=(\d+)$/ ){
					$h{$nfsinst}->{'nfsclient.options.bsize'} = $1;
				} elsif ( $mountopt =~ /^namlen=(\d+)$/ ){
					$h{$nfsinst}->{'nfsclient.options.namlen'} = $1;
				} elsif ( $mountopt =~ /^acregmin=(\d+)$/ ){
					$h{$nfsinst}->{'nfsclient.options.acregmin'} = $1;
				} elsif ( $mountopt =~ /^acregmax=(\d+)$/ ){
					$h{$nfsinst}->{'nfsclient.options.acregmax'} = $1;
				} elsif ( $mountopt =~ /^acdirmin=(\d+)$/ ){
					$h{$nfsinst}->{'nfsclient.options.acdirmin'} = $1;
				} elsif ( $mountopt =~ /^acdirmax=(\d+)$/ ){
					$h{$nfsinst}->{'nfsclient.options.acdirmax'} = $1;
				} elsif ( $mountopt =~ /^(soft|hard)$/ ){
					$h{$nfsinst}->{'nfsclient.options.recovery'} = "$1";
				} elsif ( $mountopt =~ /^(posix)$/ ){
					$h{$nfsinst}->{'nfsclient.options.posix'} = 1;
				} elsif ( $mountopt =~ /^(nocto)$/ ){
					$h{$nfsinst}->{'nfsclient.options.cto'} = 0;
				} elsif ( $mountopt =~ /^(noac)$/ ){
					$h{$nfsinst}->{'nfsclient.options.ac'} = 0;
				} elsif ( $mountopt =~ /^(nolock)$/ ){
					$h{$nfsinst}->{'nfsclient.options.lock'} = 0;
				} elsif ( $mountopt =~ /^(noacl)$/ ){
					$h{$nfsinst}->{'nfsclient.options.acl'} = 0;
				} elsif ( $mountopt =~ /^(nordirplus)$/ ){
					$h{$nfsinst}->{'nfsclient.options.rdirplus'} = 0;
				} elsif ( $mountopt =~ /^(nosharecache)$/ ){
					$h{$nfsinst}->{'nfsclient.options.sharecache'} = 0;
				} elsif ( $mountopt =~ /^(noresvport)$/ ){
					$h{$nfsinst}->{'nfsclient.options.resvport'} = 0;
				} elsif ( $mountopt =~ /^proto=(tcp|udp|rdma)$/ ){
					$h{$nfsinst}->{'nfsclient.options.proto'} = "$1";
				} elsif ( $mountopt =~ /^port=(\d+)$/ ){
					$h{$nfsinst}->{'nfsclient.options.port'} = $1;
				} elsif ( $mountopt =~ /^timeo=(\d+)$/ ){
					$h{$nfsinst}->{'nfsclient.options.timeo'} = $1;
				} elsif ( $mountopt =~ /^retrans=(\d+)$/ ){
					$h{$nfsinst}->{'nfsclient.options.retrans'} = $1;
				} elsif ( $mountopt =~ /^sec=(null|sys|krb5|krb5i|krb5p|lkey|lkeyi|lkeyp|spkm|spkmi|spkmp|unknown)$/ ){
					$h{$nfsinst}->{'nfsclient.options.sec'} = "$1";
				} elsif ( $mountopt =~ /^mountaddr=(.*)$/ ){ # NFS3 only
					$h{$nfsinst}->{'nfsclient.options.mountaddr'} = "$1";
				} elsif ( $mountopt =~ /^mountvers=(\d+)$/ ){ # NFS3 only
					$h{$nfsinst}->{'nfsclient.options.mountvers'} = $1;
				} elsif ( $mountopt =~ /^mountport=(\d+)$/ ){ # NFS3 only
					$h{$nfsinst}->{'nfsclient.options.mountport'} = $1;
				} elsif ( $mountopt =~ /^mountproto=(udp|tcp|auto|udp6|tcp6)$/ ){ # NFS3 only
					$h{$nfsinst}->{'nfsclient.options.mountproto'} = "$1"
				} elsif ( $mountopt =~ /^clientaddr=(.*)$/ ){ # NFS4 only
					$h{$nfsinst}->{'nfsclient.options.clientaddr'} = "$1";
				} elsif ( $mountopt =~ /^(fsc)$/ ){
					$h{$nfsinst}->{'nfsclient.options.fsc'} = 1;
				} elsif ( $mountopt =~ /^(migration)$/ ){
					$h{$nfsinst}->{'nfsclient.options.migration'} = 1;
				} elsif ( $mountopt =~ /^lookupcache=(none|pos)$/ ){
					$h{$nfsinst}->{'nfsclient.options.lookupcache'} = "$1";
				} elsif ( $mountopt =~ /^local_lock=(none|all|flock|posix)$/ ){
					$h{$nfsinst}->{'nfsclient.options.local_lock'} = "$1";
				}
        		}
		}
		#age
		if ($line =~ /^\tage:\t(.*)$/) {
                        $h{$nfsinst}->{'nfsclient.age'} = $1;
                }
		#caps
		if ($line =~ /^\tcaps:\t(.*)$/) {
                        $h{$nfsinst}->{'nfsclient.capabilities'} = $1;
                }
		#sec
		if ($line =~ /^\tsec:\t(.*)$/) {
                        $h{$nfsinst}->{'nfsclient.security'} = $1;
                }
		# events
		if ($line =~ /^\tevents:\t(.*)$/) {
			($h{$nfsinst}->{'nfsclient.events.inoderevalidate'},
			 $h{$nfsinst}->{'nfsclient.events.dentryrevalidate'},
			 $h{$nfsinst}->{'nfsclient.events.datainvalidate'},
			 $h{$nfsinst}->{'nfsclient.events.attrinvalidate'},
			 $h{$nfsinst}->{'nfsclient.events.vfsopen'},
			 $h{$nfsinst}->{'nfsclient.events.vfslookup'},
			 $h{$nfsinst}->{'nfsclient.events.vfsaccess'},
			 $h{$nfsinst}->{'nfsclient.events.vfsupdatepage'},
			 $h{$nfsinst}->{'nfsclient.events.vfsreadpage'},
			 $h{$nfsinst}->{'nfsclient.events.vfsreadpages'},
			 $h{$nfsinst}->{'nfsclient.events.vfswritepage'},
			 $h{$nfsinst}->{'nfsclient.events.vfswritepages'},
			 $h{$nfsinst}->{'nfsclient.events.vfsgetdents'},
			 $h{$nfsinst}->{'nfsclient.events.vfssetattr'},
			 $h{$nfsinst}->{'nfsclient.events.vfsflush'},
			 $h{$nfsinst}->{'nfsclient.events.vfsfsync'},
			 $h{$nfsinst}->{'nfsclient.events.vfslock'},
			 $h{$nfsinst}->{'nfsclient.events.vfsrelease'},
			 $h{$nfsinst}->{'nfsclient.events.congestionwait'},
			 $h{$nfsinst}->{'nfsclient.events.setattrtrunc'},
			 $h{$nfsinst}->{'nfsclient.events.extendwrite'},
			 $h{$nfsinst}->{'nfsclient.events.sillyrename'},
			 $h{$nfsinst}->{'nfsclient.events.shortread'},
			 $h{$nfsinst}->{'nfsclient.events.shortwrite'},
			 $h{$nfsinst}->{'nfsclient.events.delay'}) =
				split(/ /, $1);
		}

		# bytes
		if ($line =~ /\tbytes:\t(.*)$/) {
			($h{$nfsinst}->{'nfsclient.bytes.read.normal'},
			 $h{$nfsinst}->{'nfsclient.bytes.write.normal'},
			 $h{$nfsinst}->{'nfsclient.bytes.read.direct'},
			 $h{$nfsinst}->{'nfsclient.bytes.write.direct'},
			 $h{$nfsinst}->{'nfsclient.bytes.read.server'},
			 $h{$nfsinst}->{'nfsclient.bytes.write.server'},
			 $h{$nfsinst}->{'nfsclient.pages.read'},
			 $h{$nfsinst}->{'nfsclient.pages.write'}) =
				split(/ /, $1);
		}

		# xprt
		if ($line =~ /\txprt:\t(.*)$/) {
			my @stats = split(/ /, $1);
			my $xprt_prot = shift(@stats);

			if( $xprt_prot eq "tcp"){

				($h{$nfsinst}->{'nfsclient.xprt.srcport'},
				 $h{$nfsinst}->{'nfsclient.xprt.bind_count'},
				 $h{$nfsinst}->{'nfsclient.xprt.connect_count'},
				 $h{$nfsinst}->{'nfsclient.xprt.connect_time'},
				 $h{$nfsinst}->{'nfsclient.xprt.idle_time'},
				 $h{$nfsinst}->{'nfsclient.xprt.sends'},
				 $h{$nfsinst}->{'nfsclient.xprt.recvs'},
				 $h{$nfsinst}->{'nfsclient.xprt.bad_xids'},
				 $h{$nfsinst}->{'nfsclient.xprt.req_u'},
				 $h{$nfsinst}->{'nfsclient.xprt.bklog_u'}) =
					@stats;

				 # Unused RDMA Elements
                                 $h{$nfsinst}->{'nfsclient.xprt.read_chunks'} = 0;
                                 $h{$nfsinst}->{'nfsclient.xprt.write_chunks'} = 0;
                                 $h{$nfsinst}->{'nfsclient.xprt.reply_chunks'} = 0;
                                 $h{$nfsinst}->{'nfsclient.xprt.total_rdma_req'} = 0;
                                 $h{$nfsinst}->{'nfsclient.xprt.total_rdma_rep'} = 0;
                                 $h{$nfsinst}->{'nfsclient.xprt.pullup'} = 0;
                                 $h{$nfsinst}->{'nfsclient.xprt.fixup'} = 0;
                                 $h{$nfsinst}->{'nfsclient.xprt.hardway'} = 0;
                                 $h{$nfsinst}->{'nfsclient.xprt.failed_marshal'} = 0;
                                 $h{$nfsinst}->{'nfsclient.xprt.bad_reply'} = 0;

			} elsif ( $xprt_prot eq "udp"){
				($h{$nfsinst}->{'nfsclient.xprt.srcport'},
                                 $h{$nfsinst}->{'nfsclient.xprt.bind_count'},
                                 $h{$nfsinst}->{'nfsclient.xprt.sends'},
                                 $h{$nfsinst}->{'nfsclient.xprt.recvs'},
                                 $h{$nfsinst}->{'nfsclient.xprt.bad_xids'},
                                 $h{$nfsinst}->{'nfsclient.xprt.req_u'},
                                 $h{$nfsinst}->{'nfsclient.xprt.bklog_u'}) =
                                        @stats;

				 # Unused TCP elements
				 $h{$nfsinst}->{'nfsclient.xprt.connect_count'} = 0;
                                 $h{$nfsinst}->{'nfsclient.xprt.connect_time'} = 0;
                                 $h{$nfsinst}->{'nfsclient.xprt.idle_time'} = 0;

				 # Unused RDMA Elements
				 $h{$nfsinst}->{'nfsclient.xprt.read_chunks'} = 0;
                                 $h{$nfsinst}->{'nfsclient.xprt.write_chunks'} = 0;
                                 $h{$nfsinst}->{'nfsclient.xprt.reply_chunks'} = 0;
                                 $h{$nfsinst}->{'nfsclient.xprt.total_rdma_req'} = 0;
                                 $h{$nfsinst}->{'nfsclient.xprt.total_rdma_rep'} = 0;
                                 $h{$nfsinst}->{'nfsclient.xprt.pullup'} = 0;
                                 $h{$nfsinst}->{'nfsclient.xprt.fixup'} = 0;
                                 $h{$nfsinst}->{'nfsclient.xprt.hardway'} = 0;
                                 $h{$nfsinst}->{'nfsclient.xprt.failed_marshal'} = 0;
                                 $h{$nfsinst}->{'nfsclient.xprt.bad_reply'} = 0;

			} elsif ( $xprt_prot eq "rdma"){
				($h{$nfsinst}->{'nfsclient.xprt.srcport'},
                                 $h{$nfsinst}->{'nfsclient.xprt.bind_count'},
                                 $h{$nfsinst}->{'nfsclient.xprt.connect_count'},
                                 $h{$nfsinst}->{'nfsclient.xprt.connect_time'},
                                 $h{$nfsinst}->{'nfsclient.xprt.idle_time'},
                                 $h{$nfsinst}->{'nfsclient.xprt.sends'},
                                 $h{$nfsinst}->{'nfsclient.xprt.recvs'},
                                 $h{$nfsinst}->{'nfsclient.xprt.bad_xids'},
                                 $h{$nfsinst}->{'nfsclient.xprt.req_u'},
                                 $h{$nfsinst}->{'nfsclient.xprt.bklog_u'},
				 $h{$nfsinst}->{'nfsclient.xprt.read_chunks'},
				 $h{$nfsinst}->{'nfsclient.xprt.write_chunks'},
				 $h{$nfsinst}->{'nfsclient.xprt.reply_chunks'},
				 $h{$nfsinst}->{'nfsclient.xprt.total_rdma_req'},
				 $h{$nfsinst}->{'nfsclient.xprt.total_rdma_rep'},
				 $h{$nfsinst}->{'nfsclient.xprt.pullup'},
				 $h{$nfsinst}->{'nfsclient.xprt.fixup'},
				 $h{$nfsinst}->{'nfsclient.xprt.hardway'},
				 $h{$nfsinst}->{'nfsclient.xprt.failed_marshal'},
				 $h{$nfsinst}->{'nfsclient.xprt.bad_reply'}) =
                                        @stats;

			} else {
				next;
			}
		}

		# per-op statistics
		# pre-populate all possible ops from v3, v4, v4.1 and set to noval
		# ops defined below
		for my $opname (@all_ops) {
			$h{$nfsinst}->{"nfsclient.ops.$opname.ops"} = $noval;
                        $h{$nfsinst}->{"nfsclient.ops.$opname.ntrans"} = $noval;
                        $h{$nfsinst}->{"nfsclient.ops.$opname.timeouts"} = $noval;
                        $h{$nfsinst}->{"nfsclient.ops.$opname.bytes_sent"} = $noval;
                        $h{$nfsinst}->{"nfsclient.ops.$opname.bytes_recv"} = $noval;
                        $h{$nfsinst}->{"nfsclient.ops.$opname.queue"} = $noval;
                        $h{$nfsinst}->{"nfsclient.ops.$opname.rtt"} = $noval;
                        $h{$nfsinst}->{"nfsclient.ops.$opname.execute"} = $noval;
		}

		if ($line =~ /\tper-op statistics$/) {
			# We'll do these a bit differently since they are not
			# all on the same line.  Just loop until we don't match
			# anymore.
			# v4 ops can have underscore
			while (<STATS> =~
	/^\s*([A-Z_]*): (\d*) (\d*) (\d*) (\d*) (\d*) (\d*) (\d*) (\d*)$/) {
				my $op_name = "\L$1";
				($h{$nfsinst}->{"nfsclient.ops.$op_name.ops"},
				 $h{$nfsinst}->{"nfsclient.ops.$op_name.ntrans"},
				 $h{$nfsinst}->{"nfsclient.ops.$op_name.timeouts"},
				 $h{$nfsinst}->{"nfsclient.ops.$op_name.bytes_sent"},
				 $h{$nfsinst}->{"nfsclient.ops.$op_name.bytes_recv"},
				 $h{$nfsinst}->{"nfsclient.ops.$op_name.queue"},
				 $h{$nfsinst}->{"nfsclient.ops.$op_name.rtt"},
				 $h{$nfsinst}->{"nfsclient.ops.$op_name.execute"}) =
					($2, $3, $4, $5, $6, $7, $8, $9);
			}
		}
	}

	close STATS;
}

#
# fetch is called once by pcp for each refresh and then the fetch callback is
# called to query each statistic individually
#
sub nfsclient_fetch {
	nfsclient_parse_proc_mountstats();

	our $pmda->replace_indom($nfsclient_indom, \%h);
}

sub nfsclient_fetch_callback {
	my ($cluster, $item, $inst) = @_; 
	my $value;

	my $lookup = pmda_inst_lookup($nfsclient_indom, $inst);
	return (PM_ERR_INST, 0) unless defined($lookup);

	my $pmid_name = pmda_pmid_name($cluster, $item)
               or die "Unknown metric name: cluster $cluster item $item\n";

	# check if we have no values for this NFS version
	$value = $lookup->{$pmid_name};
	return ($value, 0) unless ($value != $noval);

	return ($lookup->{$pmid_name}, 1);

}

# the PCP::PMDA->new line is parsed by the check_domain rule of the PMDA build
# process, so there are special requirements:  no comments, the domain has to
# be a bare number.
#
our $pmda = PCP::PMDA->new('nfsclient', 62);

# metrics go here, with full descriptions

# general - cluster 0
$pmda->add_metric(pmda_pmid(0,1), PM_TYPE_STRING, $nfsclient_indom,
		PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		'nfsclient.export',
		'Export',
		'');
$pmda->add_metric(pmda_pmid(0,2), PM_TYPE_STRING, $nfsclient_indom,
		PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		'nfsclient.mountpoint',
		'Mount Point',
		'');

# opts - cluster 1
#
# Options in order:
# NULL is not present
# from super.c in nfs_show_mount_options
#
# ro | rw
# sync | NULL
# noatime | NULL
# nodiratime | NULL
# vers=([2-4](\.[0-9])?)
# rsize=%u
# wsize=%u
# bsize=%u | NULL
# namlen=%u
# acregmin=%u | NULL
# acregmax=%u | NULL
# acdirmin=%u | NULL
# acdirmax=%u | NULL
# soft | hard
# posix | NULL
# nocto | NULL
# noac | NULL
# nolock | NULL
# noacl | NULL
# nordirplus | NULL
# nosharecache | NULL
# noresvport | NULL
# proto=(tcp|udp|rdma)
# port=%u | NULL
# timeo=%lu
# retrans=%u
# sec=(null|sys|krb5|krb5i|krb5p|lkey|lkeyi|lkeyp|spkm|spkmi|spkmp|unknown)   # null and unknown are those exact literal strings

# NFS 3 only
# Below block may not exist at all if NFS_MOUNT_LEGACY_INTERFACE
# mountaddr="ip4addr|ip6addr|unspecified"
# mountvers=%u | NULL
# mountport=%u | NULL
# mountproto=(udp|tcp|auto|udp6|tcp6|NULL)    # NULL if showdefaults is false

# NFS 4 Only
# clientaddr=(ip4address|ip6address)

# fsc | NULL
# migration | NULL
# lookupcache=(none|pos) | NULL
# local_lock=(none|all|flock|posix)

# TODO
# deprecated after 2.6.25, do we care about these ?
# intr|nointr

$pmda->add_metric(pmda_pmid(1,1), PM_TYPE_STRING, $nfsclient_indom,
                PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
                'nfsclient.options.string',
                'Full Options String',
                '');
$pmda->add_metric(pmda_pmid(1,2), PM_TYPE_STRING, $nfsclient_indom,
                PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
                'nfsclient.options.readmode',
                'rw or ro mount mode',
                '');
$pmda->add_metric(pmda_pmid(1,3), PM_TYPE_U32, $nfsclient_indom,
                PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
                'nfsclient.options.sync',
                'boolean sync mount option used',
                '');
$pmda->add_metric(pmda_pmid(1,4), PM_TYPE_U32, $nfsclient_indom,
                PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
                'nfsclient.options.atime',
                'boolean atime mount option used',
                '');
$pmda->add_metric(pmda_pmid(1,5), PM_TYPE_U32, $nfsclient_indom,
                PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
                'nfsclient.options.diratime',
                'boolean diratime mount option used',
                '');
$pmda->add_metric(pmda_pmid(1,6), PM_TYPE_STRING, $nfsclient_indom,
                PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
                'nfsclient.options.vers',
                'nfs version',
                '');
$pmda->add_metric(pmda_pmid(1,7), PM_TYPE_U32, $nfsclient_indom,
                PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
                'nfsclient.options.rsize',
                'rsize mount parameter',
                '');
$pmda->add_metric(pmda_pmid(1,8), PM_TYPE_U32, $nfsclient_indom,
                PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
                'nfsclient.options.wsize',
                'wsize mount parameter',
                '');
$pmda->add_metric(pmda_pmid(1,9), PM_TYPE_U32, $nfsclient_indom,
                PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
                'nfsclient.options.bsize',
                'bsize mount parameter',
                '');
$pmda->add_metric(pmda_pmid(1,10), PM_TYPE_U32, $nfsclient_indom,
                PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
                'nfsclient.options.namlen',
                'namlen mount parameter',
                '');
$pmda->add_metric(pmda_pmid(1,11), PM_TYPE_U32, $nfsclient_indom,
                PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
                'nfsclient.options.acregmin',
                'acregmin mount parameter',
                '');
$pmda->add_metric(pmda_pmid(1,12), PM_TYPE_U32, $nfsclient_indom,
                PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
                'nfsclient.options.acregmax',
                'acregmax mount parameter',
                '');
$pmda->add_metric(pmda_pmid(1,13), PM_TYPE_U32, $nfsclient_indom,
                PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
                'nfsclient.options.acdirmin',
                'acdirmin mount parameter',
                '');
$pmda->add_metric(pmda_pmid(1,14), PM_TYPE_U32, $nfsclient_indom,
                PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
                'nfsclient.options.acdirmax',
                'acdirmax mount parameter',
                '');
$pmda->add_metric(pmda_pmid(1,15), PM_TYPE_STRING, $nfsclient_indom,
                PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
                'nfsclient.options.recovery',
                'hard or soft recovery behavior',
                '');
$pmda->add_metric(pmda_pmid(1,16), PM_TYPE_U32, $nfsclient_indom,
                PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
                'nfsclient.options.posix',
                'boolean posix mount option used',
                '');
$pmda->add_metric(pmda_pmid(1,17), PM_TYPE_U32, $nfsclient_indom,
                PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
                'nfsclient.options.cto',
                'boolean cto mount option used',
                '');
$pmda->add_metric(pmda_pmid(1,18), PM_TYPE_U32, $nfsclient_indom,
                PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
                'nfsclient.options.ac',
                'boolean ac mount option used',
                '');
$pmda->add_metric(pmda_pmid(1,19), PM_TYPE_U32, $nfsclient_indom,
                PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
                'nfsclient.options.lock',
                'boolean lock mount option used',
                '');
$pmda->add_metric(pmda_pmid(1,20), PM_TYPE_U32, $nfsclient_indom,
                PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
                'nfsclient.options.acl',
                'boolean acl mount option used',
                '');
$pmda->add_metric(pmda_pmid(1,21), PM_TYPE_U32, $nfsclient_indom,
                PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
                'nfsclient.options.rdirplus',
                'boolean rdirplus mount option used',
                '');
$pmda->add_metric(pmda_pmid(1,22), PM_TYPE_U32, $nfsclient_indom,
                PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
                'nfsclient.options.sharecache',
                'boolean sharecache mount option used',
                '');
$pmda->add_metric(pmda_pmid(1,23), PM_TYPE_U32, $nfsclient_indom,
                PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
                'nfsclient.options.resvport',
                'boolean resvport mount option used',
                '');
$pmda->add_metric(pmda_pmid(1,24), PM_TYPE_STRING, $nfsclient_indom,
                PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
                'nfsclient.options.proto',
                'nfs protocol: udp|tcp|rdma',
                '');
$pmda->add_metric(pmda_pmid(1,25), PM_TYPE_U32, $nfsclient_indom,
                PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
                'nfsclient.options.port',
                'port mount parameter',
                '');
$pmda->add_metric(pmda_pmid(1,26), PM_TYPE_U32, $nfsclient_indom,
                PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
                'nfsclient.options.timeo',
                'timeo mount parameter',
                '');
$pmda->add_metric(pmda_pmid(1,27), PM_TYPE_U32, $nfsclient_indom,
                PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
                'nfsclient.options.retrans',
                'retrans mount parameter',
                '');
$pmda->add_metric(pmda_pmid(1,28), PM_TYPE_STRING, $nfsclient_indom,
                PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
                'nfsclient.options.sec',
                'sec mount parameter',
                '');
# NFS3 only
$pmda->add_metric(pmda_pmid(1,29), PM_TYPE_STRING, $nfsclient_indom,
                PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
                'nfsclient.options.mountaddr',
                'mountaddr mount parameter',
                '');
$pmda->add_metric(pmda_pmid(1,30), PM_TYPE_U32, $nfsclient_indom,
                PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
                'nfsclient.options.mountvers',
                'mountvers mount parameter',
                '');
$pmda->add_metric(pmda_pmid(1,31), PM_TYPE_U32, $nfsclient_indom,
                PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
                'nfsclient.options.mountport',
                'mountport mount parameter',
                '');
$pmda->add_metric(pmda_pmid(1,32), PM_TYPE_STRING, $nfsclient_indom,
                PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
                'nfsclient.options.mountproto',
                'mountproto mount parameter',
                '');
# NFS4 only
$pmda->add_metric(pmda_pmid(1,33), PM_TYPE_STRING, $nfsclient_indom,
                PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
                'nfsclient.options.clientaddr',
                'clientaddr mount parameter',
                '');
# All
$pmda->add_metric(pmda_pmid(1,34), PM_TYPE_U32, $nfsclient_indom,
                PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
                'nfsclient.options.fsc',
                'fsc mount parameter',
                '');
$pmda->add_metric(pmda_pmid(1,35), PM_TYPE_U32, $nfsclient_indom,
                PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
                'nfsclient.options.migration',
                'migration mount parameter',
                '');
$pmda->add_metric(pmda_pmid(1,36), PM_TYPE_STRING, $nfsclient_indom,
                PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
                'nfsclient.options.lookupcache',
                'lookupcache mount parameter',
                '');
$pmda->add_metric(pmda_pmid(1,37), PM_TYPE_STRING, $nfsclient_indom,
                PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
                'nfsclient.options.local_lock',
                'local_lock mount parameter',
                '');

# age - cluster 2
$pmda->add_metric(pmda_pmid(2,1), pmda_ulong, $nfsclient_indom,
                  PM_SEM_INSTANT, pmda_units(0,1,0,0,PM_TIME_SEC,0),
                  'nfsclient.age',
                  'Age in Seconds',
                  '');
# caps - cluster 3 - Any use in parsing this ?
$pmda->add_metric(pmda_pmid(3,1), PM_TYPE_STRING, $nfsclient_indom,
                PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
                'nfsclient.capabilities',
                'Capabilities',
                '');

# sec - cluster 4 - Any use in parsing this ?
$pmda->add_metric(pmda_pmid(4,1), PM_TYPE_STRING, $nfsclient_indom,
                PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
                'nfsclient.security',
                'Security Flavor',
                '');

# events - cluster 5
$pmda->add_metric(pmda_pmid(5,1), PM_TYPE_U32, $nfsclient_indom,
		  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  'nfsclient.events.inoderevalidate',
		  'NFSIOS_INODEREVALIDATE',
'incremented in __nfs_revalidate_inode whenever an inode is revalidated');

$pmda->add_metric(pmda_pmid(5,2), PM_TYPE_U32, $nfsclient_indom,
		  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  'nfsclient.events.dentryrevalidate',
		  'NFSIOS_DENTRYREVALIDATE',
'incremented in nfs_lookup_revalidate whenever a dentry is revalidated');

$pmda->add_metric(pmda_pmid(5,3), PM_TYPE_U32, $nfsclient_indom,
		  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  'nfsclient.events.datainvalidate',
		  'NFSIOS_DATAINVALIDATE',
'incremented in nfs_invalidate_mapping_nolock when data cache for an inode ' .
'is invalidated');

$pmda->add_metric(pmda_pmid(5,4), PM_TYPE_U32, $nfsclient_indom,
		  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  'nfsclient.events.attrinvalidate',
		  'NFSIOS_ATTRINVALIDATE',
'incremented in nfs_zap_caches_locked and nfs_update_inode when an the ' .
'attribute cache for an inode has been invalidated');

$pmda->add_metric(pmda_pmid(5,5), PM_TYPE_U32, $nfsclient_indom,
		  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  'nfsclient.events.vfsopen',
		  'NFSIOS_VFSOPEN',
'incremented in nfs_file_open and nfs_opendir whenever a file or directory ' .
'is opened');

$pmda->add_metric(pmda_pmid(5,6), PM_TYPE_U32, $nfsclient_indom,
		  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  'nfsclient.events.vfslookup',
		  'NFSIOS_VFSLOOKUP',
'incremented in nfs_lookup on every lookup');

$pmda->add_metric(pmda_pmid(5,7), PM_TYPE_U32, $nfsclient_indom,
		  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  'nfsclient.events.vfsaccess',
		  '', '');

$pmda->add_metric(pmda_pmid(5,8), PM_TYPE_U32, $nfsclient_indom,
		  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  'nfsclient.events.vfsupdatepage',
		  '', '');

$pmda->add_metric(pmda_pmid(5,9), PM_TYPE_U32, $nfsclient_indom,
		  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  'nfsclient.events.vfsreadpage',
		  '', '');

$pmda->add_metric(pmda_pmid(5,10), PM_TYPE_U32, $nfsclient_indom,
		  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  'nfsclient.events.vfsreadpages',
		  '', '');

$pmda->add_metric(pmda_pmid(5,11), PM_TYPE_U32, $nfsclient_indom,
		  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  'nfsclient.events.vfswritepage',
		  '', '');

$pmda->add_metric(pmda_pmid(5,12), PM_TYPE_U32, $nfsclient_indom,
		  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  'nfsclient.events.vfswritepages',
		  '', '');

$pmda->add_metric(pmda_pmid(5,13), PM_TYPE_U32, $nfsclient_indom,
		  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  'nfsclient.events.vfsgetdents',
		  '', '');

$pmda->add_metric(pmda_pmid(5,14), PM_TYPE_U32, $nfsclient_indom,
		  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  'nfsclient.events.vfssetattr',
		  '', '');

$pmda->add_metric(pmda_pmid(5,15), PM_TYPE_U32, $nfsclient_indom,
		  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  'nfsclient.events.vfsflush',
		  '', '');

$pmda->add_metric(pmda_pmid(5,16), PM_TYPE_U32, $nfsclient_indom,
		  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  'nfsclient.events.vfsfsync',
		  '', '');

$pmda->add_metric(pmda_pmid(5,17), PM_TYPE_U32, $nfsclient_indom,
		  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  'nfsclient.events.vfslock',
		  '', '');

$pmda->add_metric(pmda_pmid(5,18), PM_TYPE_U32, $nfsclient_indom,
		  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  'nfsclient.events.vfsrelease',
		  '', '');

$pmda->add_metric(pmda_pmid(5,19), PM_TYPE_U32, $nfsclient_indom,
		  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  'nfsclient.events.congestionwait',
		  '', '');

$pmda->add_metric(pmda_pmid(5,20), PM_TYPE_U32, $nfsclient_indom,
		  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  'nfsclient.events.setattrtrunc',
		  '', '');

$pmda->add_metric(pmda_pmid(5,21), PM_TYPE_U32, $nfsclient_indom,
		  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  'nfsclient.events.extendwrite',
		  '', '');

$pmda->add_metric(pmda_pmid(5,22), PM_TYPE_U32, $nfsclient_indom,
		  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  'nfsclient.events.sillyrename',
		  '', '');

$pmda->add_metric(pmda_pmid(5,23), PM_TYPE_U32, $nfsclient_indom,
		  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  'nfsclient.events.shortread',
		  '', '');

$pmda->add_metric(pmda_pmid(5,24), PM_TYPE_U32, $nfsclient_indom,
		  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  'nfsclient.events.shortwrite',
		  '', '');

$pmda->add_metric(pmda_pmid(5,25), PM_TYPE_U32, $nfsclient_indom,
		  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  'nfsclient.events.delay',
		  '', '');

# bytes - cluster 6
$pmda->add_metric(pmda_pmid(6,1), pmda_ulong, $nfsclient_indom,
		  PM_SEM_COUNTER, pmda_units(1,0,0,PM_SPACE_BYTE,0,0),
		  'nfsclient.bytes.read.normal',
		  'NFSIOS_NORMALREADBYTES',
'the number of bytes read by applications via the read system call interface');

$pmda->add_metric(pmda_pmid(6,2), PM_TYPE_U64, $nfsclient_indom,
		  PM_SEM_COUNTER, pmda_units(1,0,0,PM_SPACE_BYTE,0,0),
		  'nfsclient.bytes.write.normal',
		  'NFSIOS_NORMALWRITTENBYTES',
'the number of bytes written by applications via the write system call ' .
'interface');

$pmda->add_metric(pmda_pmid(6,3), PM_TYPE_U64, $nfsclient_indom,
		  PM_SEM_COUNTER, pmda_units(1,0,0,PM_SPACE_BYTE,0,0),
		  'nfsclient.bytes.read.direct',
		  'NFSIOS_DIRECTREADBYTES',
'the number of bytes read by applications from files opened with the ' .
'O_DIRECT flag');

$pmda->add_metric(pmda_pmid(6,4), PM_TYPE_U64, $nfsclient_indom,
		  PM_SEM_COUNTER, pmda_units(1,0,0,PM_SPACE_BYTE,0,0),
		  'nfsclient.bytes.write.direct',
		  'NFSIOS_DIRECTWRITTENBYTES',
'the number of bytes written by applications to files opened with the ' .
'O_DIRECT flag');

$pmda->add_metric(pmda_pmid(6,5), PM_TYPE_U64, $nfsclient_indom,
		  PM_SEM_COUNTER, pmda_units(1,0,0,PM_SPACE_BYTE,0,0),
		  'nfsclient.bytes.read.server',
		  'NFSIOS_SERVERREADBYTES',
'the number of bytes read from the nfs server by the nfs client via nfs ' .
'read requests');

$pmda->add_metric(pmda_pmid(6,6), PM_TYPE_U64, $nfsclient_indom,
		  PM_SEM_COUNTER, pmda_units(1,0,0,PM_SPACE_BYTE,0,0),
		  'nfsclient.bytes.write.server',
		  'NFSIOS_SERVERWRITTENBYTES',
'the number of bytes written to the nfs server by the nfs client via nfs ' .
'write requests');

$pmda->add_metric(pmda_pmid(6,7), PM_TYPE_U64, $nfsclient_indom,
		  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  'nfsclient.pages.read',
		  'NFSIOS_READPAGES',
'the number of pages read via nfs_readpage or nfs_readpages');

$pmda->add_metric(pmda_pmid(6,8), PM_TYPE_U64, $nfsclient_indom,
		  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  'nfsclient.pages.write',
		  'NFSIOS_WRITEPAGES',
'the number of pages written via nfs_writepage or nfs_writepages');

# xprt - cluster 7
#
# We have three possible transports:  udp, tcp, rdma.
# Fill in 0 for ones that dont apply.  Types for RDMA from net/sunrpc/xprtrdma/transport.c : xprt_rdma_print_stats
#
$pmda->add_metric(pmda_pmid(7,1), PM_TYPE_STRING, $nfsclient_indom,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'nfsclient.xprt.srcport',
		  'tcp source port',
'source port on the client');

$pmda->add_metric(pmda_pmid(7,2), PM_TYPE_U32, $nfsclient_indom,
		  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  'nfsclient.xprt.bind_count',
		  'count of rpcbind get_port calls',
'incremented in rpcb_getport_async: \"obtain the port for a given RPC ' .
'service on a given host\"');

$pmda->add_metric(pmda_pmid(7,3), PM_TYPE_U32, $nfsclient_indom,
		  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  'nfsclient.xprt.connect_count',
		  'count of tcp connects',
'incremented in xs_tcp_finish_connecting and xprt_connect_status.  This is ' .
'a count of the number of tcp (re)connects (only of which is active at a ' .
'time) for this mount.');

$pmda->add_metric(pmda_pmid(7,4), PM_TYPE_U32, $nfsclient_indom,
		  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  'nfsclient.xprt.connect_time',
		  'jiffies waiting for connect',
'Summed in xprt_connect_status, it is stored and printed in jiffies.  This ' .
'the sum of all connection attempts: how long was spent waiting to connect.');

$pmda->add_metric(pmda_pmid(7,5), PM_TYPE_U32, $nfsclient_indom,
		  PM_SEM_INSTANT, pmda_units(0,1,0,0,PM_TIME_SEC,0),
		  'nfsclient.xprt.idle_time',
		  'transport idle time',
'How long has it been since the transport has been used.  Stored and ' .
'calculated in jiffies and printed in seconds.');

$pmda->add_metric(pmda_pmid(7,6), PM_TYPE_U32, $nfsclient_indom,
		  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  'nfsclient.xprt.sends',
                  'count of tcp transmits',
'Incremented in xprt_transmit upon transmit completion of each rpc.');

$pmda->add_metric(pmda_pmid(7,7), PM_TYPE_U32, $nfsclient_indom,
		  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  'nfsclient.xprt.recvs',
		  'count of tcp receives',
'Incremented in xprt_complete_rqst when reply processing is complete.');

$pmda->add_metric(pmda_pmid(7,8), PM_TYPE_U32, $nfsclient_indom,
		  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  'nfsclient.xprt.bad_xids',
		  'count of bad transaction identifiers',
'When processing an rpc reply it is necessary to look up the original ' .
'rpc_rqst using xprt_lookup_rqst.  If the rpc_rqst that prompted the reply ' .
'cannot be found on the transport bad_xids is incremented.');

$pmda->add_metric(pmda_pmid(7,9), PM_TYPE_U64, $nfsclient_indom,
		  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  'nfsclient.xprt.req_u',
                  'average requests on the wire',
'FIXME:  The comment in struct stat says: \"average requests on the wire\", ' .
'but the actual calculation in xprt_transmit is: ' .
'xprt->stat.req_u += xprt->stat.sends - xprt->stat.recvs;\n' .
'This stat may be broken.');

$pmda->add_metric(pmda_pmid(7,10), PM_TYPE_U64, $nfsclient_indom,
		  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  'nfsclient.xprt.backlog_u',
		  'backlog queue utilization',
'FIXME: here is the calculation in xprt_transmit:  ' .
'xprt->stat.bklog_u += xprt->backlog.qlen;\n ' .
'qlen is incremented in __rpc_add_wait_queue and decremented in ' .
'__rpc_remove_wait_queue.');

$pmda->add_metric(pmda_pmid(7,11), pmda_ulong, $nfsclient_indom,
                  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
                  'nfsclient.xprt.read_chunks',
                  '',
		  '');

$pmda->add_metric(pmda_pmid(7,12), pmda_ulong, $nfsclient_indom,
                  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
                  'nfsclient.xprt.write_chunks',
                  '',
		  '');

$pmda->add_metric(pmda_pmid(7,13), pmda_ulong, $nfsclient_indom,
                  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
                  'nfsclient.xprt.reply_chunks',
                  '',
		  '');

$pmda->add_metric(pmda_pmid(7,14), PM_TYPE_U64, $nfsclient_indom,
                  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
                  'nfsclient.xprt.total_rdma_req',
                  '',
		  '');

$pmda->add_metric(pmda_pmid(7,15), PM_TYPE_U64, $nfsclient_indom,
                  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
                  'nfsclient.xprt.total_rdma_rep',
                  '',
		  '');

$pmda->add_metric(pmda_pmid(7,16), PM_TYPE_U64, $nfsclient_indom,
                  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
                  'nfsclient.xprt.pullup',
                  '',
		  '');

$pmda->add_metric(pmda_pmid(7,17), PM_TYPE_U64, $nfsclient_indom,
                  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
                  'nfsclient.xprt.fixup',
                  '',
		  '');

$pmda->add_metric(pmda_pmid(7,18), pmda_ulong, $nfsclient_indom,
                  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
                  'nfsclient.xprt.hardway',
                  '',
		  '');

$pmda->add_metric(pmda_pmid(7,19), pmda_ulong, $nfsclient_indom,
                  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
                  'nfsclient.xprt.failed_marshal',
                  '',
		  '');

$pmda->add_metric(pmda_pmid(7,20), pmda_ulong, $nfsclient_indom,
                  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
                  'nfsclient.xprt.bad_reply',
                  '',
		  '');

# ops - cluster 8
#
# Different versions have different ops.  We just List them all and fill in
# only what is appropriate for this version. Non matching ones will return noval
# from above.

# ops available in v3, v4 and v4.1
#
our @common_ops = ('null', 'getattr', 'setattr', 'lookup', 'access', 'readlink',
           'read', 'write', 'create', 'symlink', 'remove',
           'rename', 'link', 'readdir',
	   'fsinfo', 'pathconf', 'commit');

# ops only in v3
#
our @v3only_ops = ('fsstat', 'mkdir', 'mknod', 'readdirplus', 'rmdir');

# ops ADDED in v4, some v3 ops are invalid : fsstat, mkdir, mknod, readdirplus, rmdir 
#
our @v4_ops = ('close', 'create_session', 'delegreturn', 'destroy_session', 'exchange_id',
	   'fs_locations', 'getacl', 'getdeviceinfo', 'get_lease_time', 'layoutcommit',
	   'layoutget', 'layoutreturn', 'lock', 'lockt', 'locku', 'lookup_root',
	   'open', 'open_confirm', 'open_downgrade', 'open_noattr', 'reclaim_complete',
	   'release_lockowner', 'renew', 'secinfo', 'sequence', 'server_caps', 'setacl',
	   'setclientid', 'setclientid_confirm', 'statfs');

# ops ADDED in v4.1, same v3 ops removed as in v4, all v4 ops should exist
#
our @v41_ops = ('bind_conn_to_session', 'destroy_clientid', 'free_stateid', 'getdevicelist',
	     'secinfo_no_name', 'test_stateid');

our @all_ops = ( @common_ops, @v3only_ops, @v4_ops, @v41_ops );

my $item = 1;
for my $op_name (@all_ops) {
	$pmda->add_metric(pmda_pmid(8, $item++), pmda_ulong, $nfsclient_indom,
			  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
			  "nfsclient.ops.$op_name.ops",
			  'count of operations',
'rpc count for this op, only bumped in rpc_count_iostats');

	$pmda->add_metric(pmda_pmid(8, $item++), pmda_ulong, $nfsclient_indom,
			  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
			  "nfsclient.ops.$op_name.ntrans",
			  'count of transmissions',
'there can be more than one transmission per rpc');

	$pmda->add_metric(pmda_pmid(8, $item++), pmda_ulong, $nfsclient_indom,
			  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
			  "nfsclient.ops.$op_name.timeouts",
			  'count of major timeouts',
'XXX fill me in');

	$pmda->add_metric(pmda_pmid(8, $item++), PM_TYPE_U64, $nfsclient_indom,
			  PM_SEM_COUNTER, pmda_units(1,0,0,PM_SPACE_BYTE,0,0),
			  "nfsclient.ops.$op_name.bytes_sent",
			  'count of bytes out',
'How many bytes are sent for this procedure type.  This indicates how much ' .
'load this procedure is putting on the network.  It includes the RPC and ULP' .
'headers, and the request payload');

	$pmda->add_metric(pmda_pmid(8, $item++), PM_TYPE_U64, $nfsclient_indom,
			  PM_SEM_COUNTER, pmda_units(1,0,0,PM_SPACE_BYTE,0,0),
			  "nfsclient.ops.$op_name.bytes_recv",
			  'count of bytes in',
'How many bytes are received for this procedure type.  This indicates how ' .
'much load this procedure is putting on the network.  It includes RPC and ' .
'ULP headers, and the request payload');

	$pmda->add_metric(pmda_pmid(8, $item++), PM_TYPE_U64, $nfsclient_indom,
			  PM_SEM_COUNTER, pmda_units(0,1,0,0,PM_TIME_MSEC,0),
			  "nfsclient.ops.$op_name.queue",
			  'milliseconds queued for transmit',
'The length of time an RPC request waits in queue before transmission in ' .
' milliseconds.');

	$pmda->add_metric(pmda_pmid(8, $item++), PM_TYPE_U64, $nfsclient_indom,
			  PM_SEM_COUNTER, pmda_units(0,1,0,0,PM_TIME_MSEC,0),
			  "nfsclient.ops.$op_name.rtt",
			  'milliseconds for rpc round trip time',
'The network + server latency of the request in milliseconds.');

	$pmda->add_metric(pmda_pmid(8, $item++), PM_TYPE_U64, $nfsclient_indom,
			  PM_SEM_COUNTER, pmda_units(0,1,0,0,PM_TIME_MSEC,0),
			  "nfsclient.ops.$op_name.execute",
			  'milliseconds for rpc execution',
'The total time the request spent from init to release in milliseconds.');
}

&nfsclient_parse_proc_mountstats;

$nfsclient_indom = $pmda->add_indom($nfsclient_indom, {}, '', '');

$pmda->set_fetch(\&nfsclient_fetch);
$pmda->set_fetch_callback(\&nfsclient_fetch_callback);

$pmda->run;
