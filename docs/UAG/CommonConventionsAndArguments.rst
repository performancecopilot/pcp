.. _CommonConventionsAndArguments:

Common Conventions and Arguments
#################################

.. contents::

This chapter deals with the user interface components that are common to most text-based utilities that make up the monitor portion of Performance Co-Pilot (PCP). 
These are the major sections in this chapter:

Section 3.1, “`Alternate Metrics Source Options`_”, details some basic standards used in the development of PCP tools.

Section 3.2, “`General PCP Tool Options`_”, details other options to use with PCP tools.

Section 3.3, “`Time Duration and Control`_”, describes the time control dialog and time-related command line options available for use with PCP tools.

Section 3.4, “`PCP Environment Variables`_”, describes the environment variables supported by PCP tools.

Section 3.5, “`Running PCP Tools through a Firewall`_”, describes how to execute PCP tools that must retrieve performance data from the Performance Metrics Collection Daemon (PMCD) 
on the other side of a TCP/IP security firewall.

Section 3.6, “`Transient Problems with Performance Metric Values`_”, covers some uncommon scenarios that may compromise performance metric integrity over the short term.

Many of the utilities provided with PCP conform to a common set of naming and syntactic conventions for command line arguments and options. 
This section outlines these conventions and their meaning. The options may be generally assumed to be honored for all utilities supporting the 
corresponding functionality.

In all cases, the man pages for each utility fully describe the supported command arguments and options.

Command line options are also relevant when starting PCP applications from the desktop using the **Alt** double-click method. This technique 
launches the **pmrun** program to collect additional arguments to pass along when starting a PCP application.

Alternate Metrics Source Options
**********************************

The default source of performance metrics is from PMCD on the local host. This default **pmcd** connection will be made using the Unix domain socket, 
if the platform supports that, else a localhost Inet socket connection is made. This section describes how to obtain metrics from sources other than this default.

Fetching Metrics from Another Host
====================================

The option **-h** *host* directs any PCP utility (such as **pmchart** or **pmie**) to make a connection with the PMCD instance running on *host*. 
Once established, this connection serves as the principal real-time source of performance metrics and metadata. The *host* specification may be more than 
a simple host name or address - it can also contain decorations specifying protocol type (secure or not), authentication information, and other connection 
attributes. Refer to the **PCPIntro(1)** man page for full details of these, and examples of use of these specifications can also be found in the 
*PCP Tutorials and Case Studies* companion document.

⁠Fetching Metrics from an Archive
======================================

The option **-a** *archive* directs the utility to treat the set of PCP archives designated by archive as the principal source of performance metrics 
and metadata. archive is a comma-separated list of names, each of which may be the base name of an archive or the name of a directory containing archives.

PCP archives are created with **pmlogger**. Most PCP utilities operate with equal facility for performance information coming from either a real-time 
feed via PMCD on some host, or for historical data from a set of PCP archives. For more information on archives and their use, see Chapter 6, Archive Logging.

The list of names (**archive**) used with the **-a** option implies the existence of the files created automatically by **pmlogger**, as listed in Table 3.1, “Physical Filenames for Components of a PCP Archive”.

**Table 3.1. Physical Filenames for Components of a PCP Archive**


+-----------------------+--------------------------------------------------------------------------------------------+
| Filename              | Contents                                                                                   |
+=======================+============================================================================================+
| **archive.** *index*  | Temporal index for rapid access to archive contents                                        |
+-----------------------+--------------------------------------------------------------------------------------------+
| **archive.** *meta*   | Metadata descriptions for performance metrics and instance domains appearing in the archive|
+-----------------------+--------------------------------------------------------------------------------------------+
| **archive.N**         | Volumes of performance metrics values, for **N** = 0,1,2,...                               |
+-----------------------+--------------------------------------------------------------------------------------------+



Most tools are able to concurrently process multiple PCP archives (for example, for retrospective analysis of performance across multiple hosts), 
and accept either multiple **-a** options or a comma separated list of archive names following the **-a** option.

