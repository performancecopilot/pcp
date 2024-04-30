.. _TimeSeriesQuerying:

Fast, Scalable Time Series Querying - pmseries
################################################

.. contents::

**pmseries** is a fast, scalable time series querying which displays information about performance metrics.

The major sections in this chapter are as follows:

Section 9.1, “`Introduction to pmseries`_”, provides an introduction to the concepts and working of **pmseries**.

Section 9.2, “`Timeseries Queries`_”, explains how query expressions are formed using the **pmseries** query language.

Section 9.3, “`Metadata Qualifiers and Metadata Operators`_”, explains various metadata properties.

Section 9.4, “`Time Specification`_”, specifies a specific time window of interest.

Section 9.5, “`Expressions`_”, explains the various arithmetic operators, functions, function references as well as their compatibility supported by **pmseries**.

Section 9.6, “`Timeseries Options`_”, explains the various timeseries options requested to **pmseries** using command line.

Section 9.7, “`PCP Environment`_”, describes environment variables used to parameterize the file and directory names used by PCP.

Section 9.8, “`PCP Grafana Plugin`_”, explains the PCP data source and lays out the path to using the PCP Grafana Plugin.

Introduction to pmseries
*************************

**pmseries** displays various types of information about performance metrics available through the scalable timeseries facilities of the Performance Co-Pilot (PCP) using a distributed key-value server such as `Valkey <https://valkey.io/>`_

By default **pmseries** communicates with a local key-value server, however the **-h** and **-p** options can be used to specify an alternate server.  If this instance is a node of a cluster, all other instances in the cluster will be discovered and used automatically.

**pmseries** runs in several different modes - either querying timeseries identifiers, metadata or values (already stored), or manually loading timeseries into a key-value server. The latter mode is seldom used, however, since `pmproxy(1) <https://man7.org/linux/man-pages/man1/pmproxy.1.html>`_ will automatically 
perform this function for local `pmlogger(1) <https://man7.org/linux/man-pages/man1/pmlogger.1.html>`_ instances, when running in its default time series mode.

Without command line options specifying otherwise, **pmseries** will issue a timeseries query to find matching timeseries and values. All timeseries are 
identified using a unique SHA-1 hash which is always displayed in a 40-hexdigit human readable form. These hashes are formed using the metadata associated 
with every metric.

Importantly, this includes all metric metadata (labels, names, descriptors). Metric labels in particular are (as far as possible) unique for every 
machine - on Linux for example the labels associated with every metric include the unique ``/etc/machine-id`` , the hostname, domainname, and other automatically 
generated machine labels, as well as any administrator-defined labels from ``/etc/pcp/labels`` . These labels can be reported with `pminfo(1) <https://man7.org/linux/man-pages/man1/pminfo.1.html>`_ 
and the *pmcd.labels* metric.

See `pmLookupLabels(3) <https://man7.org/linux/man-pages/man3/pmLookupLabels.3.html>`_, `pmLookupInDom(3) <https://man7.org/linux/man-pages/man3/pmLookupInDom.3.html>`_, 
`pmLookupName(3) <https://man7.org/linux/man-pages/man3/pmLookupName.3.html>`_ and `pmLookupDesc(3) <https://man7.org/linux/man-pages/man3/pmLookupDesc.3.html>`_ for 
detailed information about metric labels and other metric metadata used in each timeseries identifier hash calculation.

The timeseries identifiers provide a higher level (and machine independent) identifier than the traditional PCP performance metric identifiers (pmID), 
instance domain identifiers (pmInDom) and metric names. See `PCPIntro(1) <https://pcp.io/man/man1/pcpintro.1.html>`_ for more details about these 
traditional identifiers. However, **pmseries** uses timeseries identifiers in much the same way that `pminfo(1) <https://man7.org/linux/man-pages/man1/pminfo.1.html>`_ 
uses the lower level indom, metric identifiers and metric names.

