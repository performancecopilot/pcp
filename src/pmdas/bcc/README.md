# BCC eBPF PMDA

This PMDA extracts live performance data from extended BPF (Berkeley Packet
Filter) in-kernel programs by using the BCC (BPF Compiler Collection) Python
frontend.

eBPF was [described by Ingo Molnár](https://lkml.org/lkml/2015/4/14/232) as:

> One of the more interesting features in this cycle is the ability to attach
> eBPF programs (user-defined, sandboxed bytecode executed by the kernel) to
> kprobes. This allows user-defined instrumentation on a live kernel image
> that can never crash, hang or interfere with the kernel negatively.

[BCC](https://github.com/iovisor/bcc) has made creating new eBPF programs
easier, and the BCC project offers a wide variety of such tools
(https://github.com/iovisor/bcc/tree/master/tools). However, basically all
these programs are individual, disjoint utilities that are mostly meant for
interactive use. This is not a suitable approach to collect, monitor and
analyze performance data in larger environments where there are hundreds, if
not thousands, installations and where human intervention is unfeasible at
best.

This PMDA loads and acts as a bridge for any number of configured, separate
BCC PMDA Python modules running eBPF programs. Each module consists of an eBPF
program (e.g. [sysfork.bpf](modules/sysfork.bpf)) and a corresponding Python
file (e.g. [sysfork.python](modules/sysfork.python)).

# Requirements

* A Linux kernel with eBPF support (RHEL 7.6+ or kernel 4.7+)
* BCC (https://github.com/iovisor/bcc) and BCC Python module (0.5+)

# Configuration

There are several modules available to trace system and application behavior
from different angles, including number of (failed) system calls (per pid),
any available kernel tracepoints, USDT/dtrace/stap probes, uprobes, kprobes,
and application internals (like new threads, objects, method calls).

Modules can be enabled and configured in the [bcc.conf](bcc.conf) file. The
`modules` setting of the `[pmda]` section lists the enabled modules:
```
[pmda]
modules = runqlat,tcplife
```

Each module has their own section with individual module-related
configuration, for example the `tcplife` module:
```
[tcplife]
module = tcplife
cluster = 3
dport = 80,443
```

In this case the tcplife module traces only TCP sessions with a remote port of
80 or 443.

For many modules process(es) to monitor can be defined as a list of names,
PIDs, or regular expressions. PMDA-wide parameter `process_refresh` can be
used to monitor newly created processes matching the list of processes.

# Installation

```
yum install pcp pcp-pmda-bcc
systemctl enable --now pmcd
cd $PCP_PMDAS_DIR/bcc
./Install
```

On many systems `$PCP_PMDAS_DIR` is `/var/lib/pcp/pmdas`.

# Uninstalling the PMDA

```
cd $PCP_PMDAS_DIR/bcc
./Remove
```

# Frontend

## CLI
```
$ pminfo -f bcc.fs.ext4.latency.open

bcc.fs.ext4.latency.open
    inst [0 or "0-1"] value 140636
    inst [1 or "2-3"] value 15512
    inst [2 or "4-7"] value 696
    inst [3 or "8-15"] value 106
    inst [4 or "16-31"] value 285
    inst [5 or "32-63"] value 87
    inst [6 or "64-127"] value 27
    inst [7 or "128-255"] value 11
    inst [8 or "256-511"] value 11
```

## Web Frontend

Most of the BCC PMDA Modules have a corresponding Vector widget
([Vector BCC Widgets](https://github.com/Netflix/vector/blob/master/src/app/charts/bcc.js)),
for example:

### tcplife

Shows information about recently closed TCP sessions:

![vector_tcplife](https://user-images.githubusercontent.com/538011/41207752-4e216ca2-6d1b-11e8-89c6-c34a42c62351.png)

### profile

Records stack traces at a specific interval, which will be rendered as a
flamegraph in Vector:

![profile](https://user-images.githubusercontent.com/538011/42831897-de7c8fca-89ef-11e8-9d35-59a89248d83c.png)

# Troubleshooting

* Check logfile for errors: `$PCP_LOG_DIR/pmcd/bcc.log`
* Check if the bcc Python module is installed: `/usr/bin/pcp python -c 'import bcc'`
* Check if BCC PMDA metrics are registered: `pminfo | grep bcc`
* Check if BCC PMDA metrics contain data: `pminfo -f bcc.proc.sysfork` (if the `sysfork` module is enabled)
* Check if BCC is working on the system: `yum install bcc-tools && /usr/share/bcc/tools/runqlat`

# License

* PMDA: [GPLv2](https://github.com/performancecopilot/pcp/blob/master/COPYING)
* eBPF/BCC programs: [Apache License 2.0](https://github.com/iovisor/bcc/blob/master/LICENSE.txt)
