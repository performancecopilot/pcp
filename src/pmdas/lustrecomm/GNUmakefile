#
# Copyright (c) 2008 Silicon Graphics, Inc.  All Rights Reserved.
# Copyright (c) 2015,2020,2026 Red Hat, Inc.
#
# Author: Scott Emery <emery@sgi.com> 
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

IAM	= lustrecomm
DOMAIN	= LUSTRECOMM
PMDAINIT = $(IAM)_init

PMDAADMDIR = $(PCP_PMDASADM_DIR)/$(IAM)
PMDATMPDIR = $(PCP_PMDAS_DIR)/$(IAM)
LOCALCONF_LINE = "lustrecomm	93	dso	$(PMDAINIT)	$(PMDATMPDIR)/$(LIBTARGET)"

DFILES 	= README
HFILES	= libreadfiles.h
CFILES	= $(IAM).c file_indexed.c file_single.c refresh_file.c timespec_routines.c

LIBTARGET = pmda_$(IAM).$(DSOSUFFIX)
CMDTARGET = pmda$(IAM)
TARGETS = $(CMDTARGET) $(LIBTARGET)

VERSION_SCRIPT = exports
HELPTARGETS = help.dir help.pag

LDOPTS	=
LLDLIBS = $(PCP_PMDALIB) $(LIB_FOR_RT)
LCFLAGS = -I.

LDIRT	= $(HELPTARGETS) domain.h *.o $(IAM).log pmda$(IAM) pmda_$(IAM).$(DSOSUFFIX) $(VERSION_SCRIPT)

MAN_SECTION = 1
MAN_PAGES = pmda$(IAM).$(MAN_SECTION)
MAN_DEST = $(PCP_MAN_DIR)/man$(MAN_SECTION)

default: build-me

include $(BUILDRULES)

ifeq "$(TARGET_OS)" "linux"
build-me:	$(HELPTARGETS) $(TARGETS)

install:	default
	$(INSTALL) -m 755 -d $(PMDAADMDIR)
	$(INSTALL) -m 755 -d $(PMDATMPDIR)
	$(INSTALL) -m 755 -t $(PMDATMPDIR) Install Remove $(CMDTARGET) $(LIBTARGET) $(PMDAADMDIR)
	$(INSTALL) -m 644 -t $(PMDATMPDIR) $(DFILES) root root_lustrecomm pmns domain.h help help.dir help.pag $(PMDAADMDIR)
	$(INSTALL) -m 644 -t $(PCP_PMNS_DIR)/root_lustrecomm root_lustrecomm $(PMDAADMDIR)/root_lustrecomm
	$(INSTALL) -S $(PMDAADMDIR)/root_lustrecomm $(PCP_PMNSADM_DIR)/root_lustrecomm
	@$(INSTALL_MAN)
else
build-me:
install:
endif

default_pcp: default

install_pcp: install

$(OBJECTS): domain.h

$(HELPTARGETS) : help
	$(NEWHELP) -n root_lustrecomm -v 2 -o help < help

$(VERSION_SCRIPT):
	$(VERSION_SCRIPT_MAKERULE)

domain.h: ../../pmns/stdpmid
	$(DOMAIN_MAKERULE)

check:: $(MAN_PAGES)
	$(MANLINT) $^
