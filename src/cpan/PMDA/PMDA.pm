package PCP::PMDA;

use strict;
use warnings;
use vars qw($VERSION @ISA @EXPORT @EXPORT_OK);

require Exporter;
require DynaLoader;

@ISA = qw(Exporter DynaLoader);
@EXPORT = qw(
    pmda_pmid pmda_pmid_name pmda_pmid_text pmda_inst_name pmda_inst_lookup
    pmda_units pmda_config pmda_uptime pmda_long pmda_ulong
    PM_ID_NULL PM_INDOM_NULL PM_IN_NULL
    PM_SPACE_BYTE PM_SPACE_KBYTE PM_SPACE_MBYTE PM_SPACE_GBYTE PM_SPACE_TBYTE
    PM_TIME_NSEC PM_TIME_USEC PM_TIME_MSEC PM_TIME_SEC PM_TIME_MIN PM_TIME_HOUR
    PM_COUNT_ONE
    PM_TYPE_NOSUPPORT PM_TYPE_32 PM_TYPE_U32 PM_TYPE_64 PM_TYPE_U64
	PM_TYPE_FLOAT PM_TYPE_DOUBLE PM_TYPE_STRING
    PM_SEM_COUNTER PM_SEM_INSTANT PM_SEM_DISCRETE
    PM_ERR_GENERIC PM_ERR_PMNS PM_ERR_NOPMNS PM_ERR_DUPPMNS PM_ERR_TEXT
	PM_ERR_APPVERSION PM_ERR_VALUE PM_ERR_TIMEOUT
	PM_ERR_NODATA PM_ERR_RESET PM_ERR_NAME PM_ERR_PMID
	PM_ERR_INDOM PM_ERR_INST PM_ERR_UNIT PM_ERR_CONV PM_ERR_TRUNC
	PM_ERR_SIGN PM_ERR_PROFILE PM_ERR_IPC PM_ERR_EOF
	PM_ERR_NOTHOST PM_ERR_EOL PM_ERR_MODE PM_ERR_LABEL PM_ERR_LOGREC
	PM_ERR_NOTARCHIVE PM_ERR_NOCONTEXT PM_ERR_PROFILESPEC PM_ERR_PMID_LOG
	PM_ERR_INDOM_LOG PM_ERR_INST_LOG PM_ERR_NOPROFILE PM_ERR_NOAGENT
	PM_ERR_PERMISSION PM_ERR_CONNLIMIT PM_ERR_AGAIN PM_ERR_ISCONN
	PM_ERR_NOTCONN PM_ERR_NEEDPORT PM_ERR_NONLEAF
	PM_ERR_PMDANOTREADY PM_ERR_PMDAREADY
	PM_ERR_TOOSMALL PM_ERR_TOOBIG PM_ERR_NYI
);
@EXPORT_OK = qw();
$VERSION = '1.14';

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

