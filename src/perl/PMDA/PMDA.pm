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
	PM_ERR_INDOM PM_ERR_INST PM_ERR_TYPE
	PM_ERR_UNIT PM_ERR_CONV PM_ERR_TRUNC
	PM_ERR_SIGN PM_ERR_PROFILE PM_ERR_IPC PM_ERR_EOF
	PM_ERR_NOTHOST PM_ERR_EOL PM_ERR_MODE PM_ERR_LABEL PM_ERR_LOGREC
	PM_ERR_LOGFILE PM_ERR_NOTARCHIVE
	PM_ERR_NOCONTEXT PM_ERR_PROFILESPEC PM_ERR_PMID_LOG
	PM_ERR_INDOM_LOG PM_ERR_INST_LOG PM_ERR_NOPROFILE PM_ERR_NOAGENT
	PM_ERR_PERMISSION PM_ERR_CONNLIMIT PM_ERR_AGAIN PM_ERR_ISCONN
	PM_ERR_NOTCONN PM_ERR_NEEDPORT PM_ERR_NONLEAF
	PM_ERR_PMDANOTREADY PM_ERR_PMDAREADY
	PM_ERR_TOOSMALL PM_ERR_TOOBIG PM_ERR_FAULT
	PM_ERR_THREAD PM_ERR_NOCONTAINER PM_ERR_BADSTORE
	PM_ERR_NYI
);
@EXPORT_OK = qw();
$VERSION = '1.15';

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
sub PM_SPACE_PBYTE	{ 5; }	# petabytes
sub PM_SPACE_EBYTE	{ 6; }	# exabytes

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
sub PM_TYPE_AGGREGATE	{ 7; }	# arbitrary binary data (aggregate)
sub PM_TYPE_AGGREGATE_STATIC { 8; } # static pointer to aggregate
sub PM_TYPE_EVENT	{ 9; }	# packed pmEventArray
sub PM_TYPE_UNKNOWN	{ 255; }

# semantics/interpretation of metric values
sub PM_SEM_COUNTER	{ 1; }	# cumulative counter (monotonic increasing)
sub PM_SEM_INSTANT	{ 3; }	# instantaneous value, continuous domain
sub PM_SEM_DISCRETE	{ 4; }	# instantaneous value, discrete domain

# error codes
# for ease of maintenance make the order of the error codes
# here the same as the output from pmerr -l
#
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
sub PM_ERR_TYPE		{ -12397; }	# Unknown or illegal metric type
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
sub PM_ERR_LOGFILE	{ -12375; }	# Missing PCP archive log file
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
sub PM_ERR_FAULT	{ -12445; }	# QA fault injected
sub PM_ERR_THREAD	{ -12398; }	# Operation not supported for multi-threaded applications
sub PM_ERR_NOCONTAINER	{ -12399; }	# Container not found 
sub PM_ERR_BADSTORE	{ -12400; }	# Bad input to pmstore
sub PM_ERR_NYI		{ -21344; }	# Functionality not yet implemented


bootstrap PCP::PMDA $VERSION;

1;
__END__

=head1 NAME

PCP::PMDA - Perl extension for Performance Metrics Domain Agents

=head1 SYNOPSIS

  use PCP::PMDA;

  $pmda = PCP::PMDA->new('myname', $MYDOMAIN);

  $pmda->connect_pmcd;

  $pmda->add_metric($pmid, $type, $indom, $sem, $units, 'name', '', '');
  $pmda->add_indom($indom, [0 => 'white', 1 => 'black', ...], '', '');

  $pmda->set_fetch(\&fetch_method);
  $pmda->set_refresh(\&refresh_method);
  $pmda->set_instance(\&instance_method);
  $pmda->set_fetch_callback(\&fetch_callback_method);
  $pmda->set_store_callback(\&store_callback_method);

  $pmda->set_user('pcp');

  $pmda->run;

=head1 DESCRIPTION

The PCP::PMDA Perl module contains the language bindings for
building Performance Metric Domain Agents (PMDAs) using Perl.
Each PMDA exports performance data for one specific domain, for
example the operating system kernel, Cisco routers, a database,
an application, etc.

=head1 METHODS

=over

=item PCP::PMDA->new(name, domain)

PCP::PMDA class constructor.  I<name> is a string that becomes the
name of the PMDA for messages and default prefix for the names of
external files used by the PMDA.  I<domain> is an integer domain
number for the PMDA, usually from the register of domain numbers
found in B<$PCP_VAR_DIR/pmns/stdpmid>.

=item $pmda->run()

Once all local setup is complete (i.e. instance domains and metrics
are registered, callbacks registered - as discussed below) the PMDA
must connect to B<pmcd>(1) to complete its initialisation and begin
answering client requests for its metrics.  This is the role performed
by I<run>, and upon invoking it all interaction within the PMDA is
done via callback routines (that is to say, under normal cicrumstances,
the I<run> routine does not return).

