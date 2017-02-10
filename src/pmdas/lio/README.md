# pcp-pmda-lio
Performance Co-Pilot PMDA for Linux LIO subsystem

Performance Co-Pilot (PCP), provides a really easy mechanism to gather, monitor and report on system and subsystem performance metrics.
The Linux LIO subsystem provides a linux I/O target subsystem for protocols like iSCSI, FCP, FCoE etc allowing storage available on one host to be exported and consumed by other hosts using industry standard storage protocols.

This project provides a PMDA (Performance Metric Domain Agent) to allow the administrator some insight into the performance of the LIO subsystem. The PMDA provides two distinct views on performance;  

**lio.summary** .... provides a roll-up of performance and configuration per LIO target instance  
**lio.lun** ........ provides per LUN performance information including (IOPS, READ MB and WRITE MB)  

*NB. the statistics provided are constrained to those metrics populated by the LIO subsystem*

## Pre-requisites
The LIO configuration is maintained within the kernel's configfs virtual filesystem. The python-rtslib library provides the interface to configfs, allowing other tools to interact with the settings and metadata held in configfs. pcp-pmda-lio uses the python-rtslib package to extract lun statistics, so in order to run the pmda you'll also need

python-rtslib (tested against RHEL7 with python-rtslib-2.1.fb57-5.el7.noarch)

**NB.** *For python3 environments like Fedora25, pmpython invokes python3.x so you'll need to have python3-rtslib installed*

On systems with SELINUX enabled, you may also find that SELINUX prevents the pmda from being automatically launched by pmcd. In this scenario the pmcd.log file will show something like;  

```  
pmcd: unexpected end-of-file at initial exchange with lio PMDA 
```  
To resolve, either turn off SELINUX (Yeah right!) or create a local policy to allow the pmda access to configfs. An example TE file is included in the repo but it is recommended to build your own TE from running audit2allow against your own system. Information about building local policies can be found on the following Centos Wiki page;

https://wiki.centos.org/HowTos/SELinux#head-d8db97e538d95b1bc5e54fc5a9ddb0c953e9a969


##Installation
1. untar/unzip the archive into ```/var/lib/pcp/pmdas```  
2. install the pmda  
```
cd lio
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
  
