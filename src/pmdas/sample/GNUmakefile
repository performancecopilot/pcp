#
# Copyright (c) 2014-2015,2020 Red Hat.  All Rights Reserved.
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

TOPDIR = ../../..
include $(TOPDIR)/src/include/builddefs

DFILES	= README help
LDIRT = *.dir *.pag

SUBDIRS = src

PMDAADMDIR = $(PCP_PMDASADM_DIR)/sample
PMDATMPDIR = $(PCP_PMDAS_DIR)/sample
PMCHARTDIR = $(PCP_SYSCONF_DIR)/pmchart
PMCHARTVARDIR = $(PCP_VAR_DIR)/config/pmchart
REWRITEVARDIR = $(PCP_VAR_DIR)/config/pmlogrewrite

default_pcp default :: $(SUBDIRS)
	$(SUBDIRS_MAKERULE)

install_pcp install :: $(SUBDIRS)
	$(SUBDIRS_MAKERULE)

install_pcp install :: default
	$(INSTALL) -m 755 -d $(PMDAADMDIR)
	$(INSTALL) -m 755 -d $(PMDATMPDIR)
	$(INSTALL) -m 755 -t $(PMDATMPDIR) Install Remove $(PMDAADMDIR)
	$(INSTALL) -m 644 -t $(PMDATMPDIR) $(DFILES) pmns root $(PMDAADMDIR)
ifeq "$(ENABLE_QT)" "true"
	$(INSTALL) -m 644 -t $(PMCHARTVARDIR)/Sample Sample.pmchart $(PMCHARTDIR)/Sample
endif
	$(INSTALL) -m 755 $(TOPDIR)/src/pmcheck/pmda.template $(PCP_SHARE_DIR)/lib/pmcheck/pmda-sample
	$(INSTALL) -m 644 -t $(REWRITEVARDIR)/sample_migrate.conf migrate.conf $(REWRITEDIR)/sample_migrate.conf

$(OBJECTS): domain.h

domain.h:	../../../pmns/stdpmid
	$(DOMAIN_MAKERULE)

include $(BUILDRULES)
