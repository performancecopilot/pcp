.. include:: ../../refs.rst

PCP Quick Reference Guide
=========================

-  `Introduction <#intro>`__
-  `Installation <#install>`__

   -  `Installing Collector Hosts <#collectors>`__
   -  `Installing Monitor Hosts <#monitors>`__
   -  `Dynamic Host Discovery <#discovery>`__
   -  `Installation Health Check <#healthcheck>`__

-  `System Level Performance Monitoring <#systemlevel>`__

   -  `Monitoring Live Performance Metrics <#live>`__
   -  `Retrospective Performance Analysis <#retro>`__
   -  `Visualizing iostat and sar Data <#visual>`__

-  `Process Level Performance Monitoring <#processes>`__

   -  `Live and Retrospective Process Monitoring <#processmon>`__
   -  `Monitoring "Hot" Processes with Hotproc <#hotproc>`__
   -  `Application Instrumentation <#instrument>`__

-  `Performance Metrics Inference <#pmie>`__
-  `Fast, Scalable Time Series Querying <#pmseries>`__
-  `Web Services <#web>`__

   -  `Performance Metrics REST APIs <#pmproxy>`__
   -  `Web Interface for Performance Metrics <#grafana>`__

-  `Derived Metrics <#derived>`__
-  `Customizing and Extending PCP <#custom>`__
-  `Additional Information <#next>`__

Introduction
============

`Performance Co-Pilot <https://pcp.io/>`__ (PCP) is an open source
framework and toolkit for monitoring, analyzing, and responding to
details of live and historical system performance. PCP has a fully
distributed, plug-in based architecture making it particularly well
suited to centralized analysis of complex environments and systems.
Custom performance metrics can be added using the C, C++, Perl, and
Python interfaces.

This page provides quick instructions how to install and use PCP on a
set of hosts of which one (a monitor host) will be used for monitoring
and analyzing itself and other hosts (collector hosts).

Installation
============

PCP is available on all recent Linux distribution releases, including
Debian/Fedora/RHEL/SUSE/Ubuntu. For other operating systems and
distributions you might want to consider installation `from
sources <https://github.com/performancecopilot/pcp/blob/main/INSTALL.md>`__.

Installing Collector Hosts
~~~~~~~~~~~~~~~~~~~~~~~~~~

+-----------------------------------------------------------------------+
| To install basic PCP tools and services and enable                    |
| collecting performance data on systemd based distributions, run::     |
|                                                                       |
| # yum install pcp  # or apt-get or dnf or zypper                      |
| # systemctl enable --now pmcd pmlogger                                |
+-----------------------------------------------------------------------+

Here we enable the Performance Metrics Collector Daemon
(`pmcd(1) <http://man7.org/linux/man-pages/man1/pmcd.1.html>`__) on the
host which then in turn will control and request metrics on behalf of
clients from various Performance Metrics Domain Agents (PMDAs). The
PMDAs provide the actual data from different components (domains) in the
system, for example from the Linux Kernel PMDA or the NFS Client PMDA.
The default configuration includes over 1000 metrics with negligible
overall overhead when queried. If no queries for metrics are sent to the
agent, it doesn't do anything at all. Local PCP archives will also
be enabled on the host for convenience with
`pmlogger(1) <http://man7.org/linux/man-pages/man1/pmlogger.1.html>`__.

+-----------------------------------------------------------------------+
| To enable PMDAs which are not enabled by default, for                 |
| example the PostgreSQL database PMDA, run the corresponding Install   |
| script::                                                              |
|                                                                       |
| # cd /var/lib/pcp/pmdas/postgresql                                    |
| # ./Install                                                           |
+-----------------------------------------------------------------------+

The client tools will contact local or remote PMCDs as needed,
communication with PMCD over the network uses TCP port 44321 by default.

Installing Monitor Host
~~~~~~~~~~~~~~~~~~~~~~~

The following additional packages can be optionally installed on the
monitoring host to extend the set of monitoring tools from the base pcp
package.

+-------------------------------------------------------------------------------+
| Install various system monitoring tools, graphical                            |
| analysis tools, and documentation::                                           |
|                                                                               |
| # yum install pcp-doc pcp-gui pcp-system-tools  # or apt-get or dnf or zypper |
+-------------------------------------------------------------------------------+

