#
# Copyright (c) 2012-2013 Red Hat.
# Copyright (c) 2011 Aconex.  All Rights Reserved.
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

my $pmda = PCP::PMDA->new('rsyslog', 107);
my $statsfile = pmda_config('PCP_LOG_DIR') . '/rsyslog/stats';
my ($es_connfail, $es_submits, $es_failed, $es_success) = (0,0,0,0);
my ($es_failed_http, $es_failed_httprequests, $es_failed_es) = (0,0,0);
my ($es_response_bad, $es_response_duplicate, $es_response_badargument, $es_response_bulkrejection, $es_response_other) = (0,0,0,0,0);
my $es_rebinds = 0;
my ($ux_submitted, $ux_discarded, $ux_ratelimiters) = (0,0,0);
my ($interval, $lasttime) = (0,0);
my ($unrecog, $ignored) = (0,0);
my ($ru_utime, $ru_stime, $ru_maxrss, $ru_minflt, $ru_majflt, $ru_inblock, $ru_oublock, $ru_nvcsw, $ru_nivcsw, $ru_openfiles) = (0,0,0,0,0,0,0,0,0,0);

my $queue_indom = 0;
my @queue_insts = ();
use vars qw(%queue_ids %queue_values);

my $unrec_origin_indom = 1;
my @unrec_origin_insts = ();
use vars qw(%unrec_origin_ids %unrec_origin_values);

my $unmat_origin_indom = 2;
my @unmat_origin_insts = ();
use vars qw(%unmat_origin_ids %unmat_origin_values);

my $action_indom = 3;
my @action_insts = ();
use vars qw(%action_ids %action_values);

my $udp_indom = 4;
my @udp_insts = ();
use vars qw(%udp_ids %udp_values);

my $udpt_indom = 5;
my @udpt_insts = ();
use vars qw(%udpt_ids %udpt_values);

my $ptcp_indom = 6;
my @ptcp_insts = ();
use vars qw(%ptcp_ids %ptcp_values);
my ($ptcp_iowq_enqueued, $ptcp_iowq_maxqsize) = (0,0);

my $fwd_indom = 7;
my @fwd_insts = ();
use vars qw(%fwd_ids %fwd_values);

sub unmatched_origin
{
    my $origin = $1;
    my $oid = undef;

    if (!defined($unmat_origin_ids{$origin})) {
	$oid = @unmat_origin_insts / 2;
	$unmat_origin_ids{$origin} = $oid;
	$unmat_origin_values{$origin} = [ 0 ];
	push @unmat_origin_insts, ($oid, $origin);
	$pmda->replace_indom($unmat_origin_indom, \@unmat_origin_insts);
    }
    $unmat_origin_values{$origin}[0]++;
}

