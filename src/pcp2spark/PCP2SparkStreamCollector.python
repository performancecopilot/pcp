# Import Required Libraries
import sys
from pyspark import SparkContext
from pyspark.streaming import StreamingContext

# Begin
if __name__ == "__main__":
        sc = SparkContext(appName="PCP2Spark-StreamingCollector");
        # 10 is the batch interval : 10 seconds
        ssc = StreamingContext(sc,2)

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