The default mode of **pmseries** operation (i.e. with no command line options) depends on the arguments it is presented. If all non-option arguments 
appear to be timeseries identifiers (in 40 hex digit form) **pmseries** will report metadata for these timeseries - refer to the **-a** option for details. 
Otherwise, the parameters will be treated as a timeseries query.

Timeseries Queries
********************

Query expressions are formed using the **pmseries** query language described below, but can be as simple as a metric name.

The following is an example of querying timeseries from all hosts that match a metric name pattern (globbed):

.. sourcecode:: none

 $ pmseries kernel.all.cpu*
 1d7b0bb3f6aec0f49c54f5210885464a53629b60
 379db729afd63fb9eff436625bd6c55a7adc5cfd
 3dd3b45bb05f96636043e5d58b52b441ce542285
 [...]
 ed2bf325ff6dc7589ec966698e5404b67252306a
 dcb2a032a308b5717bf605ba8f8737e9c6e1ed19

To identify timeseries expression operands, the query language uses the general syntax:

.. sourcecode:: none

 [metric.name] '{metadata qualifiers}' '[time specification]'

The *metric.name* component restricts the timeseries query to any matching PCP metric name (the list of metric names for a PCP archive or live host is 
reported by `pminfo(1) <https://man7.org/linux/man-pages/man1/pminfo.1.html>`_ with no arguments beyond -- **host** or -- **archive**). The **pmseries** 
syntax extends on that of **pminfo** and allows for `glob(7) <https://man7.org/linux/man-pages/man7/glob.7.html>`_ based pattern matching within the 
metric name. The above describes operands available as the leaves of **pmseries** expressions, which may include functions, arithmetic operators and other 
features. See the `EXPRESSIONS`_ section below for further details.

Metadata Qualifiers and Metadata Operators
********************************************

Metadata qualifiers are enclosed by "curly" braces ( **{}** ), and further restrict the query results to timeseries operands with various metadata 
properties. These qualifiers are based on metric or instance names, and metric label values, and take the general form *metadata.name* OPERATOR *value* , such as:

.. sourcecode:: none

 instance.name == "cpu0"
 metric.name != "kernel.all.pswitch"

When using label names, the metadata qualifier is optional and can be dropped, such as:

.. sourcecode:: none

 label.hostname == "www.acme.com"
 hostname == "www.acme.com"

For metric and instance names only the string operators apply, but for metric label values all operators are available. The set of available operators is:

Boolean operators
====================

All string (label, metrics and instances) and numeric (label) values can be tested for equality ("==") or inequality ("!=").

String operators
===================

Strings can be subject to pattern matching in the form of glob matching ("~~"), regular expression matching ("=~"), and regular expression non-matching 
("!~"). The ":" operator is equivalent to "~~" - i.e., regular expression matching.

Relational operators (numeric label values only)
==================================================

Numeric label values can be subject to the less than ("<"), greater than (">"), less than or equal ("<="), greater than or equal (">="), equal ("==") and 
not equal ("!=") operators.

Logical operators
===================

Multiple metadata qualifiers can be combined with the logical operators for AND ("&&") and OR ("||") as in many programming languages. The comma 
(",") character is equivalent to logical AND ("&&").

Time Specification
********************

The final (optional) component of a query allows the user to specify a specific time window of interest. Any time specification will result in values 
being returned for all matching timeseries only for the time window specified.

The specification is "square" bracket ( **[]** ) enclosed, and consists of one or more comma-separated components. Each component specifies some aspect 
related to time, taking the general form: **keyword** : *value* , such as:

.. sourcecode:: none

 samples:10

Sample count
==============

The number of samples to return, specified via either the **samples** or (equivalent) **count** keyword. The *value* provided must be a positive integer. 
If no end time is explicitly set (see "Time window" later) then the most recent samples will be returned.

Sample interval
=================

An interval between successive samples can be requested using the **interval** or (equivalent) **delta** keyword. The *value* provided should be either a 
numeric or string value that will be parsed by `pmParseInterval(3) <https://man7.org/linux/man-pages/man3/pmParseInterval.3.html>`_, such as **5** (seconds) or **2min** (minutes).