To enable centralized archive collection on the monitoring host, its
pmlogger is configured to fetch performance metrics from collector
hosts. Add each collector host to the pmlogger configuration file
/etc/pcp/pmlogger/control and then restart the pmlogger service on the
monitoring host.

+---------------------------------------------------------------------------------------------------------------+
| Enable recording of metrics from remote host                                                                  |
| **acme.com**::                                                                                                |
|                                                                                                               |
| # echo acme.com n n PCP_LOG_DIR/pmlogger/acme.com -r -T24h10m -c config.acme.com >> /etc/pcp/pmlogger/control |
| # systemctl restart pmlogger                                                                                  |
+---------------------------------------------------------------------------------------------------------------+

The health of the remote log collector will be done every half an hour.
You can also run /usr/libexec/pcp/bin/pmlogger_check -V -C (on
Fedora/RHEL) or /usr/lib/pcp/bin/pmlogger_check -V -C (on Debian/Ubuntu)
manually to do a health check.

Note that a default configuration file (config.acme.com above) will be
generated if it does not exist already. This process is optional (a
custom configuration for each host can be provided instead), see the
`pmlogconf(1) manual
page <http://man7.org/linux/man-pages/man1/pmlogconf.1.html>`__ for
details on this.

Dynamic Host Discovery
~~~~~~~~~~~~~~~~~~~~~~

