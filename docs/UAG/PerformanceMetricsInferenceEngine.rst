.. _PerformanceMetricsInferenceEngine:

Performance Metrics Inference Engine
#####################################

.. contents::

The Performance Metrics Inference Engine (**pmie**) is a tool that provides automated monitoring of, and reasoning about, system performance within the 
Performance Co-Pilot (PCP) framework.

The major sections in this chapter are as follows:

Section 5.1, “`Introduction to pmie`_”, provides an introduction to the concepts and design of **pmie**.

Section 5.2, “`Basic pmie Usage`_”, describes the basic syntax and usage of **pmie**.

Section 5.3, “`Specification Language for pmie`_”, discusses the complete **pmie** rule specification language.

Section 5.4, “`pmie Examples`_”, provides an example, covering several common performance scenarios.

Section 5.5, “`Developing and Debugging pmie Rules`_”, presents some tips and techniques for **pmie** rule development.

Section 5.6, “`Caveats and Notes on pmie`_”, presents some important information on using **pmie**.

Section 5.7, “`Creating pmie Rules with pmieconf`_”, describes how to use the **pmieconf** command to generate **pmie** rules.

Section 5.8, “`Management of pmie Processes`_”, provides support for running **pmie** as a daemon.

Introduction to pmie
*********************

Automated reasoning within Performance Co-Pilot (PCP) is provided by the Performance Metrics Inference Engine, (**pmie**), which is an applied artificial 
intelligence application.

The **pmie** tool accepts expressions describing adverse performance scenarios, and periodically evaluates these against streams of performance metric 
values from one or more sources. When an expression is found to be true, **pmie** is able to execute arbitrary actions to alert or notify the system 
administrator of the occurrence of an adverse performance scenario. These facilities are very general, and are designed to accommodate the automated 
execution of a mixture of generic and site-specific performance monitoring and control functions.

The stream of performance metrics to be evaluated may be from one or more hosts, or from one or more PCP archives. In the latter case, **pmie** may be 
used to retrospectively identify adverse performance conditions.

Using **pmie**, you can filter, interpret, and reason about the large volume of performance data made available from PCP collector systems or PCP archives.

Typical **pmie** uses include the following:

* Automated real-time monitoring of a host, a set of hosts, or client-server pairs of hosts to raise operational alarms when poor performance is detected in a production environment

* Nightly processing of archives to detect and report performance regressions, or quantify quality of service for service level agreements or management reports, or produce advance warning of pending performance problems

* Strategic performance management, for example, detection of slightly abnormal to chronic system behavior, trend analysis, and capacity planning

The **pmie** expressions are described in a language with expressive power and operational flexibility. It includes the following operators and functions:

* Generalized predicate-action pairs, where a predicate is a logical expression over the available performance metrics, and the action is arbitrary. Predefined actions include the following:

  *  Launch a visible alarm with **pmconfirm**; see the **pmconfirm(1)** man page.
  *  Post an entry to the system log file; see the **syslog(3)** man page.
  *  Post an entry to the PCP noticeboard file ``${PCP_LOG_DIR}/NOTICES``; see the **pmpost(1)** man page.
  *  Execute a shell command or script, for example, to send e-mail, initiate a pager call, warn the help desk, and so on.
  *  Echo a message on standard output; useful for scripts that generate reports from retrospective processing of PCP archives.

* Arithmetic and logical expressions in a C-like syntax.

* Expression groups may have an independent evaluation frequency, to support both short-term and long-term monitoring.

* Canonical scale and rate conversion of performance metric values to provide sensible expression evaluation.

* Aggregation functions of **sum, avg, min**, and **max**, that may be applied to collections of performance metrics values clustered over multiple hosts, or multiple instances, or multiple consecutive samples in time.

* Universal and existential quantification, to handle expressions of the form “for every....” and “at least one...”.

* Percentile aggregation to handle statistical outliers, such as “for at least 80% of the last 20 samples, ...”.

* Macro processing to expedite repeated use of common subexpressions or specification components.

* Transparent operation against either live-feeds of performance metric values from PMCD on one or more hosts, or against PCP archives of previously accumulated performance metric values.

The power of **pmie** may be harnessed to automate the most common of the deterministic system management functions that are responses to changes in system performance. For example, disable a batch stream if 
the DBMS transaction commit response time at the ninetieth percentile goes over two seconds, or stop accepting uploads and send e-mail to the *sysadmin* alias if free space in a storage system falls below five 
percent.

Moreover, the power of **pmie** can be directed towards the exceptional and sporadic performance problems. For example, if a network packet storm is expected, enable IP header tracing for ten seconds, and send 
e-mail to advise that data has been collected and is awaiting analysis. Or, if production batch throughput falls below 50 jobs per minute, activate a pager to the systems administrator on duty.

Obviously, **pmie** customization is required to produce meaningful filtering and actions in each production environment. The **pmieconf** tool provides a convenient customization method, allowing the user to 
generate parameterized **pmie** rules for some of the more common performance scenarios.

Basic pmie Usage
*****************

This section presents and explains some basic examples of **pmie** usage. The **pmie** tool accepts the common PCP command line arguments, as described in Chapter 3, :ref:`CommonConventionsandArguments`. In addition, **pmie** accepts the following command line arguments:

+-----------+----------------------------------------------------------------------------------------------------+
| **-d**    | Enables interactive debug mode.                                                                    |
+-----------+----------------------------------------------------------------------------------------------------+
| **-v**    | Verbose mode: expression values are displayed.                                                     |
+-----------+----------------------------------------------------------------------------------------------------+
| **-V**    | Verbose mode: annotated expression values are displayed.                                           |
+-----------+----------------------------------------------------------------------------------------------------+
| **-W**    | When-verbose mode: when a condition is true, the satisfying expression bindings are displayed.     |
+-----------+----------------------------------------------------------------------------------------------------+

One of the most basic invocations of this tool is this form::

 pmie filename

In this form, the expressions to be evaluated are read from *filename*. In the absence of a given *filename*, 
expressions are read from standard input, which may be your system keyboard.

pmie use of PCP services
=============================

Before you use **pmie**, it is strongly recommended that you familiarize yourself with the concepts from the Section 1.2, “:ref:`Conceptual Foundations`”. The discussion in this section serves as a very brief review of these concepts.

PCP makes available thousands of performance metrics that you can use when formulating expressions for **pmie** to evaluate. If you want to find out which metrics are currently available on your system, use this command::

 pminfo

Use the **pmie** command line arguments to find out more about a particular metric. In `Example 5.1. pmie with the -f Option`_, to fetch new metric values from host **dove**, you use the **-f** flag:

.. _Example 5.1. pmie with the -f Option:

**Example 5.1. pmie with the -f Option**

.. sourcecode:: none
  
 pminfo -f -h dove disk.dev.total

This produces the following response:

