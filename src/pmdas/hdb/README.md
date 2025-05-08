# pcp-pmda-hdb

pcp-pmda-hdb is a [Performance Co-Pilot (PCP)](https://pcp.io/) Performance Metric Domain Agent (PMDA) for [SAP HANA (hdb)](https://www.sap.com/products/hana.html).

> **:warning: WARNING: Alpha Software**  
> This project is in an early stage and should not be used in productive environments.

* [Installation](#installation)
* [Usage](#usage)
* [Features](#features)
* [Known Issues and Limitations](#known-issues-and-limitations)
* [Contributing](#contributing)
* [License](#license)

## Installation

### Dependencies
pcp-pmda-hdb connects to HANA via the [hdbcli](https://pypi.org/project/hdbcli/) Python module.
```bash
pip3 install hdbcli
dnf install -y pcp python3-pcp pcp-zeroconf
```

### Install
```bash
# get pcp-pmda-hdb
git clone <REPO>
cd pcp-pmda-hdb

# copy files
sudo mkdir -p /var/lib/pcp/pmdas/hdb
sudo cp pmdahdb.py /var/lib/pcp/pmdas/hdb/pmdahdb.python
sudo cp Install Remove pmdahdb.conf /var/lib/pcp/pmdas/hdb/
sudo chown -R root /var/lib/pcp/pmdas/hdb/
cd /var/lib/pcp/pmdas/hdb/

# Set connection parameters in pmdahdb.conf, see Configuration section of this document
sudo $EDITOR pmdahdb.conf
sudo chmod 640 pmdahdb.conf

# actual installation with pcp
sudo ./Install 
> Updating the Performance Metrics Name Space (PMNS) ...
> Terminate PMDA if already installed ...
> Updating the PMCD control file, and notifying PMCD ...
> Check hdb metrics have appeared ... 188 metrics and 1522 values

# check hdb-pmda is marked as installed
pcp | grep hdb
```

## Usage
List available metrics: 
```
pminfo -t hdb
hdb.io.aysnc_requests_count [Number of active asynchronous input and/or output requests on the host]
hdb.io.file_handles_count [Number of allocated file handles on the host]
hdb.cpu.total_time_idle_milliseconds [CPU idle time in milliseconds]
hdb.cpu.total_time_iowait_milliseconds [CPU time spent waiting for I/O in milliseconds]
hdb.cpu.total_time_system_milliseconds [CPU time spent in system mode in milliseconds]
hdb.cpu.total_time_user_milliseconds [CPU time spent in user mode in milliseconds]
hdb.alerts.active_count [Number of currently active alerts reported by the statistics server per rating]
hdb.memory.oom_events.statement_memory_limit_count [Number of out-of-memory (OOM) events since last reset caused by a statement memory limit.]
hdb.memory.oom_events.process_allocation_limit_count [Number of out-of-memory (OOM) events since last reset caused by a process allocation limit.]
[...]
```

Read all metrics
```
pminfo -Ff hdb

hdb.io.aysnc_requests_count
    inst [0 or "hxehost"] value 10240

hdb.io.file_handles_count
    inst [0 or "hxehost"] value 2816
[...]
```

Read a metric
```
pminfo -dfmtT hdb.version

hdb.version PMID: 158.0.2 [HANA Version (Revision)]
    Data Type: string  InDom: PM_INDOM_NULL 0xffffffff
    Semantics: discrete  Units: none
Help:
HANA Version (Revision)
    value "2.00.054.00.1611906357"
```

Read a metric with multiple instances
```
pminfo -dfmtT hdb.services.memory.stack_size_bytes

hdb.services.memory.stack_size_bytes PMID: 158.4.3 [Stack size in bytes]
    Data Type: 64-bit unsigned int  InDom: 158.3 0x27800003
    Semantics: instant  Units: byte
Help:
Stack size in bytes
    inst [0 or "hxehost.39001.nameserver"] value 148111360
    inst [1 or "hxehost.39003.indexserver"] value 129761280
    inst [2 or "hxehost.39006.webdispatcher"] value 40894464
    inst [3 or "hxehost.39010.compileserver"] value 29360128
```
The [official PCP documentation](https://pcp.io/documentation.html) contains the complete documentation on PCP.

RHEL specific information can be found in the [Red Hat Enterprise Linux PCP Documentation](https://access.redhat.com/documentation/en-us/red_hat_enterprise_linux/8/html-single/monitoring_and_managing_system_status_and_performance/index#setting-up-pcp_monitoring-and-managing-system-status-and-performance)

If you want to visualise the metrics via Grafana, have a look at this blog post: [Visualizing Performance with Grafana](https://www.redhat.com/en/blog/visualizing-system-performance-rhel-8-using-performance-co-pilot-pcp-and-grafana-part-1).

### Configuration
`pmdahdb.conf` contains the configuration of pcp-pmda-hdb.
Curently there is only one `hdb` section that contains the SAP HANA connection parameters.
[Quoted passwords](https://help.sap.com/viewer/102d9916bf77407ea3942fef93a47da8/1.0.11/en-US/61662e3032ad4f8dbdb5063a21a7d706.html#reference_ndt_cmm_ht) are supported.
```ini
[hdb]
host=hana-host
port=39015
user=SYSTEM
password="Secret"
```

## Features

pcp-pmda-hdb can be used in scale-out scenarios. 
All metrics are Host and Port aware.
Host and Port are included in the instance names (`<host>.<port>.<instance>`).

### Metrics
| Metric | Description |
|---|---|
| hdb.admission_control.measurement.memory_allocation_percent | Last measured memory size, as a percentage of the global allocation limit |
| hdb.admission_control.measurement.memory_size_gigabytes | Last measured memory size in GB |
| hdb.admission_control.measurement.timestamp | The time at which the last memory size was measured |
| hdb.admission_control.queue_size | Current waiting request queued count |
| hdb.admission_control.total_admit_count | Accumulated request admission count |
| hdb.admission_control.total_dequeue_count | Accumulated request dequeued count (the executed request count) |
| hdb.admission_control.total_enqueue_count | Accumulated request enqueued count |
| hdb.admission_control.total_reject_count | Accumulated request rejection count |
| hdb.admission_control.total_timeout_count | Accumulated request dequeued count due to timeout (the rejected request count) |
| hdb.admission_control.wait_time.avg_microseconds | Average wait time of the request in the queue in microseconds |
| hdb.admission_control.wait_time.last_microseconds | Last wait time of the request in the queue in microseconds |
| hdb.admission_control.wait_time.max_microseconds | Maximum wait time of the request in the queue in microseconds |
| hdb.admission_control.wait_time.min_microseconds | Minimum wait time of the request in the queue in microseconds |
| hdb.admission_control.wait_time.sum_microseconds | Total wait time of the request in the queue in microseconds |
| hdb.alerts.active_count | Number of currently active alerts reported by the statistics server per rating |
| hdb.backup.cancel_pending_count | Number of currently running backups with pending cancellation |
| hdb.backup.max_current_runtime_seconds | Maximum runtime of currently running (state=running or cancel pending) backups |
| hdb.backup.max_successful_runtime_seconds | Maximum runtime of successful backups |
| hdb.backup.running_count | Number of currently running backups |
| hdb.backup.total_canceled_count | Total number of canceled backups |
| hdb.backup.total_failed_count | Total number of failed backups |
| hdb.backup.total_successful_count | Total number of successful backups |
| hdb.buffer_cache.allocated_size_bytes | Allocated memory for the buffer cache in bytes |
| hdb.buffer_cache.hit_ratio | Ratio of pages found in the buffer cache to pages requested from the buffer cache |
| hdb.buffer_cache.max_size_bytes | Maximum buffer cache memory capacity in bytes |
| hdb.buffer_cache.reuse_count | Number of times that a buffer is released for reuse by the cache |
| hdb.buffer_cache.used_size_bytes | Used memory for the buffer cache in bytes |
| hdb.buffer_cache_pool.buffers_reuse_count | Number of buffers released from the LRU list for the pool so that a requested page can be cached |
| hdb.buffer_cache_pool.free_buffers_count | Number of free buffers for the pool |
| hdb.buffer_cache_pool.growth_percent | Rate, as a percentage, at which the buffer pool can grow |
| hdb.buffer_cache_pool.hot_list_buffers_count | Number of buffers in the hot buffer list for the pool |
| hdb.buffer_cache_pool.lru_list_buffers_count | Number of buffers in the LRU chain for the pool |
| hdb.buffer_cache_pool.out_of_buffer_event_count | Number number of times that an out-of-buffer situation occurred while requesting buffers from the pool. |
| hdb.buffer_cache_pool.total_buffers_count | Number of buffers allocated to the pool |
| hdb.cache.entries_count | Number of entries in the cache instance |
| hdb.cache.hits_count | Number of cache hits for the cache instance |
| hdb.cache.inserts_count | Number of insertions into the cache instance |
| hdb.cache.invalidations_count | Number of invalidations in the cache instance |
| hdb.cache.miss_count | Number of cache misses for the cache instance |
| hdb.cache.total_size_bytes | Maximum available memory budget in bytes available for the cache instance |
| hdb.cache.used_size_bytes | Memory in bytes used by the cache instance |
| hdb.column_unloads.explicit_count | Number of explicit column unloads |
| hdb.column_unloads.low_memory_count | Number of column unloads caused by memory management's automatic shrink on out of memory (OOM) |
| hdb.column_unloads.merge_count | Number of column unloads due to merges |
| hdb.column_unloads.shrink_count | Number of column unloads caused by a manual shrink |
| hdb.connections.idle_count | Total number of connections where no statement is being executed |
| hdb.connections.queuing_count | Total number of queued connections |
| hdb.connections.running_count | Total number of connections where a statement is being executed |
| hdb.cpu.total_time_idle_milliseconds | CPU idle time in milliseconds |
| hdb.cpu.total_time_iowait_milliseconds | CPU time spent waiting for I/O in milliseconds |
| hdb.cpu.total_time_system_milliseconds | CPU time spent in system mode in milliseconds |
| hdb.cpu.total_time_user_milliseconds | CPU time spent in user mode in milliseconds |
| hdb.instance_id | SAP instance ID |
| hdb.instance_number | Instance number |
| hdb.io.aysnc_requests_count | Number of active asynchronous input and/or output requests on the host |
| hdb.io.file_handles_count | Number of allocated file handles on the host |
| hdb.memory.allocated_bytes | Size of the memory pool for all SAP HANA processes in bytes |
| hdb.memory.code_size_bytes | Code size, including shared libraries of SAP HANA processes in bytes |
| hdb.memory.host_allocation_limit_bytes | Allocation limit for all processes in bytes |
| hdb.memory.host_free_bytes | Free physical memory on the host in bytes |
| hdb.memory.host_used_bytes | Used physical memory on the host in bytes |
| hdb.memory.oom_events.global_allocation_limit_count | Number of out-of-memory (OOM) events since last reset cause by the global allocation limit. |
| hdb.memory.oom_events.process_allocation_limit_count | Number of out-of-memory (OOM) events since last reset caused by a process allocation limit. |
| hdb.memory.oom_events.statement_memory_limit_count | Number of out-of-memory (OOM) events since last reset caused by a statement memory limit. |
| hdb.memory.peak_used_bytes | Peak memory from the memory pool used by SAP HANA processes since the instance started (this is a sample-based value) in bytes |
| hdb.memory.shared_size_bytes | Shared memory size of SAP HANA processes in bytes |
| hdb.memory.swap_free_bytes | Free swap memory on the host in bytes |
| hdb.memory.swap_used_bytes | Used swap memory on the host in bytes |
| hdb.memory.used_bytes | Amount of memory from the memory pool that is currently being used by SAP HANA processes in bytes |
| hdb.metadata_locks.total_wait_time_microseconds | Accumulated lock wait time (in microseconds) for metadata locks for all available services from database start up until the current time. |
| hdb.metadata_locks.total_waits_count | Accumulated lock wait count for metadata locks for all available services from database start up until the current time. |
| hdb.mvcc.acquired_lock_count | Number of acquired records locks |
| hdb.mvcc.data_versions_count | Number of all MVCC data versions per service |
| hdb.mvcc.metadata_versions_count | Number of all MVCC metadata versions per service |
| hdb.mvcc.read_write_lag | Difference between minimum closed write transaction ID and what all transactions can see |
| hdb.mvcc.snapshot_lag | Difference between global MVCC timestamp and minimal MVCC timestamp which at least one transaction holds |
| hdb.mvcc.versions_count | Number of all MVCC versions on the host |
| hdb.plan_cache.cached_plans.count | Total number of cached plans in SQL Plan Cache |
| hdb.plan_cache.cached_plans.execution_count | Number of total plan executions for cached plans |
| hdb.plan_cache.cached_plans.preparation_count | Total number of plan preparations for cached plans |
| hdb.plan_cache.cached_plans.total_cursor_duration_microseconds | Total cursor duration for cached plans in microseconds |
| hdb.plan_cache.cached_plans.total_execution_time_microseconds | Total execution time for cached plans in microseconds |
| hdb.plan_cache.cached_plans.total_preparation_time_microseconds | Total plan preparation duration for cached plans in microseconds |
| hdb.plan_cache.capacity_bytes | SQL Plan Cache capacity in bytes |
| hdb.plan_cache.evicted_plans.avg_cache_time_microseconds | Average duration in microseconds between plan cache insertion and eviction |
| hdb.plan_cache.evicted_plans.count | Number of evicted plans from SQL Plan Cache |
| hdb.plan_cache.evicted_plans.execution_count | Number of total plan executions for evicted plans |
| hdb.plan_cache.evicted_plans.preparation_count | Number of total plan preparations for evicted plans |
| hdb.plan_cache.evicted_plans.size_bytes | Accumulated total size of evicted plans in bytes |
| hdb.plan_cache.evicted_plans.total_cursor_duration_microseconds | Total cursor duration in microseconds for evicted plans |
| hdb.plan_cache.evicted_plans.total_execution_time_microseconds | Total execution time in microseconds for evicted plans |
| hdb.plan_cache.evicted_plans.total_preparation_time_microseconds | Total duration in microseconds for plan preparation for all evicted plans |
| hdb.plan_cache.fill_ratio_percent | SQL Plan Cache fill ratio in percent |
| hdb.plan_cache.hit_ratio | SQL Plan Cache hit ratio |
| hdb.plan_cache.hits_count | Number of hit counts from SQL Plan Cache |
| hdb.plan_cache.lookups_count | Nmber of plan lookup counts from SQL Plan Cache |
| hdb.plan_cache.size_bytes | SQL Plan Cache size in bytes |
| hdb.record_locks.acquired_count | Number of locks that are currently acquired |
| hdb.record_locks.memory_allocated_bytes | Allocated memory for record locks in bytes |
| hdb.record_locks.memory_used_bytes | Used memory for record locks in bytes |
| hdb.record_locks.total_wait_time_microseconds | Accumulated lock wait time (in microseconds) for record locks for all available services from database start up until the current time. |
| hdb.record_locks.total_waits_count | Accumulated lock wait count for record locks for all available services from database start up until the current time. |
| hdb.schemas.memory.used_bytes | Total used (column store) memory by schema in bytes |
| hdb.schemas.total_merge_count | Total number of delta merges on the schema across all tables and partitions |
| hdb.schemas.total_read_count | Total number of read accesses on the schema across all tables and partitions |
| hdb.schemas.total_write_count | Total number of write accesses on the schema across all tables and partitions |
| hdb.services.cpu_percent | CPU usage percentage of the current process |
| hdb.services.cpu_time_milliseconds | CPU usage of the current process since the start in milliseconds as if there would be 1 core |
| hdb.services.memory.allocated_free_size_bytes | Allocated free memory in bytes |
| hdb.services.memory.code_size_bytes | Code size, including shared libraries, in bytes |
| hdb.services.memory.compactors_allocated_size_bytes | Part of the memory pool that can potentially (if unpinned) be freed during a memory shortage in bytes |
| hdb.services.memory.compactors_freeable_size_bytes | Part of the memory pool that can be freed during a memory shortage in bytes |
| hdb.services.memory.effective_allocation_limit_size_bytes | Effective maximum memory pool size, in bytes, considering the pool sizes of other processes |
| hdb.services.memory.fragmented_size_bytes | Memory held by SAP HANA's memory management that cannot be easily reused for new memory allocations in bytes |
| hdb.services.memory.guaranteed_size_bytes | Minimum guaranteed memory for the process in bytes |
| hdb.services.memory.heap_allocated_size_bytes | Heap memory allocated from the memory pool in bytes |
| hdb.services.memory.heap_used_size_bytes | Heap memory used from the memory pool in bytes |
| hdb.services.memory.heap_used_size_percent | Heap memory used in percent |
| hdb.services.memory.logical_size_bytes | Virtual memory size from the operating system perspective in bytes |
| hdb.services.memory.physical_size_bytes | Physical memory size from the operating system perspective in bytes |
| hdb.services.memory.shared_allocated_size_bytes | Shared memory allocated from the memory pool in bytes |
| hdb.services.memory.shared_used_size_percent | Shared memory used in percent |
| hdb.services.memory.stack_size_bytes | Stack size in bytes |
| hdb.services.memory.used_size_bytes | Memory in use from the memory pool in bytes |
| hdb.services.memory.virtual_address_space_total_size_bytes | Total size of the virtual address space in bytes |
| hdb.services.memory.virtual_address_space_used_size_bytes | Used size of the virtual address space in bytes |
| hdb.services.open_files_count | Number of open files |
| hdb.services.sql_executor_threads_active_count | Number of active SQL Executor threads per indexserver |
| hdb.services.sql_executor_threads_inactive_count | Number of inactive SQL Executor threads per indexserver |
| hdb.services.threads_active_count | Number of active threads |
| hdb.services.threads_total_count | Number of total threads |
| hdb.statements.compilation_rate | Current statement preparation count per minute |
| hdb.statements.execution_count | Total count of all executed statements for data manipulation, data definition, and system control |
| hdb.statements.execution_rate | Current statement execution count per minute |
| hdb.statements.peak_compilation_rate | Peak statement preparation count per minute |
| hdb.statements.peak_execution_rate | peak statement execution count per minute |
| hdb.table_locks.total_wait_time_microseconds | Accumulated lock wait time (in microseconds) for table locks for all available services from database start up until the current time. |
| hdb.table_locks.total_waits_count | Accumulated lock wait count for table locks for all available services from database start up until the current time. |
| hdb.transactions.aborting_count | Current number of aborting transactions |
| hdb.transactions.active_count | Current number of active transactions |
| hdb.transactions.active_prepare_count | Current number of transactions in active prepare status |
| hdb.transactions.blocked_count | Number of currently blocked transactions waiting for locks |
| hdb.transactions.blocked_metadata_count | Number of currently blocked transactions waiting for metadata locks |
| hdb.transactions.blocked_record_count | Number of currently blocked transactions waiting for record locks |
| hdb.transactions.blocked_table_count | Number of currently blocked transactions waiting for table locks |
| hdb.transactions.commit_count | Number of transaction commits |
| hdb.transactions.commit_rate | Current number of commits per minute |
| hdb.transactions.inactive_count | Current number of inactive transactions |
| hdb.transactions.partial_aborting_count | Current number of transactions in partial aborting status |
| hdb.transactions.peak_commit_rate | Peak number of commits per minute |
| hdb.transactions.peak_rollback_rate | Peak rollback of rollbacks per minute |
| hdb.transactions.peak_rollbacks_rate | Current number of rollbacks per minute |
| hdb.transactions.peak_transaction_rate | Peak transaction count per minute |
| hdb.transactions.peak_update_rate | Peak update transaction count per minute |
| hdb.transactions.precomitted_count | Current number of precommitted transactions |
| hdb.transactions.rollback_count | Number of transaction rollbacks |
| hdb.transactions.transaction_rate | Current transaction count per minute |
| hdb.transactions.update_count | Number of update transactions |
| hdb.transactions.update_rate | Current update transaction count per minute |
| hdb.version | HANA Version (Revision) |
| hdb.volumes.data.total_size_bytes | Size of data volumes as reported by the file system in bytes |
| hdb.volumes.data.used_size_bytes | Size of used and shadow pages in the data volume files in bytes |
| hdb.volumes.fill_ratio | Displays the fill ratio of the data volume |
| hdb.volumes.io.active_async_reads_count | Number of active asynchronous reads |
| hdb.volumes.io.active_async_writes_count | Number of active asynchronous writes |
| hdb.volumes.io.async_reads_trigger_ratio | Trigger-ratio of asynchronous reads |
| hdb.volumes.io.async_writes_trigger_ratio | Trigger-ratio of asynchronous writes |
| hdb.volumes.io.blocked_write_requests_count | Number of blocked write requests |
| hdb.volumes.io.max_blocked_write_requests_count | Maximum number of blocked write requests |
| hdb.volumes.io.total_appends_count | Total number of appends |
| hdb.volumes.io.total_async_reads_count | Total number of triggered asynchronous reads |
| hdb.volumes.io.total_async_writes_count | Total number of triggered asynchronous writes |
| hdb.volumes.io.total_failed_reads_count | Total number of failed reads |
| hdb.volumes.io.total_failed_writes_count | Total number of failed writes |
| hdb.volumes.io.total_full_retry_reads_count | Total number of full retry reads |
| hdb.volumes.io.total_full_retry_writes_count | Total number of full retry writes |
| hdb.volumes.io.total_read_bytes | Total number of read data in bytes |
| hdb.volumes.io.total_read_time_microseconds | Total read time in microseconds |
| hdb.volumes.io.total_reads_count | Total number of synchronous reads |
| hdb.volumes.io.total_short_reads_count | Total number of reads that read fewer bytes than requested |
| hdb.volumes.io.total_short_writes_count | Total number of writes that wrote fewer bytes than requested |
| hdb.volumes.io.total_time_microseconds | Total I/O time in microseconds |
| hdb.volumes.io.total_write_time_microseconds | Total write time in microseconds |
| hdb.volumes.io.total_writes_count | Total number of appends |
| hdb.volumes.io.total_written_bytes | Total number of written data in bytes |
| hdb.volumes.log.total_size_bytes | Size of log volumes as reported by the file system in bytes |
| hdb.volumes.total_size_bytes | Total size of the data volume in bytes |
| hdb.volumes.used_size_bytes | Used size of the data volume in bytes |

## Known Issues and Limitations

pcp-pmda-hdb currently does not support Multitenant Database Container (MDC) systems.
The agent can only target a single database container at the moment (this container can be a tenant database).

pcp-pmda-hdb is not tested with Python2.

## Contributing
Pull requests are welcome. For major changes, please open an issue first to discuss what you would like to change.
Please make sure to update tests as appropriate.

### Dev Setup
```
dnf install -y pcp python3-pcp pylint python3-pylint shellcheck
pip3 install hdbcli
pip3 install mypy
pip3 install isort
pip3 install flake8
```

### Running Tests
Unit tests (`pmdahdb_test.py`) require a running HANA instance (HANA Express is sufficient). 
Connection paramneters need to be set via the environment variables `HDB_HOST`, `HDB_PORT`, `HDB_USER`, and `HDB_PASSWORD`.

Installation and Removal procedure can be tested with `hack/run-integration-tests.py`

Integration with the PCP ecosystem can be interactively tested with `hack/debug-pmda.sh`.

## License
[GNU General Public License v3.0](./LICENSE)
