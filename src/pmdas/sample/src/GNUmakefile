#
# Copyright (c) 2014-2015 Red Hat.
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

TOPDIR = ../../../..
include $(TOPDIR)/src/include/builddefs

#
# The TARGETS macro must be set to the name of the executable we
# are building.
#
IAM = sample
DOMAIN = SAMPLE
CMDTARGET = pmda$(IAM)$(EXECSUFFIX)
LIBTARGET = pmda_$(IAM).$(DSOSUFFIX)
TARGETS = $(CMDTARGET) $(LIBTARGET)
PMDAINIT = $(IAM)_init

PMDAADMDIR = $(PCP_PMDASADM_DIR)/sample
PMDATMPDIR = $(PCP_PMDAS_DIR)/sample

CFILES = pmda.c sample.c percontext.c events.c proc.c
HFILES = percontext.h events.h libpcp.h proc.h
VERSION_SCRIPT = exports

LDIRT = domain.h libpcp.h sample.log $(TARGETS) $(VERSION_SCRIPT)
LCFLAGS += $(INVISIBILITY)
LLDLIBS = $(PCP_PMDALIB) $(LIB_FOR_RT)

default:	$(TARGETS)

pmda.o sample.o percontext.o: percontext.h
sample.o events.o: events.h
pmda.o: $(VERSION_SCRIPT)

$(OBJECTS): domain.h $(HFILES)

domain.h: ../../../pmns/stdpmid
	$(DOMAIN_MAKERULE)

$(VERSION_SCRIPT):
	$(VERSION_SCRIPT_MAKERULE)

install:	default
	$(INSTALL) -m 755 -d $(PMDAADMDIR)
	$(INSTALL) -m 755 -d $(PMDATMPDIR)
	$(INSTALL) -m 644 -t $(PMDATMPDIR) domain.h $(CFILES) $(HFILES) $(PMDAADMDIR)
	$(INSTALL) -m 644 -t $(PMDATMPDIR)/Makefile GNUmakefile.install $(PMDAADMDIR)/Makefile
	$(INSTALL) -m 755 -t $(PMDATMPDIR) $(TARGETS) $(PMDAADMDIR)

include $(BUILDRULES)

default_pcp:	default

install_pcp:	install

libpcp.h:	$(TOPDIR)/src/include/pcp/libpcp.h
	rm -f libpcp.h
	$(LN_S) $(TOPDIR)/src/include/pcp/libpcp.h libpcp.h

events.o pmda.o sample.o:	$(TOPDIR)/src/include/pcp/libpcp.h
proc.o:		proc.h
