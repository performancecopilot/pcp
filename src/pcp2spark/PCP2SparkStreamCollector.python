#!/usr/bin/env pmpython

# pylint: disable=invalid-name

""" PCP2Spark Stream Collector example """

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

    # Display the metrics we have streamed
    # Start the program
    # The program will run until manual termination
    metrics.pprint()
    ssc.start()
    ssc.awaitTermination()
