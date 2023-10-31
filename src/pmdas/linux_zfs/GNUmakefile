#
# Copyright (c) 2013-2016,2020 Red Hat.
# Copyright (c) 2000,2003,2004,2008 Silicon Graphics, Inc.  All Rights Reserved.
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

IAM		= zfs
DOMAIN		= ZFS
CMDTARGET	= pmda$(IAM)
LIBTARGET	= pmda_$(IAM).$(DSOSUFFIX)
PMDAINIT	= $(IAM)_init
PMDATMPDIR	= $(PCP_PMDAS_DIR)/$(IAM)
PMDAADMDIR	= $(PCP_PMDASADM_DIR)/$(IAM)

CFILES		= zfs_utils.c zfs_pools.c zfs_zilstats.c zfs_zfetchstats.c zfs_xuiostats.c zfs_vdev_mirrorstats.c zfs_vdev_cachestats.c zfs_fmstats.c zfs_dnodestats.c zfs_dmu_tx.c zfs_dbufstats.c zfs_arcstats.c zfs_abdstats.c pmda.c
HFILES		= zfs_utils.h zfs_pools.h zfs_zilstats.h zfs_zfetchstats.h zfs_xuiostats.h zfs_vdev_mirrorstats.h zfs_vdev_cachestats.h zfs_fmstats.h zfs_dnodestats.h zfs_dmu_tx.h zfs_dbufstats.h zfs_arcstats.h zfs_abdstats.h clusters.h

SCRIPTS		= Install Remove
VERSION_SCRIPT	= exports
LDIRT		= domain.h $(VERSION_SCRIPT)

LLDLIBS		= $(PCP_PMDALIB)
LCFLAGS		= $(INVISIBILITY)

MAN_SECTION	= 1
MAN_PAGES	= pmda$(IAM).$(MAN_SECTION)
MAN_DEST	= $(PCP_MAN_DIR)/man$(MAN_SECTION)

default:	build-me

include $(BUILDRULES)

ifeq "$(TARGET_OS)" "linux"
build-me: $(LIBTARGET) $(CMDTARGET)

install: default
	$(INSTALL) -m 755 -d $(PMDAADMDIR)
	$(INSTALL) -m 755 -d $(PMDATMPDIR)
	$(INSTALL) -m 644 -t $(PMDATMPDIR) domain.h help root pmns $(PMDAADMDIR)
	$(INSTALL) -m 755 -t $(PMDATMPDIR) $(LIBTARGET) $(CMDTARGET) $(SCRIPTS) $(PMDAADMDIR)
	@$(INSTALL_MAN)
else
build-me:
install:
endif

default_pcp : default

install_pcp : install

$(VERSION_SCRIPT):
	$(VERSION_SCRIPT_MAKERULE)

domain.h: ../../pmns/stdpmid
	$(DOMAIN_MAKERULE)

$(OBJECTS): domain.h
pmda.o:	$(HFILES)
pmda.o: $(VERSION_SCRIPT)

pmda.o:	$(TOPDIR)/src/include/pcp/libpcp.h

check:: $(CFILES) $(HFILES)
	$(CLINT) $^

check:: $(MAN_PAGES)
	$(MANLINT) $^
