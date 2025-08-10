.. _WritingPMDA:

Writing A PMDA
################

.. contents::

This chapter constitutes a programmer's guide to writing a Performance Metrics Domain Agent (PMDA) for Performance Co-Pilot (PCP).

The presentation assumes the developer is using the standard PCP **libpcp_pmda** library, as documented in the **PMDA(3)** and associated man pages.

Implementing a PMDA
*********************
The job of a PMDA is to gather performance data and report them to the Performance Metrics Collection Daemon (PMCD) in response to requests from PCP monitoring 
tools routed to the PMDA via PMCD.

An important requirement for any PMDA is that it have low latency response to requests from PMCD. Either the PMDA must use a quick access method and a single 
thread of control, or it must have asynchronous refresh and two threads of control: one for communicating with PMCD, the other for updating the performance data.

The PMDA is typically acting as a gateway between the target domain (that is, the performance instrumentation in an application program or service) and the PCP 
framework. The PMDA may extract the information using one of a number of possible export options that include a shared memory segment or **mmap** file; a sequential 
log file (where the PMDA parses the tail of the log file to extract the information); a snapshot file (the PMDA rereads the file as required); or application-specific 
communication services (IPC).

.. note:: The choice of export methodology is typically determined by the source of the instrumentation (the target domain) rather than by the PMDA.

`Procedure 2.1. Creating a PMDA`_ describes the suggested steps for designing and implementing a PMDA:

.. _Procedure 2.1. Creating a PMDA:

**Procedure 2.1. Creating a PMDA**

1. Determine how to extract the metrics from the target domain.

2. Select an appropriate architecture for the PMDA (daemon or DSO, IPC, **pthreads** or single threaded).

3. Define the metrics and instances that the PMDA will support.

4. Implement the functionality to extract the metric values.

5. Assign Performance Metric Identifiers (PMIDs) for the metrics, along with names for the metrics in the Performance Metrics Name Space (PMNS). These concepts 
   will be further expanded in Section 2.3, “`Domains, Metrics, Instances and Labels`_”

6. Specify the help file and control data structures for metrics and instances that are required by the standard PMDA implementation library functions.

7. Write code to supply the metrics and associated information to PMCD.

8. Implement any PMDA-specific callbacks, and PMDA initialization functions.

9. Exercise and test the PMDA with the purpose-built PMDA debugger; see the **dbpmda(1)** man page.

10. Install and connect the PMDA to a running PMCD process; see the **pmcd(1)** man page.

11. Where appropriate, define **pmie** rule templates suitable for alerting or notification systems. For more information, see the **pmie(1)** and **pmieconf(1)** man pages.

12. Where appropriate, define **pmlogger** configuration templates suitable for creating PCP archives containing the new metrics. For more information, see the 
    **pmlogconf(1)** and **pmlogger(1)** man pages.

⁠PMDA Architecture
******************

This section discusses the two methods of connecting a PMDA to a PMCD process:

1. As a separate process using some interprocess communication (IPC) protocol.

2. As a dynamically attached library (that is, a dynamic shared object or DSO).

Overview
==========

All PMDAs are launched and controlled by the PMCD process on the local host. PMCD receives requests from the monitoring tools and forwards them to the PMDAs. 
Responses, when required, are returned through PMCD to the clients. The requests fall into a small number of categories, and the PMDA must handle each request type. 
For a DSO PMDA, each request type corresponds to a method in the agent. For a daemon PMDA, each request translates to a message or protocol data unit (PDU) that 
may be sent to a PMDA from PMCD.

For a daemon PMDA, the following request PDUs must be supported:

**PDU_FETCH**

Request for metric values (see the **pmFetch(3)** man page.)

**PDU_PROFILE**

A list of instances required for the corresponding metrics in subsequent fetches (see the **pmAddProfile(3)** man page).

**PDU_INSTANCE_REQ**

Request for a particular instance domain for instance descriptions (see the **pmGetInDom(3)** man page).

**PDU_DESC_REQ**

Request for metadata describing metrics (see the **pmLookupDesc(3)** man page).

**PDU_TEXT_REQ**

Request for metric help text (see the **pmLookupText(3)** man page).

**PDU_RESULT**

Values to store into metrics (see the **pmStore(3)** man page).

The following request PDUs may optionally be supported:

**PDU_PMNS_NAMES**

Request for metric names, given one or more identifiers (see the **pmLookupName(3)** man page.)

**PDU_PMNS_CHILD**

A list of immediate descendent nodes of a given namespace node (see the **pmGetChildren(3)** man page).

**PDU_PMNS_TRAVERSE**

Request for a particular sub-tree of a given namespace node (see the **pmTraversePMNS(3)** man page).

**PDU_PMNS_IDS**

Perform a reverse name lookup, mapping a metric identifier to a name (see the **pmNameID(3)** man page).

**PDU_ATTR**

Handle connection attributes (key/value pairs), such as client credentials and other authentication information (see the **__pmParseHostAttrsSpec(3)** man page).

**PDU_LABEL_REQ**

Request for metric labels (see the **pmLookupLabels(3)** man page).

Each PMDA is associated with a unique domain number that is encoded in the domain field of metric and instance identifiers, and PMCD uses the domain number to 
determine which PMDA can handle the components of any given client request.

DSO PMDA
==========

Each PMDA is required to implement a function that handles each of the request types. By implementing these functions as library functions, a PMDA can be 
implemented as a dynamically shared object (DSO) and attached by PMCD at run time with a platform-specific call, such as **dlopen**; see the **dlopen(3)** 
man page. This eliminates the need for an IPC layer (typically a pipe) between each PMDA and PMCD, because each request becomes a function call rather than 
a message exchange. The required library functions are detailed in Section 2.5, “`PMDA Interface`_”.

A PMDA that interacts with PMCD in this fashion must abide by a formal initialization protocol so that PMCD can discover the location of the library functions 
that are subsequently called with function pointers. When a DSO PMDA is installed, the PMCD configuration file, ``${PCP_PMCDCONF_PATH}``, is updated to reflect the 
domain and name of the PMDA, the location of the shared object, and the name of the initialization function. The initialization sequence is discussed in 
Section 2.6, “`Initializing a PMDA`_”.

As superuser, install the simple PMDA as a DSO, as shown in `Example 2.1. Simple PMDA as a DSO`_, and observe the changes in the PMCD configuration file. The 
output may differ slightly depending on the operating system you are using, any other PMDAs you have installed or any PMCD access controls you have in place.

.. _Example 2.1. Simple PMDA as a DSO:

**Example 2.1. Simple PMDA as a DSO**

.. sourcecode:: none

 # cat ${PCP_PMCDCONF_PATH}
 # Performance Metrics Domain Specifications
 # 
 # This file is automatically generated during the build
 # Name  Id      IPC     IPC Params      File/Cmd
 root    1       pipe    binary          /var/lib/pcp/pmdas/root/pmdaroot
 pmcd    2       dso     pmcd_init       ${PCP_PMDAS_DIR}/pmcd/pmda_pmcd.so
 proc    3       pipe    binary          ${PCP_PMDAS_DIR}/linux/pmda_proc.so -d 3
 linux   60      dso     linux_init      ${PCP_PMDAS_DIR}/linux/pmda_linux.so
 mmv     70      dso     mmv_init        /var/lib/pcp/pmdas/mmv/pmda_mmv.so
 simple  254     dso     simple_init     ${PCP_PMDAS_DIR}/simple/pmda_simple.so
 
As can be seen from the contents of ``${PCP_PMCDCONF_PATH}``, the DSO version of the simple PMDA is in a library named **pmda_simple.so** and has an initialization 
function called **simple_init**. The domain of the simple PMDA is 254, as shown in the column headed **Id**.

.. note:: 
   For some platforms the DSO file name will not be **pmda_simple.so**. On Mac OS X it is **pmda_simple.dylib** and on Windows it is **pmda_simple.dll**.
   
Daemon PMDA
============

A DSO PMDA provides the most efficient communication between the PMDA and PMCD. This approach has some disadvantages resulting from the DSO PMDA being the same 
process as PMCD:

* An error or bug that causes a DSO PMDA to exit also causes PMCD to exit, which affects all connected client tools.

* There is only one thread of control in PMCD; as a result, a computationally expensive PMDA, or worse, a PMDA that blocks for I/O, adversely affects the performance of PMCD.

* PMCD runs as the "pcp" user; so all DSO PMDAs must also run as this user.

* A memory leak in a DSO PMDA also causes a memory leak for PMCD.

Consequently, many PMDAs are implemented as a daemon process.

The **libpcp_pmda** library is designed to allow simple implementation of a PMDA that runs as a separate process. The library functions provide a message passing 
layer acting as a generic wrapper that accepts PDUs, makes library calls using the standard DSO PMDA interface, and sends PDUs. Therefore, you can implement a PMDA 
as a DSO and then install it as either a daemon or a DSO, depending on the presence or absence of the generic wrapper.

The PMCD process launches a daemon PMDA with **fork** and **execv** (or **CreateProcess** on Windows). You can easily connect a pipe to the PMDA using standard 
input and output. The PMCD process may also connect to a daemon PMDA using IPv4 or IPv6 TCP/IP, or UNIX domain sockets if the platform supports that; see the 
**tcp(7), ip(7), ipv6(7)** or **unix(7)** man pages.

As superuser, install the simple PMDA as a daemon process as shown in `Example 2.2. Simple PMDA as a Daemon`_. Again, the output may differ due to operating 
system differences, other PMDAs already installed, or access control sections in the PMCD configuration file.

.. _Example 2.2. Simple PMDA as a Daemon:

**Example 2.2. Simple PMDA as a Daemon**

The specification for the simple PMDA now states the connection type of **pipe** to PMCD and the executable image for the PMDA is ``${PCP_PMDAS_DIR}/simple/pmdasimple``, 
using domain number 253.

.. sourcecode:: none

 # cd ${PCP_PMDAS_DIR}/simple
 # ./Install
 ... 
 Install simple as a daemon or dso agent? [daemon] daemon 
 PMCD should communicate with the daemon via pipe or socket? [pipe] pipe
 ...
 # cat ${PCP_PMCDCONF_PATH}
 # Performance Metrics Domain Specifications
 # 
 # This file is automatically generated during the build
 # Name  Id      IPC     IPC Params      File/Cmd
 root    1       pipe    binary          /var/lib/pcp/pmdas/root/pmdaroot
 pmcd    2       dso     pmcd_init       ${PCP_PMDAS_DIR}/pmcd/pmda_pmcd.so
 proc    3       pipe    binary          ${PCP_PMDAS_DIR}/linux/pmda_proc.so -d 3
 linux   60      dso     linux_init      ${PCP_PMDAS_DIR}/linux/pmda_linux.so
 mmv     70      dso     mmv_init        /var/lib/pcp/pmdas/mmv/pmda_mmv.so
 simple  253     pipe    binary          ${PCP_PMDAS_DIR}/simple/pmdasimple -d 253

Caching PMDA
=============

When either the cost or latency associated with collecting performance metrics is high, the PMDA implementer may choose to trade off the currency of the 
performance data to reduce the PMDA resource demands or the fetch latency time.

