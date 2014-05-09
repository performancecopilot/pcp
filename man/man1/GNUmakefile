#
# Copyright (c) 2012-2014 Red Hat.
# Copyright (c) 2000,2004 Silicon Graphics, Inc.  All Rights Reserved.
# 
# This program is free software; you can redistribute it and/or modify it
# under the terms of the GNU General Public License as published by the
# Free Software Foundation; either version 2 of the License, or (at your
# option) any later version.
# 
# This program is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
# or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
# for more details.
#

TOPDIR = ../..
include $(TOPDIR)/src/include/builddefs
-include ./GNUlocaldefs

MAN_SECTION	= 1

MAN_PAGES	= \
	chkhelp.1 dbpmda.1 mkaf.1 newhelp.1 pcp.1 pcpintro.1 pmafm.1 \
	pmcd.1 pmcd_wait.1 pmclient.1 pmconfig.1 pmdbg.1 pmdumplog.1 \
	pmerr.1 pmgenmap.1 pmhostname.1 pmie.1 pmie_check.1 pminfo.1 \
	pmlc.1 pmlock.1 pmlogcheck.1 pmlogconf.1 pmlogextract.1 \
	pmlogger.1 pmlogger_check.1 pmnewlog.1 pmnsadd.1 pmnsdel.1 \
	pmnsmerge.1 pmpost.1 pmprobe.1 pmsocks.1 pmstat.1 pmstore.1 \
	pmtrace.1 pmval.1 pmlogsummary.1 pmdate.1 pmlogmv.1 \
	pmloglabel.1 genpmda.1 pmproxy.1 pmlogreduce.1 \
	autofsd-probe.1 pmie2col.1 telnet-probe.1 pmsleep.1 pmsignal.1 \
	pmieconf.1 pmiestatus.1 pmevent.1 pmcpp.1 pmlogrewrite.1 \
	pmatop.1 pmcollectl.1 pmwtf.1 collectl2pcp.1 pmmgr.1 pmfind.1 \
	pmchart.1 pmdumptext.1 pmquery.1 pmsnap.1 pmtime.1 \
	\
	pmdaapache.1 pmdabash.1 pmdacisco.1 \
	pmdagfs2.1 pmdagluster.1 \
	pmdakernel.1 \
	pmdalogger.1 pmdamailq.1 pmdammv.1 pmdamounts.1 \
	pmdasample.1 pmdasendmail.1 pmdashping.1 pmdasimple.1 \
	pmdasummary.1 \
	pmdatrace.1 pmdatrivial.1 pmdatxmon.1 pmdaweblog.1 \
	pmdazswap.1

LINUX_PMDA_PAGES = \
	pmdalmsensors.1 pmdalustrecomm.1 pmdaproc.1 pmdaxfs.1 pmdajbd2.1
ROOMTEMP_PMDA_PAGES = pmdaroomtemp.1
SYSTEMD_PMDA_PAGES = pmdasystemd.1
RPM_PMDA_PAGES = pmdarpm.1
IB_PMDA_PAGES = pmdaib.1
WEBD_PAGES = pmwebd.1

ifeq "$(TARGET_OS)" "linux"
MAN_PAGES	+= $(LINUX_PMDA_PAGES)
else
OTHER_PAGES	+= $(LINUX_PMDA_PAGES)
endif
ifneq "$(findstring $(TARGET_OS),solaris linux)" ""
MAN_PAGES	+= $(ROOMTEMP_PMDA_PAGES)
else
OTHER_PAGES	+= $(ROOMTEMP_PMDA_PAGES)
endif
ifeq "$(HAVE_RPMLIB)" "1"
MAN_PAGES	+= $(RPM_PMDA_PAGES)
else
OTHER_PAGES	+= $(RPM_PMDA_PAGES)
endif
ifneq "$(PMDA_SYSTEMD)" ""
MAN_PAGES	+= $(SYSTEMD_PMDA_PAGES)
else
OTHER_PAGES	+= $(SYSTEMD_PMDA_PAGES)
endif
ifneq "$(PMDA_INFINIBAND)" ""
MAN_PAGES	+= $(IB_PMDA_PAGES)
else
OTHER_PAGES	+= $(IB_PMDA_PAGES)
endif
ifeq "$(HAVE_LIBMICROHTTPD)" "1"
MAN_PAGES	+= $(WEBD_PAGES)
else
OTHER_PAGES	+= $(WEBD_PAGES)
endif

MAN_DEST	= $(PCP_MAN_DIR)/man$(MAN_SECTION)
LSRCFILES	= $(MAN_PAGES) $(OTHER_PAGES)

default :: default_pcp 

default_pcp : $(MAN_PAGES)

install :: install_pcp

install_pcp : default_pcp
	@MAN_PAGES="$(MAN_PAGES)"; $(INSTALL_MAN)

include $(BUILDRULES)