.. sourcecode:: none

 disk.dev.total
     inst [0 or "xscsi/pci00.01.0/target81/lun0/disc"] value 131233
     inst [4 or "xscsi/pci00.01.0/target82/lun0/disc"] value 4
     inst [8 or "xscsi/pci00.01.0/target83/lun0/disc"] value 4
     inst [12 or "xscsi/pci00.01.0/target84/lun0/disc"] value 4
     inst [16 or "xscsi/pci00.01.0/target85/lun0/disc"] value 4
     inst [18 or "xscsi/pci00.01.0/target86/lun0/disc"] value 4

This reveals that on the host **dove**, the metric **disk.dev.total** has six instances, one for each disk on the system.

Use the following command to request help text (specified with the **-T** flag) to provide more information about performance metrics:

.. sourcecode:: none

 pminfo -T network.interface.in.packets

The metadata associated with a performance metric is used by **pmie** to determine how the value should be interpreted. You can examine the descriptor that encodes 
the metadata by using the **-d** flag for **pminfo**, as shown in `Example 5.2. pmie with the -d and -h Options`_ :

.. _Example 5.2. pmie with the -d and -h Options:

**Example 5.2. pmie with the -d and -h Options**

.. sourcecode:: none

 pminfo -d -h somehost mem.util.cached kernel.percpu.cpu.user

In response, you see output similar to this:

.. sourcecode:: none

 mem.util.cached
     Data Type: 64-bit unsigned int  InDom: PM_INDOM_NULL 0xffffffff
     Semantics: instant  Units: Kbyte

 kernel.percpu.cpu.user
     Data Type: 64-bit unsigned int  InDom: 60.0 0xf000000
     Semantics: counter  Units: millisec

.. note::
   A cumulative counter such as **kernel.percpu.cpu.user** is automatically converted by **pmie** into a rate (measured in events per second, or count/second), while 
   instantaneous values such as **mem.util.cached** are not subjected to rate conversion. Metrics with an instance domain (**InDom** in the **pminfo** output) of **PM_INDOM_NULL** 
   are singular and always produce one value per source. However, a metric like **kernel.percpu.cpu.user** has an instance domain, and may produce multiple values per 
   source (in this case, it is one value for each configured CPU).

⁠Simple pmie Usage
===================

`Example 5.3. pmie with the -v Option`_ directs the inference engine to evaluate and print values (specified with the **-v** flag) for a single performance metric (the 
simplest possible expression), in this case **disk.dev.total**, collected from the local PMCD:

.. _Example 5.3. pmie with the -v Option:

**Example 5.3. pmie with the -v Option**

::

 pmie -v
 iops = disk.dev.total;
 Ctrl+D
 iops:      ?      ?
 iops:   14.4      0
 iops:   25.9  0.112
 iops:   12.2      0
 iops:   12.3   64.1
 iops:  8.594  52.17
 iops:  2.001  71.64

On this system, there are two disk spindles, hence two values of the expression **iops** per sample. Notice that the values for the first sample are unknown 
(represented by the question marks [?] in the first line of output), because rates can be computed only when at least two samples are available. The subsequent 
samples are produced every ten seconds by default. The second sample reports that during the preceding ten seconds there was an average of 14.4 transfers per second 
on one disk and no transfers on the other disk.

Rates are computed using time stamps delivered by PMCD. Due to unavoidable inaccuracy in the actual sampling time (the sample interval is not exactly 10 seconds), 
you may see more decimal places in values than you expect. Notice, however, that these errors do not accumulate but cancel each other out over subsequent samples.

In `Example 5.3. pmie with the -v Option`_, the expression to be evaluated was entered using the keyboard, followed by the end-of-file character [**Ctrl+D**]. 
Usually, it is more convenient to enter expressions into a file (for example, **myrules**) and ask **pmie** to read the file. Use this command syntax::

 pmie -v myrules

Please refer to the **pmie(1)** man page for a complete description of **pmie** command line options.

⁠Complex pmie Examples
======================

This section illustrates more complex **pmie** expressions of the specification language. Section 5.3, “`Specification Language for pmie`_”, provides a complete 
description of the **pmie** specification language.

The following arithmetic expression computes the percentage of write operations over the total number of disk transfers.

::

 (disk.all.write / disk.all.total) * 100;

The **disk.all** metrics are singular, so this expression produces exactly one value per sample, independent of the number of disk devices.

.. note::

 If there is no disk activity, **disk.all.total** will be zero and **pmie** evaluates this expression to be not a number. When **-v** is used, any such values are displayed as question marks.

The following logical expression has the value **true** or **false** for each disk::

 disk.dev.total > 10 && 
 disk.dev.write > disk.dev.read;

The value is true if the number of writes exceeds the number of reads, and if there is significant disk activity (more than 10 transfers per second). 
`Example 5.4. Printed pmie Output`_ demonstrates a simple action:

.. _Example 5.4. Printed pmie Output:


**Example 5.4. Printed pmie Output**

.. sourcecode:: none

 some_inst disk.dev.total > 60
           -> print "[%i] high disk i/o";

This prints a message to the standard output whenever the total number of transfers for some disk (**some_inst**) exceeds 60 transfers per second. The **%i** (instance) 
in the message is replaced with the name(s) of the disk(s) that caused the logical expression to be **true**.

Using **pmie** to evaluate the above expressions every 3 seconds, you see output similar to `Example 5.5. Labelled pmie Output`_. Notice the introduction of labels for each **pmie** expression.

.. _Example 5.5. Labelled pmie Output:

**Example 5.5. Labelled pmie Output**

.. sourcecode:: none

 pmie -v -t 3sec
 pct_wrt = (disk.all.write / disk.all.total) * 100;
 busy_wrt = disk.dev.total > 10 &&
            disk.dev.write > disk.dev.read;
 busy = some_inst disk.dev.total > 60
            -> print "[%i] high disk i/o ";
 Ctrl+D
 pct_wrt:       ? 
 busy_wrt:      ?      ?
 busy:          ?
 
 pct_wrt:   18.43
 busy_wrt:  false  false
 busy:      false
 
 Mon Aug  5 14:56:08 2012: [disk2] high disk i/o
 pct_wrt:   10.83
 busy_wrt:  false  false
 busy:      true 
 
 pct_wrt:   19.85
 busy_wrt:   true  false
 busy:      false
 
 pct_wrt:       ?
 busy_wrt:  false  false
 busy:      false
 
 Mon Aug  5 14:56:17 2012: [disk1] high disk i/o [disk2] high disk i/o
 pct_wrt:   14.8
 busy_wrt:  false  false
 busy:   true

The first sample contains unknowns, since all expressions depend on computing rates. Also notice that the expression **pct_wrt** may have an undefined value whenever 
all disks are idle, as the denominator of the expression is zero. If one or more disks is busy, the expression **busy** is true, and the message from the **print** 
in the action part of the rule appears (before the **-v** values).

Specification Language for pmie
********************************

This section describes the complete syntax of the **pmie** specification language, as well as macro facilities and the issue of sampling and evaluation frequency. 
The reader with a preference for learning by example may choose to skip this section and go straight to the examples in Section 5.4, “`pmie Examples`_”.

Complex expressions are built up recursively from simple elements:

