#
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

IAM	= pipe
CMDTARGET = pmdapipe$(EXECSUFFIX)
CFILES	= event.c util.c pipe.c
HFILES	= event.h util.h
LLDLIBS = $(PCP_PMDALIB)
DOMAIN	= PIPE
LDIRT	= domain.h *.o $(IAM).log $(CMDTARGET)

PMDAADMDIR = $(PCP_PMDASADM_DIR)/$(IAM)
PMDACONFIG = $(PCP_SYSCONF_DIR)/$(IAM)
PMDATMPDIR = $(PCP_PMDAS_DIR)/$(IAM)

default: build-me

include $(BUILDRULES)

ifneq "$(TARGET_OS)" "mingw"
build-me: $(CMDTARGET)

install:	build-me
	$(INSTALL) -m 755 -d $(PMDAADMDIR)
	$(INSTALL) -m 755 -d $(PMDATMPDIR)
	$(INSTALL) -m 755 -t $(PMDATMPDIR) Install Remove $(PMDAADMDIR)
	$(INSTALL) -m 644 -t $(PMDATMPDIR) README root help pmns domain.h $(PMDAADMDIR)
	$(INSTALL) -m 755 -t $(PMDATMPDIR)/$(CMDTARGET) $(CMDTARGET) $(PMDAADMDIR)/$(CMDTARGET)
	$(INSTALL) -m 755 -d $(PMDACONFIG)
	$(INSTALL) -m 755 -d $(PMDACONFIG).conf.d
	$(INSTALL) -m 644 -t $(PMDATMPDIR)/sample.conf pipe.conf $(PMDACONFIG)/sample.conf
else
build-me:
install:
endif

event.o pipe.o: event.h
util.o event.o pipe.o: util.h

default_pcp : default

install_pcp : install

$(OBJECTS): domain.h

domain.h: ../../pmns/stdpmid
	$(DOMAIN_MAKERULE)

event.o util.o:	$(TOPDIR)/src/include/pcp/libpcp.h
