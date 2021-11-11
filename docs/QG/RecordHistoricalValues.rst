.. _RecordHistoricalValues:

Record historical values for use with the pcp-dstat tool
###########################################################

* *pcp-dstat* is a general performance analysis tool to view multiple system resources instantly, for example - we can compare disk usage in combination with interrupts from a disk controller, or compare the network bandwidth numbers directly with the disk throughput (in the same interval).

* It also cleverly gives the most detailed information in columns and clearly indicates in what magnitude and unit the output is being displayed. Less confusion, fewer mistakes, more efficient.

* The latest generation of Dstat, *pcp-dstat*, allows for analysis of historical performance data (in the PCP archive format created by *pmlogger*(1)), as well as distributed systems analysis of live performance data from remote hosts running the *pmcd*(1) process).

1. To list all available plugin names:

.. code-block:: bash

   pcp dstat --list

2. To relate disk-throughput with network-usage (eth0), total CPU-usage and system counters:

.. code-block:: bash

    $ pcp dstat -dnyc -N eth0 -C total -f 5

3. To report 10 samples from metrics recorded in a PCP archive 20180729 from 2:30 AM:

.. code-block:: bash
    
    $ pcp --origin '@02:30' -a 20180729 dstat --time --cpu-adv --sys 1 10


We can also examine the same metrics live from a remote host:
    
.. code-block:: bash

    $ pcp --host www.acme.com dstat --time --cpu-adv --sys 1 10