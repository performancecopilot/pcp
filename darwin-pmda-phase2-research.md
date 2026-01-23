# Darwin PMDA Phase 2 - Metrics Expansion Research

## Executive Summary

This document presents comprehensive research on additional macOS metrics that can be exposed through the Darwin PMDA. Phase 1 (PR #2442) successfully added 60+ metrics covering memory compression, network protocols, VFS, and process counts. This Phase 2 research catalogs all remaining opportunities grouped by capability area.

**Target Platform**: Apple Silicon Macs (M1/M2/M3/M4 series). Intel Macs will gracefully degrade - metrics will be registered but return no values where hardware-specific access is unavailable.

---

## Phase 1 Completion Status (PR #2442)

### Successfully Implemented
- **Memory Compression**: compressions, decompressions, compressor pages, compressed memory
- **VFS Statistics**: files count/max/free, vnodes count/max
- **Process Counts**: kernel.all.nprocs, kernel.all.nthreads
- **UDP Protocol**: 8 metrics (datagrams in/out, errors, buffer errors)
- **ICMP Protocol**: 8 metrics (messages, errors, echo requests/replies)
- **TCP States**: 11 connection state counters
- **TCP Protocol**: 15 metrics (opens, failures, segments, retransmits, errors)
- **Socket Stats**: TCP/UDP socket counts

### Current Totals
- **Darwin PMDA**: ~186 metrics across 12 clusters
- **Darwin_proc PMDA**: ~35 per-process metrics

---

## Phase 2 Metric Categories

### Category 1: Thermal & Temperature Monitoring

**Why It Matters**: Desktop/laptop users need visibility into thermal throttling, fan behavior, and temperature trends for performance diagnosis.

#### 1.1 CPU/GPU Temperature Sensors (via SMC)
| Metric | Description | API |
|--------|-------------|-----|
| `thermal.cpu.die` | CPU die temperature (°C) | IOKit SMC Tp* keys (Apple Silicon) |
| `thermal.cpu.proximity` | CPU proximity sensor | IOKit SMC TA*P keys |
| `thermal.gpu.die` | GPU die temperature | IOKit SMC Tg* keys (Apple Silicon) |
| `thermal.package` | SoC package temperature | IOKit SMC TCXC key |
| `thermal.ambient` | Ambient air temperature | IOKit SMC TA0P key |

**Platform Notes**:
- Apple Silicon: Uses Tp* (CPU), Tg* (GPU) key patterns
- Intel Macs: Metrics registered but return no values (graceful degradation)
- May require `com.apple.private.smcsensor.user-access` entitlement on some versions

#### 1.2 Fan Metrics (via SMC)
| Metric | Description | API |
|--------|-------------|-----|
| `hinv.nfan` | Number of fans | SMC FNum key |
| `thermal.fan.speed` | Current RPM (per-fan instance) | SMC F*Ac keys |
| `thermal.fan.target` | Target RPM (per-fan instance) | SMC F*Tg keys |
| `thermal.fan.mode` | Mode: 0=auto, 1=manual | SMC F*Md keys |
| `thermal.fan.min` | Minimum RPM | SMC F*Mn keys |
| `thermal.fan.max` | Maximum RPM | SMC F*Mx keys |

**Note**: Many Apple Silicon Macs (MacBook Air M1/M2, Mac mini M1/M2, Mac Studio base config) are fanless. Metrics will return 0 fans detected on these systems.

#### 1.3 Thermal Pressure
| Metric | Description | API |
|--------|-------------|-----|
| `thermal.pressure.level` | Thermal pressure (0-3 scale) | `notify_register_check("com.apple.system.thermalpressurelevel")` |
| `thermal.pressure.state` | Nominal/Fair/Serious/Critical | Same notification API |

**Implementation Approach**: Direct SMC Access (community-documented)

The SMC interface is well-understood through community reverse-engineering. Reference implementations:
- **iSMC** (https://github.com/dkorunic/iSMC) - CLI tool supporting Apple Silicon
- **SMCKit** (https://github.com/beltex/SMCKit) - Swift/Objective-C library

**SMC Communication Pattern:**
```c
// IOKit connection to AppleSMC service
io_connect_t conn;
IOServiceOpen(IOServiceGetMatchingService(kIOMasterPortDefault,
    IOServiceMatching("AppleSMC")), mach_task_self(), 0, &conn);

// Read SMC key (e.g., "Tp01" for Apple Silicon CPU temperature)
SMCKeyData_t inputStruct, outputStruct;
inputStruct.key = fourCharToInt("Tp01");
inputStruct.data8 = SMC_CMD_READ_KEYINFO;
IOConnectCallStructMethod(conn, KERNEL_INDEX_SMC, &inputStruct, ...);
```

**Data Format Conversion:**
- Temperatures: `sp78` format (signed fixed-point) → divide by 256.0 for °C
- Fan speeds: `fpe2` format (unsigned fixed-point) → divide by 4.0 for RPM

**Graceful Degradation:**
- If SMC access denied, return PM_ERR_APPVERSION (not available)
- Log warning once, don't spam

---

### Category 2: GPU & Graphics Monitoring

**Why It Matters**: Video editing, gaming, ML workloads need GPU visibility.

#### 2.1 GPU Hardware Inventory
| Metric | Description | API |
|--------|-------------|-----|
| `hinv.ngpu` | Number of GPUs | IOKit IOGPU enumeration |
| `hinv.gpu.model` | GPU model string (per-GPU) | IORegistry IOGLBundleName |
| `hinv.gpu.vram` | VRAM size (MB, per-GPU) | IORegistry properties |

#### 2.2 GPU Utilization
| Metric | Description | API |
|--------|-------------|-----|
| `gpu.util` | GPU utilization % (per-GPU) | IOKit PerformanceStatistics["Device Utilization %"] |
| `gpu.activity` | GPU activity % | PerformanceStatistics["GPU Activity %"] |

#### 2.3 GPU Memory
| Metric | Description | API |
|--------|-------------|-----|
| `gpu.memory.used` | VRAM used (MB) | IOKit PerformanceStatistics |
| `gpu.memory.free` | VRAM free (MB) | Calculated from total - used |

**Implementation Complexity**: MEDIUM - Pattern exists in htop vendor code, no entitlements needed

---

### Category 3: Power & Battery Monitoring

**Why It Matters**: Laptop users need battery health and power consumption visibility.

#### 3.1 Battery Status (via IOPowerSources)
| Metric | Description | API |
|--------|-------------|-----|
| `power.battery.present` | Battery present flag | IOPSCopyPowerSourcesInfo() |
| `power.battery.charging` | Currently charging | kIOPSIsChargingKey |
| `power.battery.charge` | Current capacity % | kIOPSCurrentCapacityKey |
| `power.battery.time_remaining` | Minutes to empty/full | kIOPSTimeToEmptyKey |
| `power.battery.health` | Battery health % | IOKit AppleSmartBattery |
| `power.battery.cycle_count` | Charge cycle count | CycleCount property |
| `power.battery.temperature` | Battery temperature | Temperature property |
| `power.battery.voltage` | Current voltage (mV) | Voltage property |
| `power.battery.amperage` | Discharge rate (mA) | Amperage property |
| `power.battery.capacity.design` | Design capacity (mAh) | DesignCapacity |
| `power.battery.capacity.max` | Current max capacity | MaxCapacity |

#### 3.2 Power Source
| Metric | Description | API |
|--------|-------------|-----|
| `power.ac.connected` | AC power connected | kIOPSPowerSourceStateKey |
| `power.source` | "AC Power" or "Battery" | kIOPSPowerSourceStateKey |

#### 3.3 Power Consumption (via IOReport - requires root)
| Metric | Description | API |
|--------|-------------|-----|
| `power.cpu.watts` | CPU power draw (mW) | IOReport framework |
| `power.gpu.watts` | GPU power draw (mW) | IOReport framework |
| `power.package.watts` | SoC total power (mW) | IOReport framework |
| `power.dram.watts` | Memory power (mW) | IOReport framework |

**Implementation Complexity**:
- Battery metrics: LOW - IOPowerSources is well-documented, no root needed
- Power consumption: HIGH - IOReport requires root/entitlements

---

### Category 4: Process I/O & Resource Tracking

**Why It Matters**: Identify disk-intensive apps, understand per-process resource usage.

#### 4.1 Process Disk I/O (via proc_pid_rusage)
| Metric | Description | API |
|--------|-------------|-----|
| `proc.io.read_bytes` | Disk bytes read (per-proc) | rusage_info_v3.ri_diskio_bytesread |
| `proc.io.write_bytes` | Disk bytes written | ri_diskio_byteswritten |
| `proc.io.logical_writes` | Logical write bytes | ri_logical_writes |

#### 4.2 Process QoS CPU Time (via RUSAGE_INFO_V4/V5)
| Metric | Description | API |
|--------|-------------|-----|
| `proc.cpu.qos.default` | CPU time at default QoS | ri_cpu_time_qos_default |
| `proc.cpu.qos.background` | Background QoS CPU time | ri_cpu_time_qos_background |
| `proc.cpu.qos.utility` | Utility QoS CPU time | ri_cpu_time_qos_utility |
| `proc.cpu.qos.user_initiated` | User-initiated QoS | ri_cpu_time_qos_user_initiated |
| `proc.cpu.qos.user_interactive` | User-interactive QoS | ri_cpu_time_qos_user_interactive |

#### 4.3 Enhanced Process Memory (via TASK_VM_INFO)
| Metric | Description | API |
|--------|-------------|-----|
| `proc.memory.virtual` | Virtual size (detailed) | task_info(TASK_VM_INFO) |
| `proc.memory.footprint` | Memory footprint | vm_info.phys_footprint |
| `proc.memory.shared` | Shared pages | vm_info.share_mode |
| `proc.memory.purgeable` | Purgeable pages | vm_info.purgable_volatile_pmap |

#### 4.4 Process File Descriptors
| Metric | Description | API |
|--------|-------------|-----|
| `proc.fd.count` | Open FD count (per-proc) | proc_pidinfo(PROC_PIDLISTFDS) |

#### 4.5 Process Network Connections
| Metric | Description | API |
|--------|-------------|-----|
| `proc.net.tcp_count` | TCP connections (per-proc) | PROC_PIDFDSOCKETINFO enumeration |
| `proc.net.udp_count` | UDP sockets (per-proc) | PROC_PIDFDSOCKETINFO enumeration |

**Implementation Complexity**: MEDIUM - APIs well-documented, no special privileges

---

### Category 5: Enhanced Network Metrics

**Why It Matters**: IPv6 adoption, detailed protocol diagnostics.

#### 5.1 IPv6 Protocol Statistics
| Metric | Description | API |
|--------|-------------|-----|
| `network.ip6.inreceives` | IPv6 packets received | sysctl net.inet6.ip6.stats |
| `network.ip6.outforwarded` | IPv6 packets forwarded | net.inet6.ip6.stats |
| `network.ip6.indiscards` | IPv6 input discards | net.inet6.ip6.stats |
| `network.ip6.outdiscards` | IPv6 output discards | net.inet6.ip6.stats |
| `network.ip6.fragcreates` | IPv6 fragments created | net.inet6.ip6.stats |
| `network.ip6.reasmoks` | IPv6 reassembly success | net.inet6.ip6.stats |

#### 5.2 IGMP/Multicast Statistics
| Metric | Description | API |
|--------|-------------|-----|
| `network.igmp.version` | IGMP version in use | sysctl net.inet.igmp.default_version |
| `network.igmp.v1_enabled` | IGMPv1 enabled | net.inet.igmp.v1enable |
| `network.igmp.v2_enabled` | IGMPv2 enabled | net.inet.igmp.v2enable |
| `network.igmp.v3_enabled` | IGMPv3 enabled | net.inet.igmp.v3enable |

#### 5.3 Advanced TCP Metrics
| Metric | Description | API |
|--------|-------------|-----|
| `network.tcp.sack_enabled` | SACK algorithm enabled | sysctl net.inet.tcp.sack |
| `network.tcp.ecn_enabled` | ECN enabled | net.inet.tcp.ecn_initiate_out |
| `network.tcp.fastopen` | TCP Fast Open state | net.inet.tcp.fastopen |
| `network.tcp.rtt_min` | Minimum RTT observed | net.inet.tcp.rtt_min |
| `network.tcp.delayed_ack` | Delayed ACK count | tcpstat structure |

**Implementation Complexity**: LOW-MEDIUM - sysctl pattern already established

---

### Category 6: Disk & Storage Metrics

**Why It Matters**: I/O latency visibility, queue depth for bottleneck detection.

#### 6.1 Disk Queue Metrics
| Metric | Description | API |
|--------|-------------|-----|
| `disk.dev.queue_depth` | Current I/O queue depth | IOKit IOBlockStorageDriver |
| `disk.dev.inflight` | In-flight I/O count | IOKit storage properties |

#### 6.2 Disk Latency (already have timing, could add)
| Metric | Description | API |
|--------|-------------|-----|
| `disk.dev.await` | Average wait time (ms) | Calculated from existing timing |
| `disk.dev.svctm` | Service time (ms) | Calculated |
| `disk.dev.util` | Utilization % | Calculated |

#### 6.3 APFS-Specific Metrics
| Metric | Description | API |
|--------|-------------|-----|
| `disk.apfs.snapshot_count` | APFS snapshots per volume | IOKit/diskutil |
| `disk.apfs.encryption` | Encryption status | IOKit properties |
| `disk.apfs.container_size` | Container total size | IOKit properties |
| `disk.apfs.container_free` | Container free space | IOKit properties |

**Implementation Complexity**:
- Queue metrics: MEDIUM - need to investigate IOKit properties
- APFS: MEDIUM - may require diskutil or IOKit APFS classes

---

### Category 7: System Resource Limits & IPC

**Why It Matters**: Capacity planning, resource exhaustion warnings.

#### 7.1 System Resource Counters (via sysctl)
| Metric | Description | API |
|--------|-------------|-----|
| `kernel.all.maxproc` | Maximum processes | sysctl kern.maxproc |
| `kernel.all.maxfiles` | Maximum open files | kern.maxfiles |
| `kernel.all.maxfilesperproc` | Max files per process | kern.maxfilesperproc |
| `kernel.all.maxprocperuid` | Max processes per UID | kern.maxprocperuid |
| `vfs.vnodes.recycled` | Recycled vnode count | kern.num_recycledvnodes |

#### 7.2 IPC & Socket Pool
| Metric | Description | API |
|--------|-------------|-----|
| `ipc.mbuf.clusters` | Mbuf cluster count | sysctl kern.ipc.nmbclusters |
| `ipc.maxsockbuf` | Max socket buffer size | kern.ipc.maxsockbuf |
| `ipc.somaxconn` | Max socket backlog | kern.ipc.somaxconn |
| `ipc.socket.defunct` | Defunct socket calls | kern.ipc.sodefunct_calls |

**Implementation Complexity**: LOW - simple sysctl reads

---

### Category 8: Scheduler & Performance Counters

**Why It Matters**: CPU scheduling efficiency, performance tuning.

#### 8.1 Scheduler Information
| Metric | Description | API |
|--------|-------------|-----|
| `kernel.sched.type` | Scheduler type ("edge") | sysctl kern.sched |
| `kernel.sched.recommended_cores` | Recommended core mask | kern.sched_recommended_cores |

#### 8.2 Monotonic Counters
| Metric | Description | API |
|--------|-------------|-----|
| `kernel.pmi.count` | Performance monitoring interrupts | kern.monotonic.pmis |

**Implementation Complexity**: LOW - sysctl reads

---

### Category 9: Device Enumeration

**Why It Matters**: Hardware inventory, device health monitoring.

#### 9.1 USB Devices
| Metric | Description | API |
|--------|-------------|-----|
| `hinv.nusb` | USB device count | IOKit IOUSBHostDevice enumeration |
| `usb.device.vendor` | Vendor ID (per-device) | IORegistry properties |
| `usb.device.product` | Product ID (per-device) | IORegistry properties |
| `usb.device.speed` | Connection speed | IORegistry properties |

#### 9.2 Thunderbolt Devices
| Metric | Description | API |
|--------|-------------|-----|
| `hinv.nthunderbolt` | Thunderbolt device count | IOKit IOThunderboltDevice |
| `thunderbolt.device.name` | Device name (per-device) | IORegistry properties |

**Implementation Complexity**: MEDIUM - IOKit enumeration pattern exists

---

### Category 10: Memory Compression Deep Dive

**Why It Matters**: macOS-specific memory behavior, pressure analysis.

#### 10.1 Compression Timing Buckets
| Metric | Description | API |
|--------|-------------|-----|
| `mem.compressor.swapouts_under_1s` | Swapouts < 1 second | sysctl vm.compressor_swapouts_under_1s |
| `mem.compressor.swapouts_under_5s` | Swapouts < 5 seconds | vm.compressor_swapouts_under_5s |
| `mem.compressor.swapouts_under_10s` | Swapouts < 10 seconds | vm.compressor_swapouts_under_10s |

#### 10.2 Compression Health
| Metric | Description | API |
|--------|-------------|-----|
| `mem.compressor.thrashing_detected` | Thrashing detection | vm.compressor_thrashing_detected |
| `mem.compressor.major_compactions` | Major compactions | vm.compressor.compactor.major* |
| `mem.compressor.lz4_compressions` | LZ4 compression count | vm.lz4_compressions |

**Implementation Complexity**: LOW - sysctl reads

---

## API Summary by Accessibility

### No Special Privileges Required
- GPU utilization (IOKit IOGPU)
- Battery status (IOPowerSources)
- All sysctl metrics (network, memory, VFS, IPC)
- Process metrics (libproc, task_info)
- USB/Thunderbolt enumeration

### May Require Root or Entitlements
- SMC temperature sensors (`com.apple.private.smcsensor.user-access`)
- IOReport power metrics (root)
- Some thermal data

### Undocumented/Reverse-Engineered
- SMC key interface (community-documented, not Apple-supported)

---

## Implementation File Organization (Proposed)

```
src/pmdas/darwin/
├── pmda.c           # Main dispatch (existing)
├── kernel.c         # Kernel metrics (existing)
├── disk.c           # Disk I/O (existing, extend for queue)
├── network.c        # Network interfaces (existing)
├── version.c        # macOS version (existing)
├── thermal.c        # NEW: Temperature & fans
├── gpu.c            # NEW: GPU monitoring
├── power.c          # NEW: Battery & power
├── tcp.c            # Existing: TCP stats
├── udp.c            # Existing: UDP stats
├── icmp.c           # Existing: ICMP stats
├── ipv6.c           # NEW: IPv6 stats
├── vfs.c            # Existing: VFS metrics
└── resources.c      # NEW: System limits & IPC

src/pmdas/darwin_proc/
├── pmda.c           # Main dispatch (existing)
├── kinfo_proc.c     # Process enumeration (existing)
├── proc_io.c        # NEW: Process I/O stats
├── proc_memory.c    # NEW: Enhanced memory
└── proc_network.c   # NEW: Per-process connections
```

---

## Metric Count Summary by Category

| Category | Estimated Metrics | Complexity |
|----------|-------------------|------------|
| 1. Thermal & Temperature | ~15 | HIGH |
| 2. GPU & Graphics | ~7 | MEDIUM |
| 3. Power & Battery | ~15 | LOW-HIGH |
| 4. Process I/O & Resources | ~15 | MEDIUM |
| 5. Enhanced Network | ~15 | LOW-MEDIUM |
| 6. Disk & Storage | ~10 | MEDIUM |
| 7. System Limits & IPC | ~10 | LOW |
| 8. Scheduler | ~3 | LOW |
| 9. Device Enumeration | ~6 | MEDIUM |
| 10. Memory Compression | ~6 | LOW |
| **TOTAL** | **~100** | |

---

## Recommended Implementation Sequence

Based on complexity, value, and dependencies:

### Wave 1: Quick Wins (LOW complexity, HIGH value)
**Estimated: ~25 metrics**

1. **System Limits & IPC** (Category 7)
   - Simple sysctl reads, follows existing patterns
   - Files: extend `kernel.c` or new `resources.c`
   - Metrics: maxproc, maxfiles, IPC pools

2. **Memory Compression Deep Dive** (Category 10)
   - Simple sysctl reads for timing buckets
   - Files: extend `kernel.c`
   - Metrics: swapout timing, thrashing detection

3. **Process I/O Statistics** (Category 4.1)
   - Uses existing `proc_pid_rusage()` pattern
   - Files: new `proc_io.c` in darwin_proc
   - Metrics: read_bytes, write_bytes per process

### Wave 2: Medium Effort (MEDIUM complexity, HIGH value)
**Estimated: ~30 metrics**

4. **GPU Monitoring** (Category 2)
   - IOKit pattern exists in htop vendor code
   - Files: new `gpu.c`
   - Metrics: utilization, VRAM, model

5. **Battery Status** (Category 3.1-3.2)
   - IOPowerSources API is well-documented
   - Files: new `power.c`
   - Metrics: charge, health, cycles, voltage

6. **Enhanced Network IPv6** (Category 5)
   - sysctl pattern matches existing TCP/UDP
   - Files: new `ipv6.c`
   - Metrics: IPv6 packet counts, errors

7. **Process QoS & Memory** (Category 4.2-4.4)
   - Extends existing libproc usage
   - Files: extend `kinfo_proc.c`
   - Metrics: QoS CPU time, memory footprint, FD count

### Wave 3: Higher Effort (HIGH complexity, HIGH value)
**Estimated: ~25 metrics**

8. **Thermal & Temperature** (Category 1)
   - SMC interface requires careful implementation
   - Files: new `thermal.c` with `smc.c` helper
   - Metrics: CPU/GPU temps, fan speeds, thermal pressure

9. **Process Network Connections** (Category 4.5)
   - Requires FD enumeration + socket inspection
   - Files: new `proc_network.c`
   - Metrics: TCP/UDP connection counts per process

10. **Disk Queue & APFS** (Category 6)
    - IOKit storage properties investigation needed
    - Files: extend `disk.c`
    - Metrics: queue depth, APFS specifics

### Wave 4: Optional/Specialized
**Estimated: ~20 metrics**

11. **Device Enumeration** (Category 9) - USB/Thunderbolt inventory
12. **Power Consumption** (Category 3.3) - IOReport (requires root)
13. **Scheduler Counters** (Category 8) - Specialized use

---

## Platform Compatibility Matrix

| Feature | Apple Silicon | Intel Mac (Degraded) | Notes |
|---------|---------------|----------------------|-------|
| **SMC Temps** | Tp*, Tg* keys | Not available | Metrics registered, no values |
| **SMC Fans** | May not exist | Not available | M-series often fanless |
| **GPU** | AGXAccelerator | IOAccelerator | Both via IOGPU class - SUPPORTED |
| **Battery** | IOPowerSources | IOPowerSources | Same API - SUPPORTED |
| **Process APIs** | libproc | libproc | Same API - SUPPORTED |
| **sysctl** | Supported | Supported | Identical interface - SUPPORTED |

**Runtime Detection:**
```c
// Detect Apple Silicon for SMC key selection
int ret = 0;
size_t size = sizeof(ret);
sysctlbyname("hw.optional.arm64", &ret, &size, NULL, 0);
bool is_apple_silicon = (ret == 1);

// On Intel Macs, SMC metrics return PM_ERR_APPVERSION (not available)
```

---

## Next Steps

1. **Confirm Wave sequencing** - Does this order make sense?
2. **Validate SMC access** - Test on Apple Silicon (M1/M2/M3/M4)
3. **Create implementation tasks** - Break down each wave into issues
4. **Begin Wave 1** - Start with quick wins

## Related Issues

- GitHub Issue: TBD (to be created)

---

## References

- Apple XNU Source: https://github.com/apple/darwin-xnu
- IOKit Documentation: https://developer.apple.com/documentation/iokit
- libproc Reference: https://developer.apple.com/documentation/kernel
- SMC Community Docs: https://github.com/dkorunic/iSMC
- PCP Darwin PMDA: src/pmdas/darwin/, src/pmdas/darwin_proc/