Time window
============

Start and end times, and alignments, affecting the returned values. The keywords match the parameters to the `pmParseTimeWindow(3) <https://man7.org/linux/man-pages/man3/pmParseTimeWindow.3.html>`_ 
function which will be used to parse them, and are: **start** or (equivalent) **begin** , **finish** or (equivalent) **end** , **align** and **offset**.

Time zones
============

The resulting timestamps can be returned having been evaluated for a specific timezone, using the **timezone** or **hostzone** keywords. The *value* 
associated with **timezone** will be interpreted by `pmNewZone(3) <https://man7.org/linux/man-pages/man3/pmNewZone.3.html>`_. A **true** or **false** 
value should be associated with **hostzone** , and when set to **true** this has the same effect as described by `pmNewContextZone(3) <https://man7.org/linux/man-pages/man3/pmNewContextZone.3.html>`_.

Expressions
*************

As described above, operands are the leaves of a query expression tree.

.. sourcecode:: none

 [metric.name] '{metadata qualifiers}' '[time specification]'

Note in most of the query expression examples below, the *metadata qualifiers* have been omitted for brevity. In all cases, multiple time series may 
qualify, particularly for the **hostname** label.

In the simple case, a query expression consists of a single operand and may just be a metric name. In the more general case, a query expression is either 
an operand or the argument to a function, or two operands in a binary arithmetic or logical expression. Most functions take a single argument (an expression), 
though some require additional arguments, e.g. **rescale**.

.. sourcecode:: none

 operand | expr operator expr | func(expr[, arg])

This grammar shows expressions may be nested, e.g. using the addition ( **+** ) operator as an example,

.. sourcecode:: none

 func1(func2(expr))
 func1(expr) + func2(expr)
 expr + func(expr)
 func(expr) + expr
 expr + expr

Rules governing compatibility of operands in an expression generally depend on the function and/or operators and are described below individually. 
An important rule is that if any time windows are specified, then all operands must cover the same number of samples, though the time windows may differ 
individually. If no time windows or sample counts are given, then **pmseries** will return a series identifier (SID) instead of a series of timestamps and 
values. This SID may be used in subsequent ``/series/values?series= SID`` REST API calls, along with a specific time window.

Arithmetic Operators
=======================

**pmseries** support addition, subtraction, division and multiplication on each value in the time series of a binary pair of operands. No unary or ternary 
operators are supported (yet). In all cases, the instance domain and the number of samples of time series operands must be the same. The metadata 
(units and dimensions) must also be compatible. Depending on the function, the result will usually have the same instance domain and (unless noted 
otherwise), the same units as the operands. The metadata dimensions (space, time, count) of the result may differ (see below).

Expression operands may have different qualifiers, e.g. you can perform binary arithmetic on metrics qualified by different labels (such as **hostname**), 
or metric names. For example, to add the two most recent samples of the process context switch (pswitch) counter metric for hosts **node88** and **node89**, 
and then perform rate conversion:

.. sourcecode:: none

 $ pmseries 'rate(kernel.all.pswitch{hostname:node88}[count:2] + 
                  kernel.all.pswitch{hostname:node89}[count:2])'
 1cf1a85d5978640ef94c68264d3ae8866cc11f7c
    [Tue Nov 10 14:39:48.771868000 2020] 71.257509 8e0a59304eb99237b89593a3e839b5bb8b9a9924

Note the resulting time series of values has one less sample than the expression operand passed to the **rate** function.

Other rules for arithmetic expressions:

1. If both operands have the semantics of a counter, then only addition and subtraction are allowed.
2. If the left operand is a counter and the right operand is not, then only multiplication or division are allowed
3. If the left operand is not a counter and the right operand is a counter, then only multiplication is allowed.
4. Addition and subtraction - the dimensions of the result are the same as the dimensions of the operands.
5. Multiplication - the dimensions of the result are the sum of the dimensions of the operands.
6. Division - the dimensions of the result are the difference of the dimensions of the operands.