One scheme for doing this is called a caching PMDA, which periodically instantiates values for the performance metrics and responds to each request from PMCD 
with the most recently instantiated (or cached) values, as opposed to instantiating current values on demand when the PMCD asks for them.

The Cisco PMDA is an example of a caching PMDA. For additional information, see the contents of the ``${PCP_PMDAS_DIR}/cisco`` directory and the **pmdacisco(1)** 
man page.

Domains, Metrics, Instances and Labels
****************************************

This section defines metrics and instances, discusses how they should be designed for a particular target domain, and shows how to implement support for them.

The examples in this section are drawn from the trivial and simple PMDAs. Refer to the ``${PCP_PMDAS_DIR}/trivial`` and ``${PCP_PMDAS_DIR}/simple`` directories, 
respectively, where both binaries and source code are available.

Overview
==========

*Domains* are autonomous performance areas, such as the operating system or a layered service or a particular application. *Metrics* are raw performance data for 
a domain, and typically quantify activity levels, resource utilization or quality of service. *Instances* are sets of related metrics, as for multiple processors, 
or multiple service classes, or multiple transaction types.

PCP employs the following simple and uniform data model to accommodate the demands of performance metrics drawn from multiple domains:

* Each metric has an identifier that is unique across all metrics for all PMDAs on a particular host.

* Externally, metrics are assigned names for user convenience--typically there is a 1:1 relationship between a metric name and a metric identifier.

* The PMDA implementation determines if a particular metric has a singular value or a set of (zero or more) values. For instance, the metric **hinv.ndisk** 
  counts the number of disks and has only one value on a host, whereas the metric **disk.dev.total** counts disk I/O operations and has one value for each disk 
  on the host.

* If a metric has a set of values, then members of the set are differentiated by instances. The set of instances associated with a metric is an *instance domain*. 
  For example, the set of metrics **disk.dev.total** is defined over an instance domain that has one member per disk spindle.

The selection of metrics and instances is an important design decision for a PMDA implementer. The metrics and instances for a target domain should have the following qualities:

* Obvious to a user

* Consistent across the domain

* Accurately representative of the operational and functional aspects of the domain

For each metric, you should also consider these questions:

* How useful is this value?

* What units give a good sense of scale?

* What name gives a good description of the metric's meaning?

* Can this metric be combined with another to convey the same useful information?

As with all programming tasks, expect to refine the choice of metrics and instances several times during the development of the PMDA.

Domains
========

Each PMDA must be uniquely identified by PMCD so that requests from clients can be efficiently routed to the appropriate PMDA. The unique identifier, the PMDA's 
domain, is encoded within the metrics and instance domain identifiers so that they are associated with the correct PMDA, and so that they are unique, regardless 
of the number of PMDAs that are connected to the PMCD process.

The default domain number for each PMDA is defined in ``${PCP_VAR_DIR}/pmns/stdpmid``. This file is a simple table of PMDA names and their corresponding domain 
number. However, a PMDA does not have to use this domain number--the file is only a guide to help avoid domain number clashes when PMDAs are installed and 
activated.

The domain number a PMDA uses is passed to the PMDA by PMCD when the PMDA is launched. Therefore, any data structures that require the PMDA's domain number must 
be set up when the PMDA is initialized, rather than declared statically. The protocol for PMDA initialization provides a standard way for a PMDA to implement 
this run-time initialization.

.. note::
   Although uniqueness of the domain number in the ``${PCP_PMCDCONF_PATH}`` control file used by PMCD is all that is required for successful starting of PMCD and 
   the associated PMDAs, the developer of a new PMDA is encouraged to add the default domain number for each new PMDA to the ``${PCP_VAR_DIR}/pmns/stdpmid.local`` 
   file and then to run the **Make.stdpmid** script in ``${PCP_VAR_DIR}/pmns`` to recreate ``${PCP_VAR_DIR}/pmns/stdpmid``; this file acts as a repository for 
   documenting the known default domain numbers.
   
Metrics
========

A PMDA provides support for a collection of metrics. In addition to the obvious performance metrics, and the measures of time, activity and resource utilization, 
the metrics should also describe how the target domain has been configured, as this can greatly affect the correct interpretation of the observed performance. For 
example, metrics that describe network transfer rates should also describe the number and type of network interfaces connected to the host (**hinv.ninterface**, 
**network.interface.speed, network.interface.duplex**, and so on)

In addition, the metrics should describe how the PMDA has been configured. For example, if the PMDA was periodically probing a system to measure quality of 
service, there should be metrics for the delay between probes, the number of probes attempted, plus probe success and failure counters. It may also be appropriate 
to allow values to be stored (see the **pmstore(1)** man page) into the delay metric, so that the delay used by the PMDA can be altered dynamically.

Data Structures
----------------

Each metric must be described in a **pmDesc** structure; see the **pmLookupDesc(3)** man page:

.. sourcecode:: none

 typedef struct { 
     pmID        pmid;           /* unique identifier */ 
     int         type;           /* base data type */ 
     pmInDom     indom;          /* instance domain */ 
     int         sem;            /* semantics of value */ 
     pmUnits     units;          /* dimension and units */ 
 } pmDesc;

This structure contains the following fields:

**pmid**

A unique identifier, Performance Metric Identifier (PMID), that differentiates this metric from other metrics across the union of all PMDAs

**type**

A data type indicator showing whether the format is an integer (32 or 64 bit, signed or unsigned); float; double; string; or arbitrary aggregate of binary data

**indom**

An instance domain identifier that links this metric to an instance domain

**sem**

An encoding of the value's semantics (counter, instantaneous, or discrete)

**units**

A description of the value's units based on dimension and scale in the three orthogonal dimensions of space, time, and count (or events)

.. note::
   This information can be observed for metrics from any active PMDA using **pminfo** command line options, for example:

   .. sourcecode:: none

     $ pminfo -d -m network.interface.out.drops

     network.interface.out.drops PMID: 60.3.11
         Data Type: 64-bit unsigned int  InDom: 60.3 0xf000003
         Semantics: counter  Units: count
         
Symbolic constants of the form **PM_TYPE\*, PM_SEM_\*, PM_SPACE_\*, PM_TIME_\***, and **PM_COUNT_\*** are defined in the **<pcp/pmapi.h>** header file. 
You may use them to initialize the elements of a **pmDesc** structure. The **pmID** type is an unsigned integer that can be safely cast to a **__pmID_int** 
structure, which contains fields defining the metric's (PMDA's) domain, cluster, and item number as shown in `Example 2.3. __pmID_int Structure`_:

.. _Example 2.3. __pmID_int Structure:

**Example 2.3. __pmID_int Structure**

.. sourcecode:: none

 typedef struct { 
         int             flag:1;
         unsigned int    domain:9;
         unsigned int    cluster:12;
         unsigned int    item:10;
 } __pmID_int;
 
For additional information, see the **<pcp/libpcp.h>** file.

The **flag** field should be ignored. The **domain** number should be set at run time when the PMDA is initialized. The **PMDA_PMID** macro defined in 
**<pcp/pmapi.h>** can be used to set the **cluster** and **item** fields at compile time, as these should always be known and fixed for a particular metric.

.. note::
   The three components of the PMID should correspond exactly to the three-part definition of the PMID for the corresponding metric in the PMNS described in Section 2.4.3, “`Name Space`_”.

A table of **pmdaMetric** structures should be defined within the PMDA, with one structure per metric as shown in `Example 2.4. pmdaMetric Structure`_.

.. _Example 2.4. pmdaMetric Structure:

**Example 2.4. pmdaMetric Structure**

.. sourcecode:: none

 typedef struct { 
     void        *m_user;        /* for users external use */ 
     pmDesc      m_desc;         /* metric description */ 
 } pmdaMetric;
 
This structure contains a **pmDesc** structure and a handle that allows PMDA-specific structures to be associated with each metric. For example, **m_user** 
could be a pointer to a global variable containing the metric value, or a pointer to a function that may be called to instantiate the metric's value.

The trivial PMDA, shown in `Example 2.5. Trivial PMDA`_, has only a singular metric (that is, no instance domain):

.. _Example 2.5. Trivial PMDA:

**Example 2.5. Trivial PMDA**

.. sourcecode:: none

 static pmdaMetric metrictab[] = {
 /* time */
  { NULL,
    { PMDA_PMID(0, 1), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT,
      PMDA_PMUNITS(0, 1, 0, 0, PM_TIME_SEC, 0) }, },
 };
 
This single metric (**trivial.time**) has the following:

* A PMID with a cluster of 0 and an item of 1. Note that this is not yet a complete PMID, the domain number which identifies the PMDA will be combined with it at runtime.

* An unsigned 32-bit integer (**PM_TYPE_U32**)

* A singular value and hence no instance domain (**PM_INDOM_NULL**)

* An instantaneous semantic value (**PM_SEM_INSTANT**)

* Dimension “time” and the units “seconds”

Semantics
-----------

The metric's semantics describe how PCP tools should interpret the metric's value. The following are the possible semantic types:

* Counter (**PM_SEM_COUNTER**)

* Instantaneous value (**PM_SEM_INSTANT**)

* Discrete value (**PM_SEM_DISCRETE**)

A counter should be a value that monotonically increases (or monotonically decreases, which is less likely) with respect to time, so that the rate of change 
should be used in preference to the actual value. Rate conversion is not appropriate for metrics with instantaneous values, as the value is a snapshot and there 
is no basis for assuming any values that might have been observed between snapshots. Discrete is similar to instantaneous; however, once observed it is presumed 
the value will persist for an extended period (for example, system configuration, static tuning parameters and most metrics with non-numeric values).

For a given time interval covering six consecutive timestamps, each spanning two units of time, the metric values in `Example 2.6. Effect of Semantics on a Metric`_ 
are exported from a PMDA (“N/A” implies no value is available):

.. _Example 2.6. Effect of Semantics on a Metric:

**Example 2.6. Effect of Semantics on a Metric**

.. sourcecode:: none

 Timestamps:         1   3   5   7   9  11 
 Value:             10  30  60  80  90 N/A
 
The default display of the values would be as follows:

.. sourcecode:: none

 Timestamps:         1   3   5   7   9  11 
 Semantics: 
 Counter           N/A  10  15  10   5 N/A 
 Instantaneous      10  30  60  80  90 N/A 
 Discrete           10  30  60  80  90  90
 
Note that these interpretations of metric semantics are performed by the monitor tool, automatically, before displaying a value and they are not transformations 
that the PMDA performs.

Instances
==========

Singular metrics have only one value and no associated instance domain. Some metrics contain a set of values that share a common set of semantics for a specific 
instance, such as one value per processor, or one value per disk spindle, and so on.

.. note::
   The PMDA implementation is solely responsible for choosing the instance identifiers that differentiate instances within the instance domain. The PMDA is also 
   responsible for ensuring the uniqueness of instance identifiers in any instance domain, as described in Section 2.3.4.1, “`Instance Identification`_”.
   
Instance Identification
------------------------

Consistent interpretation of instances and instance domains require a few simple rules to be followed by PMDA authors. The PMDA library provides a series of 
**pmdaCache** routines to assist.