sub rsyslog_parser
{
    ( undef, $_ ) = @_;

    #$pmda->log("rsyslog_parser got line: $_");
    if (! m|rsyslogd-pstats:|) {
	# Not a log entry we can process
	$ignored++;
	return;
    }

    my $timenow = time;
    if ($lasttime != 0) {
	if ($timenow > $lasttime) {
	    $interval = $timenow - $lasttime;
	    $lasttime = $timenow;
	}
    } else {
	$lasttime = $timenow;
    }

    if (m| origin=(.*?) |) {
	# "Modern" statistics
	my $origin = $1;

	if ($origin eq "imuxsock") {
	    if (m| submitted=(\d+) ratelimit\.discarded=(\d+) ratelimit\.numratelimiters=(\d+)|) {
		# Modern capture of the imuxsock action data
		($ux_submitted, $ux_discarded, $ux_ratelimiters) = ($1,$2,$3);
	    }
	    else {
		unmatched_origin($origin);
	    }
	}
	elsif ($origin eq "omelasticsearch") {
	    if (m| submitted=(\d+) failed\.http=(\d+) failed\.httprequests=(\d+) failed\.checkConn=(\d+) failed\.es=(\d+) response\.success=(\d+) response\.bad=(\d+) response\.duplicate=(\d+) response\.badargument=(\d+) response\.bulkrejection=(\d+) response\.other=(\d+) rebinds=(\d+)|) {
		# Modern capture of the omelasticsearch action data
		my $submitted = $1;
		my $failed_http = $2;
		my $failed_httprequests = $3;
		my $failed_checkConn = $4;
		my $failed_es = $5;
		my $response_success = $6;
		my $response_bad = $7;
		my $response_duplicate = $8;
		my $response_badargument = $9;
		my $response_bulkrejection = $10;
		my $response_other = $11;
		my $rebinds = $12;
		($es_connfail, $es_submits, $es_failed, $es_success) = (
		    $failed_checkConn, $submitted, ($failed_http + $failed_httprequests + $failed_es + $response_bad + $response_duplicate + $response_badargument + $response_bulkrejection + $response_other), $response_success
		);
		# New
		($es_failed_http, $es_failed_httprequests, $es_failed_es) = ($failed_http, $failed_httprequests, $failed_es);
		($es_response_bad, $es_response_duplicate, $es_response_badargument, $es_response_bulkrejection, $es_response_other) = (
		    $response_bad, $response_duplicate, $response_badargument, $response_bulkrejection, $response_other
		);
		$es_rebinds = $rebinds;
	    }
	    elsif (m| submitted=(\d+) failed\.http=(\d+) failed\.httprequests=(\d+) failed\.checkConn=(\d+) failed\.es=(\d+) response\.success=(\d+) response\.bad=(\d+) response\.duplicate=(\d+) response\.badargument=(\d+) response\.bulkrejection=(\d+) response\.other=(\d+)|) {
		# Modern capture of the omelasticsearch action data
		my $submitted = $1;
		my $failed_http = $2;
		my $failed_httprequests = $3;
		my $failed_checkConn = $4;
		my $failed_es = $5;
		my $response_success = $6;
		my $response_bad = $7;
		my $response_duplicate = $8;
		my $response_badargument = $9;
		my $response_bulkrejection = $10;
		my $response_other = $11;
		($es_connfail, $es_submits, $es_failed, $es_success) = (
		    $failed_checkConn, $submitted, ($failed_http + $failed_httprequests + $failed_es + $response_bad + $response_duplicate + $response_badargument + $response_bulkrejection + $response_other), $response_success
		);
		# New
		($es_failed_http, $es_failed_httprequests, $es_failed_es) = ($failed_http, $failed_httprequests, $failed_es);
		($es_response_bad, $es_response_duplicate, $es_response_badargument, $es_response_bulkrejection, $es_response_other) = (
		    $response_bad, $response_duplicate, $response_badargument, $response_bulkrejection, $response_other
		);
	    }
	    elsif (m| submitted=(\d+) failed\.http=(\d+) failed\.httprequests=(\d+) failed\.checkConn=(\d+) failed\.es=(\d+)|) {
		# Psuedo-modern capture of the omelasticsearch action data
		my $submitted = $1;
		my $failed_http = $2;
		my $failed_httprequests = $3;
		my $failed_checkConn = $4;
		my $failed_es = $5;
		($es_connfail, $es_submits, $es_failed, $es_success) = ($failed_checkConn, $submitted, ($failed_http + $failed_httprequests + $failed_es), 0);
		# New
		($es_failed_http, $es_failed_httprequests, $es_failed_es) = ($failed_http, $failed_httprequests, $failed_es);
	    }
	    else {
		unmatched_origin($origin);
	    }
	}
	elsif ($origin eq "impstats") {
	    if (m| utime=(\d+) stime=(\d+) maxrss=(\d+) minflt=(\d+) majflt=(\d+) inblock=(\d+) oublock=(\d+) nvcsw=(\d+) nivcsw=(\d+) openfiles=(\d+)|) {
		($ru_utime, $ru_stime, $ru_maxrss, $ru_minflt, $ru_majflt, $ru_inblock, $ru_oublock, $ru_nvcsw, $ru_nivcsw, $ru_openfiles) = ($1,$2,$3,$4,$5,$6,$7,$8,$9,$10);
	    }
	    elsif (m| utime=(\d+) stime=(\d+) maxrss=(\d+) minflt=(\d+) majflt=(\d+) inblock=(\d+) oublock=(\d+) nvcsw=(\d+) nivcsw=(\d+)|) {
		($ru_utime, $ru_stime, $ru_maxrss, $ru_minflt, $ru_majflt, $ru_inblock, $ru_oublock, $ru_nvcsw, $ru_nivcsw, $ru_openfiles) = ($1,$2,$3,$4,$5,$6,$7,$8,$9,0);
	    }
	    else {
		unmatched_origin($origin);
	    }
	}
	elsif ($origin eq "core.queue") {
	    if (m|pstats: (.+): origin=core\.queue size=(\d+) enqueued=(\d+) full=(\d+) discarded\.full=(\d+) discarded\.nf=(\d+) maxqsize=(\d+)|) {
		# Modern capture of queue data
		my ($qname, $qid) = ($1, undef);

		if (!defined($queue_ids{$qname})) {
		    $qid = @queue_insts / 2;
		    $queue_ids{$qname} = $qid;
		    push @queue_insts, ($qid, $qname);
		    $pmda->replace_indom($queue_indom, \@queue_insts);
		}
		$queue_values{$qname} = [ $2, $3, $4, $7, $5, $6 ];
	    }
	    else {
		unmatched_origin($origin);
	    }
	}
	elsif ($origin eq "core.action") {
	    if (m|pstats: (.+): origin=core\.action processed=(\d+) failed=(\d+) suspended=(\d+) suspended\.duration=(\d+) resumed=(\d+)|) {
		# Modern capture of action data
		my ($aname, $aid) = ($1, undef);

		if (!defined($action_ids{$aname})) {
		    $aid = @action_insts / 2;
		    $action_ids{$aname} = $aid;
		    push @action_insts, ($aid, $aname);
		    $pmda->replace_indom($action_indom, \@action_insts);
		}
		$action_values{$aname} = [ $2, $3, $4, $5, $6 ];
	    }
	    else {
		unmatched_origin($origin);
	    }
	}
	elsif ($origin eq "imudp") {
	    if (m|pstats: imudp\((.+)\): origin=imudp submitted=(\d+) disallowed=(\d+)|) {
		# Modern capture of imudp data
		my ($udpname, $udpid) = ($1, undef);

		if (!defined($udp_ids{$udpname})) {
		    $udpid = @udp_insts / 2;
		    $udp_ids{$udpname} = $udpid;
		    push @udp_insts, ($udpid, $udpname);
		    $pmda->replace_indom($udp_indom, \@udp_insts);
		}
		$udp_values{$udpname} = [ $2, $3 ];
	    }
	    elsif (m|pstats: imudp\((.+)\): origin=imudp submitted=(\d+)|) {
		# Modern capture of imudp data
		my ($udpname, $udpid) = ($1, undef);

		if (!defined($udp_ids{$udpname})) {
		    $udpid = @udp_insts / 2;
		    $udp_ids{$udpname} = $udpid;
		    push @udp_insts, ($udpid, $udpname);
		    $pmda->replace_indom($udp_indom, \@udp_insts);
		}
		$udp_values{$udpname} = [ $2, 0 ];
	    }
	    elsif (m|pstats: imudp\((.+)\): origin=imudp called\.recvmmsg=(\d+) called\.recvmsg=(\d+) msgs\.received=(\d+)|) {
		# Modern capture of imudp *thread* data
		my ($utname, $utid) = ($1, undef);

		if (!defined($udpt_ids{$utname})) {
		    $utid = @udpt_insts / 2;
		    $udpt_ids{$utname} = $utid;
		    push @udpt_insts, ($utid, $utname);
		    $pmda->replace_indom($udpt_indom, \@udpt_insts);
		}
		$udpt_values{$utname} = [ $2, $3, $4 ];
	    }
	    else {
		unmatched_origin($origin);
	    }
	}
	elsif ($origin eq "imptcp") {
	    if (m|pstats: imptcp\((.+)\): origin=imptcp submitted=(\d+) sessions\.opened=(\d+) sessions\.openfailed=(\d+) sessions\.closed=(\d+) bytes\.received=(\d+) bytes\.decompressed=(\d+)|) {
		# Modern capture of imptcp data
		my ($ptcpname, $ptcpid) = ($1, undef);

		if (!defined($ptcp_ids{$ptcpname})) {
		    $ptcpid = @ptcp_insts / 2;
		    $ptcp_ids{$ptcpname} = $ptcpid;
		    push @ptcp_insts, ($ptcpid, $ptcpname);
		    $pmda->replace_indom($ptcp_indom, \@ptcp_insts);
		}
		$ptcp_values{$ptcpname} = [ $2, $3, $4, $5, $6, $7 ];
	    }
	    elsif (m|pstats: imptcp\((.+)\): origin=imptcp submitted=(\d+) bytes\.received=(\d+) bytes\.decompressed=(\d+)|) {
		# Modern capture of imptcp data
		my ($ptcpname, $ptcpid) = ($1, undef);

		if (!defined($ptcp_ids{$ptcpname})) {
		    $ptcpid = @ptcp_insts / 2;
		    $ptcp_ids{$ptcpname} = $ptcpid;
		    push @ptcp_insts, ($ptcpid, $ptcpname);
		    $pmda->replace_indom($ptcp_indom, \@ptcp_insts);
		}
		$ptcp_values{$ptcpname} = [ $2, 0, 0, 0, $3, $4 ];
	    }
	    elsif (m|pstats: io-work-q: origin=imptcp enqueued=(\d+) maxqsize=(\d+)|) {
		($ptcp_iowq_enqueued, $ptcp_iowq_maxqsize) = ($1, $2);
	    }
	    else {
		unmatched_origin($origin);
	    }
	}
	elsif ($origin eq "omfwd") {
	    if (m|pstats: (.+): origin=omfwd bytes\.sent=(\d+)|) {
		# Modern capture of omfwd data
		my ($fwdname, $fwdid) = ($1, undef);

		if (!defined($fwd_ids{$fwdname})) {
		    $fwdid = @fwd_insts / 2;
		    $fwd_ids{$fwdname} = $fwdid;
		    push @fwd_insts, ($fwdid, $fwdname);
		    $pmda->replace_indom($fwd_indom, \@fwd_insts);
		}
		$fwd_values{$fwdname} = [ $2 ];
	    }
	    else {
		unmatched_origin($origin);
	    }
	}
	else {
	    # Count unrecognized origin values.
	    my $oid = undef;

	    if (!defined($unrec_origin_ids{$origin})) {
		$oid = @unrec_origin_insts / 2;
		$unrec_origin_ids{$origin} = $oid;
		$unrec_origin_values{$origin} = [ 0 ];
		push @unrec_origin_insts, ($oid, $origin);
		$pmda->replace_indom($unrec_origin_indom, \@unrec_origin_insts);
	    }
	    $unrec_origin_values{$origin}[0]++;
	}
    }
    elsif (m|imuxsock: submitted=(\d+) ratelimit\.discarded=(\d+) ratelimit\.numratelimiters=(\d+)|) {
	# Legacy capture of the imuxsock action data
	($ux_submitted, $ux_discarded, $ux_ratelimiters) = ($1,$2,$3);
    }
    elsif (m|elasticsearch: connfail=(\d+) submits=(\d+) failed=(\d+) success=(\d+)|) {
	# Legacy capture of the omelasticsearch action data
	($es_connfail, $es_submits, $es_failed, $es_success) = ($1,$2,$3,$4);
    }
    elsif (m|pstats: (.+): size=(\d+) enqueued=(\d+) full=(\d+) maxqsize=(\d+)|) {
	# Legacy capture of queue data
	my ($qname, $qid) = ($1, undef);

	if (!defined($queue_ids{$qname})) {
	    $qid = @queue_insts / 2;
	    $queue_ids{$qname} = $qid;
	    push @queue_insts, ($qid, $qname);
	    $pmda->replace_indom($queue_indom, \@queue_insts);
	}
	$queue_values{$qname} = [ $2, $3, $4, $5, 0, 0 ];
    }
    else {
	# Count of unrecognized rsyslog-pstats log lines.
	$unrecog++;
    }
}

