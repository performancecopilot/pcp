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
# Author: Marko Myllynen <myllynen@redhat.com>
# Contributors: Raul Mahiques <rmahique@redhat.com>

use strict;
use warnings;
use PCP::PMDA;
use POSIX;
use MIME::Base64;

my $have_ldap = eval {
	# check if we have LDAP
	require Net::LDAP;
	Net::LDAP->import();
	use Net::LDAP::Util;
	1;
};
if (!$have_ldap) { die("Net::LDAP unavailable on this platform"); }

# Default values
our $aname = 'ds389';
our $server = 'localhost';
# Default LDAP version to use.
our $ldapver = 3;
our $binddn = 'cn=Directory Manager';
our $bindpw = 'Manager12';
# Default scope
our $dfscope  = 'base';
# Default LDAP filter
our $dffilter = '(objectclass=*)';
# Default LDAP attributes to retrieve
our $dattrs = ['*', '+'];
# How often it will check
our $query_interval = 2; # seconds
# Metrics defaults
our $mpm_type = PM_TYPE_U32;
our $mpm_indom = PM_INDOM_NULL;
our $mpm_sem = PM_SEM_INSTANT;
#  Format:  dim_space, dim_time, dim_count, scale_space, scale_time, scale_count
our $mpmda_units = '0,0,1,0,0,'.PM_COUNT_ONE;
our @add_met = ();


# Default base and metrics
our %dataclusters = (
        '0' => ['0','0','cn=monitor','cn.',$dfscope,$dffilter,$dattrs],
        '1' => ['1','0','cn=monitor,cn=userRoot,cn=ldbm database,cn=plugins,cn=config','userroot.',$dfscope,$dffilter,$dattrs],
);
our @def_met = (
        [0,0,$mpm_type,$mpm_indom,$mpm_sem,$mpmda_units,'threads'],
        [0,1,$mpm_type,$mpm_indom,$mpm_sem,$mpmda_units,'currentconnections'],
        [0,2,PM_TYPE_U64,$mpm_indom,PM_SEM_COUNTER,$mpmda_units,'totalconnections'],
        [0,3,$mpm_type,$mpm_indom,$mpm_sem,$mpmda_units,'currentconnectionsatmaxthreads'],
        [0,4,$mpm_type,$mpm_indom,$mpm_sem,$mpmda_units,'maxthreadsperconnhits'],
        [0,5,$mpm_type,$mpm_indom,$mpm_sem,$mpmda_units,'dtablesize'],
        [0,6,$mpm_type,$mpm_indom,$mpm_sem,$mpmda_units,'readwaiters'],
        [0,7,PM_TYPE_U64,$mpm_indom,$mpm_sem,$mpmda_units,'opsinitiated'],
        [0,8,PM_TYPE_U64,$mpm_indom,$mpm_sem,$mpmda_units,'opscompleted'],
        [0,9,PM_TYPE_U64,$mpm_indom,$mpm_sem,$mpmda_units,'entriessent'],
        [0,10,PM_TYPE_U64,$mpm_indom,$mpm_sem,'1,0,0,'.PM_SPACE_BYTE.',0,0','bytessent'],
        [0,11,$mpm_type,$mpm_indom,$mpm_sem,'0,1,0,0,'.PM_TIME_SEC.',0','uptime'],
        [1,0,$mpm_type,$mpm_indom,$mpm_sem,'0,0,0,0,0,0','readonly'],
        [1,1,PM_TYPE_U64,$mpm_indom,PM_SEM_COUNTER,$mpmda_units,'entrycachehits'],
        [1,2,PM_TYPE_U64,$mpm_indom,PM_SEM_COUNTER,$mpmda_units,'entrycachetries'],
        [1,3,PM_TYPE_U64,$mpm_indom,$mpm_sem,$mpmda_units,'entrycachehitratio'],
        [1,4,PM_TYPE_U64,$mpm_indom,$mpm_sem,'1,0,0,'.PM_SPACE_BYTE.',0,0','currententrycachesize'],
        [1,5,PM_TYPE_U64,$mpm_indom,PM_SEM_DISCRETE,'1,0,0,'.PM_SPACE_BYTE.',0,0','maxentrycachesize'],
        [1,6,$mpm_type,$mpm_indom,$mpm_sem,$mpmda_units,'currententrycachecount'],
        [1,7,PM_TYPE_32,$mpm_indom,$mpm_sem,$mpmda_units,'maxentrycachecount'],
        [1,8,PM_TYPE_U64,$mpm_indom,PM_SEM_COUNTER,$mpmda_units,'dncachehits'],
        [1,9,PM_TYPE_U64,$mpm_indom,PM_SEM_COUNTER,$mpmda_units,'dncachetries'],
        [1,10,$mpm_type,$mpm_indom,$mpm_sem,$mpmda_units,'dncachehitratio'],
        [1,11,$mpm_type,$mpm_indom,$mpm_sem,'1,0,0,'.PM_SPACE_BYTE.',0,0','currentdncachesize'],
        [1,12,$mpm_type,$mpm_indom,PM_SEM_DISCRETE,'1,0,0,'.PM_SPACE_BYTE.',0,0','maxdncachesize'],
        [1,13,$mpm_type,$mpm_indom,$mpm_sem,$mpmda_units,'currentdncachecount'],
        [1,14,PM_TYPE_32,$mpm_indom,$mpm_sem,$mpmda_units,'maxdncachecount'],
        [1,15,$mpm_type,$mpm_indom,$mpm_sem,$mpmda_units,'normalizeddncachehits'],
        [1,16,PM_TYPE_32,$mpm_indom,$mpm_sem,$mpmda_units,'normalizeddncachetries'],
        [1,17,PM_TYPE_32,$mpm_indom,$mpm_sem,$mpmda_units,'normalizeddncachehitratio'],
        [1,18,PM_TYPE_32,$mpm_indom,$mpm_sem,'1,0,0,'.PM_SPACE_BYTE.',0,0','currentnormalizeddncachesize'],
        [1,19,PM_TYPE_32,$mpm_indom,$mpm_sem,'1,0,0,'.PM_SPACE_BYTE.',0,0','maxnormalizeddncachesize'],
        [1,20,PM_TYPE_32,$mpm_indom,$mpm_sem,$mpmda_units,'currentnormalizeddncachecount'],
        [1,21,PM_TYPE_32,$mpm_indom,$mpm_sem,$mpmda_units,'normalizeddncachemisses'],
);







