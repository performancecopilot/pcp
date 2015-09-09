package PCP::LogSummary;

use strict;
use vars qw($VERSION @ISA @EXPORT @EXPORT_OK);

require Exporter;

@ISA         = qw(Exporter);
@EXPORT      = qw(new metric_instance);
@EXPORT_OK   = qw( );
$VERSION     = '1.01';

sub new
{
    my ( $self, $archive, $metricsref, $start, $finish ) = @_;
    my $opts    = '-F -N -z -biImMy -p6';
    my @metrica = @{$metricsref};
    my $metrics = ' ';
    my %results;

    foreach my $m (@metrica) { $metrics .= $m . ' '; }
    if (defined($start)) { $opts .= " -S'$start'"; }
    if (defined($finish)) { $opts .= " -T'$finish'"; }

    open SUMMARY, "pmlogsummary $opts $archive $metrics |" 
		|| die "pmlogsummary: $!\n";

    # metric,[inst],stocavg,timeavg,minimum,mintime,maximum,maxtime,count,units
    LINE: while (<SUMMARY>) {
	# print "Input line: $_\n";
	m/^(\S.+),(.*),(\S+),(\S+),(\S+),(.+),(\S+),(.+),(\S+),(.*)$/
		|| next LINE;

	# If counter metric doesn't cover 90% of archive, metric name
	# is preceded by an asterix.  Chop this off and set a flag.
	my $metric = $1;
	$metric =~ s/^\*//;
	my $asterix = ($metric ne $1);

	my %result;
	$result{'average'} = $3;
	$result{'timeavg'} = $4;
	$result{'minimum'} = $5;
	$result{'mintime'} = $6;
	$result{'maximum'} = $7;
	$result{'maxtime'} = $8;
	$result{'samples'} = $9;
	$result{'units'} = $10;
	$result{'ninety%'} = $asterix;

	my $key = $1;
	if ($2 ne "") { $key .= $2; }
	$results{$key} = \%result;

	# print "key=", $key, " average=$3\n";
    }
    close SUMMARY;
    return \%results;
}

sub metric_instance
{
    my ( $metric, $instance ) = @_;
    return "$metric\[\"$instance\"\]";
}

1;
__END__

=head1 NAME

PCP::LogSummary - Perl interface for pmlogsummary(1)

=head1 SYNOPSIS

  use PCP::LogSummary;

  my $summary = new PCP::LogSummary($log, \@metrics, $start, $end);

=head1 DESCRIPTION

The PCP::LogSummary module is a wrapper around the Performance Co-Pilot
pmlogsummary(1) command.
Its primary purpose is to automate the production of post-processed
pmlogsummary data, in particular to automate the step where the
summarised data is imported into a spreadsheet for further anaylsis.
This has proven to often be an iterative process - done manually it
involves much cutting+pasting, and can be a significant time waster.

=head2 EXPORT

new

metric_instance

=head1 SEE ALSO

pmlogsummary(1).

The PCP mailing list pcp@oss.sgi.com can be used for questions about
this module.

Further details can be found at http://www.pcp.io

=head1 AUTHOR

Nathan Scott, E<lt>nathans@debian.orgE<gt>

=head1 COPYRIGHT AND LICENSE

Copyright (C) 2008 by Aconex

This library is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License, version 2 (see
the "COPYING" file in the PCP source tree for further details).

=cut
