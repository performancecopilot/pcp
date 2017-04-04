#
# Copyright (c) 2012-2015 Red Hat.
# Copyright (c) 2009-2010 Josef 'Jeff' Sipek <jeffpc@josefsipek.net>
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
use Time::HiRes qw ( time );

use vars qw( $pmda );
use vars qw( %caches );
use vars qw( %logstats );
my @logfiles = ( '/var/log/mail.log', '/var/log/maillog', '/var/log/mail' );
my $logfile;
my $qshape = 'qshape';
my $refresh = 5; # 5 seconds between qshape refreshes
my $qshape_args = "-b 10 -t $refresh";

my $cached = 0;

my $postfix_queues_indom = 0;
my @postfix_queues_dom = (
		0 => 'total',
		1 => '0-5 mins',
		2 => '5-10 mins',
		3 => '10-20 mins',
		4 => '20-40 mins',
		5 => '40-80 mins',
		6 => '80-160 mins',
		7 => '160-320 mins',
		8 => '320-640 mins',
		9 => '640-1280 mins',
		10=> '1280+ mins',
	     );

my $postfix_sent_indom = 1;
my @postfix_sent_dom = (
		0 => 'smtp',
		1 => 'local',
	     );

my $postfix_received_indom = 2;
my @postfix_received_dom = (
		0 => 'local',
		1 => 'smtp',
	     );

my $setup = defined($ENV{'PCP_PERL_PMNS'}) || defined($ENV{'PCP_PERL_DOMAIN'});

sub postfix_do_refresh
{
    QUEUE:
    foreach my $qname ("maildrop", "incoming", "hold", "active", "deferred") {
	unless (open(PIPE, "$qshape $qshape_args $qname |")) {
	    $pmda->log("couldn't execute '$qshape $qshape_args $qname'");
	    next QUEUE;
	}
	while(<PIPE>) {
	    last if (/^[\t ]*TOTAL /);
	}
	close PIPE;

	unless (/^[\t ]*TOTAL /) {
	    $pmda->log("malformed output for '$qshape $qshape_args $qname': $_");
	    next QUEUE;
	}

	s/^[\t ]*//;
	s/[\t ]+/ /g;

	my @items = split(/ /);

	$caches{$qname}{0}  = $items[1];
	$caches{$qname}{1}  = $items[2];
	$caches{$qname}{2}  = $items[3];
	$caches{$qname}{3}  = $items[4];
	$caches{$qname}{4}  = $items[5];
	$caches{$qname}{5}  = $items[6];
	$caches{$qname}{6}  = $items[7];
	$caches{$qname}{7}  = $items[8];
	$caches{$qname}{8}  = $items[9];
	$caches{$qname}{9}  = $items[10];
	$caches{$qname}{10} = $items[11];
    }
}

