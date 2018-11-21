#
# Exercise libpcp_import ... Perl version of check_import.c
#
# Copyright (c) 2009 Ken McDonell.  All Rights Reserved.
# Copyright (c) 2018 Red Hat.
#
use strict;
use warnings;

$| = 1;		# don't buffer output

use PCP::LogImport;

print "PCP::LogImport symbols ...\n";
foreach (%PCP::LogImport::) {
    print "$_\n";
}
print "\n";

pmiDump();

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
pmiDump();

$_ = pmiSetHostname("somehost.com");
check($_, "pmiSetHostname");
$_ = pmiSetTimezone("GMT-12");
check($_, "pmiSetTimezone");

$_ = pmiUseContext(3);
check($_, "pmiUseContext");

$_ = pmiUseContext($ctx1);
check($_, "pmiUseContext");

$_ = pmiAddMetric("my.metric.foo", pmID_build(PMI_DOMAIN,0,1), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT, pmiUnits(1,-1,0,PM_SPACE_MBYTE,PM_TIME_SEC,0));
check($_, "pmiAddMetric");
$_ = pmiAddMetric("my.metric.bar", PM_ID_NULL, PM_TYPE_U64, pmInDom_build(PMI_DOMAIN,1), PM_SEM_INSTANT, pmiUnits(1,-1,0,PM_SPACE_MBYTE,PM_TIME_SEC,0));
check($_, "pmiAddMetric");
$_ = pmiAddMetric("my.metric.foo", 1, 2, 3, 4, pmiUnits(0,0,0,0,0,0));
check($_, "pmiAddMetric");
$_ = pmiAddMetric("my.metric.long", PM_ID_NULL, PM_TYPE_64, PM_INDOM_NULL, PM_SEM_INSTANT, pmiUnits(0,0,0,0,0,0));
check($_, "pmiAddMetric");
$_ = pmiAddMetric("my.metric.double", PM_ID_NULL, PM_TYPE_DOUBLE, PM_INDOM_NULL, PM_SEM_INSTANT, pmiUnits(0,0,0,0,0,0));
check($_, "pmiAddMetric");
$_ = pmiAddMetric("my.metric.string", PM_ID_NULL, PM_TYPE_STRING, PM_INDOM_NULL, PM_SEM_INSTANT, pmiUnits(0,0,0,0,0,0));
check($_, "pmiAddMetric");
$_ = pmiAddMetric("my.dup.pmid", pmID_build(PMI_DOMAIN,0,3), PM_TYPE_32, PM_INDOM_NULL, PM_SEM_INSTANT, pmiUnits(0,0,0,0,0,0));
check($_, "pmiAddMetric");
$_ = pmiAddMetric("my.metric.float", PM_ID_NULL, PM_TYPE_FLOAT, PM_INDOM_NULL, PM_SEM_DISCRETE, pmiUnits(0,0,0,0,0,0));
check($_, "pmiAddMetric");

$_ = pmiAddInstance(pmInDom_build(PMI_DOMAIN,1), "eek really", 1);
check($_, "pmiAddInstance");
$_ = pmiAddInstance(pmInDom_build(PMI_DOMAIN,1), "eek", 2);
check($_, "pmiAddInstance");
$_ = pmiAddInstance(pmInDom_build(PMI_DOMAIN,1), "blah", 3);
check($_, "pmiAddInstance");
$_ = pmiAddInstance(pmInDom_build(PMI_DOMAIN,1), "not-blah-again", 3);
check($_, "pmiAddInstance");

$_ = pmiPutValue("my.metric.foo", "", 123);
check($_, "pmiPutValue");
$_ = pmiPutValue("my.metric.foo", "should be null", "1234");
check($_, "pmiPutValue");
$_ = pmiPutValue("my.metric.bar", "eek", "4567890123456");
check($_, "pmiPutValue");
$_ = pmiPutValue("my.metric.bar", "blah", "4567890123457");
check($_, "pmiPutValue");
$_ = pmiPutValue("my.metric.bar", "not-blah", "4567890123457");
check($_, "pmiPutValue");
$_ = pmiPutValue("my.metric.bar", "", "42");
check($_, "pmiPutValue");
$_ = pmiPutValue("my.metric.string", "", "a new string value");
check($_, "pmiPutValue");
$_ = pmiPutValue("my.metric.long", "", 123456789012345);
check($_, "pmiPutValue");
$_ = pmiPutValue("my.metric.double", "", "1.23456789012");
check($_, "pmiPutValue");
$_ = pmiPutValue("my.metric.float", "", "-1.234567");
check($_, "pmiPutValue");

