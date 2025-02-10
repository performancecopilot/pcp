.. _MonitoringSystemPerformance:

Monitoring System Performance
#############################

.. contents::

This chapter describes the performance monitoring tools available in Performance Co-Pilot (PCP). This product provides a group of commands and tools 
for measuring system performance. Each tool is described completely by its own man page. The man pages are accessible through the **man** command. 
For example, the man page for the tool **pmrep** is viewed by entering the following command::
 
 man pmrep

The following major sections are covered in this chapter:

Section 4.1, “`The pmstat Command`_”, discusses **pmstat**, a utility that provides a periodic one-line summary of system performance.

Section 4.2, “`The pmrep Command`_”, discusses **pmrep**, a utility that shows the current values for named performance metrics.

Section 4.3, “`The pmval Command`_”, describes **pmval**, a utility that displays performance metrics in a textual format.

Section 4.4, “`The pminfo Command`_”, describes **pminfo**, a utility that displays information about performance metrics.

Section 4.5, “`The pmstore Command`_”, describes the use of the **pmstore** utility to arbitrarily set or reset selected performance metric values.

The following sections describe the various graphical and text-based PCP tools used to monitor local or remote system performance.

The pmstat Command
******************

The **pmstat** command provides a periodic, one-line summary of system performance. This command is intended to monitor system performance at the highest 
level, after which other tools may be used for examining subsystems to observe potential performance problems in greater detail. After entering the 
**pmstat** command, you see output similar to the following, with successive lines appearing periodically:

.. sourcecode:: none

 pmstat
 @ Thu Aug 15 09:25:56 2017
  loadavg                      memory      swap        io    system         cpu
    1 min   swpd   free   buff  cache   pi   po   bi   bo   in   cs  us  sy  id
     1.29 833960  5614m 144744 265824    0    0    0 1664  13K  23K   6   7  81
     1.51 833956  5607m 144744 265712    0    0    0 1664  13K  24K   5   7  83
     1.55 833956  5595m 145196 271908    0    0  14K 1056  13K  24K   7   7  74
     
An additional line of output is added every five seconds. The **-t** *interval* option may be used to vary the update interval (i.e. the sampling interval).

The output from **pmstat** is directed to standard output, and the columns in the report are interpreted as follows:

**loadavg**

The 1-minute load average (runnable processes).

**memory**

The swpd column indicates average swap space used during the interval (all columns reported in Kbytes unless otherwise indicated). The **free** 
column indicates average free memory during the interval. The **buff** column indicates average buffer memory in use during the interval. The **cache** 
column indicates average cached memory in use during the interval.

**swap**

Reports the average number of pages that are paged-in (**pi**) and paged-out (**po**) per second during the interval. It is normal for the paged-in values 
to be non-zero, but the system is suffering memory stress if the paged-out values are non-zero over an extended period.

**io**

The **bi** and **bo** columns indicate the average rate per second of block input and block output operations respectfully, during the interval. 
These rates are independent of the I/O block size. If the values become large, they are reported as thousands of operations per second (K suffix) 
or millions of operations per second (M suffix).

**system**

Context switch rate (**cs**) and interrupt rate (**in**). Rates are expressed as average operations per second during the interval. Note that the 
interrupt rate is normally at least HZ (the clock interrupt rate, and **kernel.all.hz** metric) interrupts per second.

**cpu**

Percentage of CPU time spent executing user code (**us**), system and interrupt code (**sy**), idle loop (**id**).

As with most PCP utilities, real-time metric, and archives are interchangeable.

For example, the following command uses a local system PCP archive *20170731* and the timezone of the host (**smash**) from which performance metrics 
in the archive were collected:

.. sourcecode:: none

 pmstat -a ${PCP_LOG_DIR}/pmlogger/smash/20170731 -t 2hour -A 1hour -z
 Note: timezone set to local timezone of host "smash"
 @ Wed Jul 31 10:00:00 2017
  loadavg                      memory      swap        io    system         cpu
    1 min   swpd   free   buff  cache   pi   po   bi   bo   in   cs  us  sy  id
     3.90  24648  6234m 239176  2913m    ?    ?    ?    ?    ?    ?   ?   ?   ?
     1.72  24648  5273m 239320  2921m    0    0    4   86  11K  19K   5   5  84
     3.12  24648  5194m 241428  2969m    0    0    0   84  10K  19K   5   5  85
     1.97  24644  4945m 244004  3146m    0    0    0   84  10K  19K   5   5  84
     3.82  24640  4908m 244116  3147m    0    0    0   83  10K  18K   5   5  85
     3.38  24620  4860m 244116  3148m    0    0    0   83  10K  18K   5   4  85
     2.89  24600  4804m 244120  3149m    0    0    0   83  10K  18K   5   4  85
 pmFetch: End of PCP archive

