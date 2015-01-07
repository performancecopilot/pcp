use strict;
use warnings;
use PCP::PMDA;

use vars qw( $pmda );

our  $lustre_indom = 0;

# llite proc root
our $LLITE_PATH = "/proc/fs/lustre/llite/";

# Configuration files for overriding the location of LLITE_PATH, mostly for testing purposes
for my $file (pmda_config('PCP_PMDAS_DIR') . '/lustre/lustre.conf', 'luster.conf') {
        eval `cat $file` unless ! -f $file;
}

# Check env variable for llite path to use.  Should be a dir with all the files
if ( defined $ENV{"LUSTRE_LLITE_PATH"} ) {
	$LLITE_PATH = $ENV{"LUSTRE_LLITES_PATH"}
}

our $noval = PM_ERR_APPVERSION;

# List of metrics we care about
#
# Bytes have: min, max, count, total
our @byte_stats = ('read_bytes', 'write_bytes', 'osc_read', 'osc_write');

# Pages have: min, max, count, total
our @page_stats = ('brw_read', 'brw_write');

# Reg have: count
our @reg_stats = ('dirty_pages_hits', 'dirty_pages_misses', 'ioctl', 'open', 'close',
	   'mmap', 'seek', 'fsync', 'readdir', 'setattr',
	   'truncate', 'flock', 'getattr', 'create', 'link', 'unlink',
	   'symlink', 'mkdir', 'rmdir', 'mknod', 'rename',
	   'statfs', 'alloc_inode', 'setxattr', 'getxattr', 'getxattr_hits', 'listxattr',
	   'removexattr', 'inode_permission');

# The stats hash is keyed on base name of the entires in /proc/fs/lustre/llite/ e.g.
# 'lustre' from lustre-ffff880378305c00.  The value is a hash ref, and that hash contains all of
# the stats data keyed on pmid_name
our %h = ();

#
# Find all the lustre file systems on the host
# Devices are of the form "foo-<16_hex_chars>"
#
# And parse the stats file underneath

sub lustre_get_fs_stats{
	$pmda->err("Doing update");
	opendir(LLITEDIR, $LLITE_PATH) || die "Can't open directory: $LLITE_PATH\n";

	while(my $ldev = readdir(LLITEDIR) ){
		$pmda->err("Checking dev $ldev");
		if( $ldev =~ /^(\w+)-([a-f0-9]{16})$/ ){

			# Get the device information

			my $mtroot = $1;
			my $hexsuper = $2;
			$h{$mtroot} = {}; # {} is an anonymous hash ref
			$h{$mtroot}->{'lustre.llite.volume'} = $mtroot;
			$h{$mtroot}->{'lustre.llite.superblock'} = $hexsuper;

			# Parse the actual stats file
			# From: lustre-2.5.3/lustre/llite/lproc_llite.c
			# Stats do not exists unless the counter has been incremented. Need to initialize to 0
			
			for my $stat_name (@byte_stats) {
				$h{$mtroot}->{"lustre.llite.$stat_name.count"} = 0;
				$h{$mtroot}->{"lustre.llite.$stat_name.min"} = 0;
				$h{$mtroot}->{"lustre.llite.$stat_name.max"} = 0;
				$h{$mtroot}->{"lustre.llite.$stat_name.total"} = 0;
			}
			for my $stat_name (@page_stats) {
				$h{$mtroot}->{"lustre.llite.$stat_name.count"} = 0;
				$h{$mtroot}->{"lustre.llite.$stat_name.min"} = 0;
				$h{$mtroot}->{"lustre.llite.$stat_name.max"} = 0;
				$h{$mtroot}->{"lustre.llite.$stat_name.total"} = 0;
			}
			for my $stat_name (@reg_stats) {
				$h{$mtroot}->{"lustre.llite.$stat_name.count"} = 0;
			}
			
			my $statspath = $LLITE_PATH . $ldev . '/stats';

			open STATS, '<', $statspath ||
                		( $pmda->err("pmdalustre failed to open $statspath: $!") &&
                		die "Can't open $statspath: $!\n") ;

			while (<STATS>) {
                		my $line = $_;
				$pmda->err("Parsing: $line");
				# Byte types first
				if( $line =~ /^(read_bytes|write_bytes|osc_read|osc_write)\s+(\d+) samples \[bytes\] (\d+) (\d+) (\d+)$/){
					my $name = $1;
					my $count = $2;
					my $min = $3;
					my $max = $4;
					my $total = $5;
					$pmda->err("Got: $name $count $min $max $total");
					$h{$mtroot}->{"lustre.llite.$name.count"} = $count;
					$h{$mtroot}->{"lustre.llite.$name.min"} = $min;
					$h{$mtroot}->{"lustre.llite.$name.max"} = $max;
					$h{$mtroot}->{"lustre.llite.$name.total"} = $total;
				# Pages types
				# I have not seen these, so can't test
				} elsif ( $line =~ /^(brw_read|brw_write)\s+(\d+) samples \[pages\] (\d+) (\d+) (\d+)$/){
					my $name = $1;
                                        my $count = $2;
                                        my $min = $3;
                                        my $max = $4;
                                        my $total = $5;
                                        $h{$mtroot}->{"lustre.llite.$name.count"} = $count;
                                        $h{$mtroot}->{"lustre.llite.$name.min"} = $min;
                                        $h{$mtroot}->{"lustre.llite.$name.max"} = $max;
                                        $h{$mtroot}->{"lustre.llite.$name.total"} = $total;
				# Regs types
				# List them all to ensure they match the metric defs below
				# TODO: generate from array of ops that is also to be used in the metric defs
				} elsif ( $line =~ /^(dirty_pages_hits|dirty_pages_misses|ioctl|open|close|mmap|seek|fsync|readdir|setattr|truncate|flock|getattr|create|link|unlink|symlink|mkdir|rmdir|mknod|rename|statfs|alloc_inode|setxattr|getxattr|getxattr_hits|listxattr|removexattr|inode_permission)\s+(\d+) samples [regs]$$/ ){
					my $name = $1;
                                        my $count = $2;
					$h{$mtroot}->{"lustre.llite.$name.count"} = $count;
				}
			}
			close( STATS );
		}
	}

	closedir( LLITEDIR );
}

