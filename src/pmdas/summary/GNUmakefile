#
# Copyright (c) 1995-2001 Silicon Graphics, Inc.  All Rights Reserved.
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

IAM	= summary
DOMAIN	= SYSSUMMARY
TARGETS	= $(IAM)$(EXECSUFFIX)

HFILES	= summary.h
CFILES	= summary.c pmda.c mainloop.c

LSRCFILES = Install README Remove help pmns root summary.pmie
LLDFLAGS= -L$(TOPDIR)/src/libpcp/src -L$(TOPDIR)/src/libpcp_pmda/src
LLDLIBS	= $(PCP_PMDALIB)
LDIRT	= domain.h *.log *.dir *.pag  $(TARGETS)

PMDADIR	= $(PCP_PMDAS_DIR)/$(IAM)

default:	build-me

include $(TOPDIR)/src/include/buildrules

ifneq "$(TARGET_OS)" "mingw"
build-me:	$(TARGETS) 

install:	build-me
	$(INSTALL) -m 755 -d $(PMDADIR)
	$(INSTALL) -m 755 $(IAM) $(PMDADIR)/pmda$(IAM)
	$(INSTALL) -m 755 Install Remove $(PMDADIR)
	$(INSTALL) -m 644 root README help pmns domain.h $(PMDADIR)
	$(INSTALL) -m 644 summary.pmie $(PMDADIR)/expr.pmie
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