# error codes
sub PM_ERR_GENERIC	{ -12345; }	# Generic error, already reported above
sub PM_ERR_PMNS		{ -12346; }	# Problems parsing PMNS definitions
sub PM_ERR_NOPMNS	{ -12347; }	# PMNS not accessible
sub PM_ERR_DUPPMNS	{ -12348; }	# Attempt to reload the PMNS
sub PM_ERR_TEXT		{ -12349; }	# Oneline or help text is not available
sub PM_ERR_APPVERSION	{ -12350; }	# Metric not supported by this version of monitored application
sub PM_ERR_VALUE	{ -12351; }	# Missing metric value(s)
sub PM_ERR_TIMEOUT	{ -12353; }	# Timeout waiting for a response from PMCD
sub PM_ERR_NODATA	{ -12354; }	# Empty archive log file
sub PM_ERR_RESET	{ -12355; }	# PMCD reset or configuration change
sub PM_ERR_NAME		{ -12357; }	# Unknown metric name
sub PM_ERR_PMID		{ -12358; }	# Unknown or illegal metric identifier
sub PM_ERR_INDOM	{ -12359; }	# Unknown or illegal instance domain identifier
sub PM_ERR_INST		{ -12360; }	# Unknown or illegal instance identifier
sub PM_ERR_UNIT		{ -12361; }	# Illegal pmUnits specification
sub PM_ERR_CONV		{ -12362; }	# Impossible value or scale conversion
sub PM_ERR_TRUNC	{ -12363; }	# Truncation in value conversion
sub PM_ERR_SIGN		{ -12364; }	# Negative value in conversion to unsigned
sub PM_ERR_PROFILE	{ -12365; }	# Explicit instance identifier(s) required
sub PM_ERR_IPC		{ -12366; }	# IPC protocol failure
sub PM_ERR_EOF		{ -12368; }	# IPC channel closed
sub PM_ERR_NOTHOST	{ -12369; }	# Operation requires context with host source of metrics
sub PM_ERR_EOL		{ -12370; }	# End of PCP archive log
sub PM_ERR_MODE		{ -12371; }	# Illegal mode specification
sub PM_ERR_LABEL	{ -12372; }	# Illegal label record at start of a PCP archive log file
sub PM_ERR_LOGREC	{ -12373; }	# Corrupted record in a PCP archive log
sub PM_ERR_NOTARCHIVE	{ -12374; }	# Operation requires context with archive source of metrics
sub PM_ERR_NOCONTEXT	{ -12376; }	# Attempt to use an illegal context
sub PM_ERR_PROFILESPEC	{ -12377; }	# NULL pmInDom with non-NULL instlist
sub PM_ERR_PMID_LOG	{ -12378; }	# Metric not defined in the PCP archive log
sub PM_ERR_INDOM_LOG	{ -12379; }	# Instance domain identifier not defined in the PCP archive log
sub PM_ERR_INST_LOG	{ -12380; }	# Instance identifier not defined in the PCP archive log
sub PM_ERR_NOPROFILE	{ -12381; }	# Missing profile - protocol botch
sub PM_ERR_NOAGENT	{ -12386; }	# No PMCD agent for domain of request
sub PM_ERR_PERMISSION	{ -12387; }	# No permission to perform requested operation

sub PM_ERR_CONNLIMIT	{ -12388; }	# PMCD connection limit for this host exceeded
sub PM_ERR_AGAIN	{ -12389; }	# Try again. Information not currently available
sub PM_ERR_ISCONN	{ -12390; }	# Already Connected
sub PM_ERR_NOTCONN	{ -12391; }	# Not Connected
sub PM_ERR_NEEDPORT	{ -12392; }	# A non-null port name is required
sub PM_ERR_NONLEAF	{ -12394; }	# Metric name is not a leaf in PMNS
sub PM_ERR_PMDANOTREADY	{ -13394; }	# PMDA is not yet ready to respond to requests
sub PM_ERR_PMDAREADY	{ -13393; }	# PMDA is now responsive to requests
sub PM_ERR_TOOSMALL	{ -12443; }	# Insufficient elements in list
sub PM_ERR_TOOBIG	{ -12444; }	# Result size exceeded
sub PM_ERR_NYI		{ -21344; }	# Functionality not yet implemented


bootstrap PCP::PMDA $VERSION;

1;
__END__

=head1 NAME

PCP::PMDA - Perl extension for Performance Metrics Domain Agents

=head1 SYNOPSIS

  use PCP::PMDA;

=head1 DESCRIPTION

The PCP::PMDA Perl module contains the language bindings for
building Performance Metric Domain Agents (PMDAs) using Perl.
Each PMDA exports performance data for one specific domain, for
example the operating system kernel, Cisco routers, a database,
an application, etc.

=head1 SEE ALSO

perl(1) and pcpintro(3).

The PCP mailing list pcp@oss.sgi.com can be used for questions about
this module.

Further details can be found at http://oss.sgi.com/projects/pcp

=head1 AUTHOR

Nathan Scott, E<lt>nathans@debian.orgE<gt>

Copyright (C) 2008-2010 by Aconex.
Copyright (C) 2004 by Silicon Graphics, Inc.

This library is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License, version 2 (see
the "COPYING" file in the PCP source tree for further details).

=cut
