#
# Copyright (c) Red Hat.
#
# Thanks to Marko Myllynen for writing this test case, and
# neatly exposing the underlying bug (Fedora/EPEL 968210).
#

use strict;
use warnings;
use Date::Parse;
use Date::Format;
use PCP::LogImport;

my $r;
my $LOW  = 1020;
my $HIGH = 1030;
my $date = "2013-05-29T00:00:00";

pmiStart("test-limit", 0);
exit 1 if pmiSetHostname("localhost");

for (my $i = 0; $i < $LOW; $i++) {
  my $metric = "x_$i";
  $r = pmiAddMetric($metric,
         PM_ID_NULL, PM_TYPE_DOUBLE, PM_INDOM_NULL,
         PM_SEM_INSTANT, pmiUnits(0,0,1,0,0,PM_COUNT_ONE));
  if ($r != 0) {
    print "pmiAddMetric failed for $metric / round $i with: ";
    print pmiErrStr($r) . "\n";
    exit 2;
  }
  $r = pmiPutValue($metric, "", $i);
  if ($r != 0) {
    print "pmiPutValue failed for $metric / round $i with: ";
    print pmiErrStr($r) . "\n";
    exit 3;
  }
}

for (my $j = $LOW; $j < $HIGH; $j++) {
  my $metric = "t_$j";
  pmiAddMetric($metric,
    PM_ID_NULL, PM_TYPE_DOUBLE, PM_INDOM_NULL,
    PM_SEM_INSTANT, pmiUnits(0,0,1,0,0,PM_COUNT_ONE));
  if ($r != 0) {
    print "pmiAddMetric failed for $metric / round $j with: ";
    print pmiErrStr($r) . "\n";
    exit 4;
  }
  pmiPutValue($metric, "", $j);
  if ($r != 0) {
    print "pmiPutValue failed for $metric / round $j with: ";
    print pmiErrStr($r) . "\n";
    exit 5;
  }
}

$r = pmiWrite(str2time($date, "UTC"), 0);
if ($r != 0) {
  print "pmiWrite failed for $date / " . str2time($date, "UTC") . " with: ";
  print pmiErrStr($r) . "\n";
  exit 6;
}

pmiEnd();
if ($r != 0) {
  print "pmiEnd failed with: ";
  print pmiErrStr($r) . "\n";
  exit 7;
}

exit 0;