* Each internal instance identifier (numeric) must be a unique 31-bit number.

* The external instance name (string) must be unique.

* When the instance name contains a space, the name to the left of the first space (the short name) must also be unique.

* Where an external instance name corresponds to some object or entity, there is an expectation that the association between the name and the object is fixed.

* It is preferable, although not mandatory, for the association between and external instance name (string) and internal instance identifier (numeric) to be persistent.

N Dimensional Data
--------------------

Where the performance data can be represented as scalar values (singular metrics) or one-dimensional arrays or lists (metrics with an instance domain), the PCP 
framework is more than adequate. In the case of metrics with an instance domain, each array or list element is associated with an instance from the instance 
domain.

To represent two or more dimensional arrays, the coordinates must be one of the following:

* Mapped onto one dimensional coordinates.
* Enumerated into the Performance Metrics Name Space (PMNS).

For example, this 2 x 3 array of values called M can be represented as instances 1,..., 6 for a metric M:

.. sourcecode:: none

  M[1]   M[2]   M[3] 
  M[4]   M[5]   M[6]

Or they can be represented as instances 1, 2, 3 for metric M1 and instances 1, 2, 3 for metric M2:

.. sourcecode:: none

  M1[1]  M1[2]  M1[3] 
  M2[1]  M2[2]  M2[3]

The PMDA implementer must decide and consistently export this encoding from the N-dimensional instrumentation to the 1-dimensional data model of the PCP. The use 
of metric label metadata - arbitrary key/value pairs - allows the implementer to capture the higher dimensions of the performance data.

In certain special cases (for example, such as for a histogram), it may be appropriate to export an array of values as raw binary data (the type encoding in the 
descriptor is **PM_TYPE_AGGREGATE**). However, this requires the development of special PMAPI client tools, because the standard PCP tools have no knowledge of 
the structure and interpretation of the binary data. The usual issues of platform-dependence must also be kept in mind for this case - endianness, word-size, 
alignment and so on - the (possibly remote) special PMAPI client tools may need this information in order to decode the data successfully.

Data Structures
-----------------

If the PMDA is required to support instance domains, then for each instance domain the unique internal instance identifier and external instance identifier should 
be defined using a **pmdaInstid** structure as shown in `Example 2.7. pmdaInstid Structure`_:

.. _Example 2.7.  pmdaInstid Structure:

**Example 2.7.  pmdaInstid Structure**

.. sourcecode:: none

 typedef struct { 
     int         i_inst;         /* internal instance identifier */ 
     char        *i_name;        /* external instance identifier */ 
 } pmdaInstid;

The **i_inst** instance identifier must be a unique integer within a particular instance domain.

The complete instance domain description is specified in a **pmdaIndom** structure as shown in `Example 2.8. pmdaIndom Structure`_:

.. _Example 2.8.  pmdaIndom Structure:

**Example 2.8.  pmdaIndom Structure**

.. sourcecode:: none

 typedef struct { 
     pmInDom     it_indom;       /* indom, filled in */ 
     int         it_numinst;     /* number of instances */ 
     pmdaInstid  *it_set;        /* instance identifiers */ 
 } pmdaIndom;
 
The **it_indom** element contains a **pmInDom** that must be unique across every PMDA. The other fields of the **pmdaIndom** structure are the number of instances 
in the instance domain and a pointer to an array of instance descriptions.

`Example 2.9. __pmInDom_int Structure`_ shows that the **pmInDom** can be safely cast to **__pmInDom_int**, which specifies the PMDA's domain and the instance 
number within the PMDA:

.. _Example 2.9. __pmInDom_int Structure:

**Example 2.9. __pmInDom_int Structure**

.. sourcecode:: none

 typedef struct { 
         int             flag:1;
         unsigned int    domain:9;   /* the administrative PMD */ 
         unsigned int    serial:22;  /* unique within PMD */         
 } __pmInDom_int;
 
As with metrics, the PMDA domain number is not necessarily known until run time; so the **domain** field must be set up when the PMDA is initialized.

For information about how an instance domain may also be associated with more than one metric, see the **pmdaInit(3)** man page.

The simple PMDA, shown in `Example 2.10. Simple PMDA`_, has five metrics and two instance domains of three instances.

.. _Example 2.10. Simple PMDA:

**Example 2.10. Simple PMDA**

.. sourcecode:: none

 /* 
  * list of instances 
  */ 
 static pmdaInstid color[] = {
     { 0, “red” }, { 1, “green” }, { 2, “blue” }
 };
 static pmdaInstid       *timenow = NULL;
 static unsigned int     timesize = 0;
 /*
  * list of instance domains
  */
 static pmdaIndom indomtab[] = {
 #define COLOR_INDOM     0
     { COLOR_INDOM, 3, color },
 #define NOW_INDOM       1
    { NOW_INDOM, 0, NULL },
 };
 /*
  * all metrics supported in this PMDA - one table entry for each
  */
 static pmdaMetric metrictab[] = {
 /* numfetch */
     { NULL,
       { PMDA_PMID(0, 0), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT,
        PMDA_PMUNITS(0, 0, 0, 0, 0, 0) }, },
 /* color */
     { NULL,
       { PMDA_PMID(0, 1), PM_TYPE_32, COLOR_INDOM, PM_SEM_INSTANT,
         PMDA_PMUNITS(0, 0, 0, 0, 0, 0) }, },
 /* time.user */
     { NULL,
       { PMDA_PMID(1, 2), PM_TYPE_DOUBLE, PM_INDOM_NULL, PM_SEM_COUNTER,
         PMDA_PMUNITS(0, 1, 0, 0, PM_TIME_SEC, 0) }, },
 /* time.sys */
     { NULL,
       { PMDA_PMID(1,3), PM_TYPE_DOUBLE, PM_INDOM_NULL, PM_SEM_COUNTER,
         PMDA_PMUNITS(0, 1, 0, 0, PM_TIME_SEC, 0) }, },
 /* now */
     { NULL,
       { PMDA_PMID(2,4), PM_TYPE_U32, NOW_INDOM, PM_SEM_INSTANT,
         PMDA_PMUNITS(0, 0, 0, 0, 0, 0) }, },
 };

The metric **simple.color** is associated, via **COLOR_INDOM**, with the first instance domain listed in **indomtab**. PMDA initialization assigns the correct 
domain portion of the instance domain identifier in **indomtab[0].it_indom** and **metrictab[1].m_desc.indom**. This instance domain has three instances: red, green, 
and blue.

The metric **simple.now** is associated, via **NOW_INDOM**, with the second instance domain listed in **indomtab**. PMDA initialization assigns the correct domain 
portion of the instance domain identifier in **indomtab[1].it_indom** and **metrictab[4].m_desc.indom**. This instance domain is dynamic and initially has no 
instances.

All other metrics are singular, as specified by **PM_INDOM_NULL**.

In some cases an instance domain may vary dynamically after PMDA initialization (for example, **simple.now**), and this requires some refinement of the default 
functions and data structures of the **libpcp_pmda** library. Briefly, this involves providing new functions that act as wrappers for **pmdaInstance** and **pmdaFetch** 
while understanding the dynamics of the instance domain, and then overriding the instance and fetch methods in the **pmdaInterface** structure during PMDA 
initialization.

For the simple PMDA, the wrapper functions are **simple_fetch** and **simple_instance**, and defaults are over-ridden by the following assignments in the 
**simple_init** function:

.. sourcecode:: none

 dp->version.any.fetch = simple_fetch;
 dp->version.any.instance = simple_instance;
 
Labels
=======

Metrics and instances can be further described through the use of metadata labels, which are arbitrary name:value pairs associated with individual metrics and 
instances. There are several applications of this concept, but one of the most important is the ability to differentiate the components of a multi-dimensional 
instance name, such as the case of the **mem.zoneinfo.numa_hit** metric which has one value per memory zone, per NUMA node.

Consider `Example 2.11. Multi-dimensional Instance Domain Labels`_:

.. _Example 2.11. Multi-dimensional Instance Domain Labels:

**Example 2.11. Multi-dimensional Instance Domain Labels**

.. sourcecode:: none
 
  $ pminfo -l mem.zoneinfo.numa_hit
 
  mem.zoneinfo.numa_hit
     inst [0 or "DMA::node0"] labels {"device_type":["numa_node","memory"],"indom_name":"per zone per numa_node","numa_node":0,"zone":"DMA"}
     inst [1 or "Normal::node0"] labels {"device_type":["numa_node","memory"],"indom_name":"per zone per numa_node","numa_node":0,"zone":"Normal"}
     inst [2 or "DMA::node1"] labels {"device_type":["numa_node","memory"],"indom_name":"per zone per numa_node","numa_node":1,"zone":"DMA"}
     inst [3 or "Normal::node1"] labels {"device_type":["numa_node","memory"],"indom_name":"per zone per numa_node","numa_node":1,"zone":"Normal"}

.. note::
   The metric labels used here individually describe the memory zone and NUMA node associated with each instance.

The PMDA implementation is only partially responsible for choosing the label identifiers that differentiate components of metrics and instances within an instance 
domain. Label sets for a singleton metric or individual instance of a set-valued metric are formed from a label hierarchy, which includes global labels applied to 
all metrics and instances from one PMAPI context.

Labels are stored and communicated within PCP using JSONB format. This format is a restricted form of JSON suitable for indexing and other operations. In JSONB 
form, insignificant whitespace is discarded, and the order of label names is not preserved. Within the PMCS a lexicographically sorted key space is always 
maintained, however. Duplicate label names are not permitted. The label with highest precedence is the only one presented. If duplicate names are presented at 
the same hierarchy level, only one will be preserved (exactly which one wins is arbitrary, so do not rely on this).

Label Hierarchy
----------------

The set of labels associated with any singleton metric or instance is formed by merging the sets of labels at each level of a hierarchy. The lower levels of the 
hierarchy have highest precedence when merging overlapping (duplicate) label names:

* Global context labels (as reported by the **pmcd.labels** metric) are the lowest precedence. The PMDA implementor has no influence over labels at this level of 
  the hierarchy, and these labels are typically supplied by **pmcd** from **/etc/pcp/labels** files.

* Domain labels, for all metrics and instances of a PMDA, are the next highest precedence.

* Instance Domain labels, associated with an InDom, are the next highest precedence.

* Metric cluster labels, associated with a PMID cluster, are the next highest precedence.

* Metric item labels, associated with an individual PMID, are the next highest precedence.

* Instance labels, associated with a metric instance identifier, have the highest precedence.

Data Structures
-----------------

In any PMDA that supports labels at any level of the hierarchy, each individual label (one name:value pair) requires a **pmLabel** structure as shown in `Example 2.12. pmLabel Structure`_:

.. _Example 2.12. pmLabel Structure:

**Example 2.12. pmLabel Structure**

.. sourcecode:: none

 typedef struct { 
     uint     name : 16;      /* label name offset in JSONB string */
     uint     namelen : 8;    /* length of name excluding the null */
     uint     flags : 8;      /* information about this label */
     uint     value : 16;     /* offset of the label value */
     uint     valuelen : 16;  /* length of value in bytes */
 } pmLabel;

