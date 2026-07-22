#
# Copyright (c) 2014-2015,2020 Red Hat.
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

IAM		= cifs
DOMAIN		= CIFS
PMDAADMDIR	= $(PCP_PMDASADM_DIR)/$(IAM)
PMDATMPDIR	= $(PCP_PMDAS_DIR)/$(IAM)
CMDTARGET	= pmda$(IAM)
LIBTARGET	= pmda_$(IAM).$(DSOSUFFIX)
PMDAINIT	= $(IAM)_init
PMCDCONF_LINE	= "cifs	121	dso	cifs_init	$(PMDATMPDIR)/$(LIBTARGET)"
LOCALCONF_LINE	= $(PMCDCONF_LINE)

CFILES		= pmda.c stats.c
HFILES		= pmdacifs.h stats.h
SCRIPTS		= Install Remove

VERSION_SCRIPT	= exports
HELPTARGETS	= help.dir help.pag
LLDLIBS		= $(PCP_PMDALIB)
DFILES		= help
LDIRT		= domain.h $(IAM).log

MAN_SECTION	= 1
MAN_PAGES	= pmda$(IAM).$(MAN_SECTION)
MAN_DEST	= $(PCP_MAN_DIR)/man$(MAN_SECTION)

default:	build-me

include $(BUILDRULES)

ifeq "$(TARGET_OS)" "linux"
build-me: $(CMDTARGET) $(LIBTARGET) $(HELPTARGETS)
	$(PMCDCONF_MAKERULE)
	$(LOCALCONF_MAKERULE)

install: default
	$(INSTALL) -m 755 -d $(PMDAADMDIR)
	$(INSTALL) -m 755 -d $(PMDATMPDIR)
	$(INSTALL) -m 644 -t $(PMDATMPDIR) domain.h help help.dir help.pag root root_cifs $(PMDAADMDIR)
	$(INSTALL) -m 755 -t $(PMDATMPDIR) $(LIBTARGET) $(CMDTARGET) $(SCRIPTS) $(PMDAADMDIR)
	$(INSTALL) -m 644 -t $(PCP_PMNS_DIR)/root_cifs root_cifs $(PMDAADMDIR)/root_cifs
	$(INSTALL) -S $(PMDAADMDIR)/root_cifs $(PCP_PMNSADM_DIR)/root_cifs
	@$(INSTALL_MAN)
else
build-me:
install:
endif

default_pcp : default

install_pcp : install

$(OBJECTS): domain.h

domain.h: ../../pmns/stdpmid
	$(DOMAIN_MAKERULE)

$(HELPTARGETS) : help
	$(NEWHELP) -n root_cifs -v 2 -o help < help

$(VERSION_SCRIPT):
	$(VERSION_SCRIPT_MAKERULE)

pmda.o:			pmdacifs.h
pmda.o stats.o:		stats.h

pmda.o:	$(TOPDIR)/src/include/pcp/libpcp.h

check:: $(MAN_PAGES)
	$(MANLINT) $^
