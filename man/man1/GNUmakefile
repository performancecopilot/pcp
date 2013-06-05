#
# Copyright (c) 2012-2013 Red Hat.
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

MAN_PAGES	= chkhelp.1 dbpmda.1 mkaf.1 newhelp.1 pcp.1 pcpintro.1 \
	pmafm.1 pmcd.1 pmcd_wait.1 pmclient.1 pmdacisco.1 pmdamailq.1 \
	pmdasample.1 pmdasendmail.1 pmdasimple.1 pmdate.1 pmdatrace.1 \
	pmdatrivial.1 pmdatxmon.1 pmdbg.1 pmdumplog.1 pmerr.1 pmgenmap.1 \
	pmhostname.1 pmie.1 pmie_check.1 pminfo.1 pmlc.1 pmconfig.1 \
	pmlock.1 pmlogcheck.1 pmlogconf.1 pmlogextract.1 pmlogger.1 \
	pmlogger_daily.1 pmnewlog.1 pmnsadd.1 pmnsdel.1 \
	pmnsmerge.1 pmpost.1 pmprobe.1 pmsocks.1 pmstat.1 pmstore.1 \
	pmtrace.1 pmval.1 pmdaweblog.1 pmlogsummary.1 pmdashping.1 \
	pmloglabel.1 genpmda.1 pmproxy.1 pmdasummary.1 pmlogreduce.1 \
	autofsd-probe.1 pmie2col.1 telnet-probe.1 pmsleep.1 pmsignal.1 \
	pmieconf.1 pmiestatus.1 pmevent.1 pmcpp.1 pmlogrewrite.1 \
	pmdabash.1 pmatop.1 pmcollectl.1 pmwtf.1 collectl2pcp.1

# same guard as in src/pmwebapi/GNUmakefile
ifeq "$(HAVE_LIBMICROHTTPD)" "1"
MAN_PAGES	+= pmwebd.1
endif

MAN_DEST	= $(PCP_MAN_DIR)/man$(MAN_SECTION)
LSRCFILES	= $(MAN_PAGES)
LSRCFILES	+= pmtop.1 pmdahotproc.1 

default :: default_pcp 

default_pcp : $(MAN_PAGES)

install :: install_pcp

install_pcp : default_pcp
	@MAN_PAGES="$(MAN_PAGES)"; $(INSTALL_MAN)

include $(BUILDRULES)