Functions
===========

Expression functions operate on vectors of time series values, and may be nested with other functions or expressions as described above. When an operand 
has multiple instances, there will generally be one result for each series of instances. For example, the result for

.. sourcecode:: none

 $ pmseries 'min(kernel.all.load[count:100])'

will be the smallest value of the 100 most recent samples, treating each of the three load average instances as a separate time series. As an example, 
for the two most recent samples for each of the three instances of the load average metric:

.. sourcecode:: none

 $ pmseries 'kernel.all.load[count:2]'
 726a325c4c1ba4339ecffcdebd240f441ea77848
     [Tue Nov 10 11:52:30.833379000 2020] 1.100000e+00 a7c96e5e2e0431a12279756d11590fa9fed8f306
     [Tue Nov 10 11:52:30.833379000 2020] 9.900000e-01 ee9b506935fd0976a893dc27242926f49326b9a1
     [Tue Nov 10 11:52:30.833379000 2020] 1.070000e+00 d5e1c360d13064c461169091997e1e8be7488133
     [Tue Nov 10 11:52:20.827134000 2020] 1.120000e+00 a7c96e5e2e0431a12279756d11590fa9fed8f306
     [Tue Nov 10 11:52:20.827134000 2020] 9.900000e-01 ee9b506935fd0976a893dc27242926f49326b9a1
     [Tue Nov 10 11:52:20.827134000 2020] 1.070000e+00 d5e1c360d13064c461169091997e1e8be7488133

Using the **min** function :

.. sourcecode:: none

 $ pmseries 'min(kernel.all.load[count:2])'
 11b965bc5f9598034ed9139fb3a78c6c0b7065ba
     [Tue Nov 10 11:52:30.833379000 2020] 1.100000e+00 a7c96e5e2e0431a12279756d11590fa9fed8f306
     [Tue Nov 10 11:52:30.833379000 2020] 9.900000e-01 ee9b506935fd0976a893dc27242926f49326b9a1
     [Tue Nov 10 11:52:30.833379000 2020] 1.070000e+00 d5e1c360d13064c461169091997e1e8be7488133

For singular metrics (with no instance domain), a single value will result, e.g. for the five most recent samples of the context switching metric:

.. sourcecode:: none

 $ pmseries 'kernel.all.pswitch[count:5]'
 d7832c4fba33bcc980b1a1b614e0508043288480
     [Tue Nov 10 12:44:59.380666000 2020] 460774294
     [Tue Nov 10 12:44:49.382070000 2020] 460747232
     [Tue Nov 10 12:44:39.378545000 2020] 460722370
     [Tue Nov 10 12:44:29.379029000 2020] 460697388
     [Tue Nov 10 12:44:19.379096000 2020] 460657412

 $ pmseries 'min(kernel.all.pswitch[count:5])'
 1b6e92fb5bc012372f54452734dd03f0f131fa06
     [Tue Nov 10 12:44:19.379096000 2020] 460657412 d7832c4fba33bcc980b1a1b614e0508043288480


Future versions of **pmseries** may provide functions that perform aggregation, interpolation, filtering or transforms in other ways, e.g. across instances 
instead of time.

Function Reference
=====================

* **max** (*expr*) : The maximum value in the time series for each instance of *expr*.

* **min** (*expr*) : The minimum value in the time series for each instance of *expr*.

* **rate** (*expr*) : The rate with respect to time of each sample. The given *expr* must have counter semantics and the result will have **instant** semantics 
  (the time dimension reduced by one). In addition, the result will have one less sample than the operand - this is because the first sample cannot be 
  rate converted (two samples are required).

