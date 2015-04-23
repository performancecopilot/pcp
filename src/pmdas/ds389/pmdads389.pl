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
use Net::LDAP;
use POSIX;

our $server = 'localhost';
our $binddn = 'cn=Directory Manager';
our $bindpw = 'Manager12';
our $scope  = 'base';
our $cnbase = 'cn=monitor';
our $urbase = 'cn=monitor,cn=userRoot,cn=ldbm database,cn=plugins,cn=config';
our $filter = '(objectclass=*)';
our $query_interval = 2; # seconds

# Configuration files for overriding the above settings
for my $file (pmda_config('PCP_PMDAS_DIR') . '/ds389/ds389.conf', './ds389.conf') {
	eval `cat $file` unless ! -f $file;
}

use vars qw( $ldap $pmda %metrics );

# Timestamps
my $ts_cn = 0;
my $ts_ur = 0;

sub ds389_connection_setup {
	if (!defined($ldap)) {
		$pmda->log("binding to $server");
		$ldap = Net::LDAP->new($server);
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

		$metrics{'ds389.' . $prefix . $attr} = $value;
	}
}

sub ds389_fetch {
	if (!defined($ldap)) {
		ds389_connection_setup();
	}
	return unless defined($ldap);

	my ($cluster) = @_;
	my $mesg;

	if ($cluster eq 0) {
		if ((strftime("%s", localtime()) - $ts_cn) > $query_interval) {
			# $pmda->log("cn search");
			$ts_cn = strftime("%s", localtime());
			$mesg = $ldap->search(scope => $scope, base => $cnbase, filter => $filter);
			if ($mesg->code) {
				$pmda->log("search failed: " . $mesg->error);
				undef $ldap;
				return;
			}
			ds389_process_entry($mesg->entry, 'cn.', 0);
		}
	}

	if ($cluster eq 1) {
		if ((strftime("%s", localtime()) - $ts_ur) > $query_interval) {
			# $pmda->log("ur search");
			$ts_ur = strftime("%s", localtime());
			$mesg = $ldap->search(scope => $scope, base => $urbase, filter => $filter);
			if ($mesg->code) {
				$pmda->log("search failed: " . $mesg->error);
				undef $ldap;
				return;
			}
			ds389_process_entry($mesg->entry, 'userroot.', 1);
		}
	}
}

sub ds389_fetch_callback {
	my ($cluster, $item, $inst) = @_;

	if ($inst != PM_INDOM_NULL)	{ return (PM_ERR_INST, 0); }

	my $pmnm = pmda_pmid_name($cluster, $item);
	my $value = $metrics{$pmnm};

	if (!defined($value))		{ return (PM_ERR_APPVERSION, 0); }

	return ($value, 1);
}

$pmda = PCP::PMDA->new('ds389', 130);

# Metrics available on 389 DS 1.3.2.23

# cn=monitor
$pmda->add_metric(pmda_pmid(0,0), PM_TYPE_U32, PM_INDOM_NULL,
		PM_SEM_INSTANT, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		'ds389.cn.threads', '', '');
$pmda->add_metric(pmda_pmid(0,1), PM_TYPE_U32, PM_INDOM_NULL,
		PM_SEM_INSTANT, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		'ds389.cn.currentconnections', '', '');
$pmda->add_metric(pmda_pmid(0,2), PM_TYPE_U64, PM_INDOM_NULL,
		PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		'ds389.cn.totalconnections', '', '');
$pmda->add_metric(pmda_pmid(0,3), PM_TYPE_U32, PM_INDOM_NULL,
		PM_SEM_INSTANT, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		'ds389.cn.currentconnectionsatmaxthreads', '', '');
$pmda->add_metric(pmda_pmid(0,4), PM_TYPE_U32, PM_INDOM_NULL,
		PM_SEM_INSTANT, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		'ds389.cn.maxthreadsperconnhits', '', '');
$pmda->add_metric(pmda_pmid(0,5), PM_TYPE_U32, PM_INDOM_NULL,
		PM_SEM_INSTANT, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		'ds389.cn.dtablesize', '', '');
$pmda->add_metric(pmda_pmid(0,6), PM_TYPE_U32, PM_INDOM_NULL,
		PM_SEM_INSTANT, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		'ds389.cn.readwaiters', '', '');
$pmda->add_metric(pmda_pmid(0,7), PM_TYPE_U64, PM_INDOM_NULL,
		PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		'ds389.cn.opsinitiated', '', '');
