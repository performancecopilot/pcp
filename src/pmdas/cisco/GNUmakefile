#
# Copyright (c) 2014-2015,2020 Red Hat.
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

IAM	= cisco
DOMAIN	= CISCO
TARGETS	= $(IAM)$(EXECSUFFIX) probe$(EXECSUFFIX)
CFILES	= cisco.c pmda.c interface.c telnet.c
HFILES	= cisco.h 
DFILES	= README

LLDLIBS	= $(PCP_PMDALIB) $(LIB_FOR_PTHREADS)

PMDATMPDIR = $(PCP_PMDAS_DIR)/$(IAM)
PMDAADMDIR = $(PCP_PMDASADM_DIR)/$(IAM)
PMCHARTDIR	= $(PCP_SYSCONF_DIR)/pmchart
PMCHARTVARDIR	= $(PCP_VAR_DIR)/config/pmchart
PMIEDIR		= $(PCP_SYSCONF_DIR)/pmieconf/$(IAM)
PMIEVARDIR	= $(PCP_VAR_DIR)/config/pmieconf/$(IAM)

LDIRT	= domain.h cisco.log *.dir *.pag so_locations a.out probe.o $(TARGETS)

default:	build-me

include $(BUILDRULES)

ifneq "$(TARGET_OS)" "mingw"
build-me:	$(TARGETS)

install:	build-me
	$(INSTALL) -m 755 -d $(PMDAADMDIR)
	$(INSTALL) -m 755 -d $(PMDATMPDIR)
	$(INSTALL) -m 755 -t $(PMDATMPDIR)/pmda$(IAM) $(IAM) $(PMDAADMDIR)/pmda$(IAM) 
	$(INSTALL) -m 755 -t $(PMDATMPDIR)/parse parse.sh $(PMDAADMDIR)/parse
	$(INSTALL) -m 755 -t $(PMDATMPDIR) probe Install Remove $(PMDAADMDIR)
	$(INSTALL) -m 644 -t $(PMDATMPDIR) $(DFILES) root help pmns domain.h $(PMDAADMDIR)
ifeq "$(ENABLE_QT)" "true"
	$(INSTALL) -m 644 -t $(PMCHARTVARDIR)/Cisco Cisco.pmchart $(PMCHARTDIR)/Cisco
endif
	$(INSTALL) -m 755 -d $(PMIEDIR)
	$(INSTALL) -m 755 -d $(PMIEVARDIR)
	$(INSTALL) -m 644 -t $(PMIEVARDIR)/in_util cisco.in_util.pmie $(PMIEDIR)/in_util
	$(INSTALL) -m 644 -t $(PMIEVARDIR)/out_util cisco.out_util.pmie $(PMIEDIR)/out_util
else
build-me:
install:
endif

$(IAM)$(EXECSUFFIX):	$(OBJECTS)

probe$(EXECSUFFIX):	probe.o interface.o telnet.o
	$(CCF) -o $@ $(LDFLAGS) probe.o interface.o telnet.o $(LDLIBS)

$(OBJECTS) probe.o: domain.h

domain.h:	../../pmns/stdpmid
	$(DOMAIN_MAKERULE)

default_pcp:	default

install_pcp:	install

$(OBJECTS) probe.o:	$(TOPDIR)/src/include/pcp/libpcp.h