sub postfix_log_parser
{
    ( undef, $_ ) = @_;
    my $do_sent = 0;

    if (/status=sent/) {
	return unless (/ postfix\//);
	$do_sent = 1;
    }
    elsif (/stat=Sent/) {
	return unless (/relay=[^,]+/);
	$do_sent = 1;
    }

    if ($do_sent == 1) {
	my $relay = "";

	if (/relay=([^,]+)/o) {
	    $relay = $1;
	}

	if ($relay !~ /\[/o) {
	    # if we are about to define a new instance, let's add it to the
	    # domain as well
	    my $idx = 0;
	    my $key;
	    my %tmp = @postfix_sent_dom;

	    foreach $key (sort keys %tmp) {
		last if ($relay eq $tmp{$key});
		$idx += 1;
	    }

	    if ((2 * $idx) == @postfix_sent_dom) {
		push(@postfix_sent_dom, $idx=>$relay);
		$pmda->replace_indom($postfix_sent_indom, \@postfix_sent_dom);
	    }

	    $logstats{"sent"}{$idx} += 1;
	} else {
	    $logstats{"sent"}{0} += 1;
	}
    } elsif (/smtpd.*client=/) {
	$logstats{"received"}{1} += 1;
    } elsif (/pickup.*(sender|uid)=/) {
	$logstats{"received"}{0} += 1;
    }
}

sub postfix_fetch_callback
{
    my ($cluster, $item, $inst) = @_;
    my $metric_name = pmda_pmid_name($cluster, $item);

    my $now = time;

    #$pmda->log("postfix_fetch_callback $metric_name $cluster:$item ($inst)\n");

    if (!defined($metric_name))    { return (PM_ERR_PMID, 0); }

    if ($cluster == 0) {
	my $qname;

	if ($now - $cached > $refresh) {
	    postfix_do_refresh();
	    $cached = $now;
	}

	$qname = $metric_name;
	$qname =~ s/^postfix\.queues\.//;

	return (PM_ERR_AGAIN, 0) unless defined($caches{$qname});
	return ($caches{$qname}{$inst}, 1);
    } elsif ($cluster == 1) {
	my $dir = $metric_name;
	$dir =~ s/^postfix\.//;

	return (PM_ERR_AGAIN, 0) unless defined($logstats{$dir});
	return (PM_ERR_AGAIN, 0) unless defined($logstats{$dir}{$inst});
	return ($logstats{$dir}{$inst}, 1);
    }

    return (PM_ERR_PMID, 0);
}

$pmda = PCP::PMDA->new('postfix', 103);
$pmda->connect_pmcd;

$pmda->add_metric(pmda_pmid(0,0), PM_TYPE_U32, $postfix_queues_indom,
	PM_SEM_INSTANT, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
	'postfix.queues.maildrop', '', '');
$pmda->add_metric(pmda_pmid(0,1), PM_TYPE_U32, $postfix_queues_indom,
	PM_SEM_INSTANT, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
	'postfix.queues.incoming', '', '');
$pmda->add_metric(pmda_pmid(0,2), PM_TYPE_U32, $postfix_queues_indom,
	PM_SEM_INSTANT, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
	'postfix.queues.hold', '', '');
$pmda->add_metric(pmda_pmid(0,3), PM_TYPE_U32, $postfix_queues_indom,
	PM_SEM_INSTANT, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
	'postfix.queues.active', '', '');
$pmda->add_metric(pmda_pmid(0,4), PM_TYPE_U32, $postfix_queues_indom,
	PM_SEM_INSTANT, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
	'postfix.queues.deferred', '', '');

$pmda->add_metric(pmda_pmid(1,0), PM_TYPE_U32, $postfix_sent_indom,
	PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
	'postfix.sent', '', '');
$pmda->add_metric(pmda_pmid(1,1), PM_TYPE_U32, $postfix_received_indom,
	PM_SEM_COUNTER, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
	'postfix.received', '', '');

$logstats{"sent"}{0} = 0;
$logstats{"received"}{0} = 0;
$logstats{"received"}{1} = 0;

# Note:
# Environment variables.
# $PMDA_POSTFIX_QSHAPE: alternative executable qshape scrpipt (for QA)
#                       ... over-rides default and command line argument.
#			... over-rides default arguments -b 10 -t $refresh
# $PMDA_POSTFIX_REFRESH: alternative refresh rate (for QA)
# $PMDA_POSTFIX_LOG: alternative postfix log file (for QA)
#
# Command line argument.
# Path to qshape Perl script (optional, instead of executable qshape
# on the $PATH).
#

if (defined($ARGV[0])) { $qshape = "perl $ARGV[0]"; }
if (defined($ENV{'PMDA_POSTFIX_QSHAPE'})) {
    $qshape = $ENV{'PMDA_POSTFIX_QSHAPE'};
    $qshape_args = '';
}
if (!$setup) { $pmda->log("qshape cmd: $qshape $qshape_args <qname>"); }

if (defined($ENV{'PMDA_POSTFIX_REFRESH'})) { $refresh = $ENV{'PMDA_POSTFIX_REFRESH'}; }

foreach my $file ( @logfiles ) {
    if ( -r $file ) {
	$logfile = $file unless ( -d $file );
    }
}
if (defined($ENV{'PMDA_POSTFIX_LOG'})) { $logfile = $ENV{'PMDA_POSTFIX_LOG'}; } 
unless(defined($logfile))
{
    $pmda->log("Fatal: No Postfix log file found in: @logfiles");
    die 'No Postfix log file found';
}
if (!$setup) { $pmda->log("logfile: $logfile"); }

$pmda->add_indom($postfix_queues_indom, \@postfix_queues_dom, '', '');
$pmda->add_indom($postfix_sent_indom, \@postfix_sent_dom, '', '');
$pmda->add_indom($postfix_received_indom, \@postfix_received_dom, '', '');
$pmda->add_tail($logfile, \&postfix_log_parser, 0);
$pmda->set_fetch_callback(\&postfix_fetch_callback);
$pmda->run;