In dynamic environments manually configuring every host is not feasible,
perhaps even impossible. The discovery service
(`pmfind(1) <http://man7.org/linux/man-pages/man1/pmfind.1.html>`__ can
be used to auto-discover and auto-configure new collector hosts and
containers for logging and/or rule inference.

+-----------------------------------------------------------------------+
| To install pmfind to begin monitoring discovered metric               |
| sources, run::                                                        |
|                                                                       |
| # systemctl enable pmfind                                             |
| # systemctl restart pmfind                                            |
+-----------------------------------------------------------------------+

+-----------------------------------------------------------------------+
| Discover use of the PCP pmcd service on the local network::           |
|                                                                       |
| $ pmfind -s pmcd                                                      |
+-----------------------------------------------------------------------+

Installation Health Check
~~~~~~~~~~~~~~~~~~~~~~~~~

Basic health check for running services, network connectivity between
hosts, and enabled PMDAs can be done simply as follows.

+-----------------------------------------------------------------------+
| Check PCP services on remote host **munch** and                       |
| historically, from a local archive for host **smash**::               |
|                                                                       |
| $ pcp -h munch                                                        |
| $ pcp -a /var/log/pcp/pmlogger/smash/20190909                         |
+-----------------------------------------------------------------------+

System Level Performance Monitoring
===================================

PCP comes with a wide range of command line utilities for accessing live
performance metrics via PMCDs or historical data using archives. The
following examples illustrate some of the most useful use cases, please
see the corresponding manual pages for each command for additional
information. In the examples below **-h <host>** could be used to query
a remote host, the default is the local host. Shell completion support
for Bash and especially for Zsh allows completing available metrics,
metricsets (with
`pmrep <http://man7.org/linux/man-pages/man1/pmrep.1.html>`__), and
available command line options.

Monitoring Live Performance Metrics
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

+-----------------------------------------------------------------------+
| Display all the enabled performance metrics on a host                 |
| with a short description::                                            |
|                                                                       |
| $ pminfo -t                                                           |
+-----------------------------------------------------------------------+

+-----------------------------------------------------------------------+
| Display detailed information about a performance metric               |
| and its current values::                                              |
|                                                                       |
| $ pminfo -dfmtT disk.partitions.read                                  |
+-----------------------------------------------------------------------+

+-----------------------------------------------------------------------+
| Monitor live disk write operations per partition with                 |
| two second interval using fixed point notation (use *-i* instance to  |
| list only certain metrics and *-r* for raw values)::                  |
|                                                                       |
| $ pmval -t 2sec -f 3 disk.partitions.write                            |
+-----------------------------------------------------------------------+

+--------------------------------------------------------------------------------------------------+
| Monitor live CPU load, memory usage, and disk write operations per partition with two second     |
| interval using fixed width columns on the remote host acme::                                     |
|                                                                                                  |
| $ pmdumptext -Xlimu -t 2sec 'kernel.all.load[1]' mem.util.used disk.partitions.write -h acme.com |
+--------------------------------------------------------------------------------------------------+

+--------------------------------------------------------------------------------+
| Monitor live process creation rate and free/used memory with two second        |
| interval printing timestamps and using GBs for output values in CSV format::   |
|                                                                                |
| $ pmrep -p -b GB -t 2sec -o csv kernel.all.sysfork mem.util.free mem.util.used |
+--------------------------------------------------------------------------------+

+-----------------------------------------------------------------------+
| Monitor system metrics in a top-like window::                         |
|                                                                       |
| $ pcp atop                                                            |
| $ pcp htop                                                            |
+-----------------------------------------------------------------------+

+-----------------------------------------------------------------------+
| Monitor system metrics in a sar-like (System Activity                 |
| Report) manner::                                                      |
|                                                                       |
| $ pcp atopsar                                                         |
+-----------------------------------------------------------------------+

+-----------------------------------------------------------------------+
| Monitor system metrics in a sar like fashion with two                 |
| second interval from two different hosts::                            |
|                                                                       |
| $ pmstat -t 2sec -h acme1.com -h acme2.com                            |
+-----------------------------------------------------------------------+

+-----------------------------------------------------------------------+
| Monitor system metrics in an iostat like fashion with                 |
| two second interval::                                                 |
|                                                                       |
| $ pmiostat -t 2sec                                                    |
+-----------------------------------------------------------------------+

+-----------------------------------------------------------------------+
| Monitor performance metrics with a GUI application with               |
| two second default interval from two different hosts. Use *File->New  |
| Chart* to select metrics to be included in a new view and use         |
| *File->Open View* to use a predefined view::                          |
|                                                                       |
| $ pmchart -t 2sec -h acme1.com -h acme2.com                           |
+-----------------------------------------------------------------------+

Retrospective Performance Analysis
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

PCP archives are located under /var/log/pcp/pmlogger/**hostname**,
and the archive names indicate the time they cover. Archives are
self-contained, and machine- and version-independent so they can be
transfered to any machine for offline analysis.

+-----------------------------------------------------------------------+
| Check the host, timezone and the time period an archive               |
| covers::                                                              |
|                                                                       |
| $ pmdumplog -L acme.com/20140902                                      |
+-----------------------------------------------------------------------+

+-----------------------------------------------------------------------+
| Check PCP configuration at the time when an archive was created::     |
|                                                                       |
| $ pcp -a acme.com/20140902                                            |
+-----------------------------------------------------------------------+

+-----------------------------------------------------------------------+
| Display all enabled performance metrics at the time when              |
| an archive was created::                                              |
|                                                                       |
| $ pminfo -a acme.com/20140902                                         |
+-----------------------------------------------------------------------+

+-----------------------------------------------------------------------+
| Display detailed information about a performance metric               |
| at the time when an archive was created::                             |
|                                                                       |
| $ pminfo -df mem.freemem -a acme.com/20140902                         |
+-----------------------------------------------------------------------+

+-----------------------------------------------------------------------+
| Dump past disk write operations per partition in an                   |
| archive using fixed point notation (use *-i* instance to list only    |
| certain metrics and *-r* for raw values)::                            |
|                                                                       |
| $ pmval -f 3 disk.partitions.write -a acme.com/20140902               |
+-----------------------------------------------------------------------+

+----------------------------------------------------------------------------------------+
| Replay past disk write operations per partition in an                                  |
| archive with two second interval using fixed point notation between 9                  |
| AM and 10 AM (use full dates with syntax like *@"2014-08-20 14:00:00"*)::              |
|                                                                                        |
| $ pmval -d -t 2sec -f 3 disk.partitions.write -S @09:00 -T @10:00 -a acme.com/20140902 |
+----------------------------------------------------------------------------------------+

+-------------------------------------------------------------------------------------------------+
| Calculate average values of performance metrics in an                                           |
| archive between 9 AM / 10 AM using table like formatting including                              |
| the time of minimum/maximum value and the actual minimum/maximum value::                        |
|                                                                                                 |
| $ pmlogsummary -HlfiImM -S @09:00 -T @10:00 acme.com/20140902 disk.partitions.write mem.freemem |
+-------------------------------------------------------------------------------------------------+

+-----------------------------------------------------------------------------------+
| Dump past CPU load, memory usage, and disk write                                  |
| operations per partition in an archive averaged over 10 minute                    |
| interval with fixed columns between 9 AM and 10 AM::                              |
|                                                                                   |
| $ pmdumptext -Xlimu -t 10m -S @09:00 -T @10:00 -a acme.com/20140902 mem.util.used |
+-----------------------------------------------------------------------------------+

+-----------------------------------------------------------------------+
| Replay vmstat(1)-like metrics (using a customizable metricset         |
| definition from the pmrep.conf configuration file) from an archive on |
| every full 5 minutes using UTC as timezone::                          |
|                                                                       |
| $ pmrep -a acme.com/20140902 -A 5min -t 5min -Z UTC :vmstat           |
+-----------------------------------------------------------------------+

+--------------------------------------------------------------------------------------+
| Summarize differences in past performance metrics                                    |
| between two archives, comparing 2 AM / 3 AM in the first archive to 9                |
| AM / 10 AM in the second archive (grep for *'+'* to quickly see                      |
| values which were zero during the first period)::                                    |
|                                                                                      |
| $ pmdiff -S @02:00 -T @03:00 -B @09:00 -E @10:00 acme.com/20140902 acme.com/20140901 |
+--------------------------------------------------------------------------------------+

+-----------------------------------------------------------------------+
| Replay past system metrics in an archive in a top(1)-like             |
| window starting 9 AM::                                                |
|                                                                       |
| $ pcp atop -b 09:00 -r acme.com/20140902                              |
| $ pcp -S @09:00 -a acme.com/20140902 atop                             |
+-----------------------------------------------------------------------+

+-----------------------------------------------------------------------+
| Dump past system metrics in a sar(1)-like fashion averaged            |
| over 10 minute interval in an archive between 9 AM and 10 AM::        |
|                                                                       |
| $ pmstat -t 10m -S @09:00 -T @10:00 -a acme.com/20140902              |
+-----------------------------------------------------------------------+

+-----------------------------------------------------------------------+
| Dump past system metrics in an iostat(1)-like fashion averaged over   |
| one hour interval in an archive::                                     |
|                                                                       |
| $ pmiostat -t 1h -a acme.com/20140902                                 |
+-----------------------------------------------------------------------+

+-----------------------------------------------------------------------+
| Dump past system metrics in a free(1)-like fashion at a specific      |
| historical time offset::                                              |
|                                                                       |
| $ pcp -a acme.com/20140902 -O @10:02 free                             |
+-----------------------------------------------------------------------+

+-----------------------------------------------------------------------+
| Replay performance metrics with a GUI application with                |
| two second default interval in an archive between 9 AM and 10 AM. Use |
| *File->New Chart* to select metrics to be included in a new view and  |
| use *File->Open View* to use a predefined view::                      |
|                                                                       |
| $ pmchart -t 2sec -S @09:00 -T @10:00 -a acme.com/20140902            |
+-----------------------------------------------------------------------+

+-----------------------------------------------------------------------+
| Merge several archives as a new combined archive (see the manual page |
| how to write configuration file to collect only certain metrics)::    |
|                                                                       |
| $ pmlogextract <archive1> <archive2> <newarchive>                     |
+-----------------------------------------------------------------------+

Visualizing iostat and sar Data
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

`iostat <http://man7.org/linux/man-pages/man1/iostat.1.html>`__ and
`sar <http://man7.org/linux/man-pages/man1/sar.1.html>`__ data can be
imported as PCP archives which then allows inspecting and visualizing
the data with PCP tools. The
`iostat2pcp(1) <http://man7.org/linux/man-pages/man1/iostat2pcp.1.html>`__
importer is in the *pcp-import-iostat2pcp* package and the
`sar2pcp(1) <http://man7.org/linux/man-pages/man1/sar2pcp.1.html>`__
importer is in the *pcp-import-sar2pcp* package.

+-----------------------------------------------------------------------+
| Import iostat data to a new PCP archive and visualize it::            |
|                                                                       |
| $ iostat -t -x 2 > iostat.out                                         |
| $ iostat2pcp iostat.out iostat.pcp                                    |
| $ pmchart -t 2sec -a iostat.pcp                                       |
+-----------------------------------------------------------------------+

+-----------------------------------------------------------------------+
| Import sar data from an existing sar archive to a new                 |
| PCP archive and visualize it (sar logs are under /var/log/sysstat on  |
| Debian/Ubuntu)::                                                      |
|                                                                       |
| $ sar2pcp /var/log/sa/sa15 sar.pcp                                    |
| $ pmchart -t 2sec -a sar.pcp                                          |
+-----------------------------------------------------------------------+

Process Level Performance Monitoring
====================================

PCP provides details of each running process via the standard PCP
interfaces and tools on the localhost but due to security and
performance considerations, most of the process related information is
not stored in archives by default. Also for security reasons, only
root can access some details of running processes of other users.

Custom application instrumentation is possible with the Memory Mapped
Value (MMV) PMDA.

Live and Retrospective Process Monitoring
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

+-----------------------------------------------------------------------+
| Display all the available process related metrics::                   |
|                                                                       |
| $ pminfo proc                                                         |
+-----------------------------------------------------------------------+

+-----------------------------------------------------------------------+
| Monitor the number of open file descriptors of the process 1234::     |
|                                                                       |
| $ pmval -t 2sec 'proc.fd.count[1234]'                                 |
+-----------------------------------------------------------------------+

+-----------------------------------------------------------------------------------------------------------+
| Monitor the CPU time, memory usage (RSS), and the number                                                  |
| of threads of the process 1234::                                                                          |
|                                                                                                           |
| $ pmdumptext -Xlimu -t 2sec 'proc.psinfo.utime[1234]' 'proc.memory.rss[1234]' 'proc.psinfo.threads[1234]' |
+-----------------------------------------------------------------------------------------------------------+

+-----------------------------------------------------------------------+
| Monitor all outgoing network metrics for the wlan0 interface::        |
|                                                                       |
| $ pmrep -i wlan0 -v network.interface.out                             |
+-----------------------------------------------------------------------+

+-----------------------------------------------------------------------+
| Display all the available process related metrics in an archive::     |
|                                                                       |
| $ pminfo proc -a acme.com/20140902                                    |
+-----------------------------------------------------------------------+

+-----------------------------------------------------------------------+
| Display the number of running processes on 2014-08-20 14:00::         |
|                                                                       |
| $ pmval -s 1 -S @"2014-08-20 14:00" proc.nprocs -a acme.com/20140820  |
+-----------------------------------------------------------------------+

Monitoring “Hot” Processes with Hotproc
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

It is also possible to monitor “hot” or “interesting” processes by name,
for example all processes of which command name is java or python. This
monitoring of “hot” processes can also be enabled or disabled based on
certain criterias or from the command line on the fly. The metrics will
be available under the namespace *hotproc*.

Configuring processes to be monitored constantly using the *hotproc*
namespace can be done using the configuration file
/var/lib/pcp/pmdas/proc/hotproc.conf - see the
`pmdaproc(1) <http://man7.org/linux/man-pages/man1/pmdaproc.1.html>`__
manual page for details. This allows monitoring these processes
regardless of their PIDs and also logging the metrics easily.

+-----------------------------------------------------------------------+
| Enable monitoring of all Java instances on the fly and                |
| display all the collected metrics::                                   |
|                                                                       |
| # pmstore hotproc.control.config 'fname == "java"'                    |
| # pminfo -f hotproc                                                   |
+-----------------------------------------------------------------------+

Application Instrumentation
~~~~~~~~~~~~~~~~~~~~~~~~~~~

Applications can be instrumented in the PCP world by using Memory Mapped
Values (MMVs).
`pmdammv <http://man7.org/linux/man-pages/man1/pmdammv.1.html>`__ is a
PMDA which exports application level performance metrics using memory
mapped files. It offers an extremely low overhead instrumentation
facility that is well-suited to long running, mission critical
applications where it is desirable to have performance metrics and
availability information permanently enabled.

Application to be instrumented with MMV need to be PCP MMV aware, APIs
are available for several languages including C, C++, Perl, and Python.
Java applications may use the separate
`Parfait <https://github.com/performancecopilot/parfait>`__ class
library for enabling MMV.

See the `Performance Co-Pilot Programmer's
Guide <https://pcp.readthedocs.io/en/latest/PG/InstrumentingApplications.html>`__
for more information about application instrumentation.

Performance Metrics Inference
=============================

Performance Metrics Inference Engine
(`pmie(1) <http://man7.org/linux/man-pages/man1/pmie.1.html>`__) can
evaluate rules and generate alarms, run scripts, or automate system
management tasks based on live or past performance metrics.

+-----------------------------------------------------------------------+
| To enable and start PMIE::                                            |
|                                                                       |
| # systemctl enable --now pmie                                         |
+-----------------------------------------------------------------------+

To enable the monitoring host to run PMIE for collector hosts, add each
host to the /etc/pcp/pmie/control configuration file.

+-----------------------------------------------------------------------------------------+
| Enable monitoring of metrics from remote host **acme.com**:                             |
|                                                                                         |
| # echo acme.com n PCP_LOG_DIR/pmie/acme.com -c config.acme.com >> /etc/pcp/pmie/control |
| # systemctl restart pmie                                                                |
+-----------------------------------------------------------------------------------------+

Some examples in plain English describing what could be done with PMIE:

-  If the number of IP received packets exceeds a threshold run a script
   to adjust firewall rules to limit the incoming traffic
-  If 3 out of 4 consecutive samples taken every minute of disk
   operations exceeds a threshold between 9 AM and 5 PM send an email
   and write a system log message
-  If all hosts in a group have CPU load over a threshold for more than
   10 minutes or they have more application processes running than a
   threshold limit generate an alarm and run a script to tune the
   application

+-----------------------------------------------------------------------+
| This example shows a PMIE script, checks its syntax,                  |
| runs it against an archive, and prints a simple message if more than  |
| 5 GB of memory was in use between 9 AM and 10 AM using one minute     |
| sampling interval::                                                   |
|                                                                       |
| $ cat pmie.ex                                                         |
|                                                                       |
| bloated = (mem.util.used > 5 Gbyte) -> print "%v memory used on %h!"; |
|                                                                       |
| $ pmie -C pmie.ex                                                     |
| $ pmie -t 1min -c pmie.ex -S @09:00 -T @10:00 -a acme.com/20140820    |
+-----------------------------------------------------------------------+

Fast, Scalable Time Series Querying
===================================

Performance Metrics Series
(`pmseries(1) <http://man7.org/linux/man-pages/man1/pmseries.1.html>`__)
works with local pmlogger and `Redis <http://redis.io>`__ servers to
allow fast, scalable performance queries spanning multiple hosts.

+-----------------------------------------------------------------------+
| To enable and start metrics series collection::                       |
|                                                                       |
| # systemctl enable --now pmlogger pmproxy redis                       |
+-----------------------------------------------------------------------+

Redis can be run standalone or in large, highly available setups. It is
also provided as a scalable service by many cloud vendors.

The metrics indexing process is designed to spread data across multiple
Redis nodes for improved query performance, so adding more nodes can
significantly improve performance.

Examples of the
`pmseries <http://man7.org/linux/man-pages/man1/pmseries.1.html>`__
query language can be found on the man page. These queries can be
executed from the command line utility, or from the
`grafana-pcp <https://github.com/performancecopilot/grafana-pcp>`__
plugin for Grafana (see the `PCP Web Services <#web>`__ section below).

PCP Web Services
================

Performance Metrics REST APIs
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The Performance Metrics Proxy Daemon
(`pmproxy(1) <http://man7.org/linux/man-pages/man1/pmproxy.1.html>`__)
is a front-end to both PMCD and PCP archives, providing a REST API
service (over HTTP/JSON) suitable for use by web-based tools wishing to
access performance data over HTTP or HTTPS. Custom applications can
access all the available PCP information using this method, including
custom metrics generated by custom PMDAs.

+-----------------------------------------------------------------------+
| To install the PCP REST APIs service::                                |
|                                                                       |
| # systemctl enable --now pmproxy                                      |
+-----------------------------------------------------------------------+

Web Interface for Performance Metrics
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

`Grafana <https://grafana.com/>`__ is the recommended web interface for
accessing PCP performance metrics over HTTP.

+-----------------------------------------------------------------------+
| To install the PCP REST APIs service::                                |
|                                                                       |
| # systemctl enable --now pmproxy redis                                |
+-----------------------------------------------------------------------+

After installing the PCP REST API services as described above, install
the `grafana-pcp <https://github.com/performancecopilot/grafana-pcp>`__
package and then point a browser toward http://localhost:3000.

Derived Metrics
===============

PCP provides a wide range of performance metrics but still in some cases
the readily available metrics may not exactly provide what is needed.
Derived metrics (see
`pmLoadDerivedConfig(3) <http://man7.org/linux/man-pages/man3/pmloadderivedconfig.3.html>`__)
may be used to extend the available metrics with new (derived) metrics
by using simple arithmetic expressions (see
`pmRegisterDerived(3) <http://man7.org/linux/man-pages/man3/pmregisterderived.3.html>`__).

The following example illustrates how to define corresponding metrics
which are displayed by sar -d but are not provided by default by PCP:

+-----------------------------------------------------------------------+
| Create a file containing definitions of derived metrics               |
| and point PCP_DERIVED_CONFIG to it when running PCP utilities::       |
|                                                                       |
| $ cat ./pcp-derive-metrics.conf                                       |
|                                                                       |
| disk.dev.avqsz = disk.dev.read_rawactive + disk.dev.write_rawactive   |
| disk.dev.avrqsz = 2 \* rate(disk.dev.total_bytes) /                   |
| rate(disk.dev.total)                                                  |
| disk.dev.await = 1000 \* (rate(disk.dev.read_rawactive) +             |
| rate(disk.dev.write_rawactive)) / rate(disk.dev.total)                |
|                                                                       |
| $ export PCP_DERIVED_CONFIG=./pcp-derive-metrics.conf                 |
| $ pmval -t 2sec -f 3 disk.dev.avqsz                                   |
| $ pmval -t 2sec -f 3 disk.dev.avrqsz -h acme.com                      |
| $ pmval -t 2sec -f 3 disk.dev.await -a acme.com/20140902              |
+-----------------------------------------------------------------------+

+-----------------------------------------------------------------------+
| Define a derived metric on the command line and monitor               |
| it with standard metrics:                                             |
|                                                                       |
| $ pmrep -t 2sec -p -b MB -e "mem.util.allcache = mem.util.bufmem +    |
| mem.util.cached + mem.util.slab" mem.util.free mem.util.allcache      |
| mem.util.used                                                         |
+-----------------------------------------------------------------------+

Customizing and Extending PCP
=============================

PCP PMDAs offer a way for administrators and developers to customize and
extend the default PCP installation. The pcp-libs-devel package contains
all the needed development related examples, headers, and libraries. New
PMDAs can easily be added, below is a quick list of references for
starting development:

-  Some examples exist below /var/lib/pcp/pmdas/ - the simple, sample,
   trivial and txmon PMDAs are easy to read PMDAs.

   -  The simple and trivial PMDAs provide implementations in C, Perl
      and Python.

-  A simple command line monitor tool is /usr/share/pcp/demos/pmclient
   (C language).
-  Good initial Python monitor examples are
   /usr/libexec/pcp/bin/pcp-\* (Fedora/RHEL) or
   /usr/lib/pcp/bin/pcp-\* (Debian/Ubuntu).

   -  Slightly more complex examples are the pcp-free, pcp-mpstat,
      pcp-numastat commands.

Additional Information
======================

-  https://pcp.io/ - PCP home page
-  https://pcp.readthedocs.io/en/latest/ - PCP docs landing page