* **rescale** (*expr* , *scale*) rescale the values in the time series for each instance of *expr* to scale (units). Note that *expr* should have **instant** 
  or **discrete** semantics (not **counter** - rate conversion should be done first if needed). The time, space and count dimensions between *expr* and 
  *scale* must be compatible. Example: rate convert the read throughput counter for each disk instance and then rescale to mbytes per second. Note the 
  native units of **disk.dev.read_bytes** is a **counter** of kbytes read from each device instance since boot.

     .. sourcecode:: none

         $ pmseries 'rescale(rate(disk.dev.read_bytes[count:4]), "mbytes/s")'

* **abs** (*expr*) : The absolute value of each value in the time series for each instance of *expr* . This has no effect if the type of *expr* is unsigned.

* **floor** (*expr*) : Rounded down to the nearest integer value of the time series for each instance of *expr*.

* **round** (*expr*) : Rounded up or down to the nearest integer for each value in the time series for each instance of *expr*.

* **log** (*expr*) : Logarithm of the values in the time series for each instance of *expr*.

* **sqrt** (*expr*) : Square root of the values in the time series for each instance of *expr*.

Compatibility
==============

All operands in an expression must have the same number of samples, but not necessarily the same time window. e.g. you could subtract some metric time 
series from today from that of yesterday by giving different time windows and different metrics or qualifiers, ensuring the same number of samples are 
given as the operands.

Operands in an expression must either all have a time window, or none. If no operands have a time window, then instead of a series of time stamps and 
values, the result will be a time series identifier (*SID*) that may be passed to the ``/series/values?series= SID`` REST API function, along with a 
time window. For further details, see `PMWEBAPI(3) <https://pcp.readthedocs.io/en/latest/api/>`_.

If the semantics of both operands in an arithmetic expression are not counter (i.e. **PM_SEM_INSTANT** or **PM_SEM_DISCRETE**) then the result will have 
semantics **PM_SEM_INSTANT** unless both operands are **PM_SEM_DISCRETE** in which case the result is also **PM_SEM_DISCRETE**.

Timeseries Options
*********************

Timeseries Metadata
=====================

Using command line options, **pmseries** can be requested to provide metadata (metric names, instance names, labels, descriptors) associated with either 
individual timeseries or a group of timeseries, for example:

.. sourcecode:: none

 $ pmseries -a dcb2a032a308b5717bf605ba8f8737e9c6e1ed19

 dcb2a032a308b5717bf605ba8f8737e9c6e1ed19
     PMID: 60.0.21
     Data Type: 64-bit unsigned int  InDom: PM_INDOM_NULL 0xffffffff
     Semantics: counter  Units: millisec
     Source: f5ca7481da8c038325d15612bb1c6473ce1ef16f
     Metric: kernel.all.cpu.nice
     labels {"agent":"linux","domainname":"localdomain",\
             "groupid":1000,"hostname":"shard",\
             "latitude":-25.28496,"longitude":152.87886,\
             "machineid":"295b16e3b6074cc8bdbda8bf96f6930a",\
             "userid":1000}

The complete set of **pmseries** metadata reporting options are:

========================================== ===============================================================================================================================
options                                    Description
========================================== ===============================================================================================================================
**-a** , **--all**                         | Convenience option to report all metadata for the given timeseries, equivalent to **-dilms**.
**-d** , **--desc**                        | Metric descriptions detailing the PMID, data type, data semantics, units, scale 
                                           | and associated instance domain. This option has a direct pminfo(1) equivalent.
**-g** *pattern* , **--glob** = *pattern*  | Provide a glob(7) pattern to restrict the report provided by the **-i** , **-l** , **-m** and **-S**.
**-i** , **--instances**                   | Metric descriptions detailing the PMID, data type, data semantics, units, scale and associated instance domain.
**-I** , **--fullindom**                   | Print the InDom in verbose mode. This option has a direct pminfo(1) equivalent.
**-l** , **--labels**                      | Print label sets associated with metrics and instances. Labels are optional 
                                           | metric metadata described in detail in pmLookupLabels(3). This option has a 
                                           | direct pminfo(1) equivalent.