.. note:: 
 The **-h** and **-a** options are almost always mutually exclusive. Currently, **pmchart** is the exception to this rule but other tools may continue to blur this line in the future.

General PCP Tool Options
**************************
The following sections provide information relevant to most of the PCP tools. It is presented here in a single place for convenience.

⁠Common Directories and File Locations
=======================================

The following files and directories are used by the PCP tools as repositories for option and configuration files and for binaries:

``${PCP_DIR}/etc/pcp.env``

Script to set PCP run-time environment variables.

``${PCP_DIR}/etc/pcp.conf``

PCP configuration and environment file.

``${PCP_PMCDCONF_PATH}``

Configuration file for Performance Metrics Collection Daemon (PMCD). Sets environment variables, including **PATH**.

``${PCP_BINADM_DIR}/pmcd``

The PMCD binary.

``${PCP_PMCDOPTIONS_PATH}``

Command line options for PMCD.

``${PCP_RC_DIR}/pmcd``

The PMCD startup script.

``${PCP_BIN_DIR}/pcptool``

Directory containing PCP tools such as **pmstat , pminfo, pmlogger, pmlogsummary, pmchart, pmie,** and so on.

``${PCP_SHARE_DIR}``

Directory containing shareable PCP-specific files and repository directories such as **bin, demos, examples** and **lib**.

``${PCP_VAR_DIR}``

Directory containing non-shareable (that is, per-host) PCP specific files and repository directories.

``${PCP_BINADM_DIR}/pcptool``

PCP tools that are typically not executed directly by the end user such as **pmcd_wait**.

``${PCP_SHARE_DIR}/lib/pcplib``

Miscellaneous PCP libraries and executables.

``${PCP_PMDAS_DIR}``

Performance Metric Domain Agents (PMDAs), one directory per PMDA.

``${PCP_VAR_DIR}/config``

Configuration files for PCP tools, typically with one directory per tool.

``${PCP_DEMOS_DIR}``

Demonstration data files and example programs.

``${PCP_LOG_DIR}``

By default, diagnostic and trace log files generated by PMCD and PMDAs. Also, the PCP archives are managed in one directory per logged host below here.

``${PCP_VAR_DIR}/pmns``

Files and scripts for the Performance Metrics Name Space (PMNS).

Alternate Performance Metric Name Spaces
==============================================

The Performance Metrics Name Space (PMNS) defines a mapping from a collection of human-readable names for performance metrics (convenient to the user) into 
corresponding internal identifiers (convenient for the underlying implementation).

The distributed PMNS used in PCP avoids most requirements for an alternate PMNS, because clients' PMNS operations are supported at the Performance Metrics 
Collection Daemon (PMCD) or by means of PMNS data in a PCP archive. The distributed PMNS is the default, but alternates may be specified using the **-n** 
*namespace* argument to the PCP tools. When a PMNS is maintained on a host, it is likely to reside in the ``${PCP_VAR_DIR}/pmns`` directory.

Time Duration and Control
**************************

The periodic nature of sampling performance metrics and refreshing the displays of the PCP tools makes specification and control of the temporal domain a 
common operation. In the following sections, the services and conventions for specifying time positions and intervals are described.

⁠Performance Monitor Reporting Frequency and Duration
=====================================================

Many of the performance monitoring utilities have periodic reporting patterns. The **-t**  *interval* and **-s** *samples* options are used to control 
the sampling (reporting) interval, usually expressed as a real number of seconds (*interval*), and the number of samples to be reported, respectively. 
In the absence of the **-s** flag, the default behavior is for the performance monitoring utilities to run until they are explicitly stopped.

The *interval* argument may also be expressed in terms of minutes, hours, or days, as described in the **PCPIntro(1)** man page.

⁠Time Window Options
=====================

The following options may be used with most PCP tools (typically when the source of the performance metrics is a PCP archive) to tailor the beginning 
and end points of a display, the sample origin, and the sample time alignment to your convenience.

