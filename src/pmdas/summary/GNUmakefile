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

IAM	= summary
DOMAIN	= SYSSUMMARY
TARGETS	= $(IAM)$(EXECSUFFIX)

HFILES	= summary.h
CFILES	= summary.c pmda.c mainloop.c

LSRCFILES = Install README Remove help pmns root summary.pmie
LLDFLAGS= -L$(TOPDIR)/src/libpcp/src -L$(TOPDIR)/src/libpcp_pmda/src
LLDLIBS	= $(PCP_PMDALIB)
LDIRT	= domain.h *.log *.dir *.pag  $(TARGETS)

PMDAADMDIR = $(PCP_PMDASADM_DIR)/$(IAM)
PMDACONFIG = $(PCP_SYSCONF_DIR)/$(IAM)
PMDATMPDIR = $(PCP_PMDAS_DIR)/$(IAM)

default:	build-me

include $(TOPDIR)/src/include/buildrules

ifneq "$(TARGET_OS)" "mingw"
build-me:	$(TARGETS) 

install:	build-me
	$(INSTALL) -m 755 -d $(PMDAADMDIR)
	$(INSTALL) -m 755 -d $(PMDATMPDIR)
	$(INSTALL) -m 755 -d $(PMDCONFIG)
	$(INSTALL) -m 755 -t $(PMDATMPDIR)/pmda$(IAM) $(IAM) $(PMDAADMDIR)/pmda$(IAM)
	$(INSTALL) -m 755 -t $(PMDATMPDIR) Install Remove $(PMDAADMDIR)
	$(INSTALL) -m 644 -t $(PMDATMPDIR) root README help pmns domain.h $(PMDAADMDIR)
	$(INSTALL) -m 644 -t $(PMDATMPDIR)/expr.pmie summary.pmie $(PMDACONFIG)/expr.pmie
else
build-me:
install:
endif

$(IAM)$(EXECSUFFIX):	$(OBJECTS)

mainloop.o summary.o pmda.o:	summary.h

$(OBJECTS): domain.h

domain.h:	../../pmns/stdpmid
	$(DOMAIN_MAKERULE)

default_%:	default
	@true

install_%:	install
	@true

mainloop.o pmda.o summary.o:	$(TOPDIR)/src/include/pcp/libpcp.h
