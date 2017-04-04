#
# Copyright (c) 2000,2003,2004 Silicon Graphics, Inc.  All Rights Reserved.
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

TOPDIR = ../../../..
include $(TOPDIR)/src/include/builddefs

IAM		= trace
DOMAIN		= TRACE
CMDTARGET	= pmdatrace$(EXECSUFFIX)
PMDADIR		= $(PCP_PMDAS_DIR)/$(IAM)

CFILES		= trace.c client.c comms.c data.c pmda.c
HFILES		= data.h client.h comms.h

LCFLAGS		= -I$(TOPDIR)/src/libpcp_trace/src
LLDFLAGS	= -L$(TOPDIR)/src/libpcp_trace/src
LLDLIBS		= -lpcp_trace $(PCP_PMDALIB)

LDIRT		= *.log *.dir *.pag domain.h $(TARGETS)

default:	build-me

include $(BUILDRULES)

ifneq "$(TARGET_OS)" "mingw"
build-me:	$(CMDTARGET)

install:	build-me
	$(INSTALL) -m 755 -d $(PMDADIR)
	$(INSTALL) -m 755 $(CMDTARGET) $(PMDADIR)/$(CMDTARGET)
	$(INSTALL) -m 644 domain.h $(PMDADIR)/domain.h
else
build-me:
install:
endif

$(IAM)$(EXECSUFFIX):	$(OBJECTS)

$(OBJECTS): domain.h

domain.h:	../../../pmns/stdpmid
	$(DOMAIN_MAKERULE)

default_pcp:	default

install_pcp:	install