The **flags** field is a bitfield identifying the hierarchy level and whether this name:value pair is intrinsic (optional) or extrinsic (part of the mandatory, 
identifying metadata for the metric or instance). All other fields are offsets and lengths in the JSONB string from an associated **pmLabelSet** structure.

Zero or more labels are specified via a label set, in a **pmLabelSet** structure as shown in `Example 2.13. pmLabelSet Structure`_:

.. _Example 2.13. pmLabelSet Structure:

**Example 2.13. pmLabelSet Structure**

.. sourcecode:: none

 typedef struct { 
     uint     inst;          /* PM_IN_NULL or the instance ID */ 
     int      nlabels;       /* count of labels or error code */
     char     *json;         /* JSONB formatted labels string */
     uint     jsonlen : 16;  /* JSON string length byte count */
     uint     padding : 16;  /* zero, reserved for future use */
     pmLabel  *labels;       /* indexing into the JSON string */
 } pmLabelSet;
 
This provides information about the set of labels associated with an entity (context, domain, indom, metric cluster, item or instance). The entity will be from 
any one level of the label hierarchy. If at the lowest hierarchy level (which happens to be highest precedence - instances) then the **inst** field will contain 
an actual instance identifier instead of PM_IN_NULL.

For information about how a label can be associated with each level of the hierarchy, see the **pmdaLabel(3)** man page.

The simple PMDA, shown in `Example 2.14. Simple PMDA`_, associates labels at the domain, indom and instance levels of the hierarchy.

.. _Example 2.14. Simple PMDA:

**Example 2.14. Simple PMDA**

.. sourcecode:: none

 static int
 simple_label(int ident, int type, pmLabelSet **lpp, pmdaExt *pmda)
 {
     int         serial;
 
     switch (type) {
     case PM_LABEL_DOMAIN:
         pmdaAddLabels(lpp, "{"role":"testing"}");
         break;
     case PM_LABEL_INDOM:
         serial = pmInDom_serial((pmInDom)ident);
         if (serial == COLOR_INDOM) {
             pmdaAddLabels(lpp, "{"indom_name":"color"}");
             pmdaAddLabels(lpp, "{"model":"RGB"}");
         }
         if (serial == NOW_INDOM) {
             pmdaAddLabels(lpp, "{"indom_name":"time"}");
             pmdaAddLabels(lpp, "{"unitsystem":"SI"}");
         }
         break;
     case PM_LABEL_CLUSTER:
     case PM_LABEL_ITEM:
         /* no labels to add for these types, fall through */
     default:
         break;
     }
     return pmdaLabel(ident, type, lpp, pmda);
 }
 
 static int
 simple_labelCallBack(pmInDom indom, unsigned int inst, pmLabelSet **lp)
 {
     struct timeslice *tsp;
 
     if (pmInDom_serial(indom) != NOW_INDOM)
         return 0;
     if (pmdaCacheLookup(indom, inst, NULL, (void *)&tsp) != PMDA_CACHE_ACTIVE)
         return 0;
     /* SI units label, value: sec (seconds), min (minutes), hour (hours) */
     return pmdaAddLabels(lp, "{"units":"%s"}", tsp-<tm_name);
 }
 
The **simple_labelCallBack** function is called indirectly via **pmdaLabel** for each instance of the **NOW_INDOM**. PMDA initialization ensures these functions 
are registered with the global PMDA interface structure for use when handling label requests, by the following assignments in the **simple_init** function:

.. sourcecode:: none

 dp->version.seven.label = simple_label;
 pmdaSetLabelCallBack(dp, simple_labelCallBack);
 
Other Issues
*************

Other issues include extracting the information, latency and threads of control, Name Space, PMDA help text, and management of evolution within a PMDA.

Extracting the Information
============================

A suggested approach to writing a PMDA is to write a standalone program to extract the values from the target domain and then incorporate this program into the 
PMDA framework. This approach avoids concurrent debugging of two distinct problems:

1. Extraction of the data

2. Communication with PMCD

These are some possible ways of exporting the data from the target domain:

* Accumulate the performance data in a public shared memory segment.

* Write the performance data to the end of a log file.

* Periodically rewrite a file with the most recent values for the performance data.

* Implement a protocol that allows a third party to connect to the target application, send a request, and receive new performance data.

* If the data is in the operating system kernel, provide a kernel interface (preferred) to export the performance data.

Most of these approaches require some further data processing by the PMDA.

Latency and Threads of Control
===============================

The PCP protocols expect PMDAs to return the current values for performance metrics when requested, and with short delay (low latency). For some target domains, 
access to the underlying instrumentation may be costly or involve unpredictable delays (for example, if the real performance data is stored on some remote host or 
network device). In these cases, it may be necessary to separate probing for new performance data from servicing PMCD requests.

An architecture that has been used successfully for several PMDAs is to create one or more child processes to obtain information while the main process 
communicates with PMCD.

At the simplest deployment of this arrangement, the two processes may execute without synchronization. Threads have also been used as a more portable multithreading 
mechanism; see the **pthreads(7)** man page.

By contrast, a complex deployment would be one in which the refreshing of the metric values must be atomic, and this may require double buffering of the data 
structures. It also requires coordination between parent and child processes.

.. warning::
   Since certain data structures used by the PMDA library are not thread-aware, only one PMDA thread of control should call PMDA library functions - this would 
   typically be the thread servicing requests from PMCD.

One caveat about this style of caching PMDA--in this (special) case it is better if the PMDA converts counts to rates based upon consecutive periodic sampling 
from the underlying instrumentation. By exporting precomputed rate metrics with instantaneous semantics, the PMDA prevents the PCP monitor tools from computing 
their own rates upon consecutive PMCD fetches (which are likely to return identical values from a caching PMDA). The finer points of metric semantics are 
discussed in Section 2.3.3.2, “`Semantics`_”

Name Space
===========

The PMNS file defines the name space of the PMDA. It is a simple text file that is used during installation to expand the Name Space of the PMCD process. The 
format of this file is described by the **pmns(5)** man page and its hierarchical nature, syntax, and helper tools are further described in the *Performance Co-Pilot User's and Administrator's Guide*.

Client processes will not be able to access the PMDA metrics if the PMNS file is not installed as part of the PMDA installation procedure on the collector host. 
The installed list of metric names and their corresponding PMIDs can be found in ``${PCP_VAR_DIR}/pmns/root``.

`Example 2.15. pmns File for the Simple PMDA`_ shows the simple PMDA, which has five metrics:

Three metrics immediately under the **simple** node

Two metrics under another non-terminal node called **simple.time**

.. _Example 2.15. pmns File for the Simple PMDA:

**Example 2.15. pmns File for the Simple PMDA**

.. sourcecode:: none

 simple {
     numfetch    SIMPLE:0:0
     color       SIMPLE:0:1
     time
     now         SIMPLE:2:4
 }
 simple.time {
     user        SIMPLE:1:2
     sys         SIMPLE:1:3
 }

Metrics that have different clusters do not have to be specified in different subtrees of the PMNS. `Example 2.16. Alternate pmns File for the Simple PMDA`_ 
shows an alternative PMNS for the simple PMDA:

.. _Example 2.16. Alternate pmns File for the Simple PMDA:

**Example 2.16. Alternate pmns File for the Simple PMDA**

.. sourcecode:: none

 simple { 
     numfetch    SIMPLE:0:0 
     color       SIMPLE:0:1 
     usertime    SIMPLE:1:2 
     systime     SIMPLE:1:3 
 }

In this example, the **SIMPLE** macro is replaced by the domain number listed in ``${PCP_VAR_DIR}/pmns/stdpmid`` for the corresponding PMDA during installation 
(for the simple PMDA, this would normally be the value 253).

If the PMDA implementer so chooses, all or a subset of the metric names and identifiers can be specified programatically. In this situation, a special asterisk 
syntax is used to denote those subtrees which are to be handles this way. `Example 2.17. Dynamic metrics pmns File for the Simple PMDA`_ shows this dynamic 
namespace syntax, for all metrics in the simple PMDA:

.. _Example 2.17. Dynamic metrics pmns File for the Simple PMDA:

**Example 2.17. Dynamic metrics pmns File for the Simple PMDA**

.. sourcecode:: none

 simple         SIMPLE:*:*
 
In this example, like the one before, the **SIMPLE** macro is replaced by the domain number, and all (simple.*) metric namespace operations must be handled by 
the PMDA. This is in contrast to the static metric name model earlier, where the host-wide PMNS file is updated and used by PMCD, acting on behalf of the agent.

PMDA Help Text
===============

For each metric defined within a PMDA, the PMDA developer is strongly encouraged to provide both terse and extended help text to describe the metric, and perhaps 
provide hints about the expected value ranges.

The help text is used to describe each metric in the visualization tools and **pminfo** with the **-T** option. The help text, such as the help text for the 
simple PMDA in `Example 2.18. Help Text for the Simple PMDA`_, is specified in a specially formatted file, normally called **help**. This file is converted to 
the expected run-time format using the **newhelp** command; see the **newhelp(1)** man page. Converted help text files are usually placed in the PMDA's directory 
below ``${PCP_PMDAS_DIR}`` as part of the PMDA installation procedure.

.. _Example 2.18. Help Text for the Simple PMDA:

**Example 2.18. Help Text for the Simple PMDA**

The two instance domains and five metrics have a short and a verbose description. Each entry begins with a line that starts with the character “@” and is followed 
by either the metric name (**simple.numfetch**) or a symbolic reference to the instance domain number (**SIMPLE.1**), followed by the short description. The verbose 
description is on the following lines, terminated by the next line starting with “@” or end of file:

.. sourcecode:: none

 @ SIMPLE.0 Instance domain “colour” for simple PMDA
 Universally 3 instances, “red” (0), “green” (1) and “blue” (3).
 
 @ SIMPLE.1 Dynamic instance domain “time” for simple PMDA
 An instance domain is computed on-the-fly for exporting current time
 information. Refer to the help text for simple.now for more details.
 
 @ simple.numfetch Number of pmFetch operations.
 The cumulative number of pmFetch operations directed to “simple” PMDA.
 
 This counter may be modified with pmstore(1).
 
 @ simple.color Metrics which increment with each fetch
 This metric has 3 instances, designated “red”, “green” and “blue”.
 
 The value of the metric is monotonic increasing in the range 0 to
 255, then back to 0.  The different instances have different starting
 values, namely 0 (red), 100 (green) and 200 (blue).
 
 The metric values my be altered using pmstore(1).
 
 @ simple.time.user Time agent has spent executing user code
 The time in seconds that the CPU has spent executing agent user code.
 
 @ simple.time.sys Time agent has spent executing system code
 The time in seconds that the CPU has spent executing agent system code.
 
 @ simple.now Time of day with a configurable instance domain
 The value reflects the current time of day through a dynamically
 reconfigurable instance domain.  On each metric value fetch request,
 the agent checks to see whether the configuration file in
 ${PCP_PMDAS_DIR}/simple/simple.conf has been modified - if it has then
 the file is re-parsed and the instance domain for this metric is again
 constructed according to its contents.
 
 This configuration file contains a single line of comma-separated time
 tokens from this set:
   “sec”  (seconds after the minute),
   “min”  (minutes after the hour),
   “hour” (hour since midnight).
 
 An example configuration file could be:  sec,min,hour
 and in this case the simple.now metric would export values for the
 three instances “sec”, “min” and “hour” corresponding respectively to
 the components seconds, minutes and hours of the current time of day.
 
 The instance domain reflects each token present in the file, and the
 values reflect the time at which the PMDA processes the fetch.
 