$hdl1 = pmiGetHandle("my.metric.foo", "");
check($hdl1, "pmiGetHandle");
$_ = pmiGetHandle("my.bad", "");
check($_, "pmiGetHandle");
$_ = pmiPutValueHandle($hdl1, "321");
check($_, "pmiPutValueHandle");
$_ = pmiPutValueHandle(0, "error");
check($_, "pmiPutValueHandle");

$_ = pmiPutText(PM_TEXT_PMID, PM_TEXT_ONELINE, pmID_build(245,0,1),
		"One line text for my.metric.foo");
check($_, "pmiPutText");
$_ = pmiPutText(PM_TEXT_PMID, PM_TEXT_HELP, pmID_build(245,0,1),
		"Full help text for my.metric.foo");
check($_, "pmiPutText");
$_ = pmiPutText(PM_TEXT_INDOM, PM_TEXT_ONELINE, pmInDom_build(245,1),
		"One line text for indom 'eek'");
check($_, "pmiPutText");
$_ = pmiPutText(PM_TEXT_INDOM, PM_TEXT_HELP, pmInDom_build(245,1),
		"Full help text for indom 'eek'");
check($_, "pmiPutText");

$_ = pmiPutLabel(PM_LABEL_CONTEXT, 0, 0, "NewContextLabel", "NewContextLabelContent" );
check($_, "pmiPutLabel");
$_ = pmiPutLabel(PM_LABEL_DOMAIN, pmID_build(245,0,0), 0,
		  "NewDomainLabel245", "NewDomainLabel245Content");
check($_, "pmiPutLabel");
$_ = pmiPutLabel(PM_LABEL_CLUSTER, pmID_build(245,0,0), 0,
		  "NewClusterLabel245_0", "NewClusterLabel245_0Content");
check($_, "pmiPutLabel");
$_ = pmiPutLabel(PM_LABEL_ITEM, pmID_build(245,0,1), 0,
		  "NewItemLabel245_0_1", "NewItemLabel245_0_1Content");
check($_, "pmiPutLabel");
$_ = pmiPutLabel(PM_LABEL_INDOM, pmInDom_build(245,1), 0,
		  "NewIndomLabel245_1", "NewIndomLabel245_1Content");
check($_, "pmiPutLabel");
$_ = pmiPutLabel(PM_LABEL_INSTANCES, pmInDom_build(245,1), 1,
		  "NewInstancesLabel245_1__1", "NewInstancesLabel245_1__1Content");
check($_, "pmiPutLabel");
$_ = pmiPutLabel(PM_LABEL_INSTANCES, pmInDom_build(245,1), 3,
		  "NewInstancesLabel245_1__3", "NewInstancesLabel245_1__3Content");
check($_, "pmiPutLabel");
$_ = pmiPutLabel(PM_LABEL_CONTEXT, 0, 0, "ContextTrue", "True" );
check($_, "pmiPutLabel");
$_ = pmiPutLabel(PM_LABEL_CONTEXT, 0, 0, "ContextFalse", "False" );
check($_, "pmiPutLabel");
$_ = pmiPutLabel(PM_LABEL_CONTEXT, 0, 0, "ContextNull", "Null" );
check($_, "pmiPutLabel");

# These are duplicates - which are ok.
$_ = pmiPutLabel(PM_LABEL_CONTEXT, 0, 0, "NewContextLabel", "NewContextLabelContent" );
check($_, "pmiPutLabel");
$_ = pmiPutLabel(PM_LABEL_DOMAIN, pmID_build(245,0,0), 0,
		  "NewDomainLabel245", "NewDomainLabel245Content");
check($_, "pmiPutLabel");
$_ = pmiPutLabel(PM_LABEL_CLUSTER, pmID_build(245,0,0), 0,
		  "NewClusterLabel245_0", "NewClusterLabel245_0Content");
check($_, "pmiPutLabel");
$_ = pmiPutLabel(PM_LABEL_ITEM, pmID_build(245,0,1), 0,
		  "NewItemLabel245_0_1", "NewItemLabel245_0_1Content");