For complete information on **pmstat** usage and command line options, see the **pmstat(1)** man page.

The pmrep Command
******************

The **pmrep** command displays performance metrics in ASCII tables, suitable for export into databases or report generators. It is a flexible command. 
For example, the following command provides continuous memory statistics on a host named **surf**:

.. sourcecode:: none

 pmrep -p -h surf kernel.all.load kernel.all.pswitch
           k.a.load  k.a.load  k.a.load  k.a.pswitch
           1 minute  5 minute  15 minut             
                                            count/s
 10:41:37     0.160     0.170     0.180          N/A
 10:41:38     0.160     0.170     0.180     1427.016
 10:41:39     0.160     0.170     0.180     2129.040
 10:41:40     0.160     0.170     0.180     5335.163
 10:41:41     0.160     0.170     0.180      723.125
 10:41:42     0.140     0.160     0.180      591.859

See the **pmrep(1)** man page for more information.

The pmval Command
******************

The **pmval** command dumps the current values for the named performance metrics. For example, the following command reports the value of performance 
metric **proc.nprocs** once per second (by default), and produces output similar to this:

.. sourcecode:: none

 pmval proc.nprocs
 metric:    proc.nprocs
 host:      localhost
 semantics: instantaneous value
 units:     none
 samples:   all
 interval:  1.00 sec
          81
          81
          82
          81

In this example, the number of running processes was reported once per second.

Where the semantics of the underlying performance metrics indicate that it would be sensible, **pmval** reports the rate of change or resource utilization.

For example, the following command reports idle processor utilization for each of four CPUs on the remote host **dove**, each five seconds apart, 
producing output of this form:

.. sourcecode:: none

 pmval -h dove -t 5sec -s 4 kernel.percpu.cpu.idle
 metric:    kernel.percpu.cpu.idle
 host:      dove
 semantics: cumulative counter (converting to rate)
 units:     millisec (converting to time utilization)
 samples:   4
 interval:  5.00 sec

 cpu:1.1.0.a cpu:1.1.0.c cpu:1.1.1.a cpu:1.1.1.c 
      1.000       0.9998      0.9998      1.000  
      1.000       0.9998      0.9998      1.000  
      0.8989      0.9987      0.9997      0.9995 
      0.9568      0.9998      0.9996      1.000

Similarly, the following command reports disk I/O read rate every minute for just the disk **/dev/disk1**, and produces output similar to the following:

.. sourcecode:: none

 pmval -t 1min -i disk1 disk.dev.read
 metric:    disk.dev.read
 host:      localhost
 semantics: cumulative counter (converting to rate)
 units:     count (converting to count / sec)
 samples:   indefinite
 interval:  60.00 sec
         disk1 
          33.67 
          48.71 
          52.33 
          11.33 
          2.333

The **-r** flag may be used to suppress the rate calculation (for metrics with counter semantics) and display the raw values of the metrics.

In the example below, manipulation of the time within the archive is achieved by the exchange of time control messages between **pmval** and **pmtime**.
::

 pmval -g -a ${PCP_LOG_DIR}/pmlogger/myserver/20170731 kernel.all.load

The **pmval** command is documented by the **pmval(1)** man page, and annotated examples of the use of **pmval** can be found in the *PCP Tutorials and Case Studies* 
companion document.

The pminfo Command
*******************

The **pminfo** command displays various types of information about performance metrics available through the Performance Co-Pilot (PCP) facilities.

The **-T** option is extremely useful; it provides help text about performance metrics:

.. sourcecode:: none

 pminfo -T mem.util.cached
 mem.util.cached
 Help:
 Memory used by the page cache, including buffered file data.
 This is in-memory cache for files read from the disk (the pagecache)
 but doesn't include SwapCached.

The **-t** option displays the one-line help text associated with the selected metrics. The **-T** option prints more verbose help text.

Without any options, **pminfo** verifies that the specified metrics exist in the namespace, and echoes those names. Metrics may be specified as arguments 
to **pminfo** using their full metric names. For example, this command returns the following response::

 pminfo hinv.ncpu network.interface.total.bytes
 hinv.ncpu 
 network.interface.total.bytes

A group of related metrics in the namespace may also be specified. For example, to list all of the **hinv** metrics you would use this command::

 pminfo hinv
 hinv.physmem
 hinv.pagesize
 hinv.ncpu
 hinv.ndisk
 hinv.nfilesys
 hinv.ninterface
 hinv.nnode
 hinv.machine
 hinv.map.scsi
 hinv.map.cpu_num
 hinv.map.cpu_node
 hinv.map.lvname
 hinv.cpu.clock
 hinv.cpu.vendor
 hinv.cpu.model
 hinv.cpu.stepping
 hinv.cpu.cache
 hinv.cpu.bogomips

