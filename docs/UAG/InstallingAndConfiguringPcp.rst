.. _InstallingAndConfiguringPcp:

Installing and Configuring Performance Co-Pilot
################################################

.. contents::

The sections in this chapter describe the basic installation and configuration steps necessary to run Performance Co-Pilot (PCP) on your systems. The following major sections are included:

Section 2.1, “`Product Structure`_” describes the main packages of PCP software and how they must be installed on each system.

Section 2.2, “`Performance Metrics Collection Daemon (PMCD)`_”, describes the fundamentals of maintaining the performance data collector.

Section 2.3, “`Managing Optional PMDAs`_”, describes the basics of installing a new Performance Metrics Domain Agent (PMDA) to collect metric data and pass it to the PMCD.

Section 2.4, “`Troubleshooting`_”, offers advice on problems involving the PMCD.

Product Structure
******************

In a typical deployment, Performance Co-Pilot (PCP) would be installed in a collector configuration on one or more hosts, from which the performance information could then be collected, 
and in a monitor configuration on one or more workstations, from which the performance of the server systems could then be monitored.

On some platforms Performance Co-Pilot is presented as multiple packages; typically separating the server components from graphical user interfaces and documentation.


+--------------------+------------------------------------------+
| pcp-X.Y.Z-rev      | package for core PCP                     |
+--------------------+------------------------------------------+
| pcp-gui-X.Y.Z-rev  | package for graphical PCP client tools   |
+--------------------+------------------------------------------+
| pcp-doc-X.Y.Z-rev  |package for online PCP documentation      +
+--------------------+------------------------------------------+

Performance Metrics Collection Daemon (PMCD)
********************************************

On each Performance Co-Pilot (PCP) collection system, you must be certain that the **pmcd** daemon is running. This daemon coordinates the gathering and exporting of performance 
statistics in response to requests from the PCP monitoring tools.

Starting and Stopping the PMCD
==============================

To start the daemon, enter the following commands as root on each PCP collection system:: 
 
 chkconfig pmcd on 
 ${PCP_RC_DIR}/pmcd start 
 
These commands instruct the system to start the daemon immediately, and again whenever the system is booted. It is not necessary to start the daemon on the monitoring system unless 
you wish to collect performance information from it as well.

To stop **pmcd** immediately on a PCP collection system, enter the following command:: 

 ${PCP_RC_DIR}/pmcd stop

⁠Restarting an Unresponsive PMCD
================================

Sometimes, if a daemon is not responding on a PCP collection system, the problem can be resolved by stopping and then immediately restarting a fresh instance of the daemon. If 
you need to stop and then immediately restart PMCD on a PCP collection system, use the **start** argument provided with the script in ``${PCP_RC_DIR}``. The command syntax is, as follows:: 

 ${PCP_RC_DIR}/pmcd start

On startup, **pmcd** looks for a configuration file at ``${PCP_PMCDCONF_PATH}``. This file specifies which agents cover which performance metrics domains and how PMCD should make 
contact with the agents. A comprehensive description of the configuration file syntax and semantics can be found in the **pmcd(1)** man page.

If the configuration is changed, **pmcd** reconfigures itself when it receives the **SIGHUP** signal. Use the following command to send the **SIGHUP** signal to the daemon:: 

 ${PCP_BINADM_DIR}/pmsignal -a -s HUP pmcd

This is also useful when one of the PMDAs managed by **pmcd** has failed or has been terminated by **pmcd**. Upon receipt of the **SIGHUP** signal, **pmcd** restarts any PMDA that 
is configured but inactive. The exception to this rule is the case of a PMDA which must run with superuser privileges (where possible, this is avoided) - for these PMDAs, a full 
**pmcd** restart must be performed, using the process described earlier (not SIGHUP).

PMCD Diagnostics and Error Messages
====================================