$pmda->add_metric(pmda_pmid(0,8), PM_TYPE_U64, PM_INDOM_NULL,
		PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		'ds389.cn.opscompleted', '', '');
$pmda->add_metric(pmda_pmid(0,9), PM_TYPE_U64, PM_INDOM_NULL,
		PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		'ds389.cn.entriessent', '', '');
$pmda->add_metric(pmda_pmid(0,10), PM_TYPE_U64, PM_INDOM_NULL,
		PM_SEM_COUNTER, pmda_units(1,0,0,PM_SPACE_BYTE,0,0),
		'ds389.cn.bytessent', '', '');
$pmda->add_metric(pmda_pmid(0,11), PM_TYPE_U32, PM_INDOM_NULL,
		PM_SEM_COUNTER, pmda_units(0,1,0,0,PM_TIME_SEC,0),
		'ds389.cn.uptime', '', ''); # calculated

# cn=monitor,cn=userRoot,cn=ldbm database,cn=plugins,cn=config
$pmda->add_metric(pmda_pmid(1,0), PM_TYPE_U32, PM_INDOM_NULL,
		PM_SEM_DISCRETE, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		'ds389.userroot.readonly', '', '');
$pmda->add_metric(pmda_pmid(1,1), PM_TYPE_U64, PM_INDOM_NULL,
		PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		'ds389.userroot.entrycachehits', '', '');
$pmda->add_metric(pmda_pmid(1,2), PM_TYPE_U64, PM_INDOM_NULL,
		PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		'ds389.userroot.entrycachetries', '', '');
$pmda->add_metric(pmda_pmid(1,3), PM_TYPE_U32, PM_INDOM_NULL,
		PM_SEM_INSTANT, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		'ds389.userroot.entrycachehitratio', '', '');
$pmda->add_metric(pmda_pmid(1,4), PM_TYPE_U64, PM_INDOM_NULL,
		PM_SEM_INSTANT, pmda_units(1,0,0,PM_SPACE_BYTE,0,0),
		'ds389.userroot.currententrycachesize', '', '');
$pmda->add_metric(pmda_pmid(1,5), PM_TYPE_U64, PM_INDOM_NULL,
		PM_SEM_DISCRETE, pmda_units(1,0,0,PM_SPACE_BYTE,0,0),
		'ds389.userroot.maxentrycachesize', '', '');
$pmda->add_metric(pmda_pmid(1,6), PM_TYPE_U32, PM_INDOM_NULL,
		PM_SEM_INSTANT, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		'ds389.userroot.currententrycachecount', '', '');
$pmda->add_metric(pmda_pmid(1,7), PM_TYPE_32, PM_INDOM_NULL,
		PM_SEM_DISCRETE, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		'ds389.userroot.maxentrycachecount', '', '');
$pmda->add_metric(pmda_pmid(1,8), PM_TYPE_U64, PM_INDOM_NULL,
		PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		'ds389.userroot.dncachehits', '', '');
$pmda->add_metric(pmda_pmid(1,9), PM_TYPE_U64, PM_INDOM_NULL,
		PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		'ds389.userroot.dncachetries', '', '');
$pmda->add_metric(pmda_pmid(1,10), PM_TYPE_U32, PM_INDOM_NULL,
		PM_SEM_INSTANT, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		'ds389.userroot.dncachehitratio', '', '');
$pmda->add_metric(pmda_pmid(1,11), PM_TYPE_U32, PM_INDOM_NULL,
		PM_SEM_INSTANT, pmda_units(1,0,0,PM_SPACE_BYTE,0,0),
		'ds389.userroot.currentdncachesize', '', '');
$pmda->add_metric(pmda_pmid(1,12), PM_TYPE_U32, PM_INDOM_NULL,
		PM_SEM_DISCRETE, pmda_units(1,0,0,PM_SPACE_BYTE,0,0),
		'ds389.userroot.maxdncachesize', '', '');
$pmda->add_metric(pmda_pmid(1,13), PM_TYPE_U32, PM_INDOM_NULL,
		PM_SEM_INSTANT, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		'ds389.userroot.currentdncachecount', '', '');
$pmda->add_metric(pmda_pmid(1,14), PM_TYPE_32, PM_INDOM_NULL,
		PM_SEM_DISCRETE, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		'ds389.userroot.maxdncachecount', '', '');

$pmda->set_refresh(\&ds389_fetch);
$pmda->set_fetch_callback(\&ds389_fetch_callback);
$pmda->set_user('pcp');
$pmda->run;