The behaviour of the I<run> method is different in the presence of
either the B<PCP_PERL_PMNS> or B<PCP_PERL_DOMAIN> environment variables.
These can be used to generate the namespace or domain number files,
which are used as part of the PMDA installation process.

=item $pmda->connect_pmcd()

Allows the PMDA to set up the IPC channel to B<pmcd>(1) and complete
the credentials handshake with B<pmcd>(1).  If I<connect_pmcd> is not
explicitly called the setup and handshake will be done when the
I<run> method is called.

The advantage of explicitly calling I<connect_pmcd> early in the life
of the PMDA is that this reduces the risk of a fatal timeout during
the credentials handshake, which may be an issue if the PMDA has
considerable work to do, e.g. determining which metrics and
instance domains are available, before calling I<run>.

=item $pmda->add_indom(indom, insts, help, longhelp)

Define a new instance domain.  The instance domain identifier is
I<indom>, which is an integer and unique across all instance domains
for single PMDA.

The instances of the instance domain are defined by I<insts> which
can be specified as either a list or a hash.

In list form, the contents of the list must provide consecutive pairs
of identifier (a integer, unique across all instances in the instance
domain) and external instance name (a string, must by unique up to the
first space, if any, across all instances in the instance domain).
For example:

 @colours = [0 => 'red', 1 => 'green', 2 => 'blue'];

In hash form, the external instance identifier (string) is used as the
hash key.  An arbitrary value can be stored along with the key (this
value is often used as a convenient place to hold the latest value for
each metric instance, for example).

 %timeslices = ('sec' => 42, 'min' => \&min_func, 'hour' => '0');

The I<help> and I<longhelp> strings are interpreted as the one-line and
expanded help text to be used for this instance domain as further
described in B<pmLookupInDomText>(3).

Refer also to the B<replace_indom>() discussion below for further details
about manipulating instance domains.

=item $pmda->add_metric(pmid, type, indom, sem, units, name, help, longhelp)

Define a new metric identified by the PMID I<pmid> and the full
metric name I<name>.

The metric's metadata is defined by I<type>, I<indom>, I<sem> and
I<units> and these parameters are used to set up the I<pmDesc>
structure as described in B<pmLookupDesc>(3).

The I<help> and I<longhelp> strings are interpreted as the one-line
and expanded help text to be used for the metric as further described
in B<pmLookupText>(3).

=item $pmda->replace_indom(index, insts)

Whenever an instance domain identified by I<index>,
previously registered using B<add_indom>(),
changes in any way, this change must be reflected by replacing the
existing mapping with a new one (I<insts>).

The replacement mapping must be a hash if the instance domain 
was registered initially with B<add_indom>() as a hash, otherwise it must be
a list.

Refer to the earlier B<add_indom>() discussion concerning these two
different types of instance domains definitions.

=item $pmda->add_pipe(command, callback, data)

Allow data to be injected into the PMDA using a B<pipe>(2).

The given I<command> is run early in the life of the PMDA, and a pipe
is formed between the PMDA and the I<command>.  Line-oriented output
is assumed (else truncation will occur), and on receipt of each line
of text on the pipe, the I<callback> function will be called.

The optional I<data> parameter can be used to specify extra data to
pass into the I<callback> routine.

=item $pmda->add_sock(hostname, port, callback, data)

Create a B<socket>(2) connection to the I<hostname>, I<port> pair.
Whenever data arrives (as above, a line-oriented protocol is best)
the I<callback> function will be called.

The optional I<data> parameter can be used to specify extra data to
pass into the I<callback> routine.

An opaque integer-sized identifier for the socket will be returned,
which can later be used in calls to B<put_sock>() as discussed below.

=item $pmda->put_sock(id, output)

Write an I<output> string to the socket identified by I<id>, which
must refer to a socket previously registered using B<add_sock>().

=item $pmda->add_tail(filename, callback, data)

Monitor the given I<filename> for the arrival of newly appended
information.  Line-oriented input is assumed (else truncation
will occur), and on receipt of each line of text on the pipe,
the I<callback> function will be called.

The optional I<data> parameter can be used to specify extra data to
pass into the I<callback> routine.

This interface deals with the issue of the file being renamed (such
as on daily log file rotation), and will attempt to automatically
re-route information from the new log file if this occurs.

=item $pmda->add_timer(timeout, callback, data)

Registers a timer with the PMDA, such that on expiry of a I<timeout>
a I<callback> routine will be called.  This is a repeating timer.

The optional I<data> parameter can be used to specify extra data to
pass into the I<callback> routine.

=item $pmda->err(message)

Report a timestamped error message into the PMDA log file.

=item $pmda->error(message)

Report a timestamped error message into the PMDA log file.

=item $pmda->log(message)

Report a timestamped informational message into the PMDA log file.

=item $pmda->set_fetch_callback(cb_function)