The **-S, -T, -O** and **-A** command line options are used by PCP applications to define a time window of interest.

**-S**  *duration*

The start option may be used to request that the display start at the nominated time. By default, the first sample of performance data is retrieved 
immediately in real-time mode, or coincides with the first sample of data of the first archive in a set of PCP archives in archive mode. For archive 
mode, the **-S** option may be used to specify a later time for the start of sampling. By default, if duration is an integer, the units are assumed to be 
seconds.

To specify an offset from the beginning of a set of PCP archives (in archive mode) simply specify the offset as the *duration*. For example, the following 
entry retrieves the first sample of data at exactly 30 minutes from the beginning of a set of PCP archives:

.. sourcecode:: none

 -S 30min

To specify an offset from the end of a set of PCP archives, prefix the *duration* with a minus sign. In this case, the first sample time precedes 
the end of archived data by the given *duration*. For example, the following entry retrieves the first sample exactly one hour preceding the last sample 
in a set of PCP archives:

.. sourcecode:: none

 -S -1hour

To specify the calendar date and time (local time in the reporting timezone) for the first sample, use the **ctime(3)** syntax preceded by an "at" 
sign (@). For example, the following entry specifies the date and time to be used:

.. sourcecode:: none

 -S '@ Mon Mar 4 13:07:47 2017'

Note that this format corresponds to the output format of the **date** command for easy "cut and paste." However, be sure to enclose the string in quotes 
so it is preserved as a single argument for the PCP tool.

For more complete information on the date and time syntax, see the **PCPIntro(1)** man page.

**-T**  *duration*

The terminate option may be used to request that the display stop at the time designated by *duration*. By default, the PCP tools keep sampling performance 
data indefinitely (in real-time mode) or until the end of a set of PCP archives (in archive mode). The **-T** option may be used to specify an earlier time to terminate sampling.

The interpretation for the *duration* argument in a **-T** option is the same as for the **-S** option, except for an unsigned time interval that is 
interpreted as being an offset from the start of the time window as defined by the default (now for real time, else start of archive set) or by a **-S** 
option. For example, these options define a time window that spans 45 minutes, after an initial offset (or delay) of 1 hour:: 

 -S 1hour -T 45mins

**-O**  *duration*

By default, samples are fetched from the start time (see the description of the **-S** option) to the terminate time (see the description of the **-T** 
option). The offset **-O** option allows the specification of a time between the start time and the terminate time where the tool should position its 
initial sample time. This option is useful when initial attention is focused at some point within a larger time window of interest, or when one PCP tool 
wishes to launch another PCP tool with a common current point of time within a shared time window.

The *duration* argument accepted by **-O** conforms to the same syntax and semantics as the *duration* argument for **-T**. For example, these options 
specify that the initial position should be the end of the time window::

 -O -0

This is most useful with the **pmchart** command to display the tail-end of the history up to the end of the time window.

**-A**  *alignment*

By default, performance data samples do not necessarily happen at any natural unit of measured time. The **-A** switch may be used to force the initial 
sample to be on the specified *alignment*. For example, these three options specify alignment on seconds, half hours, and whole hours:

.. sourcecode:: none

 -A 1sec 
 -A 30min 
 -A 1hour

The **-A** option advances the time to achieve the desired alignment as soon as possible after the start of the time window, whether this is the default 
window, or one specified with some combination of **-A** and **-O** command line options.

Obviously the time window may be overspecified by using multiple options from the set **-t, -s, -S, -T, -A,** and **-O**. Similarly, the time window 
may shrink to nothing by injudicious choice of options.

In all cases, the parsing of these options applies heuristics guided by the principal of "least surprise"; the time window is always well-defined (with the end never earlier than the start), but may shrink to nothing in the extreme.

Timezone Options
================

All utilities that report time of day use the local timezone by default. The following timezone options are available:

**-z**

