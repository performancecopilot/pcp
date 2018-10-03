# PCP to Apache Spark Exporter (pcp2spark) README

pcp2spark is a customizable performance metrics exporter tool from
PCP to Apache Spark. Any available performance metric, live or
archived, system and/or application, can be selected for exporting
using either command line arguments or a configuration file.

pcp2spark acts as a bridge which provides a network socket stream on
a given address/port which an Apache Spark worker task can connect to
and pull the configured PCP metrics from pcp2spark exporting them
using the streaming extensions of the Apache Spark API.

# General Setup

A general setup for making use of pcp2spark would involve the user
configuring pcp2spark for the PCP metrics to export followed by
starting the pcp2spark application. The pcp2spark application will
then wait and listen on the given address/port for a connection from
an Apache Spark worker thread to be started. The worker thread will
then connect to pcp2spark.

When an Apache Spark worker thread has connected pcp2spark will begin
streaming PCP metric data to Apache Spark until the worker thread
completes or the connection is interrupted. If the connection is
interrupted or the socket is closed from the Apache Spark worker
thread pcp2spark will exit.

For an example Apache Spark worker job which will connect to an
pcp2spark instance on a given address/port and pull in pcp metric
data please look at the example in the examples directory of
pcp2spark.

# More Information

For more information on the command line arguments and configuration
of pcp2spark please see pcp2spark(1) and for more information on
Spark streaming please see Apache Spark pages:

https://www.mankier.com/1/pcp2spark
https://spark.apache.org/streaming/

# Get Started with Spark Streaming

## Installation

Follow this tutorial to install Spark:

https://www.tutorialspoint.com/apache_spark/apache_spark_installation.htm

## A pcp2spark Example

Here we will use pcp2spark to import PCP metrics and print in Spark

### PCP2SparkStreamCollector.python

The example to run with python:

```python
# PCP2SparkStreamCollector
#
# A basic Spark Streaming collector worker using the Spark Streaming
# API to import PCP metrics into Spark in conjunction with pcp2spark.

import sys
from pyspark import SparkContext
from pyspark.streaming import StreamingContext

if __name__ == "__main__":
    sc = SparkContext(appName="PCP2SparkStreamCollector")
    # 5 is the batch interval: 5 seconds
    ssc = StreamingContext(sc, 5)

    # Checkpoint for backups
    ssc.checkpoint("file:///tmp/spark")

    # Define the socket where pcp2spark is listening for a connection.
    # metrics is not an RDD but a sequence of constantly changing RDDs
    # argv1 = address of pcp2spark, argv2 = port of pcp2spark
    metrics = ssc.socketTextStream(sys.argv[1], int(sys.argv[2]))

    ## Display the metrics we have streamed
    ## Start the program
    ## The program will run until manual termination
    metrics.pprint()
    ssc.start()
    ssc.awaitTermination()
```

### Start pcp2spark

Open a shell and start pcp2spark in the command line using one metric:

```shell
$ pcp2spark -t 5 disk.all.write
```

### Submit the Python Script

Open a shell and start the Spark worker script:

```shell
$ spark-submit PCP2SparkStreamCollector.py localhost 44325
```

Note that as of Spark 2.3.2 _spark-submit_ auto-detects only Python
files ending with _.py_ but not with _.python_.

### Expected Output

On the same shell we will see the output from the worker as it starts:

```shell
...
-------------------------------------------
Time: 2018-09-20 18:21:32
-------------------------------------------
{"@host-id": "t460p","@timestamp": 1537464090740,"disk": {"all": {"write": 6.799}}}

-------------------------------------------
Time: 2018-09-20 18:21:37
-------------------------------------------
{"@host-id": "t460p","@timestamp": 1537464095740,"disk": {"all": {"write": 12.001}}}

...
```
