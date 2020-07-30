#
# Copyright (c) 2012 Nathan Scott.  All Rights Reversed.
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

IAM		= bash
DOMAIN		= BASH

TARGETS		= $(IAM)$(EXECSUFFIX)
LLDLIBS		= $(PCP_PMDALIB)
PMDATMPDIR	= $(PCP_PMDAS_DIR)/$(IAM)
PMDAADMDIR	= $(PCP_PMDASADM_DIR)/$(IAM)
LDIRT		= domain.h *.log *.dir *.pag so_locations $(TARGETS)

CFILES		= event.c bash.c util.c
HFILES		= event.h
SCRIPTS		= bashproc.sh
SAMPLES		= test-child.sh test-trace.sh

default:	build-me

include $(BUILDRULES)

ifeq "$(TARGET_OS)" "mingw"
build-me:
install:
else
build-me: $(TARGETS)

install:	default
	$(INSTALL) -m 755 -d $(PMDAADMDIR)
	$(INSTALL) -m 755 -d $(PMDATMPDIR)
	$(INSTALL) -o $(PCP_USER) -g $(PCP_GROUP) -m 775 -d $(PCP_TMP_DIR)/$(IAM)
	$(INSTALL) -m 755 -t $(PMDATMPDIR)/pmda$(IAM) $(IAM) $(PMDAADMDIR)/pmda$(IAM)
	$(INSTALL) -m 755 -t $(PMDATMPDIR) Install Remove $(SAMPLES) $(PMDAADMDIR)
	$(INSTALL) -m 644 -t $(PMDATMPDIR) root help pmns domain.h $(PMDAADMDIR)
	$(INSTALL) -m 644 -t $(PCP_SHARE_DIR)/lib/bashproc.sh bashproc.sh $(PCP_LIBADM_DIR)/bashproc.sh
endif

$(IAM)$(EXECSUFFIX):	$(OBJECTS)

$(OBJECTS): domain.h
event.o bash.o util.o: event.h

domain.h: ../../pmns/stdpmid
	$(DOMAIN_MAKERULE)

default_pcp : default

install_pcp : install

$(OBJECTS):	$(TOPDIR)/src/include/pcp/libpcp.h