Management of Evolution within a PMDA
======================================

Evolution of a PMDA, or more particularly the underlying instrumentation to which it provides access, over time naturally results in the appearance of new metrics 
and the disappearance of old metrics. This creates potential problems for PMAPI clients and PCP tools that may be required to interact with both new and former 
versions of the PMDA.

The following guidelines are intended to help reduce the complexity of implementing a PMDA in the face of evolutionary change, while maintaining predictability 
and semantic coherence for tools using the PMAPI, and for end users of those tools.

* Try to support as full a range of metrics as possible in every version of the PMDA. In this context, *support* means responding sensibly to requests, even if 
  the underlying instrumentation is not available.

* If a metric is not supported in a given version of the underlying instrumentation, the PMDA should respond to **pmLookupDesc** requests with a **pmDesc** 
  structure whose **type** field has the special value **PM_TYPE_NOSUPPORT**. Values of fields other than **pmid** and **type** are immaterial, but 
  `Example 2.19. Setting Values`_ is typically benign:
  
.. _Example 2.19. Setting Values:

**Example 2.19. Setting Values**

.. sourcecode:: none
 
 pmDesc dummy = { 
      .pmid  = PMDA_PMID(3,0),           /* pmid, fill this in */
      .type  = PM_TYPE_NOSUPPORT,        /* this is the important part */
      .indom = PM_INDOM_NULL,            /* singular,causes no problems */
      .sem   = 0,                        /* no semantics */
      .units = PMDA_PMUNITS(0,0,0,0,0,0) /* no units */
 };

* If a metric lacks support in a particular version of the underlying instrumentation, the PMDA should respond to **pmFetch** requests with a **pmResult** in which 
  no values are returned for the unsupported metric. This is marginally friendlier than the other semantically acceptable option of returning an illegal PMID error 
  or **PM_ERR_PMID**.

* Help text should be updated with annotations to describe different versions of the underlying product, or product configuration options, for which a specific 
  metric is available. This is so **pmLookupText** can always respond correctly.

* The **pmStore** operation should fail with return status of **PM_ERR_PERMISSION** if a user or application tries to amend the value of an unsupported metric.

* The value extraction, conversion, and printing functions (**pmExtractValue, pmConvScale, pmAtomStr, pmTypeStr**, and **pmPrintValue**) return the **PM_ERR_CONV** 
  error or an appropriate diagnostic string, if an attempt is made to operate on a value for which **type** is **PM_TYPE_NOSUPPORT**.

* If performance tools take note of the **type** field in the **pmDesc** structure, they should not manipulate values for unsupported metrics. Even if tools ignore 
  **type** in the metric's description, following these development guidelines ensures that no misleading value is ever returned; so there is no reason to call the 
  extraction, conversion, and printing functions.
  
PMDA Interface
****************

This section describes an interface for the request handling callbacks in a PMDA. This interface is used by PMCD for communicating with DSO PMDAs and is also used 
by daemon PMDAs with **pmdaMain**.

Overview
==========

Both daemon and DSO PMDAs must handle multiple request types from PMCD. A daemon PMDA communicates with PMCD using the PDU protocol, while a DSO PMDA defines 
callbacks for each request type. To avoid duplicating this PDU processing (in the case of a PMDA that can be installed either as a daemon or as a DSO), and to 
allow a consistent framework, **pmdaMain** can be used by a daemon PMDA as a wrapper to handle the communication protocol using the same callbacks as a DSO PMDA. 
This allows a PMDA to be built as both a daemon and a DSO, and then to be installed as either.

To further simplify matters, default callbacks are declared in **<pcp/pmda.h>**:

+------------------+
| pmdaFetch        |
+------------------+
| pmdaProfile      |
+------------------+
| pmdaInstance     |
+------------------+
| pmdaDesc         |
+------------------+
| pmdaText         |
+------------------+
| pmdaStore        |
+------------------+
| pmdaPMID         |
+------------------+
| pmdaName         |
+------------------+
| pmdaChildren     |
+------------------+
| pmdaAttribute    |
+------------------+
| pmdaLabel        |
+------------------+

Each callback takes a **pmdaExt** structure as its last argument. This structure contains all the information that is required by the default callbacks in most 
cases. The one exception is **pmdaFetch**, which needs an additional callback to instantiate the current value for each supported combination of a performance 
metric and an instance.

Therefore, for most PMDAs all the communication with PMCD is automatically handled by functions in **libpcp.so** and **libpcp_pmda.so**.

Trivial PMDA
-------------

The trivial PMDA uses all of the default callbacks as shown in `Example 2.20. Request Handling Callbacks in the Trivial PMDA`_. The additional callback for 
**pmdaFetch** is defined as **trivial_fetchCallBack**:

.. _Example 2.20. Request Handling Callbacks in the Trivial PMDA:

**Example 2.20. Request Handling Callbacks in the Trivial PMDA**

.. sourcecode:: none

 static int
 trivial_fetchCallBack(pmdaMetric *mdesc, unsigned int inst, pmAtomValue *atom)
 {
    __pmID_int      *idp = (__pmID_int *)&(mdesc->m_desc.pmid);
 
    if (idp->cluster != 0 || idp->item != 0)
        return PM_ERR_PMID;
    if (inst != PM_IN_NULL)
        return PM_ERR_INST;
    atom->l = time(NULL);
    return 0;
 }

This function checks that the PMID and instance are valid, and then places the metric value for the current time into the **pmAtomValue** structure.

The callback is set up by a call to **pmdaSetFetchCallBack** in **trivial_init**. As a rule of thumb, the API routines with named ending with **CallBack** are 
helpers for the higher PDU handling routines like **pmdaFetch**. The latter are set directly using the PMDA Interface Structures, as described in Section 
2.5.2, “`PMDA Structures`_”.

Simple PMDA
-------------

The simple PMDA callback for **pmdaFetch** is more complicated because it supports more metrics, some metrics are instantiated with each fetch, and one instance 
domain is dynamic. The default **pmdaFetch** callback, shown in `Example 2.21. Request Handling Callbacks in the Simple PMDA`_, is replaced by **simple_fetch** 
in **simple_init**, which increments the number of fetches and updates the instance domain for **INDOM_NOW** before calling **pmdaFetch**:

.. _Example 2.21. Request Handling Callbacks in the Simple PMDA:

**Example 2.21. Request Handling Callbacks in the Simple PMDA**

.. sourcecode:: none
 
 static int
 simple_fetch(int numpmid, pmID pmidlist[], pmResult **resp, pmdaExt *pmda)
 {
     numfetch++;
     simple_timenow_check();
     simple_timenow_refresh();
     return pmdaFetch(numpmid, pmidlist, resp, pmda);
 }

The callback for **pmdaFetch** is defined as **simple_fetchCallBack**. The PMID is extracted from the **pmdaMetric** structure, and if valid, the appropriate field 
in the **pmAtomValue** structure is set. The available types and associated fields are described further in Section 3.4, “:ref:`Performance Metric Descriptions`” and 
:ref:`Example 3.18. pmAtomValue Structure <Example 3.18. pmAtomValue Structure>`.

.. note::
   Note that PMID validity checking need only check the cluster and item numbers, the domain number is guaranteed to be valid and the PMDA should make no 
   assumptions about the actual domain number being used at this point.

The **simple.numfetch** metric has no instance domain and is easily handled first as shown in `Example 2.22. simple.numfetch Metric`_:

.. _Example 2.22. simple.numfetch Metric:

**Example 2.22. simple.numfetch Metric**

