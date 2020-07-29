#
# Copyright (c) 2000-2001,2003,2004 Silicon Graphics, Inc.  All Rights Reserved.
# Copyright (c) 2020 Red Hat.
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

DFILES		= help
LTARGETS	= help.dir
LDIRT		= *.log *.dir *.pag domain.h so_locations
TARGETS		= help.dir

SUBDIRS = src

PMDATMPDIR = $(PCP_PMDAS_DIR)/pmcd
PMDAADMDIR = $(PCP_PMDASADM_DIR)/pmcd
REWRITEDIR = $(PCP_SYSCONF_DIR)/pmlogrewrite
REWRITEVARDIR = $(PCP_VAR_DIR)/config/pmlogrewrite
CONF_LINE = "pmcd	2	dso	pmcd_init	$(PMDATMPDIR)/pmda_pmcd.$(DSOSUFFIX)"

default_pcp default :: $(TARGETS) 

default_pcp default :: $(SUBDIRS) 
	$(SUBDIRS_MAKERULE)
	@if [ -f ../pmcd.conf ]; then \
	    if [ `grep -c $(CONF_LINE) ../pmcd.conf` -eq 0 ]; then \
		echo $(CONF_LINE) >> ../pmcd.conf ; \
	    fi; \
	fi

install_pcp install :: $(SUBDIRS)
	$(SUBDIRS_MAKERULE)

install_pcp install :: $(SUBDIRS)
	$(INSTALL) -m 755 -d $(PMDAADMDIR)
	$(INSTALL) -m 755 -d $(PMDATMPDIR)
	$(INSTALL) -m 644 -t $(PMDATMPDIR) help.dir help.pag $(PMDAADMDIR)
	$(INSTALL) -m 644 -t $(PCP_PMNS_DIR)/root_pmcd root_pmcd $(PCP_PMNSADM_DIR)/root_pmcd
	$(INSTALL) -m 644 -t $(REWRITEVARDIR)/pmcd_migrate.conf migrate.conf $(REWRITEDIR)/pmcd_migrate.conf

help.dir: help root_pmcd 
	$(NEWHELP) -v 2 -n root_pmcd -o help < help

include $(BUILDRULES)
