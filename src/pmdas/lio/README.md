# pcp-pmda-lio
Performance Co-Pilot PMDA for Linux LIO subsystem

Performance Co-Pilot (PCP), provides a really easy mechanism to gather, monitor and report on system and subsystem performance metrics.
The Linux LIO subsystem provides a linux I/O target subsystem for protocols like iSCSI, FCP, FCoE etc allowing storage available on one host to be exported and consumed by other hosts using industry standard storage protocols.

This project provides a PMDA (Performance Metric Domain Agent) to allow the administrator some insight into the performance of the LIO subsystem. The PMDA provides two distinct views on performance;  

**lio.summary** .... provides a roll-up of performance and configuration per LIO target instance  
**lio.lun** ........ provides per LUN performance information including (IOPS, READ MB and WRITE MB)  

*NB. the statistics provided are constrained to those metrics populated by the LIO subsystem*

## Pre-requisites
The LIO configuration is maintained within the kernel's configfs virtual filesystem. The rtslib library provides the interface to configfs, allowing other tools to interact with the settings and metadata held in configfs. pcp-pmda-lio uses the python-rtslib package to extract lun statistics.


##Installation
1. install the PMDA
```
cd /var/lib/pcp/pmdas/lio
./Install
```

Any errors will be shown in the /var/log/pcp/pmcd/lio.log file  

## Removal
To remove the PMDA run the 'Remove' script in the /var/lib/pcp/pmdas/lio directory  
```
>./Remove
```

##Available Metrics
###lio.summary
Per LIO target Instance

| field name | Description |
| ---------- | ----------- |
| total_iops | Sum of IOPS across all LUNs exported by the target |
| total_read_mb | Sum of Read MB across all LUNs (becomes MB/s within pmchart) |
| total_write_mb | Sum of Write MB across all LUNs |
| total_clients | Number of clients (NodeACLs) defined to the target |
| total_size | Sum of the capacity from each LUN (GB) |
| total_luns | Number of LUNs defined |
| active_sessions | Count of the number of clients actively logged into the target |
| tpgs | Number of TPGs defined to the target |

###lio.lun
Per LUN (defined to and exported by the target instance)

| field name | Description |
| ---------- | ----------- |
| iops | IOPS (read + write) seen by the LUN |
| read_mb | Read MB at the the LUN, pmchart can shows this as MB/s |
| write_mb | Write MB at the LUN |
| size | LUN size (GB) |
  
