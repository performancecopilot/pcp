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

use strict;
use warnings;
use PCP::PMDA;

my $pmda;

sub bozo_fetch_callback		# must return array of value,status
{
    my ($cluster, $item, $inst) = @_;

    return (PM_ERR_INST, 0) unless ($inst == PM_IN_NULL);
    if ($cluster == 0) {
	if ($item == 0)	{ ("my interesting string", 1); }
    	else		{ (PM_ERR_PMID, 0); }
    }
    else { (PM_ERR_PMID, 0); }
}

$pmda = PCP::PMDA->new('bozo', 234);

$pmda->add_metric(pmda_pmid(0,0), PM_TYPE_STRING, PM_INDOM_NULL,
		  PM_SEM_INSTANT, pmda_units(0,0,0,0,0,0),
		  'bozo.string',
		  'Yawn.', 'Long yawn.');

$pmda->set_fetch_callback(\&bozo_fetch_callback);

$pmda->run;

=pod

=head1 NAME

pmdabozo - bozo Perl PMDA

=head1 DESCRIPTION

You're not serious.

=head1 SEE ALSO

pmcd(1).