1. Performance metric values are obtained from PMCD for real-time sources, otherwise from PCP archives.
2. Metrics values may be combined using arithmetic operators to produce arithmetic expressions.
3. Arithmetic expressions may be compared using relational operators to produce logical expressions.
4. Logical expressions may be combined using Boolean operators, including powerful quantifiers.
5. Aggregation operators may be used to compute summary expressions, for either arithmetic or logical operands.
6. The final logical expression may be used to initiate a sequence of actions.

Basic pmie Syntax
==================

The **pmie** rule specification language supports a number of basic syntactic elements.

⁠Lexical Elements
-----------------

All **pmie** expressions are composed of the following lexical elements:

**Identifier**

Begins with an alphabetic character (either upper or lowercase), followed by zero or more letters, the numeric digits, and the special characters period (.) and 
underscore (_), as shown in the following example:

.. sourcecode:: none

 x, disk.dev.total and my_stuff

As a special case, an arbitrary sequence of letters enclosed by apostrophes (') is also interpreted as an *identifier*; for example:

.. sourcecode:: none

 'vms$slow_response'

**Keyword**

The aggregate operators, units, and predefined actions are represented by keywords; for example, **some_inst**, **print**, and **hour**.

**Numeric constant**

Any likely representation of a decimal integer or floating point number; for example, 124, 0.05, and -45.67

**String constant**

An arbitrary sequence of characters, enclosed by double quotation marks (**"x"**).

Within quotes of any sort, the backslash (\) may be used as an escape character as shown in the following example:

.. sourcecode:: none

 "A \"gentle\" reminder"


Comments
---------

Comments may be embedded anywhere in the source, in either of these forms:

+--------------+---------------------------------------------------------------------------+
| /* text \*/  | Comment, optionally spanning multiple lines, with no nesting of comments. |
+--------------+---------------------------------------------------------------------------+
| // text      | Comment from here to the end of the line.                                 |
+--------------+---------------------------------------------------------------------------+

⁠Macros
-------

When they are fully specified, expressions in **pmie** tend to be verbose and repetitive. The use of macros can reduce repetition and improve readability and 
modularity. Any statement of the following form associates the macro name **identifier** with the given string constant.

.. sourcecode:: none

 identifier = "string";

Any subsequent occurrence of the macro name **identifier** is replaced by the string most recently associated with a macro definition for **identifier**.

.. sourcecode:: none

 $identifier 

For example, start with the following macro definition:

.. sourcecode:: none

 disk = "disk.all";

You can then use the following syntax::

 pct_wrt = ($disk.write / $disk.total) * 100;

.. note::
   Macro expansion is performed before syntactic parsing; so macros may only be assigned constant string values.

Units
------

The inference engine converts all numeric values to canonical units (seconds for time, bytes for space, and events for count). To avoid surprises, you are encouraged to specify the units for numeric constants. If units are specified, they are checked for dimension compatibility against the metadata for the associated performance metrics.

The syntax for a **units** specification is a sequence of one or more of the following keywords separated by either a space or a slash (/), to denote per: **byte, KByte, MByte, GByte, TByte, nsec, nanosecond, usec, microsecond, msec, millisecond, sec, second, min, minute, hour, count, Kcount, Mcount, Gcount,** or **Tcount**. Plural forms are also accepted.

The following are examples of units usage::

 disk.dev.blktotal > 1 Mbyte / second; 
 mem.util.cached < 500 Kbyte;

.. note::
   If you do not specify the units for numeric constants, it is assumed that the constant is in the canonical units of seconds for time, bytes for space, and events for count, and the dimensionality of the constant is assumed to be correct. Thus, in the following expression, the **500** is interpreted as 500 bytes.

   ::

      mem.util.cached < 500
      
Setting Evaluation Frequency
=============================

The identifier name **delta** is reserved to denote the interval of time between consecutive evaluations of one or more expressions. Set **delta** as follows::

 delta = number [units];

If present, **units** must be one of the time units described in the preceding section. If absent, **units** are assumed to be **seconds**. For example, the following 
expression has the effect that any subsequent expressions (up to the next expression that assigns a value to **delta**) are scheduled for evaluation at a fixed frequency, once every five minutes.

.. sourcecode:: none

 delta = 5 min;

The default value for **delta** may be specified using the **-t** command line option; otherwise **delta** is initially set to be 10 seconds.

pmie Metric Expressions
=========================

The performance metrics namespace (PMNS) provides a means of naming performance metrics, for example, **disk.dev.read**. PCP allows an application to retrieve one or more values for a performance metric from a designated source (a collector host running PMCD, or a set of PCP archives). To specify a single value for some performance metric requires the metric name to be associated with all three of the following:

1. A particular host (or source of metrics values) 
2. A particular instance (for metrics with multiple values)
3. A sample time

The permissible values for hosts are the range of valid hostnames as provided by Internet naming conventions.

The names for instances are provided by the Performance Metrics Domain Agents (PMDA) for the instance domain associated with the chosen performance metric.

The sample time specification is defined as the set of natural numbers 0, 1, 2, and so on. A number refers to one of a sequence of sampling events, from the current sample 0 to its predecessor 1, whose predecessor was 2, and so on. 
This scheme is illustrated by the time line shown in `Figure 5.1. Sampling Time Line`_.

.. _Figure 5.1. Sampling Time Line:

.. figure:: ../../images/sampling-timeline.svg

    Figure 5.1. Sampling Time Line

Each sample point is assumed to be separated from its predecessor by a constant amount of real time, the **delta**. The most recent sample point is always zero. 
The value of **delta** may vary from one expression to the next, but is fixed for each expression; for more information on the sampling interval, see 
Section 5.3.2, “`Setting Evaluation Frequency`_”.

For **pmie**, a metrics expression is the name of a metric, optionally qualified by a host, instance and sample time specification. Special characters introduce 
the qualifiers: colon (**:**) for hosts, hash or pound sign (**#**) for instances, and at (**@**) for sample times. The following expression refers to the previous 
value (**@1**) of the counter for the disk read operations associated with the disk instance **#disk1** on the host **moomba**.

.. sourcecode:: none

 disk.dev.read :moomba #disk1 @1

In fact, this expression defines a point in the three-dimensional (3D) parameter space of {**host**} x {**instance**} x {**sample time**} as shown in `Figure 5.2. Three-Dimensional Parameter Space`_.

.. _Figure 5.2. Three-Dimensional Parameter Space:

.. figure:: ../../images/parameter-space.svg

    Figure 5.2. Three-Dimensional Parameter Space

A metric expression may also identify sets of values corresponding to one-, two-, or three-dimensional slices of this space, according to the following rules:

1. A metric expression consists of a PCP metric name, followed by optional host specifications, followed by optional instance specifications, and finally, optional sample time specifications.

2. A host specification consists of one or more host names, each prefixed by a colon (**:**). For example: **:indy :far.away.domain.com :localhost**

3. A missing host specification implies the default **pmie** source of metrics, as defined by a **-h** option on the command line, or the first named archive in an 
   **-a** option on the command line, or PMCD on the local host.

4. An instance specification consists of one or more instance names, each prefixed by a hash or pound (**#**) sign. For example: **#eth0 #eth2**

   Recall that you can discover the instance names for a particular metric, using the pminfo command. See Section 5.2.1, “`pmie use of PCP services`_”.

  Within the **pmie** grammar, an instance name is an identifier. If the instance name contains characters other than alphanumeric characters, enclose the instance name in single quotes; for example, **#\\'/boot\\'  #\\'/usr\\'**

5. A missing instance specification implies all instances for the associated performance metric from each associated **pmie** source of metrics.

6. A sample time specification consists of either a single time or a range of times. A single time is represented as an at (**@**) followed by a natural number. 
   A range of times is an at (**@**), followed by a natural number, followed by two periods (**..**) followed by a second natural number. The ordering of the end 
   points in a range is immaterial. For example, **@0..9** specifies the last 10 sample times.

7. A missing sample time specification implies the most recent sample time.

The following metric expression refers to a three-dimensional set of values, with two hosts in one dimension, five sample times in another, and the number of instances 
in the third dimension being determined by the number of configured disk spindles on the two hosts.

::

 disk.dev.read :foo :bar @0..4
 
pmie Rate Conversion
=====================

Many of the metrics delivered by PCP are cumulative counters. Consider the following metric::

 disk.all.total

A single value for this metric tells you only that a certain number of disk I/O operations have occurred since boot time, and that information may be invalid if the 
counter has exceeded its 32-bit range and wrapped. You need at least two values, sampled at known times, to compute the recent rate at which the I/O operations are 
being executed. The required syntax would be this::

 (disk.all.total @0 - disk.all.total @1) / delta

The accuracy of **delta** as a measure of actual inter-sample delay is an issue. **pmie** requests samples, at intervals of approximately **delta**, while the results 
exported from PMCD are time stamped with the high-resolution system clock time when the samples were extracted. For these reasons, a built-in and implicit rate 
conversion using accurate time stamps is provided by **pmie** for performance metrics that have counter semantics. For example, the following expression is 
unconditionally converted to a rate by pmie.

::

 disk.all.total
 
pmie Arithmetic Expressions
============================

Within **pmie**, simple arithmetic expressions are constructed from metrics expressions (see Section 5.3.3, “`pmie Metric Expressions`_”) and numeric constants, 
using all of the arithmetic operators and precedence rules of the C programming language.

All **pmie** arithmetic is performed in double precision.

Section 5.3.8, “`pmie Intrinsic Operators`_”, describes additional operators that may be used for aggregate operations to reduce the dimensionality of an arithmetic expression.

⁠pmie Logical Expressions
=========================

A number of logical expression types are supported:

* Logical constants
* Relational expressions
* Boolean expressions
* Quantification operators

Logical Constants
------------------

Like in the C programming language, **pmie** interprets an arithmetic value of zero to be false, and all other arithmetic values are considered true.

⁠Relational Expressions
-----------------------

Relational expressions are the simplest form of logical expression, in which values may be derived from arithmetic expressions using **pmie** relational operators. 
For example, the following is a relational expression that is true or false, depending on the aggregate total of disk read operations per second being greater than 50.

::

 disk.all.read > 50 count/sec

All of the relational logical operators and precedence rules of the C programming language are supported in **pmie**.

As described in Section 5.3.3, “`pmie Metric Expressions`_”, arithmetic expressions in **pmie** may assume set values. The relational operators are also required to 
take constant, singleton, and set-valued expressions as arguments. The result has the same dimensionality as the operands. Suppose the rule in `Example 5.6. Relational Expressions`_ is given:

.. _Example 5.6. Relational Expressions:

**Example 5.6. Relational Expressions**

::
 
 hosts = ":gonzo";
 intfs = "#eth0 #eth2";
 all_intf = network.interface.in.packets
                $hosts $intfs @0..2 > 300 count/sec;

Then the execution of **pmie** may proceed as follows:

::

 pmie -V uag.11
 all_intf: 
        gonzo: [eth0]      ?      ?      ? 
        gonzo: [eth2]      ?      ?      ?
 all_intf:
        gonzo: [eth0]  false      ?      ?
        gonzo: [eth2]  false      ?      ?
 all_intf:
        gonzo: [eth0]   true  false      ?
        gonzo: [eth2]  false  false      ?
 all_intf:
        gonzo: [eth0]   true   true  false
        gonzo: [eth2]  false  false  false

At each sample, the relational operator greater than (>) produces six truth values for the cross-product of the **instance** and **sample time** dimensions.

Section 5.3.6.4, “`Quantification Operators`_”, describes additional logical operators that may be used to reduce the dimensionality of a relational expression.

⁠Boolean Expressions
--------------------

The regular Boolean operators from the C programming language are supported: conjunction (**&&**), disjunction (**||**) and negation (**!**).

As with the relational operators, the Boolean operators accommodate set-valued operands, and set-valued results.

Quantification Operators
-------------------------

Boolean and relational operators may accept set-valued operands and produce set-valued results. In many cases, rules that are appropriate for performance management 
require a set of truth values to be reduced along one or more of the dimensions of hosts, instances, and sample times described in Section 5.3.3, “`pmie Metric Expressions`_”. 
The **pmie** quantification operators perform this function.

Each quantification operator takes a one-, two-, or three-dimension set of truth values as an operand, and reduces it to a set of smaller dimension, by quantification 
along a single dimension. For example, suppose the expression in the previous example is simplified and prefixed by **some_sample**, to produce the following expression::

 intfs = "#eth0 #eth2"; 
 all_intf = some_sample network.interface.in.packets
                      $intfs @0..2 > 300 count/sec;

Then the expression result is reduced from six values to two (one per interface instance), such that the result for a particular instance will be false unless the 
relational expression for the same interface instance is true for at least one of the preceding three sample times.

There are existential, universal, and percentile quantification operators in each of the *host, instance*, and *sample time* dimensions to produce the nine operators as follows:

+--------------------+----------------------------------------------------------------------------------------------------+
| some_host          | True if the expression is true for at least one host for the same instance and sample time.        |
+--------------------+----------------------------------------------------------------------------------------------------+
| all_host           | True if the expression is true for every host for the same instance and sample time.               |
+--------------------+----------------------------------------------------------------------------------------------------+
| N%_host            | True if the expression is true for at least N% of the hosts for the same instance and sample time. |
+--------------------+----------------------------------------------------------------------------------------------------+
| some_inst          | True if the expression is true for at least one instance for the same host and sample time.        |
+--------------------+----------------------------------------------------------------------------------------------------+
| all_instance       | True if the expression is true for every instance for the same host and sample time.               |
+--------------------+----------------------------------------------------------------------------------------------------+
| N%_instance        | True if the expression is true for at least N% of the instances for the same host and sample time. |
+--------------------+----------------------------------------------------------------------------------------------------+
| some_sample time   | True if the expression is true for at least one sample time for the same host and instance.        |
+--------------------+----------------------------------------------------------------------------------------------------+
| all_sample time    | True if the expression is true for every sample time for the same host and instance.               |
+--------------------+----------------------------------------------------------------------------------------------------+
| N%_sample time     | True if the expression is true for at least N% of the sample times for the same host and instance. |
+--------------------+----------------------------------------------------------------------------------------------------+

These operators may be nested. For example, the following expression answers the question: “Are all hosts experiencing at least 20% of their disks busy either reading or writing?”

::

 Servers = ":moomba :babylon";
 all_host ( 
     20%_inst disk.dev.read $Servers > 40 || 
     20%_inst disk.dev.write $Servers > 40
 );

The following expression uses different syntax to encode the same semantics::

 all_host (
     20%_inst (
         disk.dev.read $Servers > 40 ||
         disk.dev.write $Servers > 40
     )
 );

.. note::
   To avoid confusion over precedence and scope for the quantification operators, use explicit parentheses.

Two additional quantification operators are available for the instance dimension only, namely **match_inst** and **nomatch_inst**, that take a regular expression and a 
boolean expression. The result is the boolean AND of the expression and the result of matching (or not matching) the associated instance name against the regular expression.

For example, this rule evaluates error rates on various 10BaseT Ethernet network interfaces (such as ecN, ethN, or efN):

.. sourcecode:: none

 some_inst
         match_inst "^(ec|eth|ef)"
                 network.interface.total.errors > 10 count/sec
 -> syslog "Ethernet errors:" " %i"
 
pmie Rule Expressions
======================

Rule expressions for **pmie** have the following syntax::

 lexpr -> actions ;

The semantics are as follows:

* If the logical expression **lexpr** evaluates **true**, then perform the *actions* that follow. Otherwise, do not perform the *actions*.
* It is required that **lexpr** has a singular truth value. Aggregation and quantification operators must have been applied to reduce multiple truth values to a single value.
* When executed, an *action* completes with a success/failure status.
* One or more *actions* may appear; consecutive *actions* are separated by operators that control the execution of subsequent *actions*, as follows:
   
   * *action-1* **&** : Always execute subsequent actions (serial execution).
   * *action-1* **|** : If *action-1* fails, execute subsequent actions, otherwise skip the subsequent actions (alternation).

An *action* is composed of a keyword to identify the action method, an optional *time* specification, and one or more arguments.

A *time* specification uses the same syntax as a valid time interval that may be assigned to **delta**, as described in Section 5.3.2, "`Setting Evaluation Frequency`_ ”. 
If the *action* is executed and the *time* specification is present, **pmie** will suppress any subsequent execution of this *action* until the wall clock time has advanced by *time*.

The arguments are passed directly to the action method.

The following action methods are provided:

**shell**

The single argument is passed to the shell for execution. This *action* is implemented using **system** in the background. The *action* does not wait for the system call to return, and succeeds unless the fork fails.

**alarm**

A notifier containing a time stamp, a single *argument* as a message, and a **Cancel** button is posted on the current display screen (as identified by the **DISPLAY** 
environment variable). Each alarm *action* first checks if its notifier is already active. If there is an identical active notifier, a duplicate notifier is not posted. 
The action succeeds unless the fork fails.

**syslog**

A message is written into the system log. If the first word of the first argument is **-p**, the second word is interpreted as the priority (see the **syslog(3)** man page); the message tag is **pcp-pmie**. 
The remaining argument is the message to be written to the system log. This action always succeeds.

**print**

A message containing a time stamp in **ctime(3)** format and the argument is displayed out to standard output (**stdout**). This action always succeeds.

Within the argument passed to an action method, the following expansions are supported to allow some of the context from the logical expression on the left to appear to be embedded in the argument:

+------------+-------------------------------------------------------------------+
| **%h**     | The value of a *host* that makes the expression true.             |
+------------+-------------------------------------------------------------------+
| **%i**     | The value of an *instance* that makes the expression true.        |
+------------+-------------------------------------------------------------------+
| **%v**     | The value of a performance metric from the logical expression.    |
+------------+-------------------------------------------------------------------+

Some ambiguity may occur in respect to which host, instance, or performance metric is bound to a %-token. In most cases, the leftmost binding in the top-level 
subexpression is used. You may need to use **pmie** in the interactive debugging mode (specify the **-d** command line option) in conjunction with the **-W** 
command line option to discover which subexpressions contributes to the %-token bindings.

.. note::
   When **pmie** is processing performance metrics from one or more PCP
   archives the *rules* will be processed in the expected manner; however,
   the *actions* are modified to report a textual facsimile of the *action*
   on the standard output that includes the action, the time in the archive
   when the *rule* predicate was true and all of the arguments for the
   *action*.
   The rationale for this is that the context in which the *action*
   would have been executed (in live mode) was at a time in the past
   and possibly on a different host (if the archive was collected
   from one host, but **pmie** is being run on a different host).
   So flooding **syslog** with misleading messages or
   an avalanche of visual alarms or
   running a shell command that might not even work on the host where **pmie**
   is being run, are all be avoided.
   Rather the output is text in a regular format suitable for post-processing
   with a range of filters and performance analysis tools.

`Example 5.7. Rule Expression Options`_ illustrates some of the options when constructing rule expressions:

.. _Example 5.7. Rule Expression Options:

**Example 5.7. Rule Expression Options**

.. sourcecode:: none

 some_inst ( disk.dev.total > 60 ) 
        -> syslog 10 mins "[%i] busy, %v IOPS " & 
           shell 1 hour "echo \ 
                'Disk %i is REALLY busy. Running at %v I/Os per second' \ 
                | Mail -s 'pmie alarm' sysadm";

In this case, **%v** and **%i** are both associated with the instances for the metric **disk.dev.total** that make the expression true. If more than one instance 
makes the expression true (more than one disk is busy), then the argument is formed by concatenating the result from each %-token binding. The text added to the 
system log file might be as shown in `Example 5.8. System Log Text`_ :

.. _Example 5.8. System Log Text:

**Example 5.8. System Log Text**

::

 Aug 6 08:12:44 5B:gonzo pcp-pmie[3371]:
                          [disk1] busy, 3.7 IOPS [disk2] busy, 0.3 IOPS

Consider the rule in `Example 5.9. Standard Output`_ :

.. _Example 5.9. Standard Output:

**Example 5.9. Standard Output**

.. sourcecode:: none

 delta = 2 sec;  // more often for demonstration purposes 
 percpu  = "kernel.percpu"; 
 // Unusual usr-sys split when some CPU is more than 20% in usr mode 
 // and sys mode is at least 1.5 times usr mode 
 // 
 cpu_usr_sys = some_inst ( 
         $percpu.cpu.sys > $percpu.cpu.user * 1.5 && 
         $percpu.cpu.user > 0.2 
    ) ->  alarm "Unusual sys time: " "%i ";

When evaluated against an archive, the following output is generated (the alarm action produces a message on standard output)::

 pmafm ${HOME}/f4 pmie cpu.head cpu.00
 alarm Wed Aug  7 14:54:48 2012: Unusual sys time: cpu0 
 alarm Wed Aug  7 14:54:50 2012: Unusual sys time: cpu0 
 alarm Wed Aug  7 14:54:52 2012: Unusual sys time: cpu0 
 alarm Wed Aug  7 14:55:02 2012: Unusual sys time: cpu0 
 alarm Wed Aug  7 14:55:06 2012: Unusual sys time: cpu0
 
pmie Intrinsic Operators
=========================

The following sections describe some other useful intrinsic operators for **pmie**. These operators are divided into three groups:

1. Arithmetic aggregation
2. The rate operator
3. Transitional operators

⁠Arithmetic Aggregation
------------------------

For set-valued arithmetic expressions, the following operators reduce the dimensionality of the result by arithmetic aggregation along one of the *host*, *instance*, 
or *sample time* dimensions. For example, to aggregate in the *host* dimension, the following operators are provided:

+----------------+--------------------------------------------------------------------------------------------------+
| **avg_host**   | Computes the average value across all *instances* for the same *host* and *sample time*          |
+----------------+--------------------------------------------------------------------------------------------------+
| **sum_host**   | Computes the total value across all *instances* for the same *host* and *sample time*            |
+----------------+--------------------------------------------------------------------------------------------------+
| **count_host** | Computes the number of values across all *instances* for the same *host* and *sample time*       |
+----------------+--------------------------------------------------------------------------------------------------+
| **min_host**   | Computes the minimum value across all *instances* for the same *host* and *sample time*          |
+----------------+--------------------------------------------------------------------------------------------------+
| **max_host**   | Computes the maximum value across all *instances* for the same *host* and *sample time*          |
+----------------+--------------------------------------------------------------------------------------------------+

Ten additional operators correspond to the forms \*_inst and \*_sample.

The following example illustrates the use of an aggregate operator in combination with an existential operator to answer the question “Does some host currently have 
two or more busy processors?”

::

 // note '' to escape - in host name 
 poke = ":moomba :'mac-larry' :bitbucket"; 
 some_host ( 
     count_inst ( kernel.percpu.cpu.user $poke + 
                  kernel.percpu.cpu.sys $poke > 0.7 ) >= 2 
     ) 
        -> alarm "2 or more busy CPUs";

⁠The rate Operator
------------------

The **rate** operator computes the rate of change of an arithmetic expression as shown in the following example::

 rate mem.util.cached

It returns the rate of change for the **mem.util.cached** performance metric; that is, the rate at which page cache memory is being allocated and released.

The **rate** intrinsic operator is most useful for metrics with instantaneous value semantics. For metrics with counter semantics,  **pmie** already performs an 
implicit rate calculation (see the Section 5.3.4, “`pmie Rate Conversion`_”) and the **rate** operator would produce the second derivative with respect to time, 
which is less likely to be useful.

Transitional Operators
-----------------------

In some cases, an action needs to be triggered when an expression changes from true to false or vice versa. The following operators take a logical expression as an operand, and return a logical expression:

* **rising**: Has the value **true** when the operand transitions from **false** to **true** in consecutive samples.
* **falling**: Has the value **false** when the operand transitions from **true** to **false** in consecutive samples.

pmie Examples
**************

The examples presented in this section are task-oriented and use the full power of the pmie specification language as described in Section 5.3, “`Specification Language for pmie`_”.

Source code for the **pmie** examples in this chapter, and many more examples, is provided within the *PCP Tutorials and Case Studies*. 
`Example 5.10. Monitoring CPU Utilization`_ and `Example 5.11. Monitoring Disk Activity`_ illustrate monitoring CPU utilization and disk activity.

.. _Example 5.10. Monitoring CPU Utilization:

**Example 5.10. Monitoring CPU Utilization**

::

 // Some Common Performance Monitoring Scenarios
 //
 // The CPU Group
 //
 delta = 2 sec;  // more often for demonstration purposes
 // common prefixes
 //
 percpu  = "kernel.percpu";
 all     = "kernel.all";
 // Unusual usr-sys split when some CPU is more than 20% in usr mode
 // and sys mode is at least 1.5 times usr mode
 //
 cpu_usr_sys =
        some_inst (
            $percpu.cpu.sys > $percpu.cpu.user * 1.5 &&
            $percpu.cpu.user > 0.2
        )
            ->  alarm "Unusual sys time: " "%i ";
 // Over all CPUs, syscall_rate > 1000 * no_of_cpus
 //
 cpu_syscall =
        $all.syscall > 1000 count/sec * hinv.ncpu
        ->  print "high aggregate syscalls: %v";
 // Sustained high syscall rate on a single CPU
 //
 delta = 30 sec;
 percpu_syscall =
        some_inst (
            $percpu.syscall > 2000 count/sec
        )
            -> syslog "Sustained syscalls per second? " "[%i] %v ";
 // the 1 minute load average exceeds 5 * number of CPUs on any host
 hosts = ":gonzo :moomba";   // change as required
 delta = 1 minute;           // no need to evaluate more often than this
 high_load =
      some_host (
            $all.load $hosts #'1 minute' > 5 * hinv.ncpu
        )
            -> alarm "High Load Average? " "%h: %v ";
            
.. _Example 5.11. Monitoring Disk Activity:

**Example 5.11. Monitoring Disk Activity**

::

 // Some Common Performance Monitoring Scenarios
 //
 // The Disk Group
 //
 delta = 15 sec;         // often enough for disks?
 // common prefixes
 //
 disk    = "disk";
 // Any disk performing more than 40 I/Os per second, sustained over
 // at least 30 seconds is probably busy
 //
 delta = 30 seconds;
 disk_busy =
        some_inst (
            $disk.dev.total > 40 count/sec
        )
 ]      -> shell "Mail -s 'Heavy sustained disk traffic' sysadm";
 // Try and catch bursts of activity ... more than 60 I/Os per second
 // for at least 25% of 8 consecutive 3 second samples
 //
 delta = 3 sec;
 disk_burst =
        some_inst (
            25%_sample (
                $disk.dev.total @0..7 > 60 count/sec
            )
        )
        -> alarm "Disk Burst? " "%i ";
 // any SCSI disk controller performing more than 3 Mbytes per
 // second is busy
 // Note: the obscure 512 is to convert blocks/sec to byte/sec,
 //       and pmie handles the rest of the scale conversion
 //
 some_inst $disk.ctl.blktotal * 512 > 3 Mbyte/sec
            -> alarm "Busy Disk Controller: " "%i ";
            
Developing and Debugging pmie Rules
************************************

Given the **-d** command line option, **pmie** executes in interactive mode, and the user is presented with a menu of options::

 pmie debugger commands
      f [file-name]      - load expressions from given file or stdin
      l [expr-name]      - list named expression or all expressions
      r [interval]       - run for given or default interval
      S time-spec        - set start time for run
      T time-spec        - set default interval for run command
      v [expr-name]      - print subexpression for %h, %i and %v bindings
      h or ?             - print this menu of commands
      q                  - quit
 pmie>

If both the **-d** option and a filename are present, the expressions in the given file are loaded before entering interactive mode. Interactive mode is useful for debugging new rules.

Caveats and Notes on pmie
**************************

The following sections provide important information for users of **pmie**.

⁠Performance Metrics Wraparound
===============================

Performance metrics that are cumulative counters may occasionally overflow their range and wraparound to 0. When this happens, an unknown value (printed as **?**) is 
returned as the value of the metric for one sample (recall that the value returned is normally a rate). You can have PCP interpolate a value based on expected rate 
of change by setting the **PCP_COUNTER_WRAP** environment variable.

⁠pmie Sample Intervals
======================

The sample interval (**delta**) should always be long enough, particularly in the case of rates, to ensure that a meaningful value is computed. Interval may vary 
according to the metric and your needs. A reasonable minimum is in the range of ten seconds or several minutes. Although PCP supports sampling rates up to hundreds 
of times per second, using small sample intervals creates unnecessary load on the monitored system.

⁠pmie Instance Names
====================

When you specify a metric instance name (*#identifier*) in a **pmie** expression, it is compared against the instance name looked up from either a live collector system or an archive as follows:

* If the given instance name and the looked up name are the same, they are considered to match.
* Otherwise, the first two space separated tokens are extracted from the looked up name. If the given instance name is the same as either of these tokens, they are considered a match.

For some metrics, notably the per process (**proc.xxx.xxx**) metrics, the first token in the looked up instance name is impossible to determine at the time you are 
writing **pmie** expressions. The above policy circumvents this problem.

⁠pmie Error Detection
======================

The parser used in **pmie** is not particularly robust in handling syntax errors. It is suggested that you check any problematic expressions individually in interactive mode::

 pmie -v -d
 pmie> f
 expression
 Ctrl+D

If the expression was parsed, its internal representation is shown::

 pmie> l

The expression is evaluated twice and its value printed::

 pmie> r 10sec

Then quit::

 pmie> q

It is not always possible to detect semantic errors at parse time. This happens when a performance metric descriptor is not available from the named host at this time. 
A warning is issued, and the expression is put on a wait list. The wait list is checked periodically (about every five minutes) to see if the metric descriptor has 
become available. If an error is detected at this time, a message is printed to the standard error stream (**stderr**) and the offending expression is set aside.

Creating pmie Rules with pmieconf
**********************************

The **pmieconf** tool is a command line utility that is designed to aid the specification of **pmie** rules from parameterized versions of the rules. **pmieconf** is 
used to display and modify variables or parameters controlling the details of the generated **pmie** rules.

**pmieconf** reads two different forms of supplied input files and produces a localized **pmie** configuration file as its output.

The first input form is a generalized **pmie** rule file such as those found below ``${PCP_VAR_DIR}/config/pmieconf``. These files contain the generalized rules 
which **pmieconf** is able to manipulate. Each of the rules can be enabled or disabled, or the individual variables associated with each rule can be edited.

The second form is an actual **pmie** configuration file (that is, a file which can be interpreted by **pmie**, conforming to the **pmie** syntax described in 
Section 5.3, “`Specification Language for pmie`_”). This file is both input to and output from **pmieconf**.

The input version of the file contains any changed variables or rule states from previous invocations of **pmieconf**, and the output version contains both the changes 
in state (for any subsequent **pmieconf** sessions) and the generated **pmie** syntax. The **pmieconf** state is embedded within a **pmie** comment block at the head 
of the output file and is not interpreted by **pmie** itself.

**pmieconf** is an integral part of the **pmie** daemon management process described in Section 5.8, “`Management of pmie Processes`_”. `Procedure 5.1. Display pmieconf Rules`_ and 
`Procedure 5.2. Modify pmieconf Rules and Generate a pmie File`_ introduce the **pmieconf** tool through a series of typical operations.

.. _Procedure 5.1. Display pmieconf Rules:

**Procedure 5.1. Display pmieconf Rules**

1. Start **pmieconf** interactively (as the superuser).

::

 pmieconf -f ${PCP_SYSCONF_DIR}/pmie/config.demo
 Updates will be made to ${PCP_SYSCONF_DIR}/pmie/config.demo

 pmieconf>

2. List the set of available **pmieconf** rules by using the **rules** command.

3. List the set of rule groups using the **groups** command.

4. List only the enabled rules, using the **rules enabled** command.

5. List a single rule:

    .. sourcecode:: none

      pmieconf> list memory.swap_low
         rule: memory.swap_low  [Low free swap space]
         help: There is only threshold percent swap space remaining - the system
               may soon run out of virtual memory.  Reduce the number and size of
               the running programs or add more swap(1) space before it
      completely
               runs out.
               predicate =
                 some_host (
                     ( 100 * ( swap.free $hosts$ / swap.length $hosts$ ) )
                       < $threshold$
                     && swap.length $hosts$ > 0        // ensure swap in use
                  )
         vars: enabled = no
               threshold = 10%
 
      pmieconf>

6. List one rule variable:

    .. sourcecode:: none

      pmieconf> list memory.swap_low threshold
         rule: memory.swap_low  [Low free swap space]
               threshold = 10%

      pmieconf>
 
.. _Procedure 5.2. Modify pmieconf Rules and Generate a pmie File:

**Procedure 5.2. Modify pmieconf Rules and Generate a pmie File**

1. Lower the threshold for the **memory.swap_low** rule, and also change the **pmie** sample interval affecting just this rule. The **delta** variable is special in 
   that it is not associated with any particular rule; it has been defined as a global **pmieconf** variable. Global variables can be displayed using the **list global** 
   command to **pmieconf**, and can be modified either globally or local to a specific rule.

   .. sourcecode:: none

    pmieconf> modify memory.swap_low threshold 5

    pmieconf> modify memory.swap_low delta "1 sec"

    pmieconf>

2. Disable all of the rules except for the **memory.swap_low** rule so that you can see the effects of your change in isolation.

   This produces a relatively simple **pmie** configuration file::

    pmieconf> disable all

    pmieconf> enable memory.swap_low

    pmieconf> status
      verbose:  off
      enabled rules:  1 of 35
      pmie configuration file:  ${PCP_SYSCONF_DIR}/pmie/config.demo
      pmie processes (PIDs) using this file:  (none found)

    pmieconf> quit

  You can also use the **status** command to verify that only one rule is enabled at the end of this step.

3. Run **pmie** with the new configuration file. Use a text editor to view the newly generated **pmie** configuration file (``${PCP_SYSCONF_DIR}/pmie/config.demo``), 
   and then run the command::

    pmie -T "1.5 sec" -v -l ${HOME}/demo.log ${PCP_SYSCONF_DIR}/pmie/config.demo
    memory.swap_low: false

    memory.swap_low: false

    cat ${HOME}/demo.log
    Log for pmie on venus started Mon Jun 21 16:26:06 2012

    pmie: PID = 21847, default host = venus

    [Mon Jun 21 16:26:07] pmie(21847) Info: evaluator exiting

    Log finished Mon Jun 21 16:26:07 2012

4. Notice that both of the **pmieconf** files used in the previous step are simple text files, as described in the **pmieconf(5)** man page::

    file ${PCP_SYSCONF_DIR}/pmie/config.demo
    ${PCP_SYSCONF_DIR}/pmie/config.demo:  PCP pmie config (V.1)
    file ${PCP_VAR_DIR}/config/pmieconf/memory/swap_low
    ${PCP_VAR_DIR}/config/pmieconf/memory/swap_low:       PCP pmieconf rules (V.1)
    
Management of pmie Processes
*****************************

The **pmie** process can be run as a daemon as part of the system startup sequence, and can thus be used to perform automated, live performance monitoring of a 
running system. To do this, run these commands (as superuser)::

 chkconfig pmie on
 ${PCP_RC_DIR}/pmie start

By default, these enable a single **pmie** process monitoring the local host, with the default set of **pmieconf** rules enabled (for more information about **pmieconf**, 
see Section 5.7, “`Creating pmie Rules with pmieconf`_”). `Procedure 5.3. Add a New pmie Instance to the pmie Daemon Management Framework`_ illustrates how you can 
use these commands to start any number of **pmie** processes to monitor local or remote machines.

.. _Procedure 5.3. Add a New pmie Instance to the pmie Daemon Management Framework:

**Procedure 5.3. Add a New pmie Instance to the pmie Daemon Management Framework**

1. Use a text editor (as superuser) to edit the ``pmie${PCP_PMIECONTROL_PATH}`` and ``${PCP_PMIECONTROL_PATH}.d`` control files. Notice the default entry, which looks like this:

   .. sourcecode:: none
   
     #Host           P?  S?  Log File                                  Arguments
     LOCALHOSTNAME   y   n   PCP_LOG_DIR/pmie/LOCALHOSTNAME/pmie.log   -c config.default

   This entry is used to enable a local **pmie** process. Add a new entry for a remote host on your local network (for example, **venus**), by using your pmie 
   configuration file (see Section 5.7, “`Creating pmie Rules with pmieconf`_”):

   .. sourcecode:: none

     #Host           P?  S?  Log File                                  Arguments
     venus           n   n   PCP_LOG_DIR/pmie/venus/pmie.log           -c config.demo

   .. note::
      Without an absolute path, the configuration file (**-c** above) will be resolved using ``${PCP_SYSCONF_DIR}/pmie`` - if **config.demo** was created in 
      `Procedure 5.2. Modify pmieconf Rules and Generate a pmie File`_ it would be used here for host **venus**, otherwise a new configuration file will be generated 
      using the default rules (at ``${PCP_SYSCONF_DIR}/pmie/config.demo``).

2. Enable **pmie** daemon management::

    chkconfig pmie on

This simple step allows **pmie** to be started as part of your machine's boot process.

3. Start the two **pmie** daemons. At the end of this step, you should see two new **pmie** processes monitoring the local and remote hosts:

   .. sourcecode:: none

     ${PCP_RC_DIR}/pmie start
     Performance Co-Pilot starting inference engine(s) ...

Wait a few moments while the startup scripts run. The **pmie** start script uses the **pmie_check** script to do most of its work.

Verify that the **pmie** processes have started::

 pcp
 Performance Co-Pilot configuration on pluto:

  platform: Linux pluto 3.10.0-0.rc7.64.el7.x86_64 #1 SMP
  hardware: 8 cpus, 2 disks, 23960MB RAM
  timezone: EST-10
      pmcd: Version 3.11.3-1, 8 agents
      pmda: pmcd proc xfs linux mmv infiniband gluster elasticsearch
      pmie: pluto: ${PCP_LOG_DIR}/pmie/pluto/pmie.log
            venus: ${PCP_LOG_DIR}/pmie/venus/pmie.log

If a remote host is not up at the time when **pmie** is started, the **pmie** process may exit. **pmie** processes may also exit if the local machine is starved of 
memory resources. To counter these adverse cases, it can be useful to have a **crontab** entry running. Adding an entry as shown in Section 5.8.1, “`Add a pmie crontab Entry`_” 
ensures that if one of the configured **pmie** processes exits, it is automatically restarted.

.. note::
   Depending on your platform, the **crontab** entry discussed here may already have been installed for you, as part of the package installation process. In this case, the file **/etc/cron.d/pcp-pmie** will exist, and the rest of this section can be skipped.
   
Add a pmie crontab Entry
==========================

To activate the maintenance and housekeeping scripts for a collection of inference engines, execute the following tasks while logged into the local host as the superuser (**root**):

1. Augment the **crontab** file for the **pcp** user. For example:
 
  .. sourcecode:: none
    
     crontab -l -u pcp > ${HOME}/crontab.txt

2. Edit ``${HOME}/crontab.txt``, adding lines similar to those from the sample ``${PCP_VAR_DIR}/config/pmie/crontab`` file for **pmie_daily** and **pmie_check**; 
   for example:

   .. sourcecode:: none

      # daily processing of pmie logs
      10     0     *     *     *    ${PCP_BINADM_DIR}/pmie_daily
      # every 30 minutes, check pmie instances are running
      25,55  *     *     *     *    ${PCP_BINADM_DIR}/pmie_check

3. Make these changes permanent with this command:

   .. sourcecode:: none

      crontab -u pcp < ${HOME}/crontab.txt

⁠Global Files and Directories
=============================

The following global files and directories influence the behavior of **pmie** and the **pmie** management scripts:

``${PCP_DEMOS_DIR}/pmie/*``

Contains sample **pmie** rules that may be used as a basis for developing local rules.

``${PCP_SYSCONF_DIR}/pmie/config.default``

Is the default **pmie** configuration file that is used when the **pmie** daemon facility is enabled. Generated by **pmieconf** if not manually setup beforehand.

``${PCP_VAR_DIR}/config/pmieconf/*/*``

Contains the **pmieconf** rule definitions (templates) in its subdirectories.

``${PCP_PMIECONTROL_PATH} and ${PCP_PMIECONTROL_PATH}.d`` files

Defines which PCP collector hosts require a daemon **pmie** to be launched on the local host, where the configuration file comes from, where the **pmie** log file 
should be created, and **pmie** startup options.

``${PCP_VAR_DIR}/config/pmlogger/crontab``

Contains default **crontab** entries that may be merged with the **crontab** entries for root to schedule the periodic execution of the **pmie_check** script, 
for verifying that **pmie** instances are running. Only for platforms where a default **crontab** is not automatically installed during the initial PCP package installation.

``${PCP_LOG_DIR}/pmie/*``

Contains the **pmie** log files for the host. These files are created by the default behavior of the ``${PCP_RC_DIR}/pmie`` startup scripts.

pmie Instances and Their Progress
===================================

The PMCD PMDA exports information about executing **pmie** instances and their progress in terms of rule evaluations and action execution rates.

``pmie_check``

This command is similar to the **pmlogger** support script, **pmlogger_check**.

``${PCP_RC_DIR}/pmie``

This start script supports the starting and stopping of multiple **pmie** instances that are monitoring one or more hosts.

``${PCP_TMP_DIR}/pmie``

The statistics that **pmie** gathers are maintained in binary data structure files. These files are located in this directory.

``pmcd.pmie metrics``

If **pmie** is running on a system with a PCP collector deployment, the PMCD PMDA exports these metrics via the **pmcd.pmie** group of metrics.
