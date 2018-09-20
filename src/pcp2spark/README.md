# PCP Exporter to Apache Spark (PCP2SPARK)

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
an apache spark worker thread to be started. The worker thread will 
then connect to pcp2spark.

When an apache spark worker thread has connected pcp2spark will begin
streaming PCP metric data to apache spark until the worker thread
completes or the connection is interrupted. If the connection is 
interrupted or the socket is closed from the apache spark worker 
thread pcp2spark will exit.

For an example apache spark worker job which will connect to an 
pcp2spark instance on a given address/port and pull in pcp metric 
data please look at the example in the examples directory of 
pcp2spark.

# More Information

For more information on the command line arguments and configuration
of pcp2spark please see pcp2spark(1) and for more information on 
spark streaming please see:

https://spark.apache.org/streaming/

# Get started with Spark Streaming

## Installations
Follow this tutorial to install spark:
https://www.tutorialspoint.com/apache_spark/apache_spark_installation.htm

## A pcp2spark example
Here we will use pcp2spark to import pcp metrics and print in spark

## PCP2SparkStreamCollector.py
The example to run with python
```shell
# PCP2SparkStreamCollector
#
# A basic spark streaming collector worker using the spark streaming
# api to import pcp metrics into spark with conjunction with pcp2spark.
#

# Import Required Libraries
import sys
from pyspark import SparkContext
from pyspark.streaming import StreamingContext

# Begin
if __name__ == "__main__":
        sc = SparkContext(appName="PCP2Spark-StreamingCollector");
        # 5 is the batch interval : 5 seconds
        ssc = StreamingContext(sc,5)

        # Checkpoint for backups
        ssc.checkpoint("file:///tmp/spark")

        # Define the socket where pcp2spark is listening for a connection
        # metrics is not an rdd but a sequence of rdd, not static, constantly changing
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
Open a shell and start pcp2spark in the command line using one metric
```shell
$ pcp2spark -t 5 disk.all.write
```

### Submit the python script
Open a shell and start the spark worker script
```shell
$ spark-submit PCP2SparkStreamCollector.py localhost 44325
```

### You'll see time slots with the output
On the same shell we will see the output from the worker as it starts
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
