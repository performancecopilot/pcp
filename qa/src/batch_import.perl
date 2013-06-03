#
# Exercise libpcp_import "batching" extensions in Perl import API
#
# Copyright (c) 2013 Red Hat.
#
use strict;
use warnings;

$| = 1;		# don't buffer output
use PCP::LogImport;

my $ctx1;
my $ctx2;
my $hdl1;
my $hdl2;

sub check
{
    my ($sts, $name) = @_;
    if ($sts < 0) { print $name . ": Error: " . pmiErrStr($sts) . "\n"; }
    else {
	print $name . ": OK";
	if ($sts != 0) { print " ->$sts"; }
	print "\n";
    }
}

$ctx1 = pmiStart("myarchive", 0);
check($ctx1, "pmiStart");

$_ = pmiSetHostname("batching.com");
check($_, "pmiSetHostname");
$_ = pmiSetTimezone("GMT-12");
check($_, "pmiSetTimezone");

$_ = pmiUseContext($ctx1);
check($_, "pmiUseContext");

$_ = pmiAddMetric("my.metric.foo", pmid_build(PMI_DOMAIN,0,1), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT, pmiUnits(1,-1,0,PM_SPACE_MBYTE,PM_TIME_SEC,0));
check($_, "pmiAddMetric");
$_ = pmiAddMetric("my.metric.bar", PM_ID_NULL, PM_TYPE_U64, pmInDom_build(PMI_DOMAIN,1), PM_SEM_INSTANT, pmiUnits(1,-1,0,PM_SPACE_MBYTE,PM_TIME_SEC,0));
check($_, "pmiAddMetric");
$_ = pmiAddMetric("my.metric.long", PM_ID_NULL, PM_TYPE_64, PM_INDOM_NULL, PM_SEM_INSTANT, pmiUnits(0,0,0,0,0,0));
check($_, "pmiAddMetric");
$_ = pmiAddMetric("my.metric.double", PM_ID_NULL, PM_TYPE_DOUBLE, PM_INDOM_NULL, PM_SEM_INSTANT, pmiUnits(0,0,0,0,0,0));
check($_, "pmiAddMetric");
$_ = pmiAddMetric("my.metric.string", PM_ID_NULL, PM_TYPE_STRING, PM_INDOM_NULL, PM_SEM_INSTANT, pmiUnits(0,0,0,0,0,0));
check($_, "pmiAddMetric");
$_ = pmiAddMetric("my.metric.float", PM_ID_NULL, PM_TYPE_FLOAT, PM_INDOM_NULL, PM_SEM_DISCRETE, pmiUnits(0,0,0,0,0,0));
check($_, "pmiAddMetric");
$_ = pmiAddMetric("my.metric.strung", PM_ID_NULL, PM_TYPE_STRING, PM_INDOM_NULL, PM_SEM_INSTANT, pmiUnits(0,0,0,0,0,0));
check($_, "pmiAddMetric");

$_ = pmiBatchPutValue("my.metric.string", "", "a third string value");
check($_, "pmiBatchPutValue");
$_ = pmiBatchWrite(2, 70000);
check($_, "pmiBatchWrite");
$_ = pmiBatchPutValue("my.metric.strung", "", "a first string value");
check($_, "pmiBatchPutValue");
$_ = pmiBatchPutValue("my.metric.string", "", "a second string value");
check($_, "pmiBatchPutValue");
$_ = pmiBatchWrite(0, 110000);
check($_, "pmiBatchWrite");
$hdl1 = pmiGetHandle("my.metric.string", "");
check($hdl1, "pmiGetHandle");
$_ = pmiBatchPutValueHandle($hdl1, "a fourth string value");
check($_, "pmiBatchPutValueHandle");
$hdl2 = pmiGetHandle("my.metric.strung", "");
check($hdl2, "pmiGetHandle");
$_ = pmiBatchPutValueHandle($hdl2, "a fifth string value");
check($_, "pmiBatchPutValueHandle");
$_ = pmiBatchWrite(3, 10000);
check($_, "pmiBatchWrite");

$_ = pmiBatchEnd();
check($_, "pmiBatchEnd");

pmiDump();