#
# fetch is called once by pcp for each refresh and then the fetch callback is
# called to query each statistic individually
#
sub lustre_fetch {
	lustre_get_fs_stats();

	our $pmda->replace_indom($lustre_indom, \%h);
}

sub lustre_fetch_callback {
	my ($cluster, $item, $inst) = @_; 

	my $lookup = pmda_inst_lookup($lustre_indom, $inst);
	return (PM_ERR_INST, 0) unless defined($lookup);

	my $pmid_name = pmda_pmid_name($cluster, $item)
               or die "Unknown metric name: cluster $cluster item $item\n";

	return ($lookup->{$pmid_name}, 1);

}

# the PCP::PMDA->new line is parsed by the check_domain rule of the PMDA build
# process, so there are special requirements:  no comments, the domain has to
# be a bare number.
#
our $pmda = PCP::PMDA->new('lustre', 437);

# metrics go here, with full descriptions

# general - cluster 0
# llite stats - cluster 1
# lnet stats - cluster 2

$pmda->add_metric(pmda_pmid(0,1), PM_TYPE_STRING, $lustre_indom,
		PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		'lustre.llite.volume',
		'Volume Name',
		'');
$pmda->add_metric(pmda_pmid(0,2), PM_TYPE_STRING, $lustre_indom,
		PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		'lustre.llite.superblock',
		'Superblock Identifier',
		'');


my $item = 1;
for my $stat_name (@byte_stats) {

	$pmda->add_metric(pmda_pmid(1, $item++), PM_TYPE_U64, $lustre_indom,
			  PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
			  "lustre.llite.$stat_name.count",
			  'number of calls',
			  '');

	$pmda->add_metric(pmda_pmid(1, $item++), PM_TYPE_U64, $lustre_indom,
			  PM_SEM_INSTANT, pmda_units(1,0,0,PM_SPACE_BYTE,0,0),
			  "lustre.llite.$stat_name.min",
			  'minimum byte value seen in a call',
			  '');

	$pmda->add_metric(pmda_pmid(1, $item++), PM_TYPE_U64, $lustre_indom,
			  PM_SEM_INSTANT, pmda_units(1,0,0,PM_SPACE_BYTE,0,0),
			  "lustre.llite.$stat_name.max",
			  'maximum byte value seen in a call',
			  '');

	$pmda->add_metric(pmda_pmid(1, $item++), PM_TYPE_U64, $lustre_indom,
			  PM_SEM_COUNTER, pmda_units(1,0,0,PM_SPACE_BYTE,0,0),
			  "lustre.llite.$stat_name.total",
			  'total byte count',
			  '');
}

for my $stat_name (@page_stats) {

        $pmda->add_metric(pmda_pmid(1, $item++), PM_TYPE_U64, $lustre_indom,
                          PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
                          "lustre.llite.$stat_name.count",
                          'number of calls',
                          '');

        $pmda->add_metric(pmda_pmid(1, $item++), PM_TYPE_U64, $lustre_indom,
                          PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
                          "lustre.llite.$stat_name.min",
                          'minimum page value seen in a call',
                          '');

        $pmda->add_metric(pmda_pmid(1, $item++), PM_TYPE_U64, $lustre_indom,
                          PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
                          "lustre.llite.$stat_name.max",
                          'maximum page value seen in a call',
                          '');

        $pmda->add_metric(pmda_pmid(1, $item++), PM_TYPE_U64, $lustre_indom,
                          PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
                          "lustre.llite.$stat_name.total",
                          'total page count',
                          '');
}

for my $stat_name (@reg_stats) {

        $pmda->add_metric(pmda_pmid(1, $item++), PM_TYPE_U64, $lustre_indom,
                          PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
                          "lustre.llite.$stat_name.count",
                          'number of calls',
                          '');

}

&lustre_get_fs_stats;

$lustre_indom = $pmda->add_indom($lustre_indom, {}, '', '');

$pmda->set_fetch(\&lustre_fetch);
$pmda->set_fetch_callback(\&lustre_fetch_callback);

$pmda->run;
