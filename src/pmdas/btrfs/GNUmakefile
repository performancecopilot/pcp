#
# Copyright (c) 2026 Red Hat.
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

IAM		= btrfs
DOMAIN		= BTRFS
CMDTARGET	= pmda$(IAM)
LIBTARGET	= pmda_$(IAM).$(DSOSUFFIX)
PMDAINIT	= $(IAM)_init
PMDATMPDIR	= $(PCP_PMDAS_DIR)/$(IAM)
PMDAADMDIR	= $(PCP_PMDASADM_DIR)/$(IAM)

CFILES		= btrfs_utils.c btrfs_fs.c btrfs_dev.c pmda.c
HFILES		= btrfs_utils.h btrfs_fs.h btrfs_dev.h clusters.h

SCRIPTS		= Install Remove
VERSION_SCRIPT	= exports
HELPTARGETS	= help.dir help.pag
LDIRT		= $(HELPTARGETS) domain.h $(VERSION_SCRIPT)

LLDLIBS		= $(PCP_PMDALIB)
LCFLAGS		= $(INVISIBILITY)

MAN_SECTION	= 1
MAN_PAGES	= pmda$(IAM).$(MAN_SECTION)
MAN_DEST	= $(PCP_MAN_DIR)/man$(MAN_SECTION)

default:	build-me

include $(BUILDRULES)

ifeq "$(TARGET_OS)" "linux"
build-me: $(LIBTARGET) $(CMDTARGET) $(HELPTARGETS)

install: default
	$(INSTALL) -m 755 -d $(PMDAADMDIR)
	$(INSTALL) -m 755 -d $(PMDATMPDIR)
	$(INSTALL) -m 644 -t $(PMDATMPDIR) domain.h help $(HELPTARGETS) root root_btrfs $(PMDAADMDIR)
	$(INSTALL) -m 755 -t $(PMDATMPDIR) $(LIBTARGET) $(CMDTARGET) $(SCRIPTS) $(PMDAADMDIR)
	$(INSTALL) -m 644 -t $(PCP_PMNS_DIR)/root_btrfs root_btrfs $(PCP_PMNSADM_DIR)/root_btrfs
	@$(INSTALL_MAN)
else
build-me:
install:
endif

default_pcp : default

install_pcp : install

$(HELPTARGETS): help
	$(NEWHELP) -n root_btrfs -v 2 -o help < help

$(VERSION_SCRIPT):
	$(VERSION_SCRIPT_MAKERULE)

domain.h: ../../pmns/stdpmid
	$(DOMAIN_MAKERULE)

$(OBJECTS): domain.h
pmda.o:	$(HFILES)
pmda.o: $(VERSION_SCRIPT)

pmda.o:	$(TOPDIR)/src/include/pcp/libpcp.h

check:: $(MAN_PAGES)
	$(MANLINT) $^