Register a callback function akin to B<pmdaSetFetchCallBack>(3).

=item $pmda->set_fetch(function)

Register a fetch function, as used by B<pmdaInit>(3).

=item $pmda->set_instance(function)

Register an instance function, as used by B<pmdaInit>(3).

=item $pmda->set_refresh(function)

Register a refresh function, which will be called once per metric
cluster, during the fetch operation.  Only clusters being requested
during this fetch will be refreshed, allowing selective metric value
updates within the PMDA.

=item $pmda->set_store_callback(cb_function)

Register an store function, used indirectly by B<pmdaInit>(3).
The I<cb_function> is called once for each metric/instance pair
into which a B<pmStore>(3) is performed.

=item $pmda->set_inet_socket(port)

Specify the IPv4 socket I<port> to be used to communicate with B<pmcd>(1).

=item $pmda->set_ipv6_socket(port)

Specify the IPv6 socket I<port> to be used to communicate with B<pmcd>(1).

=item $pmda->set_unix_socket(socket_name)

Specify the filesystem I<socket_name> path to be used for communication
with B<pmcd>(1).

=item $pmda->set_user(username)

Run the PMDA under the I<username> user account, instead of the
default (root) user.

=back

=head1 HELPER METHODS

=over

=item pmda_pmid(cluster, item)

Construct a Performance Metric Identifier (PMID) from the domain
number (passed as an argument to the I<new> constructor), the
I<cluster> (an integer in the range 0 to 2^12-1) and the
I<item> (an integer in the range 0 to 2^10-1).

Every performance metric exported from a PMDA must have a unique
PMID.

=item pmda_pmid_name(cluster, item)

Perform a reverse metric identifier to name lookup - given the metric
I<cluster> and I<item> numbers, returns the metric name string.

=item pmda_pmid_text(cluster, item)

Returns the one-line metric help text string - given the metric
I<cluster> and I<item> numbers, returns the help text string.

=item pmda_inst_name(index, instance)

Perform a reverse instance identifier to instance name lookup
for the instance domain identified by I<index>.
Given the
internal I<instance> identifier, returns the external instance name string.

=item pmda_inst_lookup(index, instance)

Given an internal I<instance> identifier (key) for the
instance domain identified by I<index> with an associated indom hash,
return the value associated with that key.
The value can be any scalar value (this includes references, of course,
so complex data structures can be referenced).

=item pmda_units(dim_space, dim_time, dim_count, scale_space, scale_time, scale_count)

Construct a B<pmUnits> structure suitable for registering a metrics metadata
via B<add_metric>().

=item pmda_config(name)

Lookup the value for configuration variable I<name> from the
I</etc/pcp.conf> file,
using B<pmGetConfig>(3).

=item pmda_uptime(now)

Return a human-readable uptime string, based on I<now> seconds since the epoch.

=item pmda_long()

Return either PM_TYPE_32 or PM_TYPE_64 depending on the platform size for a
signed long integer.

=item pmda_ulong()

Return either PM_TYPE_U32 or PM_TYPE_U64 depending on the platform size for an
unsigned long integer.

=back

=head1 MACROS

Most of the PM_* macros from the PCP C headers are available.

For example the I<type> of a metric's value may be directly
specified as one of 
B<PM_TYPE_32>, B<PM_TYPE_U32>, B<PM_TYPE_64>, B<PM_TYPE_U64>,
B<PM_TYPE_FLOAT>, B<PM_TYPE_DOUBLE>, B<PM_TYPE_STRING> or
B<PM_TYPE_NOSUPPORT>.

=head1 DEBUGGING

Perl PMDAs do not follow the B<-D> convention of other PCP applications
for enabling run-time diagnostics and tracing.  Rather the environment
variable B<PCP_PERL_DEBUG> needs to be set to a string value matching
the syntax accepted for the option value for B<-D> elsewhere, see
B<__pmParseDebug>(3).

This requires a little trickery.  The B<pmcd>(1) configuration file
(B<PCP_PMCDCONF_PATH> from I</etc/pcp.conf>) needs hand editing.
This is best demonstrated by example.

Replace this line

 foo  242  pipe  binary  python  /somepath/foo.py

with

 foo  242  pipe  binary  python  \
     sh -c "PCP_PERL_DEBUG=pdu,fetch /usr/bin/python /somepath/foo.py"

=head1 SEE ALSO

perl(1) and PCPIntro(1).

The PCP mailing list pcp@oss.sgi.com can be used for questions about
this module.

Further details can be found at http://www.pcp.io

=head1 AUTHOR

The Performance Co-Pilot development team.

Copyright (C) 2014 Red Hat.
Copyright (C) 2008-2010 Aconex.
Copyright (C) 2004 Silicon Graphics, Inc.

This library is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License, version 2 (see
the "COPYING" file in the PCP source tree for further details).

=cut
