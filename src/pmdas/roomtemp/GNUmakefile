#
# Copyright (c) 2000-2001,2003 Silicon Graphics, Inc.  All Rights Reserved.
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

IAM	= roomtemp
HFILES = dsread.h
CFILES	= roomtemp.c dsread.c
HFILES	= dsread.h
CMDTARGET = pmdaroomtemp
LLDFLAGS = -L.
LLDLIBS = -lmlan $(PCP_PMDALIB)
LCFLAGS = -I.
DFILES	= README help
DOMAIN	= ROOMTEMP
LDIRT	= domain.h $(IAM).log pmda$(IAM) pmda_$(IAM).so libmlan.a

SUBDIRS = mlan

PMDAADMDIR = $(PCP_PMDASADM_DIR)/$(IAM)
PMDATMPDIR = $(PCP_PMDAS_DIR)/$(IAM)

MAN_SECTION = 1
MAN_PAGES = pmda$(IAM).$(MAN_SECTION)
MAN_DEST = $(PCP_MAN_DIR)/man$(MAN_SECTION)

default: build-me

include $(BUILDRULES)

.NOTPARALLEL:

ifeq "$(findstring $(TARGET_OS),solaris linux)" ""
build-me:
	$(SUBDIRS_MAKERULE)

install:
else
build-me: libmlan.a $(CMDTARGET)

install: default 
	$(INSTALL) -m 755 -d $(PMDAADMDIR)
	$(INSTALL) -m 755 -d $(PMDATMPDIR)
	$(INSTALL) -m 755 -t $(PMDATMPDIR) Install Remove $(CMDTARGET) $(PMDAADMDIR)
	$(INSTALL) -m 644 -t $(PMDATMPDIR) $(DFILES) root pmns domain.h $(PMDAADMDIR)
	@$(INSTALL_MAN)
endif

$(OBJECTS): domain.h

domain.h: ../../pmns/stdpmid
	$(DOMAIN_MAKERULE)

libmlan.a : $(SUBDIRS)
	$(SUBDIRS_MAKERULE)
	rm -f libmlan.a
	ln -sf mlan/libmlan.a .

default_pcp : default

install_pcp : install

check:: $(MAN_PAGES)
	$(MANLINT) $^
