#
# Copyright (c) 2013 Red Hat.
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

IAM		= xfs
DOMAIN		= XFS
CMDTARGET	= pmdaxfs
LIBTARGET	= pmda_xfs.so
PMDADIR		= $(PCP_PMDAS_DIR)/$(IAM)
CONF_LINE	= "xfs	11	pipe	binary		$(PMDADIR)/$(CMDTARGET) -d 11"

CFILES		= proc_fs_xfs.c filesys.c pmda.c
HFILES		= proc_fs_xfs.h filesys.h clusters.h indom.h

LSRCFILES 	= help root_xfs linux_xfs_migrate.conf
LDIRT		= help.dir help.pag domain.h

LLDLIBS		= $(PCP_PMDALIB)

default:	build-me

include $(BUILDRULES)

ifeq "$(TARGET_OS)" "linux"
build-me: domain.h $(LIBTARGET) $(CMDTARGET) help.dir help.pag
	@if [ `grep -c $(CONF_LINE) ../pmcd.conf` -eq 0 ]; then \
		echo $(CONF_LINE) >> ../pmcd.conf ; \
	fi

install: default
	$(INSTALL) -m 755 -d $(PMDADIR)
	$(INSTALL) -m 644 domain.h help help.dir help.pag $(PMDADIR)
	$(INSTALL) -m 755 $(LIBTARGET) $(CMDTARGET) $(PMDADIR)
	$(INSTALL) -m 644 root_xfs $(PCP_VAR_DIR)/pmns/root_xfs
	$(INSTALL) -m 644 linux_xfs_migrate.conf $(PCP_VAR_DIR)/config/pmlogrewrite/linux_xfs_migrate.conf
else
build-me:
install:
endif

help.dir help.pag : help
	$(RUN_IN_BUILD_ENV) $(TOPDIR)/src/newhelp/newhelp -n root_xfs -v 2 -o help < help

default_pcp : default

install_pcp : install

domain.h: ../../pmns/stdpmid
	$(DOMAIN_MAKERULE)

pmda.o proc_fs_xfs.o:	proc_fs_xfs.h
filesys.o pmda.o:	filesys.h
