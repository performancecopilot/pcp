#!/usr/bin/perl -w
# Usenet news demo PMDA
# $Id: pmdanews.pl,v 1.2 2004/08/24 01:01:58 kenmcd Exp $
#
# Copyright (c) 2004 Silicon Graphics, Inc.  All Rights Reserved.
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
# You should have received a copy of the GNU General Public License along
# with this program; if not, write to the Free Software Foundation, Inc.,
# 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
# 
# Contact information: Silicon Graphics, Inc., 1500 Crittenden Lane,
# Mountain View, CA 94043, USA, or: http://www.sgi.com
# 

use strict;
use PCP::PMDA;

use vars qw( $total $news_regex %news_hash @news_count @news_last @newsgroups );
my ($nnrpd_count, $rn_count, $trn_count, $xrn_count, $vn_count) = (0,0,0,0,0);
my $news_file = '/var/lib/pcp/pmdas/news/active';	# '/var/news/active'
my $news_indom = 0;


# ---custom callbacks follow---

sub news_fetch {	# called once per ``fetch'' pdu, before callbacks
    my ( $group, $cmd );

    $total = 0;
    seek ACTIVE,0,0;
    while (<ACTIVE>) {
	next unless /$news_regex/;
	$group = $news_hash{$1} - 1;
	$news_last[$group] = $2;
	$total += $news_count[$group] = $2 - $3;
    }

    ($nnrpd_count, $rn_count, $trn_count, $xrn_count, $vn_count) = (0,0,0,0,0);
    if (open(READERS, 'ps -ef |')) {
	while (<READERS>) {
	    s/\b(:?\d\d:){2}\d\d\b/ Mmm DD /;	# replace times with dates
	    s/\s*?(\S+?\s+?){8}//;		# nuke the first eight fields
	    next unless /(\S+)\s*?/;		# prepare $cmd with command name
	    $cmd = $1;
	    ($cmd eq 'in.nnrpd' || $cmd =~ /\/in\.nnrpd$/) && $nnrpd_count++;
	    ($cmd eq 'rn'  || $cmd =~ /\/rn$/)	&& $rn_count++;
	    ($cmd eq 'trn' || $cmd =~ /\/trn$/)	&& $trn_count++;
	    ($cmd eq 'xrn' || $cmd =~ /\/xrn$/)	&& $xrn_count++;
	    ($cmd eq 'vn'  || $cmd =~ /\/vrn$/)	&& $vn_count++;
	}
	close READERS;
    }
    else {
	print STDERR "Cannot execute 'ps' to count news reader processes\n";
    }
}

sub news_fetch_callback {	# must return array of value,status
    my ($cluster, $item, $inst) = @_;

    return (PM_ERR_INST, 0) unless ( $inst == -1 ||
		( $cluster == 0 && ($item == 301 || $item == 302) &&
				$inst > 0 && $inst <= ($#newsgroups+1)/2 ) );
    if ($cluster == 0) {
	if ($item == 201)	{ ($total, 1); }		# articles.total
	elsif ($item == 301)	{ ($news_count[$inst-1], 1); }	# articles.count
	elsif ($item == 302)	{ ($news_last[$inst-1], 1); }	# articles.last
	elsif ($item == 101)	{ ($nnrpd_count, 1); }		# readers.nnrpd
	elsif ($item == 111)	{ ($rn_count, 1); }		# readers.rn
	elsif ($item == 112)	{ ($trn_count, 1); }		# readers.trn
	elsif ($item == 113)	{ ($xrn_count, 1); }		# readers.xrn
	elsif ($item == 114)	{ ($vn_count, 1); }		# readers.vn
    	else			{ (PM_ERR_PMID, 0); }
    }
    else { (PM_ERR_PMID, 0); }
}

# ---end of custom routines---

# ---local routines follow---

sub news_init {
    ($#newsgroups > 0 && $#newsgroups % 2 != 0)
		|| die "Invalid newsgroups array has been specified\n";
    open(ACTIVE, $news_file) || die "Can't open $news_file: $!\n";

    # build regex using the given newsgroup names
    $_ = "^($newsgroups[1]";
    $news_hash{$newsgroups[1]} = $newsgroups[0];
    for (my $i = 2; $i < $#newsgroups; $i += 2) {
	$_ .= "|$newsgroups[$i+1]";
	$news_hash{$newsgroups[$i+1]} = $newsgroups[$i];
    }
    $news_regex = $_ . ") (\\d+) (\\d+) y\$";
}

# ---end of local routines---


my $pmda = PCP::PMDA->new('news', 28);

$pmda->add_metric(pmda_pmid(0,201), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  'news.articles.total',
		  'Total number of articles received for each newsgroup',
'Total number of articles received for each newsgroup.
Note this is the historical running total, see news.articles.count for
the current total of un-expired articles by newsgroup.');

$pmda->add_metric(pmda_pmid(0,301), PM_TYPE_U32, $news_indom,
		  PM_SEM_INSTANT, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  'news.articles.count',
		  'Total number of un-expired articles in each newsgroup', '');

$pmda->add_metric(pmda_pmid(0,302), PM_TYPE_U32, $news_indom,
		  PM_SEM_INSTANT, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  'news.articles.last', '', '');
$pmda->add_metric(pmda_pmid(0,101), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  'news.readers.nnrpd', '', '');
$pmda->add_metric(pmda_pmid(0,111), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  'news.readers.rn', '', '');
$pmda->add_metric(pmda_pmid(0,112), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  'news.readers.trn', '', '');
$pmda->add_metric(pmda_pmid(0,113), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  'news.readers.xrn', '', '');
$pmda->add_metric(pmda_pmid(0,114), PM_TYPE_U32, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,0,1,0,0,PM_COUNT_ONE),
		  'news.readers.vn', '', '');

@newsgroups = (
    1,  'comp.sys.sgi',
    2,  'comp.sys.sgi.graphics',
    3,  'comp.sys.sgi.hardware',
    4,  'sgi.bad-attitude',
    5,  'sgi.engr.all',
);
$pmda->add_indom( $news_indom, \@newsgroups, '', '' );

$pmda->set_fetch( \&news_fetch );
$pmda->set_fetch_callback( \&news_fetch_callback );

&news_init;
$pmda->run;