If there is a problem with **pmcd**, the first place to investigate should be the **pmcd.log** file. By default, this file is in the ``${PCP_LOG_DIR}/pmcd`` directory.

PMCD Options and Configuration Files
=====================================

There are two files that control PMCD operation. These are the ``${PCP_PMCDCONF_PATH}`` and ``${PCP_PMCDOPTIONS_PATH}`` files. The **pmcd.options** file contains the command line 
options used with PMCD; it is read when the daemon is invoked by ``${PCP_RC_DIR}/pmcd``. The **pmcd.conf** file contains configuration information regarding domain agents and the 
metrics that they monitor. These configuration files are described in the following sections.

The pmcd.options File
----------------------

Command line options for the PMCD are stored in the ``${PCP_PMCDOPTIONS_PATH}`` file. The PMCD can be invoked directly from a shell prompt, or it can be invoked by ``${PCP_RC_DIR}/pmcd`` 
as part of the boot process. It is usual and normal to invoke it using ``${PCP_RC_DIR}/pmcd``, reserving shell invocation for debugging purposes.

The PMCD accepts certain command line options to control its execution, and these options are placed in the **pmcd.options** file when ``${PCP_RC_DIR}/pmcd`` is being used to start the 
daemon. The following options (amongst others) are available:

**-i**  *address*

For hosts with more than one network interface, this option specifies the interface on which this instance of the PMCD accepts connections.
Multiple **-i** options may be specified. The default in the absence of any **-i** option is for PMCD to accept connections on all interfaces.

**-l**  *file*

Specifies a log file. If no **-l** option is specified, the log file name is **pmcd.log** and it is created in the directory ``${PCP_LOG_DIR}/pmcd/``.

**-s**  *file*

Specifies the path to a local unix domain socket (for platforms supporting this socket family only). The default value is ``${PCP_RUN_DIR}/pmcd.socket``.

**-t**  *seconds*

Specifies the amount of time, in seconds, before PMCD times out on protocol data unit (PDU) exchanges with PMDAs. If no time out is specified, the default is five seconds. 
Setting time out to zero disables time outs (not recommended, PMDAs should always respond quickly).

The time out may be dynamically modified by storing the number of seconds into the metric pmcd.control.timeout using pmstore.

**-T**  *mask*

Specifies whether connection and PDU tracing are turned on for debugging purposes.

See the **pmcd(1)** man page for complete information on these options.

The default **pmcd.options** file shipped with PCP is similar to the following:: 


 # command-line options to pmcd, uncomment/edit lines as required

 # longer timeout delay for slow agents
 # -t 10

 # suppress timeouts
 # -t 0

 # make log go someplace else
 # -l /some/place/else

 # debugging knobs, see pmdbg(1)
 # -D N
 # -f

 # Restricting (further) incoming PDU size to prevent DOS attacks
 # -L 16384 

 # enable event tracing bit fields
 #   1   trace connections
 #   2   trace PDUs
 # 256   unbuffered tracing
 # -T 3

 # setting of environment variables for pmcd and
 # the PCP rc scripts. See pmcd(1) and PMAPI(3).
 # PMCD_WAIT_TIMEOUT=120
 