sub rsyslog_fetch_callback
{
    my ($cluster, $item, $inst) = @_;

    #$pmda->log("rsyslog_fetch_callback for PMID: $cluster.$item ($inst)");

    return (PM_ERR_AGAIN,0) unless ($interval != 0);

    if ($cluster == 0) {
	return (PM_ERR_INST, 0) unless ($inst == PM_IN_NULL);
	if ($item == 0) { return ($interval, 1); }

	if ($item == 1) { return ($ux_submitted, 1); }
	if ($item == 2) { return ($ux_discarded, 1); }
	if ($item == 3) { return ($ux_ratelimiters, 1); }

	if ($item == 8) { return ($es_connfail, 1); }
	if ($item == 9) { return ($es_submits, 1); }
	if ($item == 10){ return ($es_failed, 1); }
	if ($item == 11){ return ($es_success, 1); }

	if ($item == 12){ return ($unrecog, 1); }
	if ($item == 13){ return ($ignored, 1); }

	if ($item == 14){ return ($es_failed_http, 1); }
	if ($item == 15){ return ($es_failed_httprequests, 1); }
	if ($item == 16){ return ($es_failed_es, 1); }
	if ($item == 17){ return ($es_response_bad, 1); }
	if ($item == 18){ return ($es_response_duplicate, 1); }
	if ($item == 19){ return ($es_response_badargument, 1); }
	if ($item == 20){ return ($es_response_bulkrejection, 1); }
	if ($item == 21){ return ($es_response_other, 1); }
	if ($item == 22){ return ($es_rebinds, 1); }

	if ($item == 23){ return ($ru_utime, 1); }
	if ($item == 24){ return ($ru_stime, 1); }
	if ($item == 25){ return ($ru_maxrss, 1); }
	if ($item == 26){ return ($ru_minflt, 1); }
	if ($item == 27){ return ($ru_majflt, 1); }
	if ($item == 28){ return ($ru_inblock, 1); }
	if ($item == 29){ return ($ru_oublock, 1); }
	if ($item == 30){ return ($ru_nvcsw, 1); }
	if ($item == 31){ return ($ru_nivcsw, 1); }
	if ($item == 32){ return ($ru_openfiles, 1); }

	if ($item == 33){ return ($ptcp_iowq_enqueued, 1); }
	if ($item == 34){ return ($ptcp_iowq_maxqsize, 1); }
    }
    elsif ($cluster == 1) {	# queues
	return (PM_ERR_INST, 0) unless ($inst != PM_IN_NULL);
	return (PM_ERR_INST, 0) unless ($inst <= @queue_insts);

	my $qname = $queue_insts[$inst * 2 + 1];
	return (PM_ERR_INST, 0) unless defined ($qname);

	my $qvref = $queue_values{$qname};
	return (PM_ERR_INST, 0) unless defined ($qvref);

	my @qvals = @$qvref;
	if ($item == 0) { return ($qvals[0], 1); }
	if ($item == 1) { return ($qvals[1], 1); }
	if ($item == 2) { return ($qvals[2], 1); }
	if ($item == 3) { return ($qvals[3], 1); }
	if ($item == 4) { return ($qvals[4], 1); }
	if ($item == 5) { return ($qvals[5], 1); }
    }
    elsif ($cluster == 2) {	# unrecognized origins
	return (PM_ERR_INST, 0) unless ($inst != PM_IN_NULL);
	return (PM_ERR_INST, 0) unless ($inst <= @unrec_origin_insts);

	my $unoname = $unrec_origin_insts[$inst * 2 + 1];
	return (PM_ERR_INST, 0) unless defined ($unoname);

	my $unovref = $unrec_origin_values{$unoname};
	return (PM_ERR_INST, 0) unless defined ($unovref);

	my @unovals = @$unovref;
	if ($item == 0) { return ($unovals[0], 1); }
    }
    elsif ($cluster == 3) {	# unmatched origins
	return (PM_ERR_INST, 0) unless ($inst != PM_IN_NULL);
	return (PM_ERR_INST, 0) unless ($inst <= @unmat_origin_insts);

	my $umoname = $unmat_origin_insts[$inst * 2 + 1];
	return (PM_ERR_INST, 0) unless defined ($umoname);

	my $umovref = $unmat_origin_values{$umoname};
	return (PM_ERR_INST, 0) unless defined ($umovref);

	my @umovals = @$umovref;
	if ($item == 0) { return ($umovals[0], 1); }
    }
    elsif ($cluster == 4) {	# actions
	return (PM_ERR_INST, 0) unless ($inst != PM_IN_NULL);
	return (PM_ERR_INST, 0) unless ($inst <= @action_insts);

	my $aname = $action_insts[$inst * 2 + 1];
	return (PM_ERR_INST, 0) unless defined ($aname);

	my $avref = $action_values{$aname};
	return (PM_ERR_INST, 0) unless defined ($avref);

	my @avals = @$avref;
	if ($item == 0) { return ($avals[0], 1); }
	if ($item == 1) { return ($avals[1], 1); }
	if ($item == 2) { return ($avals[2], 1); }
	if ($item == 3) { return ($avals[3], 1); }
	if ($item == 4) { return ($avals[4], 1); }
    }
    elsif ($cluster == 5) {	# udp
	return (PM_ERR_INST, 0) unless ($inst != PM_IN_NULL);
	return (PM_ERR_INST, 0) unless ($inst <= @udp_insts);

	my $uname = $udp_insts[$inst * 2 + 1];
	return (PM_ERR_INST, 0) unless defined ($uname);

	my $uvref = $udp_values{$uname};
	return (PM_ERR_INST, 0) unless defined ($uvref);

	my @uvals = @$uvref;
	if ($item == 0) { return ($uvals[0], 1); }
	if ($item == 1) { return ($uvals[1], 1); }
    }
    elsif ($cluster == 6) {	# udp.thread
	return (PM_ERR_INST, 0) unless ($inst != PM_IN_NULL);
	return (PM_ERR_INST, 0) unless ($inst <= @udpt_insts);

	my $utname = $udpt_insts[$inst * 2 + 1];
	return (PM_ERR_INST, 0) unless defined ($utname);

	my $utvref = $udpt_values{$utname};
	return (PM_ERR_INST, 0) unless defined ($utvref);

	my @utvals = @$utvref;
	if ($item == 0) { return ($utvals[0], 1); }
	if ($item == 1) { return ($utvals[1], 1); }
	if ($item == 2) { return ($utvals[2], 1); }
    }
    elsif ($cluster == 7) {	# ptcp
	return (PM_ERR_INST, 0) unless ($inst != PM_IN_NULL);
	return (PM_ERR_INST, 0) unless ($inst <= @ptcp_insts);

	my $pname = $ptcp_insts[$inst * 2 + 1];
	return (PM_ERR_INST, 0) unless defined ($pname);

	my $pvref = $ptcp_values{$pname};
	return (PM_ERR_INST, 0) unless defined ($pvref);

	my @pvals = @$pvref;
	if ($item == 0) { return ($pvals[0], 1); }
	if ($item == 1) { return ($pvals[1], 1); }
	if ($item == 2) { return ($pvals[2], 1); }
	if ($item == 3) { return ($pvals[3], 1); }
	if ($item == 4) { return ($pvals[4], 1); }
	if ($item == 5) { return ($pvals[5], 1); }
    }
    elsif ($cluster == 8) {	# fwd
	return (PM_ERR_INST, 0) unless ($inst != PM_IN_NULL);
	return (PM_ERR_INST, 0) unless ($inst <= @fwd_insts);

	my $fname = $fwd_insts[$inst * 2 + 1];
	return (PM_ERR_INST, 0) unless defined ($fname);

	my $fvref = $fwd_values{$fname};
	return (PM_ERR_INST, 0) unless defined ($fvref);

	my @fvals = @$fvref;
	if ($item == 0) { return ($fvals[0], 1); }
    }
    return (PM_ERR_PMID, 0);
}