Forces times to be reported in the timezone of the host that provided the metric values (the PCP collector host). When used in conjunction with **-a** 
and multiple archives, the convention is to use the timezone from the first named archive.

**-Z**  *timezone*

Sets the TZ variable to a timezone string, as defined in **environ(7)**, for example, **-Z UTC** for universal time.

PCP Environment Variables
*************************

When you are using PCP tools and utilities and are calling PCP library functions, a standard set of defined environment variables are available in the 
``${PCP_DIR}/etc/pcp.conf`` file. These variables are generally used to specify the location of various PCP pieces in the file system and may be loaded 
into shell scripts by sourcing the ``${PCP_DIR}/etc/pcp.env`` shell script. They may also be queried by C, C++, perl and python programs using the 
**pmGetConfig** library function. If a variable is already defined in the environment, the values in the **pcp.conf** file do not override those values; 
that is, the values in pcp.conf serve only as installation defaults. For additional information, see the **pcp.conf(5)**, **pcp.env(5)**, and **pmGetConfig(3)** man pages.

The following environment variables are recognized by PCP (these definitions are also available on the **PCPIntro(1)** man page):

**PCP_COUNTER_WRAP**

Many of the performance metrics exported from PCP agents expect that counters increase monotonically. Under some circumstances, one value of a metric may be smaller than the previously fetched value. This can happen when a counter of finite precision overflows, when the PCP agent has been reset or restarted, or when the PCP agent exports values from an underlying instrumentation that is subject to asynchronous discontinuity.

If set, the **PCP_COUNTER_WRAP** environment variable indicates that all such cases of a decreasing counter should be treated as a counter overflow; and hence the values are assumed to have wrapped once in the interval between consecutive samples. Counter wrapping was the default in versions before the PCP release 1.3.

**PCP_STDERR**

Specifies whether **pmprintf()** error messages are sent to standard error, an **pmconfirm** dialog box, or to a named file; see the **pmprintf(3)** 
man page. Messages go to standard error if **PCP_STDERR** is unset or set without a value. If this variable is set to **DISPLAY**, then messages go to 
an **pmconfirm** dialog box; see the **pmconfirm(1)** man page. Otherwise, the value of **PCP_STDERR** is assumed to be the name of an output file.

**PMCD_CONNECT_TIMEOUT**

When attempting to connect to a remote PMCD on a system that is booting or at the other end of a slow network link, some PMAPI routines could potentially block for a long time until the remote system responds. These routines abort and return an error if the connection has not been established after some specified interval has elapsed. The default interval is 5 seconds. This may be modified by setting this variable in the environment to a larger number of seconds for the desired time out. This is most useful in cases where the remote host is at the end of a slow network, requiring longer latencies to establish the connection correctly.

**PMCD_PORT**

This TCP/IP port is used by PMCD to create the socket for incoming connections and requests. The default is port number 44321, which you may override by setting this variable to a different port number. If a non-default port is in effect when PMCD is started, then every monitoring application connecting to that PMCD must also have this variable set in its environment before attempting a connection.

**PMCD_LOCAL**

This setting indicates that PMCD must only bind to the loopback interface for incoming connections and requests. In this mode, connections from remote hosts are not possible.

**PMCD_RECONNECT_TIMEOUT**

When a monitor or client application loses its connection to a PMCD, the connection may be reestablished by calling the **pmReconnectContext(3)** PMAPI 
function. However, attempts to reconnect are controlled by a back-off strategy to avoid flooding the network with reconnection requests. By default, 
the back-off delays are 5, 10, 20, 40, and 80 seconds for consecutive reconnection requests from a client (the last delay is repeated for any further 
attempts after the last delay in the list). Setting this environment variable to a comma-separated list of positive integers redefines the back-off delays. 
For example, setting the delays to **1,2** will back off for 1 second, then back off every 2 seconds thereafter.

**PMCD_REQUEST_TIMEOUT**

