#!/bin/sh

count=000
state=/tmp/unbound-qa.txt
[ -f $state ] && count=`cat $state`
count=`expr $count + 1`

# produce some mocked output
count=`printf "%03d" $count`
[ -f unbound/unbound-control-stats-$count ] || count=001
cat unbound/unbound-control-stats-$count

# remember where we reached for next fetch
echo $count > $state