die "Cannot find a valid rsyslog statistics named pipe: " . $statsfile . "\n" unless -p $statsfile;

$pmda->connect_pmcd;

$pmda->add_metric(pmda_pmid(0,0), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT,
	pmda_units(0,1,0,0,PM_TIME_SEC,PM_COUNT_ONE), 'rsyslog.interval',
	'Time interval observed between samples', '');

$pmda->add_metric(pmda_pmid(0,1), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	pmda_units(0,0,1,0,0,PM_COUNT_ONE), 'rsyslog.imuxsock.submitted',
	'Cumulative count of unix domain socket input messages queued',
	"Cumulative count of messages successfully queued to the rsyslog\n" .
	"main message queueing core that arrived on unix domain sockets.");
$pmda->add_metric(pmda_pmid(0,2), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	pmda_units(0,0,1,0,0,PM_COUNT_ONE), 'rsyslog.imuxsock.discarded',
	'Count of unix domain socket messages discarded due to rate limiting',
	"Cumulative count of messages that are were discarded due to their\n" .
	"priority being at or below rate-limit-severity and their sending\n" .
	"process being deemed to be sending messages too quickly (refer to\n" .
	"parameters ratelimitburst, ratelimitinterval and ratelimitseverity");
$pmda->add_metric(pmda_pmid(0,3), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	pmda_units(0,0,1,0,0,PM_COUNT_ONE), 'rsyslog.imuxsock.numratelimiters',
	'Count of messages received that could be subject to rate limiting',
	"Cumulative count of messages that rsyslog received and performed a\n" .
	"credentials (PID) lookup for subsequent rate limiting decisions.\n" .
	"The message would have to be at rate-limit-severity or lower, with\n" .
	"rate limiting enabled, in order for this count to be incremented.");

