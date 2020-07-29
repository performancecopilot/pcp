#
# Copyright (c) 2001 Alan Bailey (bailey@mcs.anl.gov or abailey@ncsa.uiuc.edu) 
# for the portions of the code supporting the initial agent functionality.
#
# Copyright (c) 2001,2004 Silicon Graphics, Inc.  All Rights Reserved.
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

IAM	= mounts
DOMAIN	= MOUNTS
TARGETS	= $(IAM)$(EXECSUFFIX)
CFILES	= mounts.c
SCRIPTS	= Install Remove
DFILES	= README

PMDAADMDIR = $(PCP_PMDASADM_DIR)/$(IAM)
PMDACONFIG = $(PCP_SYSCONF_DIR)/$(IAM)
PMDATMPDIR = $(PCP_PMDAS_DIR)/$(IAM)
PMCHARTDIR = $(PCP_SYSCONF_DIR)/pmchart
PMCHARTVARDIR = $(PCP_VAR_DIR)/config/pmchart

LDIRT	= domain.h *.o $(IAM).log pmda$(IAM) pmda_$(IAM).so $(TARGETS) \
	help.pag help.dir
LLDLIBS	= $(PCP_PMDALIB)

default:	build-me

include $(BUILDRULES)

# This PMDA is only valid on platforms with a mount table (e.g. /proc/mounts)
ifeq "$(findstring $(TARGET_OS),mingw darwin)" ""
build-me:	$(TARGETS)

install:	default
	$(INSTALL) -m 755 -d $(PMDAADMDIR)
	$(INSTALL) -m 755 -d $(PMDATMPDIR)
	$(INSTALL) -m 755 -t $(PMDATMPDIR)/pmda$(IAM) $(IAM) $(PMDAADMDIR)/pmda$(IAM)
	$(INSTALL) -m 755 -t $(PMDATMPDIR) $(SCRIPTS) $(PMDAADMDIR)
	$(INSTALL) -m 644 -t $(PMDATMPDIR) $(DFILES) pmns help root domain.h $(PMDAADMDIR)
	$(INSTALL) -m 755 -d $(PMDACONFIG)
	$(INSTALL) -m 644 -t $(PMDATMPDIR)/mounts.conf mounts.conf $(PMDACONFIG)/mounts.conf
else
build-me:
install:
endif

$(IAM)$(EXECSUFFIX):	$(OBJECTS)

default_pcp:	default

install_pcp:	install

$(OBJECTS): domain.h

domain.h: ../../pmns/stdpmid
	$(DOMAIN_MAKERULE)
