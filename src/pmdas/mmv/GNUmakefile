#
# Copyright (c) 2013 Red Hat.
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
LSRCFILES = $(LCFILES) README.demos Makefile.demos
TARGETS = $(CMDTARGET) $(LTARGET)

LLDFLAGS = -L$(TOPDIR)/src/libpcp_mmv/src -L$(TOPDIR)/src/libpcp/src
LLDLIBS = -lpcp_mmv $(PCPLIB)
LDIRT = mmvdump acme

PMDADIR = $(PCP_PMDAS_DIR)/$(IAM)
DEMODIR = $(PCP_DEMOS_DIR)/$(IAM)
DEMOFILES = README.demos Makefile.demos
CONF_LINE = "mmv	70	dso	mmv_init	$(PCP_PMDAS_DIR)/mmv/pmda_mmv.$(DSOSUFFIX)"

default_pcp default :: $(TARGETS)

default_pcp default :: $(SUBDIRS)
	$(SUBDIRS_MAKERULE)
	@if [ -f ../pmcd.conf ]; then \
	    if [ `grep -c $(CONF_LINE) ../pmcd.conf` -eq 0 ]; then \
		echo $(CONF_LINE) >> ../pmcd.conf ; \
	    fi; \
	fi

include $(BUILDRULES)

install_pcp install :: $(SUBDIRS)
	$(SUBDIRS_MAKERULE)

install_pcp install :: $(SUBDIRS)
	$(INSTALL) -m 755 -d $(PMDADIR)
	$(INSTALL) -m 755 $(CMDTARGET) $(PMDADIR)/$(CMDTARGET)
	$(INSTALL) -m 755 -d $(DEMODIR)
	$(INSTALL) -m 644 Makefile.demos $(DEMODIR)/Makefile
	$(INSTALL) -m 644 README.demos $(DEMODIR)/README
	$(INSTALL) -m 644 $(CFILES) $(LCFILES) $(DEMODIR)

# check-build only, binary not installed (but source is)
$(LTARGET):	acme.c
	$(CCF) -o $@ $^ $(LDFLAGS) $(LDLIBS)
