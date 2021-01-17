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
modules = runqlat,tcpperpid
```

Each module has their own section with individual module-related
configuration, for example the `tcpperpid` module:
```
[tcpperpid]
module = tcpperpid
cluster = 11
dport = 80,443
```

In this case the `tcpperpid` module traces only TCP sessions with a remote
port of 80 or 443.

For many modules process(es) to monitor can be defined as a list of names,
PIDs, or regular expressions. PMDA-wide parameter `process_refresh` can be
used to monitor newly created processes matching the list of processes.

# Installation

```
yum install pcp pcp-pmda-bcc
systemctl enable --now pmcd
cd $PCP_PMDAS_DIR/bcc
vi bcc.conf
./Install
```

On most systems `$PCP_PMDAS_DIR` is `/var/lib/pcp/pmdas`.

# Uninstalling the PMDA

```
cd $PCP_PMDAS_DIR/bcc
./Remove
```

# Frontend

## CLI
```
$ pminfo -f bcc.disk.all.latency                                

bcc.disk.all.latency
    inst [0 or "0-1"] value 0
    inst [1 or "2-3"] value 0
    inst [2 or "4-7"] value 0
    inst [3 or "8-15"] value 0
    inst [4 or "16-31"] value 0
    inst [5 or "32-63"] value 1956
    inst [6 or "64-127"] value 5715
    inst [7 or "128-255"] value 6321
    inst [8 or "256-511"] value 3416
```

## Web Frontend

All BCC PMDA modules can be visualized in Grafana, using the [grafana-pcp plugin](https://grafana-pcp.readthedocs.io).

# Troubleshooting

* Check logfile for errors: `$PCP_LOG_DIR/pmcd/bcc.log`
* Check if the bcc Python module is installed: `/usr/bin/pcp python -c 'import bcc'`
* Check if BCC PMDA metrics are registered: `pminfo | grep bcc`
* Check if BCC PMDA metrics contain data: `pminfo -f bcc.proc.sysfork` (if the `sysfork` module is enabled)
* Check if BCC is working on the system: `yum install bcc-tools && /usr/share/bcc/tools/runqlat`

# License

* PMDA: [GPLv2](https://github.com/performancecopilot/pcp/blob/master/COPYING)
* eBPF/BCC programs: [Apache License 2.0](https://github.com/iovisor/bcc/blob/master/LICENSE.txt)