.. sourcecode:: none

 static int
 simple_fetchCallBack(pmdaMetric *mdesc, unsigned int inst, pmAtomValue *atom)
 {
    int             i;
    static int      oldfetch;
    static double   usr, sys;
    __pmID_int      *idp = (__pmID_int *)&(mdesc->m_desc.pmid);
 
    if (inst != PM_IN_NULL &&
        !(idp->cluster == 0 && idp->item == 1) &&
        !(idp->cluster == 2 && idp->item == 4))
        return PM_ERR_INST;
    if (idp->cluster == 0) {
        if (idp->item == 0) {                   /* simple.numfetch */
            atom->l = numfetch;
        }

In `Example 2.23. simple.color Metric`_, the **inst** parameter is used to specify which instance is required for the **simple.color** metric:

.. _Example 2.23. simple.color Metric:

**Example 2.23. simple.color Metric**

.. sourcecode:: none


       else if (idp->item == 1) {              /* simple.color */
             switch (inst) {
             case 0:                             /* red */
                 red = (red + 1) % 256;
                 atom->l = red;
                 break;
             case 1:                             /* green */
                 green = (green + 1) % 256;
                 atom->l = green;
                 break;
             case 2:                             /* blue */
                 blue = (blue + 1) % 256;
                 atom->l = blue;
                 break;
             default:
                 return PM_ERR_INST;
             }
        }
        else
            return PM_ERR_PMID;

In `Example 2.24. simple.time Metric`_, the **simple.time** metric is in a second cluster and has a simple optimization to reduce the overhead of calling **times** 
twice on the same fetch and return consistent values from a single call to **times** when both metrics **simple.time.user** and **simple.time.sys** are requested 
in a single **pmFetch**. The previous fetch count is used to determine if the **usr** and **sys** values should be updated:

.. _Example 2.24. simple.time Metric:

**Example 2.24. simple.time Metric**

.. sourcecode:: none
 
    else if (idp->cluster == 1) {               /* simple.time */
        if (oldfetch < numfetch) {
            __pmProcessRunTimes(&usr, &sys);
            oldfetch = numfetch;
        }
        if (idp->item == 2)                     /* simple.time.user */
            atom->d = usr;
        else if (idp->item == 3)                /* simple.time.sys */
            atom->d = sys;
        else
            return PM_ERR_PMID;
     }

In `Example 2.25. simple.now Metric`_, the **simple.now** metric is in a third cluster and uses **inst** again to select a specific instance from the **INDOM_NOW** 
instance domain. The values associated with instances in this instance domain are managed using the **pmdaCache(3)** helper routines, which provide efficient interfaces 
for managing more complex instance domains:

.. _Example 2.25. simple.now Metric:

**Example 2.25. simple.now Metric**

.. sourcecode:: none

     else if (idp->cluster == 2) {
         if (idp->item == 4) {                 /* simple.now */
             struct timeslice *tsp;
             sts = pmdaCacheLookup(*now_indom, inst, NULL, (void *)&tsp);
             if (sts != PMDA_CACHE_ACTIVE) {
                 if (sts < 0)
                     pmNotifyErr(LOG_ERR, "pmdaCacheLookup failed: inst=%d: %s",
                                   inst, pmErrStr(sts));
                 return PM_ERR_INST;
             }
             atom->l = tsp->tm_field;
         }
         else 
             return PM_ERR_PMID;
     }
     
simple_store in the Simple PMDA
----------------------------------

The simple PMDA permits some of the metrics it supports to be modified by **pmStore** as shown in `Example 2.26. simple_store in the Simple PMDA`_. 
For additional information, see the **pmstore(1)** and **pmStore(3)** man pages.

.. _Example 2.26. simple_store in the Simple PMDA:

**Example 2.26. simple_store in the Simple PMDA**

The **pmdaStore** callback (which returns **PM_ERR_PERMISSION** to indicate no metrics can be altered) is replaced by **simple_store** in **simple_init**. 
This replacement function must take the same arguments so that it can be assigned to the function pointer in the **pmdaInterface** structure.

The function traverses the **pmResult** and checks the cluster and unit of each PMID to ensure that it corresponds to a metric that can be changed. Checks are 
made on the values to ensure they are within range before being assigned to variables in the PMDA that hold the current values for exported metrics:

.. sourcecode:: none

 static int
 simple_store(pmResult *result, pmdaExt *pmda)
 {
     int         i, j, val, sts = 0;
     pmAtomValue av;
     pmValueSet  *vsp = NULL;
     __pmID_int  *pmidp = NULL;
 
     /* a store request may affect multiple metrics at once */
     for (i = 0; i < result->numpmid; i++) {
         vsp = result->vset[i];
         pmidp = (__pmID_int *)&vsp->pmid;
         if (pmidp->cluster == 0) {  /* storable metrics are cluster 0 */
             switch (pmidp->item) {
             case 0:                           /* simple.numfetch */
                 val = vsp->vlist[0].value.lval;
                 if (val < 0) {
                     sts = PM_ERR_SIGN;
                     val = 0;
                 }
                 numfetch = val;
                 break;
             case 1:                             /* simple.color */
                 /* a store request may affect multiple instances at once */
                 for (j = 0; j < vsp->numval && sts == 0; j++) {
                     val = vsp->vlist[j].value.lval;
                     if (val < 0) {
                         sts = PM_ERR_SIGN;
                         val = 0;
                     } if (val > 255) {
                         sts = PM_ERR_CONV;
                         val = 255;
                     }

The **simple.color** metric has an instance domain that must be searched because any or all instances may be specified. Any instances that are not supported in 
this instance domain should cause an error value of **PM_ERR_INST** to be returned as shown in `Example 2.27. simple.color and PM_ERR_INST Errors`_:

.. _Example 2.27. simple.color and PM_ERR_INST Errors:

**Example 2.27. simple.color and PM_ERR_INST Errors**

.. sourcecode:: none
 
                        switch (vsp->vlist[j].inst) {
                        case 0:                         /* red */
                            red = val;
                            break;
                        case 1:                         /* green */
                            green = val;
                            break;
                        case 2:                         /* blue */
                            blue = val;
                            break;
                        default:
                            sts = PM_ERR_INST;
                        }

Any other PMIDs in cluster 0 that are not supported by the simple PMDA should result in an error value of **PM_ERR_PMID** as shown in 
`Example 2.28. PM_ERR_PMID Errors`_:

.. _Example 2.28. PM_ERR_PMID Errors:

**Example 2.28. PM_ERR_PMID Errors**

.. sourcecode:: none
 
                 default:
                     sts = PM_ERR_PMID;
                     break;
             }
         }

Any metrics that cannot be altered should generate an error value of **PM_ERR_PERMISSION**, and metrics not supported by the PMDA should result in an error value 
of **PM_ERR_PMID** as shown in `Example 2.29. PM_ERR_PERMISSION and PM_ERR_PMID Errors`_:

.. _Example 2.29. PM_ERR_PERMISSION and PM_ERR_PMID Errors:

**Example 2.29. PM_ERR_PERMISSION and PM_ERR_PMID Errors**

.. sourcecode:: none
 
         else if ((pmidp->cluster == 1 &&
                  (pmidp->item == 2 || pmidp->item == 3)) ||
                  (pmidp->cluster == 2 && pmidp->item == 4)) {
             sts = PM_ERR_PERMISSION;
             break;
         }
         else {
             sts = PM_ERR_PMID;
             break;
         }
     }
     return sts;
 }

The structure **pmdaExt** *pmda* argument is not used by the **simple_store** function above.

.. note::
   When using storable metrics, it is important to consider the implications. It is possible **pmlogger** is actively sampling the metric being modified, for 
   example, which may cause unexpected results to be persisted in an archive. Consider also the use of client credentials, available via the **attribute** callback 
   of the **pmdaInterface** structure, to appropriately limit access to any modifications that might be made via your storable metrics.
   
Return Codes for pmdaFetch Callbacks
--------------------------------------

In **PMDA_INTERFACE_1** and **PMDA_INTERFACE_2**, the return codes for the **pmdaFetch** callback function are defined:

+----------------------------+--------------------------------------------------------------------------------------------+
| Value                      | Meaning                                                                                    |
+============================+============================================================================================+
| < 0                        | Error code (for example, **PM_ERR_PMID, PM_ERR_INST** or **PM_ERR_AGAIN**)                 |
+----------------------------+--------------------------------------------------------------------------------------------+
| 0                          | Success                                                                                    |
+----------------------------+--------------------------------------------------------------------------------------------+

In **PMDA_INTERFACE_3** and all later versions, the return codes for the **pmdaFetch** callback function are defined:

+----------------------------+--------------------------------------------------------------------------------------------+
| Value                      | Meaning                                                                                    |
+============================+============================================================================================+
| < 0                        | Error code (for example, **PM_ERR_PMID, PM_ERR_INST**)                                     |
+----------------------------+--------------------------------------------------------------------------------------------+
| 0                          | Metric value not currently available                                                       |
+----------------------------+--------------------------------------------------------------------------------------------+
| > 0                        | Success                                                                                    |
+----------------------------+--------------------------------------------------------------------------------------------+

PMDA Structures
=================

PMDA structures used with the **pcp_pmda** library are defined in **<pcp/pmda.h>**. `Example 2.30. pmdaInterface Structure Header`_ and 
`Example 2.32. pmdaExt Structure`_ describe the **pmdaInterface** and **pmdaExt** structures.

.. _Example 2.30. pmdaInterface Structure Header:

**Example 2.30. pmdaInterface Structure Header**

The callbacks must be specified in a **pmdaInterface** structure:

.. sourcecode:: none

 typedef struct {
     int domain;     /* set/return performance metrics domain id here */
     struct {
         unsigned int pmda_interface : 8;  /* PMDA DSO version */
         unsigned int pmapi_version : 8;   /* PMAPI version */
         unsigned int flags : 16;          /* optional feature flags */
     } comm;             /* set/return communication and version info */
     int status;         /* return initialization status here */
     union {
         ...

This structure is passed by PMCD to a DSO PMDA as an argument to the initialization function. This structure supports multiple (binary-compatible) versions--the 
second and subsequent versions have support for the **pmdaExt** structure. Protocol version one is for backwards compatibility only, and should not be used in 
any new PMDA.

To date there have been six revisions of the interface structure:

1. Version two added the **pmdaExt** structure, as mentioned above.

2. Version three changed the fetch callback return code semantics, as mentioned in Section 2.5.1.4, “`Return Codes for pmdaFetch Callbacks`_”.

3. Version four added support for dynamic metric names, where the PMDA is able to create and remove metric names on-the-fly in response to changes in the 
   performance domain (**pmdaPMID, pmdaName, pmdaChildren** interfaces)

4. Version five added support for per-client contexts, where the PMDA is able to track arrival and disconnection of PMAPI client tools via PMCD (**pmdaGetContext** 
   helper routine). At the same time, support for **PM_TYPE_EVENT** metrics was implemented, which relies on the per-client context concepts (**pmdaEvent\*** 
   helper routines).

5. Version six added support for authenticated client contexts, where the PMDA is informed of user credentials and other PMCD attributes of the connection between 
   individual PMAPI clients and PMCD (**pmdaAttribute** interface)

6. Version seven added support for metadata labels, where the PMDA is able to associate name:value pairs in a hierarchy such that additional metadata, above and 
   beyond the metric descriptors, is associated with metrics and instances (**pmdaLabel** interface)
   
**Example 2.31. pmdaInterface Structure, Latest Version**

.. sourcecode:: none
 
    ...
     union {
         ...
         /*
          * PMDA_INTERFACE7
          */ 
         struct {
             pmdaExt *ext;
             int     (*profile)(pmdaInProfile *, pmdaExt *);
             int     (*fetch)(int, pmID *, pmResult **, pmdaExt *);
             int     (*desc)(pmID, pmDesc *, pmdaExt *);
             int     (*instance)(pmInDom, int, char *, pmdaInResult **, pmdaExt *);
             int     (*text)(int, int, char **, pmdaExt *);
             int     (*store)(pmResult *, pmdaExt *);
             int     (*pmid)(const char *, pmID *, pmdaExt *);
             int     (*name)(pmID, char ***, pmdaExt *);
             int     (*children)(const char *, int, char ***, int **, pmdaExt *);
             int     (*attribute)(int, int, const char *, int, pmdaExt *);
             int     (*label)(int, int, pmLabelSet **, pmdaExt *);
         } seven;
     } version;
 } pmdaInterface;
 
.. note::
   Each new interface version is always defined as a superset of those that preceded it, only adds fields at the end of the new structure in the union, and is 
   always binary backwards-compatible. **And thus it shall remain**. For brevity, we have shown only the latest interface version (seven) above, but all prior 
   versions still exist, build, and function. In other words, PMDAs built against earlier versions of this header structure (and PMDA library) function correctly 
   with the latest version of the PMDA library.
   
.. _Example 2.32. pmdaExt Structure:

**Example 2.32. pmdaExt Structure**

Additional PMDA information must be specified in a **pmdaExt** structure:

.. sourcecode:: none

 typedef struct {
     unsigned int e_flags;       /* PMDA_EXT_FLAG_* bit field */
     void        *e_ext;         /* used internally within libpcp_pmda */
     char        *e_sockname;    /* socket name to pmcd */
     char        *e_name;        /* name of this pmda */
     char        *e_logfile;     /* path to log file */
     char        *e_helptext;    /* path to help text */
     int         e_status;       /* =0 is OK */
     int         e_infd;         /* input file descriptor from pmcd */
     int         e_outfd;        /* output file descriptor to pmcd */
     int         e_port;         /* port to pmcd */
     int         e_singular;     /* =0 for singular values */
     int         e_ordinal;      /* >=0 for non-singular values */
     int         e_direct;       /* =1 if pmid map to meta table */
     int         e_domain;       /* metrics domain */
     int         e_nmetrics;     /* number of metrics */
     int         e_nindoms;      /* number of instance domains */
     int         e_help;         /* help text comes via this handle */
     pmProfile   *e_prof;        /* last received profile */
     pmdaIoType  e_io;           /* connection type to pmcd */
     pmdaIndom   *e_indoms;      /* instance domain table */
     pmdaIndom   *e_idp;         /* instance domain expansion */
     pmdaMetric  *e_metrics;     /* metric description table */
     pmdaResultCallBack e_resultCallBack; /* to clean up pmResult after fetch */
     pmdaFetchCallBack  e_fetchCallBack;  /* to assign metric values in fetch */
     pmdaCheckCallBack  e_checkCallBack;  /* callback on receipt of a PDU */
     pmdaDoneCallBack   e_doneCallBack;   /* callback after PDU is processed */
     /* added for PMDA_INTERFACE_5 */
     int         e_context;      /* client context id from pmcd */
     pmdaEndContextCallBack e_endCallBack;  /* callback after client context closed */
     /* added for PMDA_INTERFACE_7 */
     pmdaLabelCallBack  e_labelCallBack;  /* callback to lookup metric instance labels */
 } pmdaExt;

The **pmdaExt** structure contains filenames, pointers to tables, and some variables shared by several functions in the **pcp_pmda** library. All fields of the 
**pmdaInterface** and **pmdaExt** structures can be correctly set by PMDA initialization functions; see the **pmdaDaemon(3)**, **pmdaDSO(3)**, **pmdaGetOptions(3)**, 
**pmdaInit(3)**, and **pmdaConnect(3)** man pages for a full description of how various fields in these structures may be set or used by **pcp_pmda** library functions.

Initializing a PMDA
*********************

Several functions are provided to simplify the initialization of a PMDA. These functions, if used, must be called in a strict order so that the PMDA can operate correctly.

Overview
=========

The initialization process for a PMDA involves opening help text files, assigning callback function pointers, adjusting the metric and instance identifiers to the 
correct domains, and much more. The initialization of a daemon PMDA also differs significantly from a DSO PMDA, since the **pmdaInterface** structure is initialized 
by **main** or the PMCD process, respectively.

Common Initialization
======================
As described in Section 2.2.2, “`DSO PMDA`_”, an initialization function is provided by a DSO PMDA and called by PMCD. Using the standard PMDA wrappers, the same 
function can also be used as part of the daemon PMDA initialization. This PMDA initialization function performs the following tasks:

* Assigning callback functions to the function pointer interface of **pmdaInterface**

* Assigning pointers to the metric and instance tables from **pmdaExt**

* Opening the help text files

* Assigning the domain number to the instance domains

* Correlating metrics with their instance domains

If the PMDA uses the common data structures defined for the **pcp_pmda** library, most of these requirements can be handled by the default **pmdaInit** 
function; see the **pmdaInit(3)** man page.

Because the initialization function is the only initialization opportunity for a DSO PMDA, the common initialization function should also perform any DSO-specific 
functions that are required. A default implementation of this functionality is provided by the **pmdaDSO** function; see the **pmdaDSO(3)** man page.

Trivial PMDA
--------------

`Example 2.33. Initialization in the Trivial PMDA`_ shows the trivial PMDA, which has no instances (that is, all metrics have singular values) and a single 
callback. This callback is for the **pmdaFetch** function called **trivial_fetchCallBack**; see the **pmdaFetch(3)** man page:

.. _Example 2.33. Initialization in the Trivial PMDA:

**Example 2.33. Initialization in the Trivial PMDA**

.. sourcecode:: none

 static char     *username;
 static int      isDSO = 1;              /* ==0 if I am a daemon */ 

 void trivial_init(pmdaInterface *dp)
 {
     if (isDSO)
         pmdaDSO(dp, PMDA_INTERFACE_2, “trivial DSO”,
                 “${PCP_PMDAS_DIR}/trivial/help”);
     else
         pmSetProcessIdentity(username);

     if (dp->status != 0)
         return;

     pmdaSetFetchCallBack(dp, trivial_fetchCallBack);
     pmdaInit(dp, NULL, 0,
              metrictab, sizeof(metrictab)/sizeof(metrictab[0]));
 }

The trivial PMDA can execute as either a DSO or daemon PMDA. A default installation installs it as a daemon, however, and the **main** routine clears *isDSO* and 
sets *username* accordingly.

The **trivial_init** routine provides the opportunity to do any extra DSO or daemon setup before calling the library **pmdaInit**. In the example, the help text is 
setup for DSO mode and the daemon is switched to run as an unprivileged user (default is **root**, but it is generally good form for PMDAs to run with the least 
privileges possible). If **dp->status** is non-zero after the **pmdaDSO** call, the PMDA will be removed by PMCD and cannot safely continue to use the **pmdaInterface** 
structure.

Simple PMDA
------------

In `Example 2.34. Initialization in the Simple PMDA`_, the simple PMDA uses its own callbacks to handle **PDU_FETCH** and **PDU_RESULT** request PDUs (for 
**pmFetch** and **pmStore** operations respectively), as well as providing **pmdaFetch** with the callback **simple_fetchCallBack**.

.. _Example 2.34. Initialization in the Simple PMDA:

**Example 2.34. Initialization in the Simple PMDA**

.. sourcecode:: none

 static int      isDSO = 1;              /* =0 I am a daemon */
 static char     *username; 
 
 void simple_init(pmdaInterface *dp)
 {
     if (isDSO)
         pmdaDSO(dp, PMDA_INTERFACE_7, “simple DSO”,
                 “${PCP_PMDAS_DIR}/simple/help”);
     else
         pmSetProcessIdentity(username);
 
     if (dp->status != 0)
         return;
 
     dp->version.any.fetch = simple_fetch;
     dp->version.any.store = simple_store;
     dp->version.any.instance = simple_instance;
     dp->version.seven.label = simple_label;
     pmdaSetFetchCallBack(dp, simple_fetchCallBack);
     pmdaSetLabelCallBack(dp, simple_labelCallBack);
     pmdaInit(dp, indomtab, sizeof(indomtab)/sizeof(indomtab[0]),
              metrictab, sizeof(metrictab)/sizeof(metrictab[0]));
 }

Once again, the simple PMDA may be installed either as a daemon PMDA or a DSO PMDA. The static variable *isDSO* indicates whether the PMDA is running as a DSO or 
as a daemon. A daemon PMDA always changes the value of this variable to 0 in *main*, for PMDAs that can operate in both modes.

Remember also, as described earlier, **simple_fetch** is dealing with a single request for (possibly many) values for metrics from the PMDA, and **simple_fetchCallBack** 
is its little helper, dealing with just one metric and one instance (optionally, if the metric happens to have an instance domain) within that larger request.

Daemon Initialization
======================

In addition to the initialization function that can be shared by a DSO and a daemon PMDA, a daemon PMDA must also meet the following requirements:

* Create the **pmdaInterface** structure that is passed to the initialization function

* Parse any command-line arguments

* Open a log file (a DSO PMDA uses PMCD's log file)

* Set up the IPC connection between the PMDA and the PMCD process

* Handle incoming PDUs

All these requirements can be handled by default initialization functions in the **pcp_pmda** library; see the **pmdaDaemon(3), pmdaGetOptions(3), pmdaOpenLog(3),** 
**pmdaConnect(3)**, and **pmdaMain(3)** man pages.

.. note::
   Optionally, a daemon PMDA may wish to reduce or change its privilege level, as seen in `Example 2.33. Initialization in the Trivial PMDA`_ and 
   `Example 2.34. Initialization in the Simple PMDA`_. Some performance domains **require** the extraction process to run as a specific user in order to access 
   the instrumentation. Many domains require the default **root** level of access for a daemon PMDA.

The simple PMDA specifies the command-line arguments it accepts using **pmdaGetOptions**, as shown in `Example 2.35. main in the Simple PMDA`_. 
For additional information, see the **pmdaGetOptions(3)** man page.

.. _Example 2.35. main in the Simple PMDA:

**Example 2.35. main in the Simple PMDA**

.. sourcecode:: none
 
 static pmLongOptions longopts[] = {
     PMDA_OPTIONS_HEADER(“Options”),
     PMOPT_DEBUG,
     PMDAOPT_DOMAIN,
     PMDAOPT_LOGFILE,
     PMDAOPT_USERNAME,
     PMOPT_HELP,
     PMDA_OPTIONS_TEXT(“\nExactly one of the following options may appear:”),
     PMDAOPT_INET,
     PMDAOPT_PIPE,
     PMDAOPT_UNIX,
     PMDAOPT_IPV6,
     PMDA_OPTIONS_END
 };
 static pmdaOptions opts = {
     .short_options = “D:d:i:l:pu:U:6:?”,
     .long_options = longopts,
 };
 
 int
 main(int argc, char **argv)
 {
     pmdaInterface       dispatch;
 
     isDSO = 0;
     pmSetProgname(argv[0]);
     pmGetUsername(&username);
     pmdaDaemon(&dispatch, PMDA_INTERFACE_7, pmGetProgname(), SIMPLE,
                “simple.log”, “${PCP_PMDAS_DIR}/simple/help”);

     pmdaGetOptions(argc, argv, &opts, &dispatch);
     if (opts.errors) {
         pmdaUsageMessage(&opts);
         exit(1);
     }
     if (opts.username)
         username = opts.username;
 
     pmdaOpenLog(&dispatch);
     simple_init(&dispatch);
     simple_timenow_check();
     pmdaConnect(&dispatch);
     pmdaMain(&dispatch);
 
     exit(0);
 }

The conditions under which **pmdaMain** will return are either unexpected error conditions (often from failed initialisation, which would already have been logged), 
or when PMCD closes the connection to the PMDA. In all cases the correct action to take is simply to exit cleanly, possibly after any final cleanup the PMDA may 
need to perform.

Testing and Debugging a PMDA
*****************************

Ensuring the correct operation of a PMDA can be difficult, because the responsibility of providing metrics to the requesting PMCD process and simultaneously 
retrieving values from the target domain requires nearly real-time communication with two modules beyond the PMDA's control. Some tools are available to assist in 
this important task.

Overview
=========

Thoroughly testing a PMDA with PMCD is difficult, although testing a daemon PMDA is marginally simpler than testing a DSO PMDA. If a DSO PMDA exits, PMCD also 
exits because they share a single address space and control thread.

The difficulty in using PMCD to test a daemon PMDA results from PMCD requiring timely replies from the PMDA in response to request PDUs. Although a timeout period 
can be set in ``${PCP_PMCDOPTIONS_PATH}``, attaching a debugger (such as **gdb**) to the PMDA process might cause an already running PMCD to close its connection 
with the PMDA. If timeouts are disabled, PMCD could wait forever to connect with the PMDA.

If you suspect a PMDA has been terminated due to a timeout failure, check the PMCD log file, usually ``${PCP_LOG_DIR}/pmcd/pmcd.log``.

A more robust way of testing a PMDA is to use the **dbpmda** tool, which is similar to PMCD except that **dbpmda** provides complete control over the PDUs that are 
sent to the PMDA, and there are no time limits--it is essentially an interactive debugger for exercising a PMDA. See the **dbpmda(3)** man page for details.

In addition, careful use of PCP debugging flags can produce useful information concerning a PMDA's behavior; see the **PMAPI(3)** and **pmdbg(1)** man pages for a 
discussion of the PCP debugging and tracing framework.

Debugging Information
======================

You can activate debugging options in PMCD and most other PCP tools with the **-D** command-line option. Supported options can be listed with the **pmdbg** 
command; see the **pmdbg(1)** man page. Setting the debug options for PMCD in ``${PCP_PMCDOPTIONS_PATH}`` might generate too much information to be useful, 
especially if there are other clients and PMDAs connected to the PMCD process.

The PMCD debugging options can also be changed dynamically by storing a new value into the metric **pmcd.control.debug**:

.. sourcecode:: none

 # pmstore pmcd.control.debug 5

Most of the **pcp_pmda** library functions log additional information if the **libpmda** option is set within the PMDA; see the **PMDA(3)** man page. The command-line 
argument **-D** is trapped by **pmdaGetOptions** to set the global debugging control options. Adding tests within the PMDA for the **appl0, appl1** and **appl2** 
trace flags permits different levels of information to be logged to the PMDA's log file.

All diagnostic, debugging, and tracing output from a PMDA should be written to the standard error stream.

Adding this segment of code to the **simple_store** metric causes a timestamped log message to be sent to the current log file whenever **pmstore** attempts to 
change **simple.numfetch** and the PCP debugging options have the **appl0** option set as shown in `Example 2.36. simple.numfetch in the Simple PMDA`_:

.. _Example 2.36. simple.numfetch in the Simple PMDA:

**Example 2.36. simple.numfetch in the Simple PMDA**

.. sourcecode:: none

    case 0: /* simple.numfetch */ 
         x
         val = vsp->vlist[0].value.lval; 
         if (val < 0) { 
             sts = PM_ERR_SIGN; 
             val = 0; 
         } 
         if (pmDebugOptions.appl0__) { 
             pmNotifyErr(LOG_DEBUG,
                   "simple: %d stored into numfetch", val); 
         } 
         numfetch = val; 
         break;

For a description of **pmstore**, see the **pmstore(1)** man page.

dbpmda Debug Utility
=====================

The **dbpmda** utility provides a simple interface to the PDU communication protocol. It allows daemon and DSO PMDAs to be tested with most request types, while 
the PMDA process may be monitored with a debugger, tracing utilities, and other diagnostic tools. The **dbpmda(1)** man page contains a sample session with the 
**simple** PMDA.

Integration of a PMDA
***********************

Several steps are required to install (or remove) a PMDA from a production PMCD environment without affecting the operation of other PMDAs or related visualization 
and logging tools.

The PMDA typically would have its own directory below ``${PCP_PMDAS_DIR}`` into which several files would be installed. In the description in Section 2.8.1, 
“`Installing a PMDA`_”, the PMDA of interest is assumed to be known by the name **newbie**, hence the PMDA directory would be ``${PCP_PMDAS_DIR}/newbie``.

.. note::
   Any installation or removal of a PMDA involves updating files and directories that are typically well protected. Hence the procedures described in this 
   section must be executed as the superuser.
   
Installing a PMDA
===================

A PMDA is fully installed when these tasks are completed:

* Help text has been installed in a place where the PMDA can find it, usually in the PMDA directory ``${PCP_PMDAS_DIR}/newbie``.

* The name space has been updated in the ``${PCP_VAR_DIR}/pmns`` directory.

* The PMDA binary has been installed, usually in the directory ``${PCP_PMDAS_DIR}/newbie``.

* The ``${PCP_PMCDCONF_PATH}`` file has been updated.

* The PMCD process has been restarted or notified (with a **SIGHUP** signal) that the new PMDA exists.

The **Makefile** should include an **install** target to compile and link the PMDA (as a DSO, or a daemon or both) in the PMDA directory. The **clobber** target 
should remove any files created as a by-product of the **install** target.

You may wish to use ``${PCP_PMDAS_DIR}/simple/Makefile`` as a template for constructing a new PMDA **Makefile**; changing the assignment of **IAM** from **simple** 
to **newbie** would account for most of the required changes.

The **Install** script should make use of the generic procedures defined in the script ``${PCP_SHARE_DIR}/lib/pmdaproc.sh``, and may be as straightforward as the 
one used for the trivial PMDA, shown in `Example 2.37. Install Script for the Trivial PMDA`_:

.. _Example 2.37. Install Script for the Trivial PMDA:

**Example 2.37. Install Script for the Trivial PMDA**

.. sourcecode:: none

 . ${PCP_DIR}/etc/pcp.env
 . ${PCP_SHARE_DIR}/lib/pmdaproc.sh
 
 iam=trivial
 pmdaSetup
 pmdainstall
 exit

The variables, shown in `Table 2.1. Variables to Control Behavior of Generic pmdaproc.sh Procedures`_, may be assigned values to modify the behavior of the 
**pmdaSetup** and **pmdainstall** procedures from ``${PCP_SHARE_DIR}/lib/pmdaproc.sh``.

.. _Table 2.1. Variables to Control Behavior of Generic pmdaproc.sh Procedures:

**Table 2.1. Variables to Control Behavior of Generic pmdaproc.sh Procedures**

.. list-table::
   :widths: 20 60 20

   * - **Shell Variable**           
     - **Use**
     - **Default**                                     
   * - **$iam**
     - Name of the PMDA; assignment to this variable is mandatory. 
       Example: **iam=newbie**
     -
   * - **$dso_opt**
     - Can this PMDA be installed as a DSO? 
     - **false**
   * - **$daemon_opt**
     - Can this PMDA be installed as a daemon?	                                                            
     - **true**                 
   * - **$perl_opt**                
     - Is this PMDA a perl script?	                                                                        
     - **false**
   * - **$python_opt**              
     - Is this PMDA a python script?	                                                                        
     - **false**               
   * - **$pipe_opt**               
     - If installed as a daemon PMDA, is the default IPC via pipes?	                                        
     - **true**                 
   * - **$socket_opt**       	   
     - If installed as a daemon PMDA, is the default IPC via an Internet socket?	                            
     - **false**                
   * - **$socket_inet_def**	       
     - If installed as a daemon PMDA, and the IPC method uses an Internet socket, the default port number.
     - 
   * - **$ipc_prot**	               
     - IPC style for PDU exchanges involving a daemon PMDA; **binary** or **text**.                           
     - **binary**          
   * - **$check_delay**	           
     - Delay in seconds between installing PMDA and checking if metrics are available.	                    
     - **3**                    
   * - **$args**	                   
     - Additional command-line arguments passed to a daemon PMDA.	 
     - 
   * - **$pmns_source**	           
     - The name of the PMNS file (by default relative to the PMDA directory).	                                
     - **pmns**                 
   * - **$pmns_name**	           
     - First-level name for this PMDA's metrics in the PMNS.	                                                
     - **$iam**                 
   * - **$help_source**	           
     - The name of the help file (by default relative to the PMDA directory).	                                
     - **help**
   * - **$pmda_name**	           
     - The name of the executable for a daemon PMDA.	                                                     
     - **pmda$iam**       
   * - **$dso_name**                
     - The name of the shared library for a DSO PMDA.	                                                        
     - **pmda$iam.$dso_suffix** 
   * - **$dso_entry**               
     - The name of the initialization function for a DSO PMDA.	                                            
     - **${iam}_init**          
   * - **$domain**                  
     - The numerical PMDA domain number (from **domain.h**).	        
     - 
   * - **$SYMDOM**       	       
     - The symbolic name of the PMDA domain number (from **domain.h**).  
     - 
   * - **$status**           	   
     - Exit status for the shell script	                                                                    
     - **0**          

In addition, the variable **do_check** will be set to reflect the intention to check the availability of the metrics once the PMDA is installed. By default this variable is **true** 
however the command-line option **-Q** to **Install** may be used to set the variable to **false**.

Obviously, for anything but the most trivial PMDA, after calling the **pmdaSetup** procedure, the **Install** script should also prompt for any PMDA-specific parameters, which are 
typically accumulated in the *args* variable and used by the **pmdainstall** procedure.

The detailed operation of the **pmdainstall** procedure involves the following tasks:

* Using default assignments, and interaction where ambiguity exists, determine the PMDA type (DSO or daemon) and the IPC parameters, if any.

* Copy the ``$pmns_source`` file, replacing symbolic references to **SYMDOM** by the desired numeric domain number from **domain**.

* Merge the PMDA's name space into the PCP name space at the non-leaf node identified by **$pmns_name**.

* If any **pmchart** views can be found (files with names ending in “.pmchart”), copy these to the standard directory (``${PCP_VAR_DIR}/config/pmchart``) with the ".pmchart" suffix removed.

* Create new help files from ``$help_source`` after replacing symbolic references to **SYMDOM** by the desired numeric domain number from **domain**.

* Terminate the old daemon PMDA, if any.

* Use the **Makefile** to build the appropriate executables.

* Add the PMDA specification to PMCD's configuration file (``${PCP_PMCDCONF_PATH}``).

* Notify PMCD. To minimize the impact on the services PMCD provides, sending a **SIGHUP** to PMCD forces it to reread the configuration file and start, restart, or remove any PMDAs that 
  have changed since the file was last read. However, if the newly installed PMDA must run using a different privilege level to PMCD then PMCD must be restarted. This is because PMCD runs 
  unprivileged after initially starting the PMDAs.

* Check that the metrics from the new PMDA are available.

There are some PMDA changes that may trick PMCD into thinking nothing has changed, and not restarting the PMDA. Most notable are changes to the PMDA executable. In these cases, you may 
need to explicitly remove the PMDA as described in Section 2.8.2, “`Removing a PMDA`_”, or more drastically, restart PMCD as follows:

.. sourcecode:: none

 # ${PCP_RC_DIR}/pmcd start

The files ``${PCP_PMDAS_DIR}/*/Install`` provide a wealth of examples that may be used to construct a new PMDA **Install** script.

Removing a PMDA
================

The simplest way to stop a PMDA from running, apart from killing the process, is to remove the entry from ``${PCP_PMCDCONF_PATH}`` and signal PMCD (with **SIGHUP**) to reread its 
configuration file. To completely remove a PMDA requires the reverse process of the installation, including an update of the Performance Metrics Name Space (PMNS).

This typically involves a **Remove** script in the PMDA directory that uses the same common procedures as the **Install** script described Section 2.8.1, “`Installing a PMDA`_”.

The ``${PCP_PMDAS_DIR}/*/Remove`` files provide a wealth of examples that may be used to construct a new PMDA **Remove** script.

Configuring PCP Tools
======================

Most PCP tools have their own configuration file format for specifying which metrics to view or to log. By using canned configuration files that monitor key metrics of the new PMDA, 
users can quickly see the performance of the target system, as characterized by key metrics in the new PMDA.

Any configuration files that are created should be kept with the PMDA and installed into the appropriate directories when the PMDA is installed.

As with all PCP customization, some of the most valuable tools can be created by defining views, scenes, and control-panel layouts that combine related performance metrics from multiple 
PMDAs or multiple hosts.

Metrics suitable for on-going logging can be specified in templated **pmlogger** configuration files for **pmlogconf** to automatically add to the **pmlogger_daily** recorded set; see the 
**pmlogger(1), pmlogconf(1)** and **pmlogger_daily(1)** man pages.

Parameterized alarm configurations can be created using the **pmieconf** facilities; see the **pmieconf(1)** and **pmie(1)** man pages.
