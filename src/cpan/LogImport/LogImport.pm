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
    pmiDump pmiErrStr pmiUnits
    pmid_build pmInDom_build
    PM_ID_NULL PM_INDOM_NULL PM_IN_NULL
    PM_SPACE_BYTE PM_SPACE_KBYTE PM_SPACE_MBYTE PM_SPACE_GBYTE PM_SPACE_TBYTE
    PM_TIME_NSEC PM_TIME_USEC PM_TIME_MSEC PM_TIME_SEC PM_TIME_MIN PM_TIME_HOUR
    PM_COUNT_ONE
    PM_TYPE_NOSUPPORT PM_TYPE_32 PM_TYPE_U32 PM_TYPE_64 PM_TYPE_U64
	PM_TYPE_FLOAT PM_TYPE_DOUBLE PM_TYPE_STRING
    PM_SEM_COUNTER PM_SEM_INSTANT PM_SEM_DISCRETE
);
%EXPORT_TAGS = qw();
@EXPORT_OK = qw();

# set the version for version checking
$VERSION = '1.00';

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

bootstrap PCP::LogImport $VERSION;

1; # don't forget to return a true value from the file

__END__

=head1 NAME

PCP::LogImport - Test

=head1 SYNOPSIS

  use PCP::LogImport;

=head1 DESCRIPTION

Blah.

=cut
