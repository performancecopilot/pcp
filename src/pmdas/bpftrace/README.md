# bpftrace PMDA
This PMDA exports metrics from [bpftrace](https://github.com/iovisor/bpftrace) scripts.
In combination with the [PCP Plugin for Grafana](https://github.com/performancecopilot/grafana-pcp) this PMDA enables on-demand live performance analysis using eBPF.
The Grafana plugin also supports the visualization of histograms collected by this PMDA.

## Quickstart (using the command line interface)
```
$ cd /var/lib/pcp/pmdas/bpftrace && sudo ./Install

$ pmstore -F bpftrace.control.register 'kretprobe:vfs_read { @bytes = hist(retval); }'

$ pminfo -f bpftrace.info.scripts
bpftrace.info.scripts
    inst [1 or "script100"] value "kretprobe:vfs_read { @bytes = hist(retval); }"

$ pminfo -f bpftrace.scripts.script100.data.bytes
bpftrace.scripts.script100.data.bytes
    inst [0 or "-inf--1"] value 409
    inst [1 or "0-0"] value 191
    inst [2 or "1-1"] value 1160
    inst [3 or "2-3"] value 85
    inst [4 or "4-7"] value 61
    inst [5 or "8-15"] value 431
    inst [6 or "16-31"] value 98
```

## Features
* start and stop multiple bpftrace scripts
* export bpftrace variables (eBPF maps) as PCP metrics:
  * single values, counters
  * maps, histograms
  * text output (by `printf()`, `time()` etc.)
* automatic removal of scripts whose values weren't requested in a specified time period

## Configuration
The configuration of this PMDA is stored in `bpftrace.conf`.

### Scripts Metadata
This PMDA supports the following metadata annotations, to be included in the bpftrace script as comments:

#### Named Scripts
```
// name: script_name
```
This annotation sets the name of a script.
Metrics will be exported in the `bpftrace.scripts.scriptname` namespace.
Named scripts will never be removed, even if their value isn't requested in a long time period.

#### Only export specific variables
```
// include: @bytes,@count
```
By default all bpftrace variables get exported as metrics.
This annotation changes the default behavior and specifies which bpftrace variables should be exported.
All other bpftrace variables will be ignored.

#### Tables
```
// table-retain-lines: 10
```
A common use case is to print tables in CSV format (using `printf()`), and display them as a table in Grafana.
This setting controls how many data lines should be retained. The table header (the first line) will be preserved.

## Thanks
Thanks to Alastair Robertson and all contributors of [bpftrace](https://github.com/iovisor/bpftrace/graphs/contributors).