If no metrics are specified, **pminfo** displays the entire collection of metrics. This can be useful for searching for metrics, when only part of the 
full name is known. For example, this command returns the following response::

 pminfo | grep nfs
 nfs.client.calls
 nfs.client.reqs
 nfs.server.calls
 nfs.server.reqs
 nfs3.client.calls
 nfs3.client.reqs
 nfs3.server.calls
 nfs3.server.reqs
 nfs4.client.calls
 nfs4.client.reqs
 nfs4.server.calls
 nfs4.server.reqs

The **-d** option causes **pminfo** to display descriptive information about metrics (refer to the **pmLookupDesc(3)** man page for an explanation of this metadata information). 
The following command and response show use of the **-d** option:

.. sourcecode:: none

 pminfo -d proc.nprocs disk.dev.read filesys.free
 proc.nprocs
     Data Type: 32-bit unsigned int  InDom: PM_INDOM_NULL 0xffffffff
     Semantics: instant  Units: none

 disk.dev.read
     Data Type: 32-bit unsigned int  InDom: 60.1 0xf000001
     Semantics: counter  Units: count

 filesys.free
     Data Type: 64-bit unsigned int  InDom: 60.5 0xf000005
     Semantics: instant  Units: Kbyte

The **-l** option causes **pminfo** to display labels about metrics (refer to the **pmLookupLabels(3)** man page for an explanation of this metadata 
information). If the metric has an instance domain, the labels associated with each instance of the metric is printed. The following command and 
response show use of the **-l** option:

.. sourcecode:: none
 
 pminfo -l -h shard kernel.pernode.cpu.user
 kernel.percpu.cpu.sys
     inst [0 or "cpu0"] labels 
 {"agent":"linux","cpu":0,"device_type":"cpu","domainname":"acme.com","groupid":1000,"hostname":"shard","indom_name":"per cpu","userid":1000}
     inst [1 or "cpu1"] labels 
 {"agent":"linux","cpu":1,"device_type":"cpu","domainname":"acme.com","groupid":1000,"hostname":"shard","indom_name":"per cpu","userid":1000}
     inst [2 or "cpu2"] labels 
 {"agent":"linux","cpu":2,"device_type":"cpu","domainname":"acme.com","groupid":1000,"hostname":"shard","indom_name":"per cpu","userid":1000}
     inst [3 or "cpu3"] labels 
 {"agent":"linux","cpu":3,"device_type":"cpu","domainname":"acme.com","groupid":1000,"hostname":"shard","indom_name":"per cpu","userid":1000}
     inst [4 or "cpu4"] labels 
 {"agent":"linux","cpu":4,"device_type":"cpu","domainname":"acme.com","groupid":1000,"hostname":"shard","indom_name":"per cpu","userid":1000}
     inst [5 or "cpu5"] labels 
 {"agent":"linux","cpu":5,"device_type":"cpu","domainname":"acme.com","groupid":1000,"hostname":"shard","indom_name":"per cpu","userid":1000}
     inst [6 or "cpu6"] labels 
 {"agent":"linux","cpu":6,"device_type":"cpu","domainname":"acme.com","groupid":1000,"hostname":"shard","indom_name":"per cpu","userid":1000}
     inst [7 or "cpu7"] labels 
 {"agent":"linux","cpu":7,"device_type":"cpu","domainname":"acme.com","groupid":1000,"hostname":"shard","indom_name":"per cpu","userid":1000}

The **-f** option to **pminfo** forces the current value of each named metric to be fetched and printed. In the example below, all metrics in the group **hinv** 
are selected:

.. sourcecode:: none

 pminfo -f hinv
 hinv.physmem
     value 15701

 hinv.pagesize
     value 16384

 hinv.ncpu
     value 4

 hinv.ndisk
     value 6

 hinv.nfilesys
     value 2

 hinv.ninterface
     value 8

 hinv.nnode
     value 2

 hinv.machine
     value "IP35"

 hinv.map.cpu_num
     inst [0 or "cpu:1.1.0.a"] value 0
     inst [1 or "cpu:1.1.0.c"] value 1
     inst [2 or "cpu:1.1.1.a"] value 2
     inst [3 or "cpu:1.1.1.c"] value 3

 hinv.map.cpu_node
     inst [0 or "node:1.1.0"] value "/dev/hw/module/001c01/slab/0/node"
     inst [1 or "node:1.1.1"] value "/dev/hw/module/001c01/slab/1/node"

 hinv.cpu.clock
     inst [0 or "cpu:1.1.0.a"] value 800
     inst [1 or "cpu:1.1.0.c"] value 800
     inst [2 or "cpu:1.1.1.a"] value 800
     inst [3 or "cpu:1.1.1.c"] value 800

 hinv.cpu.vendor
     inst [0 or "cpu:1.1.0.a"] value "GenuineIntel"
     inst [1 or "cpu:1.1.0.c"] value "GenuineIntel"
     inst [2 or "cpu:1.1.1.a"] value "GenuineIntel"
     inst [3 or "cpu:1.1.1.c"] value "GenuineIntel"

 hinv.cpu.model
     inst [0 or "cpu:1.1.0.a"] value "0"
     inst [1 or "cpu:1.1.0.c"] value "0"
     inst [2 or "cpu:1.1.1.a"] value "0"
     inst [3 or "cpu:1.1.1.c"] value "0"

 hinv.cpu.stepping
     inst [0 or "cpu:1.1.0.a"] value "6"
     inst [1 or "cpu:1.1.0.c"] value "6"
     inst [2 or "cpu:1.1.1.a"] value "6"
     inst [3 or "cpu:1.1.1.c"] value "6"

 hinv.cpu.cache
     inst [0 or "cpu:1.1.0.a"] value 0
     inst [1 or "cpu:1.1.0.c"] value 0
     inst [2 or "cpu:1.1.1.a"] value 0
     inst [3 or "cpu:1.1.1.c"] value 0

 hinv.cpu.bogomips
     inst [0 or "cpu:1.1.0.a"] value 1195.37
     inst [1 or "cpu:1.1.0.c"] value 1195.37
     inst [2 or "cpu:1.1.1.a"] value 1195.37
     inst [3 or "cpu:1.1.1.c"] value 1195.37