$pmda->add_metric(pmda_pmid(0,8), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	pmda_units(0,0,1,0,0,PM_COUNT_ONE), 'rsyslog.elasticsearch.connfail',
	'Count of failed connections while attempting to send events',
	"Number of times rsyslog detected a connection loss via a HTTP health" .
	"check (either the 'connfail' or 'failed.checkConn' metric).");
$pmda->add_metric(pmda_pmid(0,9), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	pmda_units(0,0,1,0,0,PM_COUNT_ONE), 'rsyslog.elasticsearch.submits',
	'Count of valid submissions of events to elasticsearch',
	"Number of messages submitted for processing (with both success and\n" .
	" error result; either the 'submits' or 'submitted' metric).");
$pmda->add_metric(pmda_pmid(0,10), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	pmda_units(0,0,1,0,0,PM_COUNT_ONE), 'rsyslog.elasticsearch.failed',
	'Count of failed attempts to send events to elasticsearch',
	"This count is often a good indicator of malformed JSON messages\n" .
	"(is either the 'failed' metric, or a combination of the 'failed.es',\n" .
	"'failed.http', 'failed.httprequests', 'response.bad', 'response.duplicate',\n" .
	"'response.badargument', 'response.bulkrejection', and 'response.other'\n" .
	"metrics).");