check($_, "pmiPutLabel");
$_ = pmiPutLabel(PM_LABEL_INDOM, pmInDom_build(245,1), 0,
		  "NewIndomLabel245_1", "NewIndomLabel245_1Content");
check($_, "pmiPutLabel");
$_ = pmiPutLabel(PM_LABEL_INSTANCES, pmInDom_build(245,1), 1,
		  "NewInstancesLabel245_1__1", "NewInstancesLabel245_1__1Content");
check($_, "pmiPutLabel");
$_ = pmiPutLabel(PM_LABEL_INSTANCES, pmInDom_build(245,1), 3,
		  "NewInstancesLabel245_1__3", "NewInstancesLabel245_1__3Content");
check($_, "pmiPutLabel");
$_ = pmiPutLabel(PM_LABEL_CONTEXT, 0, 0, "ContextTrue", "True" );
check($_, "pmiPutLabel");
$_ = pmiPutLabel(PM_LABEL_CONTEXT, 0, 0, "ContextFalse", "False" );
check($_, "pmiPutLabel");
$_ = pmiPutLabel(PM_LABEL_CONTEXT, 0, 0, "ContextNull", "Null" );
check($_, "pmiPutLabel");

# These are replacements - which are ok.
    $_ = pmiPutLabel(PM_LABEL_CONTEXT, 0, 0, "NewContextLabel", "ReplacementContextLabelContent" );
check($_, "pmiPutLabel");
$_ = pmiPutLabel(PM_LABEL_DOMAIN, pmID_build(245,0,0), 0,
		  "NewDomainLabel245", "ReplacementDomainLabel245Content");
check($_, "pmiPutLabel");
$_ = pmiPutLabel(PM_LABEL_CLUSTER, pmID_build(245,0,0), 0,
		  "NewClusterLabel245_0", "ReplacementClusterLabel245_0Content");
check($_, "pmiPutLabel");
$_ = pmiPutLabel(PM_LABEL_ITEM, pmID_build(245,0,1), 0,
		  "NewItemLabel245_0_1", "ReplacementItemLabel245_0_1Content");
check($_, "pmiPutLabel");
$_ = pmiPutLabel(PM_LABEL_INDOM, pmInDom_build(245,1), 0,
		  "NewIndomLabel245_1", "ReplacementIndomLabel245_1Content");
check($_, "pmiPutLabel");
$_ = pmiPutLabel(PM_LABEL_INSTANCES, pmInDom_build(245,1), 1,
		  "NewInstancesLabel245_1__1", "ReplacementInstancesLabel245_1__1Content");
check($_, "pmiPutLabel");
$_ = pmiPutLabel(PM_LABEL_INSTANCES, pmInDom_build(245,1), 3,
		  "NewInstancesLabel245_1__3", "ReplacementInstancesLabel245_1__3Content");
check($_, "pmiPutLabel");

pmiDump();

$_ = pmiWrite(int(365.25*30*24*60*60), 0);
check($_, "pmiWrite");
$_ = pmiPutValueHandle($hdl1, "4321");
check($_, "pmiPutValueHandle");
$_ = pmiPutValue("my.metric.string", "", "a second string value");
check($_, "pmiPutValue");
$_ = pmiWrite(-1, -1);
check($_, "pmiWrite");
$_ = pmiPutValue("my.metric.string", "", "a third string value");
check($_, "pmiPutValue");
$_ = pmiWrite(-1, -1);
check($_, "pmiWrite");
$_ = pmiWrite(-1, -1);
check($_, "pmiWrite");

$_ = pmiPutMark();
check($_, "pmiPutMark");

$_ = pmiEnd();
check($_, "pmiEnd");

$ctx2 = pmiStart("myotherarchive", 1);
check($ctx2, "pmiStart");
$_ = pmiAddInstance(pmInDom_build(PMI_DOMAIN,1), "other", 2);
check($_, "pmiAddInstance");
$hdl2 = pmiGetHandle("my.metric.bar", "eek");
check($hdl2, "pmiGetHandle");
$_ = pmiPutValueHandle($hdl2, "6543210987654");
check($_, "pmiPutValueHandle");
$_ = pmiPutValueHandle(3, "error");
check($_, "pmiPutValueHandle");

pmiDump();
