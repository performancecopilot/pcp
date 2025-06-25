#
# Copyright (c) 1995-2001 Silicon Graphics, Inc.  All Rights Reserved.
# Copyright (c) 2015,2020 Red Hat.
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

IAM	= shping
DOMAIN	= SHPING

CMDTARGETS	= $(IAM)$(EXECSUFFIX)
HFILES	= shping.h
CFILES	= shping.c pmda.c
CONFIG	= sample.conf
OTHERS	= README root help pmns
CHARTS	= shping.CPUTime.pmchart shping.RealTime.pmchart
PMIECS	= shping.status.pmie shping.response.pmie
LDIRT	= *.log *.dir *.pag so_locations a.out domain.h $(CMDTARGETS)
LLDLIBS	= $(PCP_PMDALIB) $(LIB_FOR_PTHREADS)

PMDATMPDIR	= $(PCP_PMDAS_DIR)/$(IAM)
PMDACONFIG	= $(PCP_SYSCONF_DIR)/$(IAM)
PMDAADMDIR	= $(PCP_PMDASADM_DIR)/$(IAM)
PMCHARTDIR	= $(PCP_SYSCONF_DIR)/pmchart
PMCHARTVARDIR	= $(PCP_VAR_DIR)/config/pmchart
PMIECONFDIR	= $(PCP_SYSCONF_DIR)/pmieconf/$(IAM)
PMIECONFVARDIR	= $(PCP_VAR_DIR)/config/pmieconf/$(IAM)
PMLOGCONFDIR	= $(PCP_SYSCONF_DIR)/pmlogconf/$(IAM)
PMLOGCONFVARDIR	= $(PCP_VAR_DIR)/config/pmlogconf/$(IAM)
REWRITEDIR	= $(PCP_SYSCONF_DIR)/pmlogrewrite
REWRITEVARDIR	= $(PCP_VAR_DIR)/config/pmlogrewrite

default:	build-me

include $(TOPDIR)/src/include/buildrules

ifneq "$(TARGET_OS)" "mingw"
build-me:	$(CMDTARGETS)

install: build-me configs
	$(INSTALL) -m 755 -d $(PMDAADMDIR)
	$(INSTALL) -m 755 -d $(PMDATMPDIR)
	$(INSTALL) -m 755 -t $(PMDATMPDIR)/pmda$(IAM) $(IAM) $(PMDAADMDIR)/pmda$(IAM) 
	$(INSTALL) -m 755 -t $(PMDATMPDIR) Install Remove $(PMDAADMDIR)
	$(INSTALL) -m 644 -t $(PMDATMPDIR) $(OTHERS) domain.h $(PMDAADMDIR)
	$(INSTALL) -m 755 -d $(PMDACONFIG)
	$(INSTALL) -m 644 -t $(PMDATMPDIR)/$(CONFIG) $(CONFIG) $(PMDACONFIG)/$(CONFIG)
	$(INSTALL) -m 644 -t $(REWRITEVARDIR)/shping_migrate.conf migrate.conf $(REWRITEDIR)/shping_migrate.conf
else
build-me:
install: configs
endif

configs:
ifeq "$(ENABLE_QT)" "true"
	$(INSTALL) -m 644 -t $(PMCHARTVARDIR)/shping.CPUTime shping.CPUTime.pmchart $(PMCHARTDIR)/shping.CPUTime
	$(INSTALL) -m 644 -t $(PMCHARTVARDIR)/shping.RealTime shping.RealTime.pmchart $(PMCHARTDIR)/shping.RealTime
endif
	$(INSTALL) -m 755 -d $(PMIECONFDIR)
	$(INSTALL) -m 755 -d $(PMIECONFVARDIR)
	$(INSTALL) -m 644 -t $(PMIECONFVARDIR)/status shping.status.pmie $(PMIECONFDIR)/status
	$(INSTALL) -m 644 -t $(PMIECONFVARDIR)/response shping.response.pmie $(PMIECONFDIR)/response
	$(INSTALL) -m 755 -d $(PMLOGCONFDIR)
	$(INSTALL) -m 755 -d $(PMLOGCONFVARDIR)
	$(INSTALL) -m 644 -t $(PMLOGCONFVARDIR)/summary pmlogconf.summary $(PMLOGCONFDIR)/summary

$(IAM)$(EXECSUFFIX):	$(OBJECTS)

$(OBJECTS): domain.h

domain.h: ../../pmns/stdpmid
	$(DOMAIN_MAKERULE)

default_pcp:	default

install_pcp:	install

shping.o:	$(TOPDIR)/src/include/pcp/libpcp.h
