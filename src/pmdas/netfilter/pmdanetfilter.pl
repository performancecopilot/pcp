#
# Copyright (c) 2012, 2015 Red Hat.
# Copyright (c) 2009 Aconex.  All Rights Reserved.
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

my $pmda = PCP::PMDA->new('netfilter', 97);
my ($procfs) = @ARGV;

sub netfilter_fetch_callback
{
    my ($cluster, $item, $inst) = @_;
    my $metric_name = pmda_pmid_name($cluster, $item);
    my ($path, $name, $value, $fh, @vals);

    if ($inst != PM_IN_NULL)	{ return (PM_ERR_INST, 0); }
    if (!defined($metric_name))	{ return (PM_ERR_PMID, 0); }

    $metric_name =~ s/\./\//;
    if (index($procfs, "ipv4") == -1){
        $metric_name =~ s/ip/nf/;
    }
    $name = $procfs . $metric_name;
    open($fh, $name) || 	return (PM_ERR_APPVERSION, 0);
    $value = <$fh>;
    close $fh;
    chomp $value;

    return ($value, 1);
}

$pmda->add_metric(pmda_pmid(0,0), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT,
		pmda_units(0,0,0,0,0,0), 'netfilter.ip_conntrack_max', '', '');
$pmda->add_metric(pmda_pmid(0,1), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT,
		pmda_units(0,0,0,0,0,0), 'netfilter.ip_conntrack_count', '', '');

$pmda->set_fetch_callback(\&netfilter_fetch_callback);
$pmda->set_user('pcp');
$pmda->run;