The **-h** option directs **pminfo** to retrieve information from the specified host. If the metric has an instance domain, 
the value associated with each instance of the metric is printed:

.. sourcecode:: none

 pminfo -h dove -f filesys.mountdir
 filesys.mountdir
     inst [0 or "/dev/xscsi/pci00.01.0/target81/lun0/part3"] value "/"
     inst [1 or "/dev/xscsi/pci00.01.0/target81/lun0/part1"] value "/boot/efi"

The **-m** option prints the Performance Metric Identifiers (PMIDs) of the selected metrics. This is useful for finding out which PMDA supplies the metric. 
For example, the output below identifies the PMDA supporting domain 4 (the leftmost part of the PMID) as the one supplying information for the metric 
**environ.extrema.mintemp**::

 pminfo -m environ.extrema.mintemp 
 environ.extrema.mintemp PMID: 4.0.3

The **-v** option verifies that metric definitions in the PMNS correspond with supported metrics, and checks that a value is available for the metric. 
Descriptions and values are fetched, but not printed. Only errors are reported.

Complete information on the **pminfo** command is found in the **pminfo(1)** man page. There are further examples of the use of **pminfo** in the 
*PCP Tutorials and Case Studies*.

The pmstore Command
********************

From time to time you may wish to change the value of a particular metric. Some metrics are counters that may need to be reset, and some are simply 
control variables for agents that collect performance metrics. When you need to change the value of a metric for any reason, the command to use is **pmstore**.

.. note::

 For obvious reasons, the ability to arbitrarily change the value of a performance metric is not supported. Rather, PCP collectors selectively allow some metrics to be modified in a very controlled fashion.

The basic syntax of the command is as follows::

 pmstore metricname value 

There are also command line flags to further specify the action. For example, the **-i** option restricts the change to one or more instances of the 
performance metric.

The *value* may be in one of several forms, according to the following rules:

1. If the metric has an integer type, then value should consist of an optional leading hyphen, followed either by decimal digits or “0x” and some hexadecimal digits; “0X” is also acceptable instead of “0x.”

2. If the metric has a floating point type, then value should be in the form of an integer (described above), a fixed point number, or a number in scientific notation.

3. If the metric has a string type, then value is interpreted as a literal string of ASCII characters.

4. If the metric has an aggregate type, then an attempt is made to interpret value as an integer, a floating point number, or a string. In the first two cases, the minimal word length encoding is used; for example, “123” would be interpreted as a four-byte aggregate, and “0x100000000” would be interpreted as an eight-byte aggregate.

The following example illustrates the use of **pmstore** to enable performance metrics collection in the **txmon** PMDA (see ``${PCP_PMDAS_DIR}/txmon`` 
for the source code of the txmon PMDA). When the metric **txmon.control.level** has the value 0, no performance metrics are collected. 
Values greater than 0 enable progressively more verbose instrumentation.
::

 pminfo -f txmon.count
 txmon.count
 No value(s) available!
 pmstore txmon.control.level 1
 txmon.control.level old value=0 new value=1
 pminfo -f txmon.count
 txmon.count
        inst [0 or "ord-entry"] value 23
        inst [1 or "ord-enq"] value 11
        inst [2 or "ord-ship"] value 10
        inst [3 or "part-recv"] value 3
        inst [4 or "part-enq"] value 2
        inst [5 or "part-used"] value 1
        inst [6 or "b-o-m"] value 0

For complete information on **pmstore** usage and syntax, see the **pmstore(1)** man page.