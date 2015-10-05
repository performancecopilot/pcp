package PCP::MMV;

use strict;
use warnings;
use vars qw($VERSION @ISA @EXPORT @EXPORT_OK);

require Exporter;
require DynaLoader;

@ISA = qw(Exporter DynaLoader);
@EXPORT = qw(
    mmv_stats_init mmv_stats_stop mmv_units
	mmv_lookup_value_desc
	mmv_inc_value mmv_set_value mmv_set_string
	mmv_stats_add mmv_stats_inc mmv_stats_set
	mmv_stats_add_fallback mmv_stats_inc_fallback
	mmv_stats_interval_start mmv_stats_interval_end
	mmv_stats_set_string
    MMV_FLAG_NOPREFIX MMV_FLAG_PROCESS
    MMV_INDOM_NULL
    MMV_TYPE_NOSUPPORT
    MMV_TYPE_I32 MMV_TYPE_U32
    MMV_TYPE_I64 MMV_TYPE_U64
    MMV_TYPE_FLOAT MMV_TYPE_DOUBLE
    MMV_TYPE_STRING MMV_TYPE_ELAPSED
    MMV_COUNT_ONE
    MMV_SEM_COUNTER MMV_SEM_INSTANT MMV_SEM_DISCRETE
    MMV_SPACE_BYTE MMV_SPACE_KBYTE MMV_SPACE_MBYTE
    MMV_SPACE_GBYTE MMV_SPACE_TBYTE
    MMV_TIME_NSEC MMV_TIME_USEC MMV_TIME_MSEC
    MMV_TIME_SEC MMV_TIME_MIN MMV_TIME_HOUR
);
@EXPORT_OK = qw();
$VERSION = '1.00';

sub MMV_INDOM_NULL	{ 0xffffffff; }

# flags for pmdammv
sub MMV_FLAG_NOPREFIX	{ 0x1; } # metric names not prefixed by file name
sub MMV_FLAG_PROCESS	{ 0x2; } # instrumented process must be running

# data type of metric values
sub MMV_TYPE_NOSUPPORT	{ 0xffffffff; }	# not implemented in this version
sub MMV_TYPE_I32	{ 0; }	# 32-bit signed integer
sub MMV_TYPE_U32	{ 1; }	# 32-bit unsigned integer
sub MMV_TYPE_I64	{ 2; }	# 64-bit signed integer
sub MMV_TYPE_U64	{ 3; }	# 64-bit signed integer
sub MMV_TYPE_FLOAT	{ 4; }	# 32-bit floating point
sub MMV_TYPE_DOUBLE	{ 5; }	# 64-bit floating point
sub MMV_TYPE_STRING	{ 6; }	# null-terminated string
sub MMV_TYPE_ELAPSED	{ 10; }	# 64-bit elapsed time

# units - space scale
sub MMV_SPACE_BYTE	{ 0; }  # bytes
sub MMV_SPACE_KBYTE	{ 1; }  # kilobytes
sub MMV_SPACE_MBYTE	{ 2; }  # megabytes
sub MMV_SPACE_GBYTE	{ 3; }  # gigabytes
sub MMV_SPACE_TBYTE	{ 4; }  # terabytes

# units - time scale
sub MMV_TIME_NSEC	{ 0; }  # nanoseconds
sub MMV_TIME_USEC	{ 1; }  # microseconds
sub MMV_TIME_MSEC	{ 2; }  # milliseconds
sub MMV_TIME_SEC	{ 3; }  # seconds
sub MMV_TIME_MIN	{ 4; }  # minutes
sub MMV_TIME_HOUR	{ 5; }  # hours

# units - count scale   (for metrics such as count events, syscalls,
# interrupts, etc - these are simply powers of ten and not enumerated here
# (e.g. 6 for 10^6, or -3 for 10^-3).
sub MMV_COUNT_ONE	{ 0; }  # 1

# semantics/interpretation of metric values
sub MMV_SEM_COUNTER	{ 1; }	# cumulative counter, monotonic increasing
sub MMV_SEM_INSTANT	{ 3; }	# instantaneous value, continuous domain
sub MMV_SEM_DISCRETE	{ 4; }	# instantaneous value, discrete domain


bootstrap PCP::MMV $VERSION;

1;
__END__

=head1 NAME

PCP::MMV - Perl module for Memory Mapped Value instrumentation

=head1 SYNOPSIS

  use PCP::MMV;

=head1 DESCRIPTION

The PCP::MMV Perl module contains the language bindings for
building Perl programs instrumented with the Performance Co-Pilot
Memory Mapped Value infrastructure - an efficient data transport
mechanism for making performance data from within a Perl program
available as PCP metrics using the MMV PMDA.

=head1 SEE ALSO

mmv_stats_init(3), mmv_inc_value(3), mmv_lookup_value_desc(3),
mmv(4) and pmda(3).

The PCP mailing list pcp@oss.sgi.com can be used for questions about
this module.

Further details can be found at http://www.pcp.io

=head1 AUTHOR

Nathan Scott, E<lt>nathans@debian.orgE<gt>

Copyright (C) 2009 by Aconex.

This library is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License, version 2 (see
the "COPYING" file in the PCP source tree for further details).

=cut
