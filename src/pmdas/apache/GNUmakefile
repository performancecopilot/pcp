#
# Copyright (C) 2000 Michal Kara.  All Rights Reserved.
# Copyright (c) 2015-2016,2020 Red Hat.
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

IAM	= apache
DOMAIN	= APACHE
TARGETS	= $(IAM)$(EXECSUFFIX)
CFILES	= apache.c
SCRIPTS	= Install Remove
DFILES	= README

PMDAADMDIR	= $(PCP_PMDASADM_DIR)/$(IAM)
PMDAVARDIR	= $(PCP_PMDAS_DIR)/$(IAM)
PMCHARTETCDIR	= $(PCP_SYSCONF_DIR)/pmchart
PMCHARTVARDIR	= $(PCP_VAR_DIR)/config/pmchart
PMLOGCONFETCDIR	= $(PCP_SYSCONF_DIR)/pmlogconf/$(IAM)
PMLOGCONFVARDIR	= $(PCP_VAR_DIR)/config/pmlogconf/$(IAM)

LDIRT	= domain.h *.o $(IAM).log pmda$(IAM) pmda_$(IAM).so $(TARGETS)
LLDFLAGS= -L$(TOPDIR)/src/libpcp_web/src
LLDLIBS	= $(PCP_WEBLIB)

default: build-me

include $(BUILDRULES)

build-me: $(TARGETS)

install: build-me
	$(INSTALL) -m 755 -d $(PMDAADMDIR)
	$(INSTALL) -m 755 -d $(PMDAVARDIR)
	$(INSTALL) -m 755 -t $(PMDAVARDIR)/pmda$(IAM) $(TARGETS) $(PMDAADMDIR)/pmda$(IAM)
	$(INSTALL) -m 755 -t $(PMDAVARDIR) $(SCRIPTS) $(PMDAADMDIR)
	$(INSTALL) -m 644 -t $(PMDAVARDIR) $(DFILES) pmns help root domain.h $(PMDAADMDIR)
ifeq "$(ENABLE_QT)" "true"
	$(INSTALL) -m 644 -t $(PMCHARTVARDIR)/Apache Apache.pmchart $(PMCHARTETCDIR)/Apache
endif
	$(INSTALL) -m 755 -d $(PMLOGCONFETCDIR)
	$(INSTALL) -m 755 -d $(PMLOGCONFVARDIR)
	$(INSTALL) -m 644 -t $(PMLOGCONFVARDIR)/processes pmlogconf.processes $(PMLOGCONFETCDIR)/processes
	$(INSTALL) -m 644 -t $(PMLOGCONFVARDIR)/summary pmlogconf.summary $(PMLOGCONFETCDIR)/summary
	$(INSTALL) -m 644 -t $(PMLOGCONFVARDIR)/uptime pmlogconf.uptime $(PMLOGCONFETCDIR)/uptime

$(TARGETS):	$(OBJECTS)

$(OBJECTS): domain.h

domain.h:	../../pmns/stdpmid
	$(DOMAIN_MAKERULE)

default_pcp : default

install_pcp : install
