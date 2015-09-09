package PCP::LogImport;

use strict;
use warnings;

require Exporter;
require DynaLoader;

our ($VERSION, @ISA, @EXPORT, @EXPORT_OK, %EXPORT_TAGS);

@ISA = qw( Exporter DynaLoader );
@EXPORT = qw(
    pmiStart pmiUseContext pmiEnd pmiSetHostname pmiSetTimezone
    pmiAddMetric pmiAddInstance pmiPutValue pmiGetHandle pmiPutValueHandle
    pmiWrite
    pmiDump pmiErrStr pmiUnits pmiID pmiInDom
    pmid_build pmInDom_build
    pmiBatchPutValue pmiBatchPutValueHandle pmiBatchWrite pmiBatchEnd
    PM_ID_NULL PM_INDOM_NULL PM_IN_NULL
    PM_SPACE_BYTE PM_SPACE_KBYTE PM_SPACE_MBYTE PM_SPACE_GBYTE PM_SPACE_TBYTE
    PM_TIME_NSEC PM_TIME_USEC PM_TIME_MSEC PM_TIME_SEC PM_TIME_MIN PM_TIME_HOUR
    PM_COUNT_ONE
    PM_TYPE_NOSUPPORT PM_TYPE_32 PM_TYPE_U32 PM_TYPE_64 PM_TYPE_U64
	PM_TYPE_FLOAT PM_TYPE_DOUBLE PM_TYPE_STRING
    PM_SEM_COUNTER PM_SEM_INSTANT PM_SEM_DISCRETE
    PMI_DOMAIN
);
%EXPORT_TAGS = qw();
@EXPORT_OK = qw();

# set the version for version checking
$VERSION = '1.02';

# metric identification
sub PM_ID_NULL		{ 0xffffffff; }
sub PM_INDOM_NULL	{ 0xffffffff; }
sub PM_IN_NULL		{ 0xffffffff; }

# units - space scale
sub PM_SPACE_BYTE	{ 0; }	# bytes
sub PM_SPACE_KBYTE	{ 1; }	# kilobytes
sub PM_SPACE_MBYTE	{ 2; }	# megabytes
sub PM_SPACE_GBYTE	{ 3; }	# gigabytes
sub PM_SPACE_TBYTE	{ 4; }	# terabytes

# units - time scale
sub PM_TIME_NSEC	{ 0; }	# nanoseconds
sub PM_TIME_USEC	{ 1; }	# microseconds
sub PM_TIME_MSEC	{ 2; }	# milliseconds
sub PM_TIME_SEC		{ 3; }	# seconds
sub PM_TIME_MIN		{ 4; }	# minutes
sub PM_TIME_HOUR	{ 5; }	# hours

# units - count scale	(for metrics such as count events, syscalls,
# interrupts, etc - these are simply powers of ten and not enumerated here
# (e.g. 6 for 10^6, or -3 for 10^-3).
sub PM_COUNT_ONE	{ 0; }	# 1

# data type of metric values
sub PM_TYPE_NOSUPPORT	{ 0xffffffff; }	# not implemented in this version
sub PM_TYPE_32		{ 0; }	# 32-bit signed integer
sub PM_TYPE_U32		{ 1; }	# 32-bit unsigned integer
sub PM_TYPE_64		{ 2; }	# 64-bit signed integer
sub PM_TYPE_U64		{ 3; }	# 64-bit unsigned integer
sub PM_TYPE_FLOAT	{ 4; }	# 32-bit floating point
sub PM_TYPE_DOUBLE	{ 5; }	# 64-bit floating point
sub PM_TYPE_STRING	{ 6; }	# array of characters

# semantics/interpretation of metric values
sub PM_SEM_COUNTER	{ 1; }	# cumulative counter (monotonic increasing)
sub PM_SEM_INSTANT	{ 3; }	# instantaneous value, continuous domain
sub PM_SEM_DISCRETE	{ 4; }	# instantaneous value, discrete domain

# reserved domain (see $PCP_VAR_DIR/pmns/stdpmid)
sub PMI_DOMAIN		{ 245; }

