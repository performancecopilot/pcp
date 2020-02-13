#! /bin/sh

NMETRICS=5
NINSTANCES=2

awk 'BEGIN {
    for(i=0; i < '$NMETRICS'; i++) {
	printf("# HELP some_metric%03d Simple gauge metric with some instances\n", i)
	printf("# Type some_metric%03d gauge\n", i)
	for (j=0; j < '$NINSTANCES'; j++) {
	    printf("some_metric%03d{someinst=\"%03d\"} %d\n", i, j, i * j)
	}
    }}' /dev/null
exit 0
