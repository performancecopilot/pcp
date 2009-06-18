package PCP::MMV;

use strict;
use warnings;
use vars qw($VERSION @ISA @EXPORT @EXPORT_OK);

require Exporter;
require DynaLoader;
require PCP::PMDA;

@ISA = qw(Exporter DynaLoader);
@EXPORT = qw(
    MMV_ENTRY_NOSUPPORT
    MMV_ENTRY_I32 MMV_ENTRY_U32
    MMV_ENTRY_I64 MMV_ENTRY_U64
    MMV_ENTRY_FLOAT MMV_ENTRY_DOUBLE
    MMV_ENTRY_STRING MMV_ENTRY_INTEGRAL
    MMV_SEM_COUNTER MMV_SEM_INSTANT MMV_SEM_DISCRETE
);
@EXPORT_OK = qw();
$VERSION = '0.01';

sub MMV_ENTRY_NOSUPPORT	{ 0xffffffff; }	# not implemented in this version
sub MMV_ENTRY_I32	{ 0; }		# 32-bit signed integer
sub MMV_ENTRY_U32	{ 1; }		# 32-bit unsigned integer
sub MMV_ENTRY_I64	{ 2; }		# 64-bit signed integer
sub MMV_ENTRY_U64	{ 3; }		# 64-bit signed integer
sub MMV_ENTRY_FLOAT	{ 4; }		# 32-bit floating point
sub MMV_ENTRY_DOUBLE	{ 5; }		# 64-bit floating point
sub MMV_ENTRY_STRING	{ 6; }		# null-terminated string
sub MMV_ENTRY_INTEGRAL	{ 10; }		# timestamp & number of outstanding

sub MMV_SEM_COUNTER	{ 1; }		# cumulative counter, monotonic increasing
sub MMV_SEM_INSTANT	{ 3; }		# instantaneous value, continuous domain
sub MMV_SEM_DISCRETE	{ 4; }		# instantaneous value, discrete domain

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

mmv(4) and pmda(3).

The PCP mailing list pcp@oss.sgi.com can be used for questions about
this module.

Further details can be found at http://oss.sgi.com/projects/pcp

=head1 AUTHOR

Nathan Scott, E<lt>nathans@debian.orgE<gt>

Copyright (C) 2009 by Aconex.

This library is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License, version 2 (see
the "COPYING" file in the PCP source tree for further details).

=cut