# Configuration files for overriding the above settings
for my $file (pmda_config('PCP_PMDAS_DIR') . "/$aname/$aname.conf", "./$aname.conf") {
	eval `cat $file` unless ! -f $file;
}

use vars qw( $ldap $pmda %metrics );

sub ds389_connection_setup {
	if (!defined($ldap)) {
		$pmda->log("binding to $server");
		$ldap = Net::LDAP->new($server,version => $ldapver);
		if (!defined($ldap)) {
			$pmda->log("bind failed, server down?");
			return;
		}
		my $mesg = $ldap->bind($binddn, password => $bindpw);
		if ($mesg->code) {
			$pmda->log("bind failed: " . $mesg->error);
			return;
		}
		$pmda->log("bind to $server ok");
	}
}

# Function copied from https://github.com/389ds/389-ds-base/blob/389-ds-base-1.3.10/ldap/admin/src/scripts/repl-monitor.pl.in
sub to_decimal_csn
{
        my ($maxcsn) = @_;
        if (!$maxcsn || $maxcsn eq "" || $maxcsn eq "Unavailable") {
                return "Unavailable";
        }

        my ($tm, $seq, $masterid, $subseq) = unpack("a8 a4 a4 a4", $maxcsn);

        $tm = hex($tm);
        $seq = hex($seq);
        $masterid = hex($masterid);
        $subseq = hex($subseq);

        return "$tm $seq $masterid $subseq";
}



sub ds389_time_to_epoch {
	my ($time) = @_;
	return mktime(substr($time,12,2),
		      substr($time,10,2),
		      substr($time,8,2),
		      substr($time,6,2),
		      substr($time,4,2) - 1,
		      substr($time,0,4) - 1900);
}