The most commonly used options have been placed in this file for your convenience. To uncomment and use an option, simply remove the pound sign (#) at the beginning of the line 
with the option you wish to use. Restart **pmcd** for the change to take effect; that is, as superuser, enter the command:: 

 ${PCP_RC_DIR}/pmcd start

⁠The pmcd.conf File
-------------------

When the PMCD is invoked, it reads its configuration file, which is ``${PCP_PMCDCONF_PATH}``. This file contains entries that specify the PMDAs used by this instance of the PMCD and 
which metrics are covered by these PMDAs. Also, you may specify access control rules in this file for the various hosts, users and groups on your network. This file is described 
completely in the **pmcd(1)** man page.

With standard PCP operation (even if you have not created and added your own PMDAs), you might need to edit this file in order to add any additional access control you wish to impose. 
If you do not add access control rules, all access for all operations is granted to the local host, and read-only access is granted to remote hosts. The **pmcd.conf** file is automatically 
generated during the software build process and on Linux, for example, is similar to the following:: 

  Performance Metrics Domain Specifications
 # 
 # This file is automatically generated during the build
 # Name  Id      IPC     IPC Params      File/Cmd
 root	 1	 pipe	 binary		 /var/lib/pcp/pmdas/root/pmdaroot
 pmcd    2       dso     pmcd_init       ${PCP_PMDAS_DIR}/pmcd/pmda_pmcd.so
 proc    3       pipe    binary          ${PCP_PMDAS_DIR}/proc/pmdaproc -d 3
 xfs     11      pipe    binary          ${PCP_PMDAS_DIR}/xfs/pmdaxfs -d 11
 linux   60      dso     linux_init      ${PCP_PMDAS_DIR}/linux/pmda_linux.so
 mmv	 70	 dso	 mmv_init	 /var/lib/pcp/pmdas/mmv/pmda_mmv.so
 
 [access]
 disallow ".*" : store;
 disallow ":*" : store;
 allow "local:*" : all;

.. note:: Even though PMCD does not run with **root** privileges, you must be very careful not to configure PMDAs in this file if you are not sure of their action. This is because all PMDAs are initially started as **root** (allowing them to assume alternate identities, such as **postgres** for example), after which **pmcd** drops its privileges. Pay close attention that permissions on this file are not inadvertently downgraded to allow public write access.

Each entry in this configuration file contains rules that specify how to connect the PMCD to a particular PMDA and which metrics the PMDA monitors. A PMDA may be attached as a Dynamic Shared Object (DSO) or by using a socket or a pair of pipes. The distinction between these attachment methods is described below.

An entry in the **pmcd.conf** file looks like this:

.. sourcecode:: none

 label_name   domain_number   type   path
 
The *label_name* field specifies a name for the PMDA. The *domain_number* is an integer value that specifies a domain of metrics for the PMDA. 
The *type* field indicates the type of entry (DSO, socket, or pipe). The *path* field is for additional information, and varies according to the type of entry.

The following rules are common to DSO, socket, and pipe syntax:

+------------------+-------------------------------------------------------+
| *label_name*     | An alphanumeric string identifying the agent.         |
+------------------+-------------------------------------------------------+
| *domain_number*  | An unsigned integer specifying the agent's domain.    |
+------------------+-------------------------------------------------------+

DSO entries follow this syntax::

 label_name domain_number dso entry-point path

The following rules apply to the DSO syntax:

+------------------+-----------------------------------------------------------------------+
| **dso**          | The entry type.                                                       |
+------------------+-----------------------------------------------------------------------+
| *entry-point*    | The name of an initialization function called when the DSO is loaded. |
+------------------+-----------------------------------------------------------------------+
| *path*           | Designates the location of the DSO. An absolute path must be used.    |
|                  | On most platforms this will be a **so** suffixed file, on Windows it  | 
|                  | is a **dll**, and on Mac OS X it is a **dylib** file.                 |
+------------------+-----------------------------------------------------------------------+

Socket entries in the **pmcd.conf** file follow this syntax:: 

 label_name domain_number socket addr_family address command [args]

The following rules apply to the socket syntax:

+------------------+-----------------------------------------------------------------------+
| **socket**       | The entry type.                                                       |
+------------------+-----------------------------------------------------------------------+
| *addr_family*    | Specifies if the socket is **AF_INET**, **AF_IPV6** or **AF_UNIX**.   |
|                  | If the socket is **INET**, the word **inet** appears in this place.   |
|                  | If the socket is **IPV6**, the word **ipv6** appears in this place.   |
|                  | If the socket is **UNIX**, the word **unix** appears in this place.   |
+------------------+-----------------------------------------------------------------------+
| *address*        | Specifies the address of the socket. For INET or IPv6 sockets, this   |
|                  | is a port number or port name. For UNIX sockets, this is the name of  |
|                  | the PMDA's socket on the local host.                                  |
+------------------+-----------------------------------------------------------------------+
| *command*        | Specifies a command to start the PMDA when the PMCD is invoked and    |
|                  | reads the configuration file.                                         |
+------------------+-----------------------------------------------------------------------+
| *args*           | Optional arguments for *command*.                                     |
+------------------+-----------------------------------------------------------------------+

Pipe entries in the **pmcd.conf** file follow this syntax:: 

 label_name domain_number pipe protocol command [args]

The following rules apply to the pipe syntax:

+------------------+-----------------------------------------------------------------------+
| **pipe**         | The entry type.                                                       |
+------------------+-----------------------------------------------------------------------+
| protocol         | Specifies whether a text-based or a binary PCP protocol should be used|
|                  | over the pipes. Historically, this parameter was able to be “text” or |
|                  | “binary.” The text-based protocol has long since been deprecated and  |
|                  | removed, however, so nowadays “binary” is the only valid value here.  |
+------------------+-----------------------------------------------------------------------+
| command          | Specifies a command to start the PMDA when the PMCD is invoked and    |
|                  | reads the configuration file.                                         |
+------------------+-----------------------------------------------------------------------+
| args             | Optional arguments for command.                                       |
+------------------+-----------------------------------------------------------------------+

Controlling Access to PMCD with pmcd.conf
------------------------------------------

You can place this option extension in the **pmcd.conf** file to control access to performance metric data based on hosts, users and groups. 
To add an access control section, begin by placing the following line at the end of your **pmcd.conf** file:: 

 [access]

Below this line, you can add entries of the following forms::
 
 allow hosts hostlist : operations ;   disallow hosts hostlist : operations ;
 allow users userlist : operations ;   disallow users userlist : operations ;
 allow groups grouplist : operations ;   disallow groups grouplist : operations ;

The keywords *users*, *groups* and *hosts* can be used in either plural or singular form.

The *userlist* and *grouplist* fields are comma-separated lists of authenticated users and groups from the local **/etc/passwd** and **/etc/groups** files, NIS (network information service) 
or LDAP (lightweight directory access protocol) service.

The *hostlist* is a comma-separated list of host identifiers; the following rules apply:

* Host names must be in the local system's **/etc/hosts** file or known to the local DNS (domain name service).
* IP and IPv6 addresses may be given in the usual numeric notations.
* A wildcarded IP or IPv6 address may be used to specify groups of hosts, with the single wildcard character * as the last-given component of the address. The wildcard .* refers to all IP (IPv4) 
  addresses. The wildcard :* refers to all IPv6 addresses. If an IPv6 wildcard contains a :: component, then the final * refers to the final 16 bits of the address only, otherwise it refers to the 
  remaining unspecified bits of the address.

The wildcard "*" refers to all users, groups or host addresses. Names of users, groups or hosts may not be wildcarded.

For example, the following *hostlist* entries are all valid:: 

 babylon
 babylon.acme.com
 123.101.27.44
 localhost
 155.116.24.*
 192.*
 .*
 fe80::223:14ff:feaf:b62c
 fe80::223:14ff:feaf:*
 fe80:*
 :*
 *
 
The operations field can be any of the following:

* A comma-separated list of the operation types described below.
* The word all to allow or disallow all operations as specified in the first field.
* The words all except and a list of operations. This entry allows or disallows all operations as specified in the first field except those listed.
* The phrase maximum N connections to set an upper bound (N) on the number of connections an individual host, user or group of users may make. This can only be added to the operations list of an allow statement.

The operations that can be allowed or disallowed are as follows:

* fetch : Allows retrieval of information from the PMCD. This may be information about a metric (such as a description, instance domain, or help text) or an actual value for a metric.
* store : Allows the PMCD to store metric values in PMDAs that permit store operations. Be cautious in allowing this operation, because it may be a security opening in large networks, 
  although the PMDAs shipped with the PCP package typically reject store operations, except for selected performance metrics where the effect is benign.

For example, here is a sample access control portion of a ``${PCP_PMCDCONF_PATH}`` file:

.. sourcecode:: none

 allow hosts babylon, moomba : all ; 
 disallow user sam : all ;
 allow group dev : fetch ; 
 allow hosts 192.127.4.* : fetch ; 
 disallow host gate-inet : store ;
 
Complete information on access control syntax rules in the **pmcd.conf** file can be found in the **pmcd(1)** man page.

Managing Optional PMDAs
************************

Some Performance Metrics Domain Agents (PMDAs) shipped with Performance Co-Pilot (PCP) are designed to be installed and activated on every collector host, for example, 
**linux**, **windows**, **darwin**, **pmcd**, and **process** PMDAs.

Other PMDAs are designed for optional activation and require some user action to make them operational. In some cases these PMDAs expect local site customization to reflect the 
operational environment, the system configuration, or the production workload. This customization is typically supported by interactive installation scripts for each PMDA.

Each PMDA has its own directory located below ``${PCP_PMDAS_DIR}``. Each directory contains a **Remove** script to unconfigure the PMDA, remove the associated metrics from the PMNS, 
and restart the **pmcd** daemon; and an **Install** script to install the PMDA, update the PMNS, and restart the PMCD daemon.

As a shortcut mechanism to support automated PMDA installation, a file named **.NeedInstall** can be created in a PMDA directory below ``${PCP_PMDAS_DIR}``. The next restart of PCP 
services will invoke that PMDAs installation automatically, with default options taken.

PMDA Installation on a PCP Collector Host
==========================================

To install a PMDA you must perform a collector installation for each host on which the PMDA is required to export performance metrics. PCP provides a distributed metric namespace (PMNS) 
and metadata, so it is not necessary to install PMDAs (with their associated PMNS) on PCP monitor hosts.

You need to update the PMNS, configure the PMDA, and notify PMCD. The **Install** script for each PMDA automates these operations, as follows:

1. Log in as **root** (the superuser).

2. Change to the PMDA's directory as shown in the following example:: 
 
     cd ${PCP_PMDAS_DIR}/cisco
  
3. In the unlikely event that you wish to use a non-default Performance Metrics Domain (PMD) assignment, determine the current PMD assignment:: 

     cat domain.h

Check that there is no conflict in the PMDs as defined in ``${PCP_VAR_DIR}/pmns/stdpmid`` and the other PMDAs currently in use (listed in ``${PCP_PMCDCONF_PATH}``). Edit **domain.h** 
to assign the new domain number if there is a conflict (this is highly unlikely to occur in a regular PCP installation).

4. Enter the following command:: 

     ./Install

 
You may be prompted to enter some local parameters or configuration options. The script applies all required changes to the control files and to the PMNS, and then notifies PMCD. 
Example 2.1, “PMNS Installation Output ” is illustrative of the interactions:: 

 Example 2.1. PMNS Installation Output
 
 Cisco hostname or IP address? [return to quit] wanmelb
 
 A user-level password may be required for Cisco “show int” command.
     If you are unsure, try the command
         $ telnet wanmelb
     and if the prompt “Password:” appears, a user-level password is
     required; otherwise answer the next question with an empty line.
 
 User-level Cisco password? ********
 Probing Cisco for list of interfaces ...

 Enter interfaces to monitor, one per line in the format
 tX where “t” is a type and one of “e” (Ethernet), or “f” (Fddi), or
 “s” (Serial), or “a” (ATM), and “X” is an interface identifier
 which is either an integer (e.g.  4000 Series routers) or two
 integers separated by a slash (e.g. 7000 Series routers).

 The currently unselected interfaces for the Cisco “wanmelb” are:
     e0 s0 s1
 Enter “quit” to terminate the interface selection process.
     Interface? [e0] s0

 The currently unselected interfaces for the Cisco “wanmelb” are:
     e0 s1
 Enter “quit” to terminate the interface selection process.
     Interface? [e0] **s1**

 The currently unselected interfaces for the Cisco “wanmelb” are:
     e0
 Enter “quit” to terminate the interface selection process.
 Interface? [e0] **quit**

 Cisco hostname or IP address? [return to quit]
 Updating the Performance Metrics Name Space (PMNS) ...
 Installing pmchart view(s) ...
 Terminate PMDA if already installed ...
 Installing files ...
 Updating the PMCD control file, and notifying PMCD ...
 Check cisco metrics have appeared ... 5 metrics and 10 values
 
PMDA Removal on a PCP Collector Host
=====================================

To remove a PMDA, you must perform a collector removal for each host on which the PMDA is currently installed.

The PMNS needs to be updated, the PMDA unconfigured, and PMCD notified. The **Remove** script for each PMDA automates these operations, as follows:

1. Log in as **root** (the superuser).
2. Change to the PMDA's directory as shown in the following example::
 
     cd ${PCP_PMDAS_DIR}/elasticsearch

3. Enter the following command::

     ./Remove

The following output illustrates the result:

.. sourcecode:: none

     Culling the Performance Metrics Name Space ...
    elasticsearch ... done
 Updating the PMCD control file, and notifying PMCD ...
 Removing files ...
 Check elasticsearch metrics have gone away ... OK
 
Troubleshooting
***************

The following sections offer troubleshooting advice on the Performance Metrics Name Space (PMNS), missing and incomplete values for performance metrics, kernel metrics and the PMCD.

Advice for troubleshooting the archive logging system is provided in Chapter 6, Archive Logging.

Performance Metrics Name Space
===============================

To display the active PMNS, use the **pminfo** command; see the **pminfo(1)** man page.

The PMNS at the collector host is updated whenever a PMDA is installed or removed, and may also be updated when new versions of PCP are installed. During these operations, 
the PMNS is typically updated by merging the (plaintext) namespace components from each installed PMDA. These separate PMNS components reside in the ``${PCP_VAR_DIR}/pmns`` 
directory and are merged into the **root** file there.

⁠Missing and Incomplete Values for Performance Metrics
======================================================

Missing or incomplete performance metric values are the result of their unavailability.

Metric Values Not Available
----------------------------

The following symptom has a known cause and resolution:

**Symptom:**

Values for some or all of the instances of a performance metric are not available.

**Cause:**

This can occur as a consequence of changes in the installation of modules (for example, a DBMS or an application package) that provide the performance instrumentation underpinning the 
PMDAs. Changes in the selection of modules that are installed or operational, along with changes in the version of these modules, may make metrics appear and disappear over time.

In simple terms, the PMNS contains a metric name, but when that metric is requested, no PMDA at the collector host supports the metric.

For archives, the collection of metrics to be logged is a subset of the metrics available, so utilities replaying from a PCP archive may not have access to all of the metrics 
available from a live (PMCD) source.

**Resolution:**

Make sure the underlying instrumentation is available and the module is active. Ensure that the PMDA is running on the host to be monitored. If necessary, create a new archive with 
a wider range of metrics to be logged.

⁠Kernel Metrics and the PMCD
============================

The following issues involve the kernel metrics and the PMCD:

* Cannot connect to remote PMCD
* PMCD not reconfiguring after hang-up
* PMCD does not start

Cannot Connect to Remote PMCD
-----------------------------

The following symptom has a known cause and resolution:

**Symptom:**

A PCP client tool (such as **pmchart**, **pmie**, or **pmlogger**) complains that it is unable to connect to a remote PMCD (or establish a PMAPI context), but you are sure that PMCD 
is active on the remote host.

**Cause:**

To avoid hanging applications for the duration of TCP/IP time outs, the PMAPI library implements its own time out when trying to establish a connection to a PMCD. If the connection 
to the host is over a slow network, then successful establishment of the connection may not be possible before the time out, and the attempt is abandoned.

Alternatively, there may be a firewall in-between the client tool and PMCD which is blocking the connection attempt.

Finally, PMCD may be running in a mode where it does not accept remote connections, or only listening on certain interface.

**Resolution:**

Establish that the PMCD on far-away-host is really alive, by connecting to its control port (TCP port number 44321 by default):: 

 telnet far-away-host 44321

This response indicates the PMCD is not running and needs restarting::

 Unable to connect to remote host: Connection refused

To restart the PMCD on that host, enter the following command::

 ${PCP_RC_DIR}/pmcd start

This response indicates the PMCD is running::

 Connected to far-away-host

Interrupt the **telnet** session, increase the PMAPI time out by setting the **PMCD_CONNECT_TIMEOUT** environment variable to some number of seconds (60 for instance), and try the PCP client tool again.

Verify that PMCD is not running in local-only mode, by looking for an enabled value (one) from::

 pminfo -f pmcd.feature.local

This setting is controlled from the **PMCD_LOCAL** environment variable usually set via ``${PCP_SYSCONFIG_DIR}/pmcd``.

If these techniques are ineffective, it is likely an intermediary firewall is blocking the client from accessing the PMCD port - resolving such issues is firewall-host platform-specific and cannot practically be covered here.

PMCD Not Reconfiguring after SIGHUP
-----------------------------------

The following symptom has a known cause and resolution:

**Symptom:**

PMCD does not reconfigure itself after receiving the **SIGHUP** signal.

**Cause:**

If there is a syntax error in ``${PCP_PMCDCONF_PATH}``, PMCD does not use the contents of the file. This can lead to situations in which the configuration file and PMCD's internal 
state do not agree.

**Resolution:**

Always monitor PMCD's log. For example, use the following command in another window when reconfiguring PMCD, to watch errors occur::

 tail -f ${PCP_LOG_DIR}/pmcd/pmcd.log

⁠PMCD Does Not Start
--------------------

The following symptom has a known cause and resolution:

**Symptom:**

If the following messages appear in the PMCD log (``${PCP_LOG_DIR}/pmcd/pmcd.log``), consider the cause and resolution:: 

 pcp[27020] Error: OpenRequestSocket(44321) bind: Address already in
 use
 pcp[27020] Error: pmcd is already running
 pcp[27020] Error: pmcd not started due to errors!

**Cause:**

PMCD is already running or was terminated before it could clean up properly. The error occurs because the socket it advertises for client connections is already being used or has not been cleared by the kernel.

**Resolution:**

Start PMCD as **root** (superuser) by typing:: 

 ${PCP_RC_DIR}/pmcd start

Any existing PMCD is shut down, and a new one is started in such a way that the symptomatic message should not appear.

If you are starting PMCD this way and the symptomatic message appears, a problem has occurred with the connection to one of the deceased PMCD's clients.

This could happen when the network connection to a remote client is lost and PMCD is subsequently terminated. The system may attempt to keep the socket open for a time to allow the remote client a chance to reestablish the connection and read any outstanding data.

The only solution in these circumstances is to wait until the socket times out and the kernel deletes it. This netstat command displays the status of the socket and any connections::

 netstat -ant | grep 44321

If the socket is in the **FIN_WAIT** or **TIME_WAIT** state, then you must wait for it to be deleted. Once the command above produces no output, PMCD may be restarted. Less commonly, you may have another program running on your system that uses the same Internet port number (44321) that PMCD uses.

Refer to the **PCPIntro(1)** man page for a description of how to override the default PMCD port assignment using the **PMCD_PORT** environment variable.
