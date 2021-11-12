.. _RecordMetricsOnLocalSystem:

Record metrics on my local system
#############################################

1. Start pmcd and pmlogger:

.. code-block:: bash

   chkconfig pmcd on
   chkconfig pmlogger on

   ${PCP_RC_DIR}/pmcd start
   Starting pmcd ...
   
   ${PCP_RC_DIR}/pmlogger start
   Starting pmlogger ...

2. Verify that the primary pmlogger instance is running:

.. code-block:: bash

   pcp

   Performance Co-Pilot configuration:

   platform: Linux pluto 3.10.0-0.rc7.64.el7.x86_64 #1 SMP
   hardware: 8 cpus, 2 disks, 23960MB RAM
   timezone: EST-10
      pmcd: Version 4.0.0-1, 8 agents
      pmda: pmcd proc xfs linux mmv infiniband gluster elasticsearch
      pmlogger: primary logger: pluto/20170815.10.00
      pmie: pluto: ${PCP_LOG_DIR}/pmie/pluto/pmie.log
            venus: ${PCP_LOG_DIR}/pmie/venus/pmie.log

3. Verify that the archive files are being created in the expected place:

.. code-block:: bash

   ls ${PCP_LOG_DIR}/pmlogger/pluto

   20170815.10.00.0
   20170815.10.00.index
   20170815.10.00.meta
   Latest
   pmlogger.log
