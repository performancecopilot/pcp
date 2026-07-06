#
# Copyright (c) 2020 - 2026 Red Hat.
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
include	$(TOPDIR)/src/include/builddefs

IAM			= hacluster
DOMAIN		= HACLUSTER
CFILES		= pmda.c pacemaker.c corosync.c sbd.c drbd.c
HFILES		= pmdahacluster.h pacemaker.h corosync.h sbd.h drbd.h
CMDTARGET	= pmda$(IAM)
LIBTARGET	= pmda_$(IAM).$(DSOSUFFIX)
PMDAINIT	= $(IAM)_init
LOCALCONF_LINE	= "hacluster	155	dso	$(PMDAINIT)	$(PMDATMPDIR)/$(LIBTARGET)"

LLDLIBS		= $(PCP_PMDALIB)
LCFLAGS		= $(INVISIBILITY)

HELPTARGETS	= help.dir help.pag
LDIRT		= $(HELPTARGETS) domain.h $(IAM).log $(VERSION_SCRIPT)

SCRIPTS		= Install Remove
VERSION_SCRIPT	= exports

PMDAADMDIR	= $(PCP_PMDASADM_DIR)/$(IAM)
PMDATMPDIR	= $(PCP_PMDAS_DIR)/$(IAM)

MAN_SECTION	= 1
MAN_PAGES	= pmda$(IAM).$(MAN_SECTION)
MAN_DEST	= $(PCP_MAN_DIR)/man$(MAN_SECTION)

default:	build-me

include $(BUILDRULES)

ifeq "$(TARGET_OS)" "linux"
build-me: $(CMDTARGET) $(LIBTARGET) $(HELPTARGETS)

install: default
	$(INSTALL) -m 755 -d $(PMDAADMDIR)
	$(INSTALL) -m 755 -d $(PMDATMPDIR)
	$(INSTALL) -m 644 -t $(PMDATMPDIR) help.dir help.pag root root_hacluster pmns domain.h help $(PMDAADMDIR)
	$(INSTALL) -m 755 -t $(PMDATMPDIR) $(LIBTARGET) $(CMDTARGET) $(SCRIPTS) $(PMDAADMDIR)
	$(INSTALL) -m 644 -t $(PCP_PMNS_DIR)/root_hacluster root_hacluster $(PMDAADMDIR)/root_hacluster
	$(INSTALL) -S $(PMDAADMDIR)/root_hacluster $(PCP_PMNSADM_DIR)/root_hacluster
	@$(INSTALL_MAN)
else
build-me:
install:
endif

default_pcp : default

install_pcp : install

$(HELPTARGETS) : help
	$(NEWHELP) -n root_hacluster -v 2 -o help < help

$(OBJECTS): domain.h

$(VERSION_SCRIPT):
	$(VERSION_SCRIPT_MAKERULE)

domain.h: ../../pmns/stdpmid
	$(DOMAIN_MAKERULE)

pmda.o pacemaker.o corosync.o: pmdahacluster.h
pmda.o pacemaker.o: pacemaker.h
pmda.o corosync.o:	corosync.h
pmda.o sbd.o:		sbd.h
pmda.o drbd.o:		drbd.h

check:: $(MAN_PAGES)
	$(MANLINT) $^