For monitor or client applications connected to PMCD, there is a possibility of the application hanging on a request for performance metrics or metadata or help text. These delays may become severe if the system running PMCD crashes or the network connection is lost or the network link is very slow. By setting this environment variable to a real number of seconds, requests to PMCD timeout after the specified number of seconds. The default behavior is to wait 10 seconds for a response from every PMCD for all applications.

**PMLOGGER_PORT**

This environment variable may be used to change the base TCP/IP port number used by **pmlogger** to create the socket to which **pmlc** instances try 
to connect. The default base port number is 4330. If used, this variable should be set in the environment before **pmlogger** is executed. If **pmlc** 
and **pmlogger** are on different hosts, then obviously **PMLOGGER_PORT** must be set to the same value in both places.

**PMLOGGER_LOCAL**

This environment variable indicates that **pmlogger** must only bind to the loopback interface for **pmlc** connections and requests. In this mode, **pmlc** 
connections from remote hosts are not possible. If used, this variable should be set in the environment before **pmlogger** is executed.

**PMPROXY_PORT**
This environment variable may be used to change the base TCP/IP port number used by **pmproxy** to create the socket to which proxied clients connect, 
on their way to a distant **pmcd**.

**PMPROXY_LOCAL**

This setting indicates that **pmproxy** must only bind to the loopback interface for incoming connections and requests. In this mode, connections from remote hosts are not possible.

Running PCP Tools through a Firewall
************************************

In some production environments, the Performance Co-Pilot (PCP) monitoring hosts are on one side of a TCP/IP firewall, and the PCP collector hosts may be on the other side.

If the firewall service sits between the monitor and collector tools, the **pmproxy** service may be used to perform both packet forwarding and DNS 
proxying through the firewall; see the **pmproxy(1)** man page. Otherwise, it is necessary to arrange for packet forwarding to be enabled for those 
TCP/IP ports used by PCP, namely 44321 (or the value of the **PMCD_PORT** environment variable) for connections to PMCD.

⁠The pmproxy service
======================

The **pmproxy** service allows PCP clients running on hosts located on one side of a firewall to monitor remote hosts on the other side. The basic 
connection syntax is as follows, where *tool* is an arbitrary PCP application, typically a monitoring tool:

.. sourcecode:: none

 pmprobe -h remotehost@proxyhost

This extended host specification syntax is part of a larger set of available extensions to the basic host naming syntax - refer to the **PCPIntro(1)** man page for further details.

Transient Problems with Performance Metric Values
*************************************************

Sometimes the values for a performance metric as reported by a PCP tool appear to be incorrect. This is typically caused by transient conditions such as metric wraparound or time skew, described below. These conditions result from design decisions that are biased in favor of lightweight protocols and minimal resource demands for PCP components.

In all cases, these events are expected to occur infrequently, and should not persist beyond a few samples.

Performance Metric Wraparound
==============================

Performance metrics are usually expressed as numbers with finite precision. For metrics that are cumulative counters of events or resource consumption, the value of the metric may occasionally overflow the specified range and wraparound to zero.

Because the value of these counter metrics is computed from the rate of change with respect to the previous sample, this may result in a transient 
condition where the rate of change is an unknown value. If the **PCP_COUNTER_WRAP** environment variable is set, this condition is treated as an overflow, and speculative rate calculations are made. In either case, the correct rate calculation for the metric returns with the next sample.

Time Dilation and Time Skew
===========================

If a PMDA is tardy in returning results, or the PCP monitoring tool is connected to PMCD via a slow or congested network, an error might be introduced in rate calculations due to a difference between the time the metric was sampled and the time PMCD sends the result to the monitoring tool.

In practice, these errors are usually so small as to be insignificant, and the errors are self-correcting (not cumulative) over consecutive samples.

A related problem may occur when the system time is not synchronized between multiple hosts, and the time stamps for the results returned from PMCD 
reflect the skew in the system times. In this case, it is recommended that NTP (network time protocol) be used to keep the system clocks on the collector 
systems synchronized; for information on NTP refer to the **ntpd(1)** man page.