$pmda->add_metric(pmda_pmid(0,11), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	pmda_units(0,0,1,0,0,PM_COUNT_ONE), 'rsyslog.elasticsearch.success',
	'Count of successfully acknowledged events from elasticsearch',
	"Number of records successfully sent in bulk index requests - counts\n" .
	"the number of successful responses.  Note that this reflects the value\n" .
	"of the 'success' metric, or the 'response.success', when present.");

$pmda->add_metric(pmda_pmid(0,12), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	pmda_units(0,0,1,0,0,PM_COUNT_ONE), 'rsyslog.impstats.ignored',
	'Count of impstats log lines ignored', '');
$pmda->add_metric(pmda_pmid(0,13), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	pmda_units(0,0,1,0,0,PM_COUNT_ONE), 'rsyslog.impstats.unrecog',
	'Count of unrecognized impstas log lines', '');

$pmda->add_metric(pmda_pmid(0,14), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	pmda_units(0,0,1,0,0,PM_COUNT_ONE), 'rsyslog.elasticsearch.failure.http',
	'Count of message failures due to connection like-problems', "");
$pmda->add_metric(pmda_pmid(0,15), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	pmda_units(0,0,1,0,0,PM_COUNT_ONE), 'rsyslog.elasticsearch.failure.httprequests',
	'Count of http request failures',
	"The count of http request failures. Note that a single http request\n" .
	"may be used to submit multiple messages, so this number may be (much)\n" .
	"lower than failed.http.");
$pmda->add_metric(pmda_pmid(0,16), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	pmda_units(0,0,1,0,0,PM_COUNT_ONE), 'rsyslog.elasticsearch.failure.es',
	'Count of failures due to Elasticsearch error replies',
	"The count of failures due to error responses from Elasticsearch. Note\n" .
	"that this counter does NOT count the number of failed messages but\n" .
	"the number of times a failure occurred (a potentially much smaller\n" .
	"number).");
$pmda->add_metric(pmda_pmid(0,17), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	pmda_units(0,0,1,0,0,PM_COUNT_ONE), 'rsyslog.elasticsearch.response.bad',
	'Count of unrecognized responses from Elasticsearch',
	"The count of times omelasticsearch received a response in a bulk index\n" .
	"response that was unrecognized or unable to be parsed. This may indicate\n" .
	"that omelasticsearch is attempting to communicate with a version of\n" .
	"Elasticsearch that is incompatible, or is otherwise sending back data\n" .
	"in the response that cannot be handled.");
$pmda->add_metric(pmda_pmid(0,18), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	pmda_units(0,0,1,0,0,PM_COUNT_ONE), 'rsyslog.elasticsearch.response.duplicate',
	'Count of duplicate records detected by Elasticsearch',
	"The count of records in the bulk index request that were duplicates of\n" .
	"already existing records - this will only be reported if using\n" .
	"writeoperation='create' and 'bulkid' to assign each record a unique ID");
$pmda->add_metric(pmda_pmid(0,19), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	pmda_units(0,0,1,0,0,PM_COUNT_ONE), 'rsyslog.elasticsearch.response.badargument',
	'Count of bad data sent by omelasticsearch',
	"The count of times omelasticsearch received a response that had a\n" .
	"status indicating omelasticsearch sent bad data to Elasticsearch. For\n" .
	"example, status 400 and an error message indicating omelasticsearch\n" .
	"attempted to store a non-numeric string value in a numeric field.");
$pmda->add_metric(pmda_pmid(0,20), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	pmda_units(0,0,1,0,0,PM_COUNT_ONE), 'rsyslog.elasticsearch.response.bulkrejection',
	'Count of bulk indexing requests rejected',
	"The count of times omelasticsearch received a response that had a\n" .
	"status indicating Elasticsearch was unable to process the record at\n" .
	"this time - status 429.");
$pmda->add_metric(pmda_pmid(0,21), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	pmda_units(0,0,1,0,0,PM_COUNT_ONE), 'rsyslog.elasticsearch.response.other',
	'Count of unexpected HTTP status codes encountered',
	"The count of times omelasticsearch received a response not recognized\n" .
	"as one of the expected responses, typically some other 4xx or 5xx http\n" .
	"status.");
$pmda->add_metric(pmda_pmid(0,22), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	pmda_units(0,0,1,0,0,PM_COUNT_ONE), 'rsyslog.elasticsearch.rebinds',
	'Count of reconnections made to Elasticsearch',
	"If using 'rebindinterval' this will be the count of times\n" .
	"omelasticsearch has reconnected to Elasticsearch");

$pmda->add_metric(pmda_pmid(0,23), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	pmda_units(0,1,0,0,PM_TIME_USEC,PM_COUNT_ONE), 'rsyslog.resources.utime',
	'Total time rsyslog spent in user mode (microseconds)','');
$pmda->add_metric(pmda_pmid(0,24), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	pmda_units(0,1,0,0,PM_TIME_USEC,PM_COUNT_ONE), 'rsyslog.resources.stime',
	'Total time rsyslog spent in system mode (microseconds)','');
$pmda->add_metric(pmda_pmid(0,25), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT,
	pmda_units(0,0,1,0,0,PM_COUNT_ONE), 'rsyslog.resources.maxrss',
	'Maximum resident set size used by rsyslog (kilobytes)','');
$pmda->add_metric(pmda_pmid(0,26), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	pmda_units(0,0,1,0,0,PM_COUNT_ONE), 'rsyslog.resources.minflt',
	'Page reclaims (soft page faults)','');
