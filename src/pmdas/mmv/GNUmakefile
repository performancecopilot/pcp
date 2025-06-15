#
# Copyright (c) 2013,2016,2020 Red Hat.
# Copyright (c) 2009-2010 Aconex.  All Rights Reserved.
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

TOPDIR = ../../..
include $(TOPDIR)/src/include/builddefs

IAM = mmv
CMDTARGET = mmvdump$(EXECSUFFIX)
CFILES = mmvdump.c

SUBDIRS = src

LCFILES = acme.c
LTARGET = acme$(EXECSUFFIX)
TARGETS = $(CMDTARGET) $(LTARGET)

LLDFLAGS = -L$(TOPDIR)/src/libpcp_mmv/src -L$(TOPDIR)/src/libpcp/src
LLDLIBS = -lpcp_mmv $(PCPLIB)
LDIRT = mmvdump acme

PMDAADMDIR = $(PCP_PMDASADM_DIR)/$(IAM)
PMDATMPDIR = $(PCP_PMDAS_DIR)/$(IAM)
LOGCONFVARDIR = $(PCP_VAR_DIR)/config/pmlogconf/$(IAM)
LOGCONFDIR = $(PCP_SYSCONF_DIR)/pmlogconf/$(IAM)
DEMODIR = $(PCP_DEMOS_DIR)/$(IAM)
DEMOFILES = README.demos Makefile.demos
REWRITEVARDIR = $(PCP_VAR_DIR)/config/pmlogrewrite
CONF_LINE_MMV = "mmv	70	dso	mmv_init	$(PMDATMPDIR)/pmda_mmv.$(DSOSUFFIX)"
CONF_LINE_PMPROXY = "pmproxy	4	dso	pmproxy_init	$(PMDATMPDIR)/pmda_mmv.$(DSOSUFFIX)"

default_pcp default :: $(TARGETS)

default_pcp default :: $(SUBDIRS)
	$(SUBDIRS_MAKERULE)
	@if [ -f ../pmcd.conf ]; then \
	    if [ `grep -c $(CONF_LINE_PMPROXY) ../pmcd.conf` -eq 0 ]; then \
		echo $(CONF_LINE_PMPROXY) >> ../pmcd.conf ; \
	    fi; \
	    if [ `grep -c $(CONF_LINE_MMV) ../pmcd.conf` -eq 0 ]; then \
		echo $(CONF_LINE_MMV) >> ../pmcd.conf ; \
	    fi; \
	fi

include $(BUILDRULES)

install_pcp install :: $(SUBDIRS)
	$(SUBDIRS_MAKERULE)

install_pcp install :: $(SUBDIRS)
	$(INSTALL) -m 755 -d $(PMDAADMDIR)
	$(INSTALL) -m 755 -d $(PMDATMPDIR)
	$(INSTALL) -m 755 -t $(PMDATMPDIR)/$(CMDTARGET) $(CMDTARGET) $(PMDAADMDIR)/$(CMDTARGET)
	$(INSTALL) -m 755 -d $(DEMODIR)
	$(INSTALL) -m 644 Makefile.demos $(DEMODIR)/Makefile
	$(INSTALL) -m 644 README.demos $(DEMODIR)/README
	$(INSTALL) -m 644 $(CFILES) $(LCFILES) $(DEMODIR)
	$(INSTALL) -m 755 -d $(LOGCONFDIR)
	$(INSTALL) -m 755 -d $(LOGCONFVARDIR)
	$(INSTALL) -m 644 -t $(LOGCONFVARDIR)/summary pmlogconf.summary $(LOGCONFDIR)/summary
	$(INSTALL) -m 644 -t $(REWRITEVARDIR)/mmv_migrate.conf migrate.conf $(REWRITEDIR)/mmv_migrate.conf

# check-build only, binary not installed (but source is)
$(LTARGET):	acme.c
	$(CCF) -o $@ $^ $(LDFLAGS) $(LDLIBS)

mmvdump.o:	$(TOPDIR)/src/include/pcp/libpcp.h
