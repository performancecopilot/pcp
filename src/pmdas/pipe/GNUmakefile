#
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

CMDTARGET = pmdapipe$(EXECSUFFIX)
CFILES	= event.c util.c pipe.c
HFILES	= event.h util.h
LLDLIBS = $(PCP_PMDALIB)

IAM	= pipe
DOMAIN	= PIPE
PMDADIR	= $(PCP_PMDAS_DIR)/$(IAM)

LDIRT	= domain.h *.o $(IAM).log $(CMDTARGET)

default: build-me

include $(BUILDRULES)

ifneq "$(TARGET_OS)" "mingw"
build-me: $(CMDTARGET)

install:	build-me
	$(INSTALL) -m 755 -d $(PMDADIR)
	$(INSTALL) -m 755 Install Remove $(PMDADIR)
	$(INSTALL) -m 644 README root help pmns domain.h $(PMDADIR)
	$(INSTALL) -m 644 pipe.conf $(PMDADIR)/sample.conf
	$(INSTALL) -m 755 -d $(PCP_SYSCONF_DIR)/pipe.conf.d
	$(INSTALL) -m 755 $(CMDTARGET) $(PMDADIR)/$(CMDTARGET)
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