$pmda->add_metric(pmda_pmid(0,27), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	pmda_units(0,0,1,0,0,PM_COUNT_ONE), 'rsyslog.resources.majflt',
	'Page faults (hard page faults)','');
$pmda->add_metric(pmda_pmid(0,28), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	pmda_units(0,0,1,0,0,PM_COUNT_ONE), 'rsyslog.resources.inblock',
	'Block input operations','');
$pmda->add_metric(pmda_pmid(0,29), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	pmda_units(0,0,1,0,0,PM_COUNT_ONE), 'rsyslog.resources.oublock',
	'Block output operations','');
$pmda->add_metric(pmda_pmid(0,30), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	pmda_units(0,0,1,0,0,PM_COUNT_ONE), 'rsyslog.resources.nvcsw',
	'Voluntary context switches','');
$pmda->add_metric(pmda_pmid(0,31), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	pmda_units(0,0,1,0,0,PM_COUNT_ONE), 'rsyslog.resources.nivcsw',
	'Involuntary context switches','');
$pmda->add_metric(pmda_pmid(0,32), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT,
	pmda_units(0,0,1,0,0,PM_COUNT_ONE), 'rsyslog.resources.openfiles',
	'Number of files rsyslog has open','');

$pmda->add_metric(pmda_pmid(0,33), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT,
	pmda_units(0,0,1,0,0,PM_COUNT_ONE), 'rsyslog.ptcp.ioworkq.enqueued',
	'Current # of TCP log entries queued to the io-work-q','');
$pmda->add_metric(pmda_pmid(0,34), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT,
	pmda_units(0,0,1,0,0,PM_COUNT_ONE), 'rsyslog.ptcp.ioworkq.maxqsize',
	'Maximum size reached for the io-work-q','');

$pmda->add_metric(pmda_pmid(1,0), PM_TYPE_U64, $queue_indom, PM_SEM_INSTANT,
	pmda_units(0,0,1,0,0,0), 'rsyslog.queues.size',
	'Current queue depth for each rsyslog queue',
	"As messages arrive they are enqueued to the main message queue\n" .
	"(for example) -this counter is incremented for each such message.");
$pmda->add_metric(pmda_pmid(1,1), PM_TYPE_U64, $queue_indom, PM_SEM_COUNTER,
	pmda_units(0,0,1,0,0,0), 'rsyslog.queues.enqueued',
	'Cumulative count of nessages enqueued to individual queues',
	"As messages arrive they are added to the main message processing\n" .
	"queue, either individually or in batches in the case of messages\n" .
	"arriving on the network.");
$pmda->add_metric(pmda_pmid(1,2), PM_TYPE_U64, $queue_indom, PM_SEM_COUNTER,
	pmda_units(0,0,1,0,0,0), 'rsyslog.queues.full',
	'Cumulative count of message arrivals with a full queue',
	"When messages are enqueued, a check is first made to ensure the\n" .
	"queue is not full.  If it is, this counter is incremented.  The\n" .
	"full-queue-handling logic will wait for a configurable time for\n" .
	"the queue congestion to ease, failing which the message will be\n" .
	"discarded.  Worth keeping an eye on this metric, as it indicates\n" .
	"rsyslog is not able to process messages quickly enough given the\n" .
	"current arrival rate.");
$pmda->add_metric(pmda_pmid(1,3), PM_TYPE_U64, $queue_indom, PM_SEM_INSTANT,
	pmda_units(0,0,1,0,0,0), 'rsyslog.queues.maxsize',
	'Maximum depth reached by an individual queue',
	"When messages arrive (for example) they are enqueued to the main\n" .
	"message queue - if the queue length on arrival is now greater than\n" .
	"ever before observed, we set this value to the current queue size");
$pmda->add_metric(pmda_pmid(1,4), PM_TYPE_U64, $queue_indom, PM_SEM_COUNTER,
	pmda_units(0,0,1,0,0,0), 'rsyslog.queues.discarded.full',
	'Number of messages discarded because the queue was full', '');
$pmda->add_metric(pmda_pmid(1,5), PM_TYPE_U64, $queue_indom, PM_SEM_COUNTER,
	pmda_units(0,0,1,0,0,0), 'rsyslog.queues.discarded.nf',
	'Number of messages discarded because the queue was nearly full',
	"When messages arrive and the queue is nearly full (over the\n" .
	"configured threshold), messages of lower-than-configured priority\n" .
	"are discarded to save space for higher severity ones.");

$pmda->add_indom($queue_indom, \@queue_insts,
	'Instance domain exporting each rsyslog queue', '');

$pmda->add_metric(pmda_pmid(2,0), PM_TYPE_U64, $unrec_origin_indom, PM_SEM_COUNTER,
	pmda_units(0,0,1,0,0,0), 'rsyslog.impstats.origins.unrecog',
	'Cumulative count for an unrecognized origin', '');

$pmda->add_indom($unrec_origin_indom, \@unrec_origin_insts,
	'Cumulative counts for unrecognized impstats origins', '');

$pmda->add_metric(pmda_pmid(3,0), PM_TYPE_U64, $unmat_origin_indom, PM_SEM_COUNTER,
	pmda_units(0,0,1,0,0,0), 'rsyslog.impstats.origins.unmatched',
	'Cumulative count for an unmatched origin entries', '');

$pmda->add_indom($unmat_origin_indom, \@unmat_origin_insts,
	'Cumulative counts for unmatched impstats origins', '');

