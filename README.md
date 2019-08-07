[![Build Status](https://travis-ci.com/Erbenos/pcp-statsd-c.svg?branch=pcp)](https://travis-ci.com/Erbenos/pcp-statsd-c)

# pmdastatsd - Performance Metric Domain Agent for StatsD

_(under development, not yet recommended for production use)_

This repository contains an agent that collects [StatsD](https://github.com/statsd/statsd) data, aggregates them and makes them available to any Performance Co-Pilot client, which is ideal for easily tracking stats in your application.

- [Features](#features)
- [Installation](#installation)
- [Configuration](#configuration)
- [Usage](#usage)
- [Roadmap](#roadmap)
- [FAQ](#faq)

# Features
- [Counter](#counter-metric) metric type
- [Gauge](#gauge-metric) metric type
- [Duration](#duration-metric) metric type, available instances
    - Minimum
    - Maximum
    - Median
    - Average
    - 90th Percentile
    - 95th Percentile
    - 99th Percentile
    - Count
    - Standard deviation
- Parsing of datagrams either with Ragel or Basic parser (with very simple tests available as of right now)
- Aggregation of duration metrics either with basic histogram or HDR histogram
- [Labels](#labels)
- Logging
- Stats about agent itself
- [Configuration](#configuration) using
    - .ini files
    - Command line arguments

# Installation

## Dependencies

- PCP version >= 4.3.3
- [chan](https://github.com/tylertreat/chan)
- [HdrHistogram_c](https://github.com/HdrHistogram/HdrHistogram_c) installed in your /usr/local dir
- [Ragel](http://www.colm.net/open-source/ragel/)

## Installation steps

1. Put contents of this repo into $PCP_PMDAS_DIR/statsd/ ($PCP_PMDAS_DIR is sourced from /etc/pcp.conf, which should be available if you have PCP installed)
2. First make sure you have "STATSD" namespace set to "510" in stdpmid file. [How-to](https://pcp.io/books/PCP_PG/html/id5189538.html)
3. Compile with **make**
4. Activate agent with **sudo make activate**

## Uninstallation steps

1. Run **sudo make deactivate** within $PCP_PMDAS_DIR/statsd/ directory
2. Remove the statsd folder

# Configuration

## Ini file

Agent looks for *pmdastatsd.ini* within it's root directory by default.
It accepts following parameters:

- **max_udp_packet_size** - Maximum allowed packet size <br>default: _1472_
- **tcp_address** - On which IP is agent listening for incoming trafic <br>default: _0.0.0.0_
- **port** - On which port is agent listening for incoming trafic <br>default: _8125_
- **verbose** - Flag controlling whether or not to allow verbose logging <br>default: _1_
- **debug** - Flag controlling whether or not to allow debug logging <br>default: _0_
- **debug_output_filename** - You can send USR1 signal that 'asks' agent to output basic information about all aggregated metric into a file with this name <br>default: _debug_
- **version** - Flag controlling whether or not to log current agent version on start <br>default: _0_
- **parser_type** - Flag specifying which algorithm to use for parsing incoming datagrams, 0 = basic, 1 = Ragel <br>default: _0_
- **duration_aggregation_type** - Flag specifying which aggregation scheme to use for duration metrics, 0 = basic, 1 = hdr histogram <br>default: _1_
- **max_unprocessed_packets** - Maximum size of packet queue that the agent will save in memory. There are 2 queues: one for packets that are waiting to be parsed and one for parsed packets before they are aggregated <br>default: _2048_

## Command line arguments

Agent accepts all arguments that any PMDA accepts by default, including those specified above, in following form:

- --max-udp, -Z
- --tcp-address, -t
- --port, -P
- --verbose, -v
- --debug, -g
- --debug-output-filename, -o
- --version, -s
- --parser-type, -r
- --duration-aggregation-type, -a
- --max-unprocessed-packets-size, -z

In case when an argument is included in both an .ini file and in command line, the values passed via command line take precedence.

# Usage

Once started, pmdastatsd will listed on specified address and port for any content in a form of:

```
<metricname>:<value>|<type>
```

There may be multiple such messages in single datagram, split by a newline character, so this:

```
<metricname>:<value>|<type>\n<metricname>:<value>|<type>
```

is valid as well.

```
<metricname> = [a-zA-Z0-9_\-/ .]{1,}
<value>      = described further in each metric type
<type>       = 'c'|'g'|'ms'
```

If verbose loggings is turned on, agent will log every message parsed and related failures.

All recorded metrics will be available under <strong>statsd.*</strong> namespace.

## Counter metric
Stores metrics as simple counters, adding any incoming values to already existing ones.

```
<metricname>:<value>|c
```

Where value is positive number.

### Example

After aggregating following messages:

```
metric:20|c
metric:10|c
metric:3.3|c
```

Value available to PCP will be:

```
pminfo -f statsd.metric
-> inst[0 or "/"] value 33.3
```

## Gauge metric
Stores metrics as modifiable values, with an option to either set, increment or decrement values.

```
<metricname>:<value>|g
```

Where value can be in a form of:
- **'-{value}'**, when negative value is supplied agent will substract stored value with value passed
- **'+{value}'**, when positive value with leading plus sign is supplied agent will add passed value to the value stored
- **'{value}'**, when value without any leading sign is supplied, agent will set the metric to the passed value 

Initial value for metric of gauge type is 0.

### Example

After aggregating following messages:

```
metric:20|g
metric:+10|g
metric:-3.3|g
```

Value available to PCP will be:

```
pminfo -f statsd.metric
-> inst [0 or "/"] value 26.7
```

## Duration metric
Aggregates values either via HDR Histogram or simply stores all values and then calculates inst ors from all values received.

```
<metricname>:<value>|ms
```

Where value is a positive number.

### Example

_With larger message count, the values may vary based on selected duration aggregation scheme._

```
metric:10|ms
metric:20|ms
```

Values available to PCP will be:

```
pminfo -f statsd.metric
->
inst [0 or "/min"] value 10
inst [1 or "/max"] value 20
inst [2 or "/median"] value 10
inst [3 or "/average"] value 15
inst [4 or "/percentile90"] value 20
inst [5 or "/percentile95"] value 20
inst [6 or "/percentile99"] value 20
inst [7 or "/count"] value 2
inst [8 or "/std_deviation"] value 5 
```

## Note
Once you send given _metricname_ with specified _type_, agent will no longer aggregate any messages with same _metricname_ but different _type_ and will throw them away.

# Labels
StatsD datagram may also contain _key:value_ pairs separated by commas like so:

```
metric,tagX=X,tagW=W:5|c
```

Where:
- _tagX_ is key, _X_ is value
- _tagW_ is key, _W_ is value

Both _key_ and _value_ of such pair are under same restrictions as _metricname_.

You may use these labels to map specific values to some PCP instances. PCP labels are also assigned to these PCP instances.

Single label:

```
metric,tagX=X:5|c
```

Such payload would map to PCP as follows (non-related labels were ommited):

```
pminfo -f --labels statsd.metric
->
inst [0 or "/tagX=X"] value 5 
inst [0 or "/tagX=X"] labels {"tagX":"X"}
```

As shown earlier you may also send payload with multiple labels. When multiple labels are supplied they are split in instance name by '::'. Example:

```
metric,tagX=X,tagW=W:5|c
```

This resolves to:

```
pminfo -f --labels statsd.metric
->
inst [0 or "/tagX=X::tagW=W"] value 5
inst [0 or "/tagX=X::tagW=W"] labels {"tagX":"X","tagW":"W"}
```

### Note
Be mindful of the fact that duration metric type already maps to instances even without any labels. Sending labeled value to a such metric creates another 9 (as there are that many hardcoded) instances.

Example:

```
metric:200|ms
metric:100|ms
metric:200|ms
metric,target=cpu0:10|ms   
metric,target=cpu0:100|ms
metric,target=cpu0:1000|ms
```

Creates 18 instances. Duration data type and label name compose instance name in following manner:

```
pminfo -f --labels statsd.metric
->
...
inst [10 or "/max::target=cpu0"] value 1000
inst [10 or "/max::target=cpu0"] labels {"target":"cpu0"}
...
```

## Hardcoded stats
Agent also exports metrics about itself:

- **statsd.pmda.received** <br>Number of datagrams that the agent has received
- **statsd.pmda.parsed** <br>Number of datagrams that were successfully parsed
- **statsd.pmda.dropped** <br>Number of datagrams that were dropped
- **statsd.pmda.aggregated** <br>Number of datagrams that were aggregated
- **statsd.pmda.metrics_tracked**
    - **counter** - Number of tracked counter metrics
    - **gauge** - Number of tracked gauge metrics
    - **duration** - Number of tracked duration metrics
    - **total** - Number of tracked metrics total
- **statsd.pmda.time_spent_parsing** <br>Total time in nanoseconds spent parsing metrics
- **statsd.pmda.time_spent_aggregating** <br>Total time in nanoseconds spent aggregating metrics

These names are blacklisted for user usage. No messages with these names will processed. While not yet reserved, whole <strong>statsd.pmda.*</strong> namespace is not recommended to use for user metrics.

# Roadmap
- Make sure code is optimized
- Add more tests
- Allow _value_ to be expressed in _e_ notation

# FAQ

## I installed both **chan** and **HdrHistogram_c** yet the program won't run... there seem to be .so missing.
You may need to make sure that /usr/local is actually looked into. You may need to add the directory to **/etc/ld.so.conf** yourself:
```
tee /etc/ld.so.conf.d/local.conf <<EOF
/usr/local/lib
/usr/local/lib64
EOF
```
Next, run as root:
```
ldconfig
```
to clear linker cache.

# Simplified inner logic flow:

Listener -> Parser -> Aggregator => Data structures (PMDA stats and StatsD metrics) guarded by locks <= PCP

Legend:
- "->" = channel
- "=>" = shared data structures