# error codes
sub PMI_ERR_DUPMETRICNAME { -20001; }
sub PMI_ERR_DUPMETRICID	{ -20002; }	# Metric pmID already defined
sub PMI_ERR_DUPINSTNAME { -20003; }	# External instance name already defined
sub PMI_ERR_DUPINSTID   { -20004; }	# Internal instance identifer already defined
sub PMI_ERR_INSTNOTNULL { -20005; }	# Non-null instance expected for a singular metric
sub PMI_ERR_INSTNULL    { -20006; }	# Null instance not allowed for a non-singular metric
sub PMI_ERR_BADHANDLE   { -20007; }	# Illegal handle
sub PMI_ERR_DUPVALUE    { -20008; }	# Value already assigned for singular metric
sub PMI_ERR_BADTYPE     { -20009; }	# Illegal metric type
sub PMI_ERR_BADSEM      { -20010; }	# Illegal metric semantics
sub PMI_ERR_NODATA      { -20011; }	# No data to output
sub PMI_ERR_BADMETRICNAME { -20012; }	# Illegal metric name
sub PMI_ERR_BADTIMESTAMP { -20013; }	# Illegal result timestamp

# Batch operations
our %pmi_batch = ();

sub pmiBatchPutValue($$$) {
  my ($name, $instance, $value) = @_;
  push @{$pmi_batch{'b'}}, [ $name, $instance, $value ];
  return 0;
}

sub pmiBatchPutValueHandle($$) {
  my ($handle, $value) = @_;
  push @{$pmi_batch{'b'}}, [ $handle, $value ];
  return 0;
}

sub pmiBatchWrite($$) {
  my ($sec, $usec) = @_;
  push @{$pmi_batch{"$sec.$usec"}}, @{delete $pmi_batch{'b'}};
  return 0;
}

sub pmiBatchEnd() {
  my ($arr, $r);
  my $ts = -1;
  # Iterate over the sorted hash and call pmiPutValue/pmiWrite accordingly
  delete $pmi_batch{'b'};
  for my $k (map { $_->[0] }
             sort { $a->[1] <=> $b->[1] || $a->[2] <=> $b->[2] }
                  map { [$_, /(\d+)\.(\d+)/] }
             keys %pmi_batch) {
    $arr = $pmi_batch{$k};
    $ts = $k if $ts eq -1;
    if ($k > $ts) {
      $r = pmiWrite(split(/\./, $ts));
      return $r if ($r != 0);
      $ts = $k;
    }
    for my $v (@$arr) {
      if (defined($v->[2])) {
        $r = pmiPutValue($v->[0], $v->[1], $v->[2]);
      } else {
        $r = pmiPutValueHandle($v->[0], $v->[1]);
      }
      return $r if ($r != 0);
    }
  }
  $r = pmiWrite(split(/\./, $ts));
  return $r if ($r != 0);
  %pmi_batch = ();
  return 0;
}

bootstrap PCP::LogImport $VERSION;

1; # don't forget to return a true value from the file

__END__

=head1 NAME

PCP::LogImport - Perl module for importing performance data to create a Performance Co-Pilot archive

=head1 SYNOPSIS

  use PCP::LogImport;

=head1 DESCRIPTION

The PCP::LogImport module contains the language bindings for building
Perl applications that import performance data from a file or real-time
source and create a Performance Co-Pilot (PCP) archive suitable for use
with the PCP tools.

The routines in this module provide wrappers around the libpcp_import
library.

=head1 SEE ALSO

pmiAddInstance(3), pmiAddMetric(3), pmiEnd(3), pmiErrStr(3),
pmiGetHandle(3), pmiPutResult(3), pmiPutValue(3), pmiPutValueHandle(3),
pmiStart(3), pmiSetHostname(3), pmiSetTimezone(3), pmiUnits(3),
pmiUseContext(3) and pmiWrite(3).

The PCP mailing list pcp@oss.sgi.com can be used for questions about
this module.

Further details can be found at http://www.pcp.io

=head1 AUTHOR

Ken McDonell, E<lt>kenj@internode.on.netE<gt>

Copyright (C) 2010 by Ken McDonell.

This library is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License, version 2 (see
the "COPYING" file in the PCP source tree for further details).

=cut