$pmda->add_metric(pmda_pmid(4,0), PM_TYPE_U64, $action_indom, PM_SEM_COUNTER,
	pmda_units(0,0,1,0,0,0), 'rsyslog.action.processed',
	'Count of messages processed',
	"The count of messages processed. Note that this includes both\n" .
	"success and failures.");
$pmda->add_metric(pmda_pmid(4,1), PM_TYPE_U64, $action_indom, PM_SEM_COUNTER,
	pmda_units(0,0,1,0,0,0), 'rsyslog.action.failed',
	'Count of messages failed during processing','');
$pmda->add_metric(pmda_pmid(4,2), PM_TYPE_U64, $action_indom, PM_SEM_COUNTER,
	pmda_units(0,0,1,0,0,0), 'rsyslog.action.suspended',
	'Count of times the action suspended itself','');
$pmda->add_metric(pmda_pmid(4,3), PM_TYPE_U64, $action_indom, PM_SEM_INSTANT,
	pmda_units(0,0,1,0,0,0), 'rsyslog.action.suspended_duration',
	'Total number of seconds action was suspended','');
$pmda->add_metric(pmda_pmid(4,4), PM_TYPE_U64, $action_indom, PM_SEM_COUNTER,
	pmda_units(0,0,1,0,0,0), 'rsyslog.action.resumed',
	'Count of times the action resumed itself','');

$pmda->add_indom($action_indom, \@action_insts,
	'Instance domain exporting each rsyslog action', '');

$pmda->add_metric(pmda_pmid(5,0), PM_TYPE_U64, $udp_indom, PM_SEM_COUNTER,
	pmda_units(0,0,1,0,0,0), 'rsyslog.udp.submitted',
	'Count of messages submitted for processing since startup', '');
$pmda->add_metric(pmda_pmid(5,1), PM_TYPE_U64, $udp_indom, PM_SEM_COUNTER,
	pmda_units(0,0,1,0,0,0), 'rsyslog.udp.disallowed',
	'Count of messages discarded due to disallowed sender', '');

$pmda->add_indom($udp_indom, \@udp_insts,
	'Instance domain exporting each rsyslog udp metric', '');

$pmda->add_metric(pmda_pmid(6,0), PM_TYPE_U64, $udpt_indom, PM_SEM_COUNTER,
	pmda_units(0,0,1,0,0,0), 'rsyslog.udp.thread.recvmmsg',
	'Count of recvmmsg() OS calls done', '');
$pmda->add_metric(pmda_pmid(6,1), PM_TYPE_U64, $udpt_indom, PM_SEM_COUNTER,
	pmda_units(0,0,1,0,0,0), 'rsyslog.udp.thread.recvmsg',
	'Count of recvmsg() OS calls done', '');
$pmda->add_metric(pmda_pmid(6,2), PM_TYPE_U64, $udpt_indom, PM_SEM_COUNTER,
	pmda_units(0,0,1,0,0,0), 'rsyslog.udp.thread.received',
	'Count of actual messages received', '');

$pmda->add_indom($udpt_indom, \@udpt_insts,
	'Instance domain exporting each rsyslog udp thread metric', '');

$pmda->add_metric(pmda_pmid(7,0), PM_TYPE_U64, $ptcp_indom, PM_SEM_COUNTER,
	pmda_units(0,0,1,0,0,0), 'rsyslog.ptcp.submitted',
	'Count of TCP messages submitted for processing since startup', '');
$pmda->add_metric(pmda_pmid(7,1), PM_TYPE_U64, $ptcp_indom, PM_SEM_COUNTER,
	pmda_units(0,0,1,0,0,0), 'rsyslog.ptcp.sessions.opened',
	'Count of TCP sessions opened', '');
$pmda->add_metric(pmda_pmid(7,2), PM_TYPE_U64, $ptcp_indom, PM_SEM_COUNTER,
	pmda_units(0,0,1,0,0,0), 'rsyslog.ptcp.sessions.openfailed',
	'Count of TCP session failures', '');
$pmda->add_metric(pmda_pmid(7,3), PM_TYPE_U64, $ptcp_indom, PM_SEM_COUNTER,
	pmda_units(0,0,1,0,0,0), 'rsyslog.ptcp.sessions.closed',
	'Count of TCP sessions closed', '');
$pmda->add_metric(pmda_pmid(7,4), PM_TYPE_U64, $ptcp_indom, PM_SEM_COUNTER,
	pmda_units(0,0,1,0,0,0), 'rsyslog.ptcp.bytes.received',
	'Count of bytes received via TCP', '');
$pmda->add_metric(pmda_pmid(7,5), PM_TYPE_U64, $ptcp_indom, PM_SEM_COUNTER,
	pmda_units(0,0,1,0,0,0), 'rsyslog.ptcp.bytes.decompressed',
	'Count of bytes decompressed for TCP', '');

$pmda->add_indom($ptcp_indom, \@ptcp_insts,
	'Instance domain exporting each rsyslog ptcp metric', '');

$pmda->add_metric(pmda_pmid(8,0), PM_TYPE_U64, $fwd_indom, PM_SEM_COUNTER,
	pmda_units(0,0,1,0,0,0), 'rsyslog.fwd.bytes',
	'Count of bytes sent through forwarder', '');

$pmda->add_indom($fwd_indom, \@fwd_insts,
	'Instance domain exporting each rsyslog fwd metric', '');

$pmda->add_tail($statsfile, \&rsyslog_parser, 0);
$pmda->set_fetch_callback(\&rsyslog_fetch_callback);
$pmda->set_user('pcp');
$pmda->run;