**-m** , **--metrics**                     | Print metric names.
**-M** , **--fullpmid**                    | Print the PMID in verbose mode. This option has a direct pminfo(1) equivalent.
**-n** , **--names**                       | Print comma-separated label names only (not values) for the labels associated with metrics and instances.
**-s** , **--series**                      | Print timeseries identifiers associated with metrics, instances and sources. 
                                           | These unique identifiers are calculated from intrinsic (non-optional) 
                                           | labels and other metric metadata associated with each PMAPI context 
                                           | (sources), metrics and instances. Archive, local context or pmcd(1) 
                                           | connections for the same host all produce the same source identifier. 
                                           | This option has a direct pminfo(1) equivalent. See also pmLookupLabels(3) 
                                           | and the **-l/--labels** option.
========================================== ===============================================================================================================================

*References* : `pminfo(1) <https://pcp.io/man/man1/pminfo.1.html>`_ , `glob(7) <https://man7.org/linux/man-pages/man7/glob.7.html>`_ , `pmLookupLabels(3) <https://man7.org/linux/man-pages/man3/pmLookupLabels.3.html>`_ , `pmcd(1) <https://man7.org/linux/man-pages/man1/pmcd.1.html>`_

Timeseries Sources
====================

A source is a unique identifier (represented externally as a 40-byte hexadecimal SHA-1 hash) that represents both the live host and/or archives from 
which each timeseries originated. The context for a source identifier (obtained with **-s** ) can be reported with:

**-S** , **--sources** : Print names for timeseries sources. These names are either hostnames or fully qualified archive paths.

It is important to note that live and archived sources can and will generate the same SHA-1 source identifier hash, provided that the context labels 
remain the same for that host (labels are stored in PCP archives and can also be fetched live from `pmcd(1) <https://man7.org/linux/man-pages/man1/pmcd.1.html>`_ ).

Timeseries Loading
=====================

Timeseries metadata and data are loaded either automatically by a local `pmproxy(1) <https://man7.org/linux/man-pages/man1/pmproxy.1.html>`_, or manually using a 
specially crafted **pmseries** query and the **-L**/ **--load** option:

.. sourcecode:: none

 $ pmseries --load "{source.path: \"$PCP_LOG_DIR/pmlogger/acme\"}"
 pmseries: [Info] processed 2275 archive records from [...]

This query must specify a source archive path, but can also restrict the import to specific timeseries (using metric names, labels, etc) and to a specific 
time window using the time specification component of the query language.

As a convenience, if the argument to load is a valid file path as determined by `access(2) <https://man7.org/linux/man-pages/man2/access.2.html>`_, then 
a short-hand form can be used:

.. sourcecode:: none

 $ pmseries --load $PCP_LOG_DIR/pmlogger/acme.0

Options
=========

The available command line options, in addition to timeseries metadata and sources options described above, are:

=============================================== =====================================================================================================================
options                                         Description
=============================================== =====================================================================================================================
**-c** *config* , **--config** = *config*       | Specify the *config* file to use.
**-h** *host* , **--host** = *host*             | Connect key server at *host*, rather than the one the localhost.
**-L** , **--load**                             | Load timeseries metadata and data into the key server.
**-p** *port* , **--port** = *port*             | Connect key server at *port*, rather than the default **6379** .
**-q** , **--query**                            | Perform a timeseries query. This is the default action.
**-t** , **--times**                            | Report time stamps numerically (in milliseconds) instead of the default human readable form.
**-v** , **--values**                           | Report all of the known values for given *label* name(s).
**-V** , **--version**                          | Display version number and exit.
**-Z** *timezone* , **--timezone** = *timezone* | Use timezone for the date and time. Timezone is in the format of the 
                                                | environment variable TZ as described in `environ(7) <https://man7.org/linux/man-pages/man7/environ.7.html>`_.
**-?** , **--help**                             | Display usage message and exit.
=============================================== =====================================================================================================================

Examples
==========

The following sample query shows several fundamental aspects of the **pmseries** query language:

.. sourcecode:: none

 $ pmseries 'kernel.all.load{hostname:"toium"}[count:2]'

 eb713a9cf472f775aa59ae90c43cd7f960f7870f
     [Thu Nov 14 05:57:06.082861000 2019] 1.0e-01 b84040ffccd54f839b65140cf139bab51cbbcf62
     [Thu Nov 14 05:57:06.082861000 2019] 6.8e-01 a60b5b3bf25e71071c41934fa4d7d251f765f30c
     [Thu Nov 14 05:57:06.082861000 2019] 6.4e-01 e1974a062375e6e62370ffadf5b0650dad739480
     [Thu Nov 14 05:57:16.091546000 2019] 1.6e-01 b84040ffccd54f839b65140cf139bab51cbbcf62
     [Thu Nov 14 05:57:16.091546000 2019] 6.7e-01 a60b5b3bf25e71071c41934fa4d7d251f765f30c
     [Thu Nov 14 05:57:16.091546000 2019] 6.4e-01 e1974a062375e6e62370ffadf5b0650dad739480

This query returns the two most recent values for all instances of the **kernel.all.load** metric with a *label.hostname* matching the regular expression 
"toium". This is a set-valued metric (i.e., a metric with an "instance domain" which in this case consists of three instances: 1, 5 and 15 minute averages). 
The first column returned is a timestamp, then a floating point value, and finally an instance identifier timeseries hash (two values returned for three 
instances, so six rows are returned). The metadata for these timeseries can then be further examined:

.. sourcecode:: none

 $ pmseries -a eb713a9cf472f775aa59ae90c43cd7f960f7870f

 eb713a9cf472f775aa59ae90c43cd7f960f7870f
     PMID: 60.2.0
     Data Type: float  InDom: 60.2 0xf000002
     Semantics: instant  Units: none
     Source: 0e89c1192db79326900d82131c31399524f0b3ee
     Metric: kernel.all.load
     inst [1 or "1 minute"] series b84040ffccd54f839b65140cf139bab51cbbcf62
     inst [5 or "5 minute"] series a60b5b3bf25e71071c41934fa4d7d251f765f30c
     inst [15 or "15 minute"] series e1974a062375e6e62370ffadf5b0650dad739480
     inst [1 or "1 minute"] labels {"agent":"linux","hostname":"toium"}
     inst [5 or "5 minute"] labels {"agent":"linux","hostname":"toium"}
     inst [15 or "15 minute"] labels {"agent":"linux","hostname":"toium"}

PCP Environment
******************

Environment variables with the prefix **PCP_** are used to parameterize the file and directory names used by PCP. On each installation, the file 
*/etc/pcp.conf* contains the local values for these variables. The ``$PCP_CONF`` variable may be used to specify an alternative configuration file, as 
described in `pcp.conf(5) <https://man7.org/linux/man-pages/man5/pcp.conf.5.html>`_.

For environment variables affecting PCP tools, see `pmGetOptions(3) <https://man7.org/linux/man-pages/man3/pmGetOptions.3.html>`_.

PCP Grafana Plugin
********************

The PCP Key Server Grafana datasource from the PCP Grafana plugin queries the fast, scalable time series capabilities provided by the **pmseries** functionality. It is intended to query historical data 
across multiple hosts and supports filtering based on labels. This data source also provides a native interface between `Grafana <https://grafana.com/>`_ and 
`Performance Co-Pilot <https://pcp.io>`_ (PCP), allowing PCP metric data to be presented in Grafana panels, such as graphs, tables, heatmaps, etc. Under the hood, 
the data source makes REST API query requests to the PCP `pmproxy(1) <https://man7.org/linux/man-pages/man1/pmproxy.1.html>`_ service, which can be running either 
locally or on a remote host. The pmproxy daemon can be local or remote and uses a key-value server (local or remote) for persistent storage. 

For more information on PCP Grafana Plugin, visit `PCP Grafana Plugin Documentation <https://grafana-pcp.readthedocs.io/en/latest/index.html#>`_ .