sub ds389_process_entry {
	my ($entry, $prefix, $cluster) = @_;
	my $currtime;
	my $startrepltime = '';
	my $endrepltime = '';
	if ($entry && $entry->can('attributes')) {
		foreach my $attr ($entry->attributes) {
			my $value = $entry->get_value($attr);

			if ($attr eq 'currenttime') {
				$currtime = ds389_time_to_epoch($value);
				next;
			}

			if ($attr eq 'starttime') {
				my $starttime = ds389_time_to_epoch($value);
				$value = $currtime - $starttime;
				$attr = 'uptime';
			}

			if ($attr eq 'nsds5replicaLastUpdateStatus') {
	                        $value = (split /\)/, (split /Error \(/, $value)[1])[0];
	                        $attr = 'replicaLastUpdateStatus';
	                }

			if ($attr eq 'nsds5replicaChangesSentSinceStartup' ) {
				my $rep_id = (split /:/, $value)[0];
				my ($sent, $skipped) = (split /\//, (split /:/, $value)[1]);
				$attr = 'replica'.$rep_id.'ChangesSentSinceStartup';
				$metrics{"$aname." . $prefix . $attr} = $sent;
				$value = $skipped;
				$attr = 'replica'.$rep_id.'ChangesSkippedSinceStartup';
        	        }

			if ($attr eq 'nsds5replicaUpdateInProgress' ) {
				if ($value =~ /^(true|TRUE)$/) {
			        	$value = 1;
				} else {
					$value = 0;
				}
	                }

			if ($attr =~ /^(nsds5replicaLastInitEnd|nsds5replicaLastUpdateStart|nsds5replicaLastInitStart)$/i ) {
				$value = ds389_time_to_epoch($value);
				if ($attr eq 'nsds5replicaLastUpdateStart') {
					$startrepltime = $value;
				}
				$metrics{"$aname." . $prefix . $attr} = $value;
			}

			if ($endrepltime ne '' && $startrepltime ne '' ) {
				$attr = 'nsds5replicaLastUpdateTime';
				$value = $endrepltime - $startrepltime;
				$startrepltime = $endrepltime = '';

			}

			if ($attr =~ /^(nsds5replicaLastUpdateEnd)$/i ) {
				$value = ds389_time_to_epoch($value);
				$endrepltime = $value;
			}

			if ($attr =~ /^nsds5AgmtMaxCSN$/i ) {
				my $maxcsn = &to_decimal_csn((split /\;/, $value)[5]);
				$value = (split / /, $maxcsn)[0];
			}

			$metrics{"$aname." . $prefix . $attr} = $value;
		}
	}
}

sub retrieve_ldap {
        my ($cluster, $ts, $base, $tname, $scope, $filter, $lattrs) = @_;
	my $mesg;
	
	if ((strftime("%s", localtime()) - $ts) > $query_interval) {
		$ts = strftime("%s", localtime());
		$mesg = $ldap->search(scope => $scope, base => $base, filter => $filter, attrs => $lattrs);
		if ($mesg->code) {
			$pmda->log("search(scope: \"$scope\", base: \"$base\", filter: \"$filter\", attrs: \"". join(' ', @$lattrs) ."\") failed: " . $mesg->error);
			undef $ldap;
			return;
		}
		ds389_process_entry($mesg->entry, $tname, $cluster);
	} 
}

sub ds389_fetch {
	if (!defined($ldap)) {
		ds389_connection_setup();
	}
	return unless defined($ldap);

	my ($cluster) = @_;
	my $mesg;

        \&retrieve_ldap($dataclusters{$cluster}[0],$dataclusters{$cluster}[1],$dataclusters{$cluster}[2],$dataclusters{$cluster}[3],$dataclusters{$cluster}[4],$dataclusters{$cluster}[5], $dataclusters{$cluster}[6]);

}

sub ds389_fetch_callback {
	my ($cluster, $item, $inst) = @_;

	if (!defined($ldap))		{ return (PM_ERR_AGAIN, 0); }
	if ($inst != PM_INDOM_NULL)	{ return (PM_ERR_INST, 0); }

	my $pmnm = pmda_pmid_name($cluster, $item);
	my $value = $metrics{$pmnm};

	if (!defined($value))		{ return (PM_ERR_APPVERSION, 0); }

	return ($value, 1);
}

$pmda = PCP::PMDA->new($aname, 130);

# Add default metrics
while  (my ($i, @met) = each @def_met) {
  if (defined($dataclusters{$def_met[$i][0]})) {
    $pmda->add_metric(pmda_pmid($def_met[$i][0],$def_met[$i][1]), $def_met[$i][2], $def_met[$i][3],$def_met[$i][4], pmda_units(split(',',$def_met[$i][5])),"$aname.$dataclusters{$def_met[$i][0]}[3]$def_met[$i][6]", '', '');
  }
};


# Add metrics from the configuration file
while  (my ($i, @met) = each @add_met) {
	if (defined($dataclusters{$add_met[$i][0]})) {
		$pmda->add_metric(pmda_pmid($add_met[$i][0],$add_met[$i][1]), $add_met[$i][2], $add_met[$i][3],$add_met[$i][4], pmda_units(split(',',$add_met[$i][5])),"$aname.$dataclusters{$add_met[$i][0]}[3]$add_met[$i][6]", '', '');
	}
};

$pmda->set_refresh(\&ds389_fetch);
$pmda->set_fetch_callback(\&ds389_fetch_callback);
$pmda->set_user('pcp');
$pmda->run;
