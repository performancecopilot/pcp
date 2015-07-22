#
#  Copyright (c) 2001 Alan Bailey (bailey@mcs.anl.gov or abailey@ncsa.uiuc.edu)
#  for the portions of the code supporting the initial agent functionality.
#  All rights reserved.
# Copyright (c)  2001,2004 Silicon Graphics, Inc.  All Rights Reserved.
# Copyright (c) 2015 Red Hat.
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


IAM	= process
DOMAIN	= PROCESS
TARGETS	= $(IAM)
CFILES	= process.c
SCRIPTS	= Install Remove
DFILES	= README
LSRCFILES= $(SCRIPTS) pmns help root $(DFILES) process.conf

PMDADIR	= $(PCP_PMDAS_DIR)/$(IAM)
PMCHART	= $(PCP_VAR_DIR)/config/pmchart

LDIRT	= domain.h *.o $(IAM).log pmda$(IAM) pmda_$(IAM).so $(TARGETS) \
	help.dir help.pag
LLDLIBS	= $(PCP_PMDALIB)

default: build-me

include $(BUILDRULES)

# This PMDA is only valid on platforms with a procfs
# It is also superceded by the cgroup functionality on Linux
# thus has not been built for some time, for reference only.
#build-me: $(TARGETS)
#install install_pcp : default
#	$(INSTALL) -m 755 -d $(PMDADIR)
#	$(INSTALL) -m 755 $(IAM) $(PMDADIR)/pmda$(IAM)
#	$(INSTALL) -m 755 $(SCRIPTS) $(PMDADIR)
#	$(INSTALL) -m 644 $(DFILES) pmns help root domain.h $(PMDADIR)
#	$(INSTALL) -m 644 process.conf $(PMDADIR)/process.conf
#else
build-me:
install:
#endif

$(IAM):	$(OBJECTS)

default_pcp : default

install_pcp : install

$(OBJECTS): domain.h

domain.h: ../../pmns/stdpmid
	$(DOMAIN_MAKERULE)
