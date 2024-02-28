#
# Copyright (c) 2021 Red Hat.
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

IAM		= sockets
DOMAIN		= SOCKETS
PMNSFILES	= root pmns
CMDTARGET	= pmda$(IAM)
LIBTARGET	= pmda_$(IAM).$(DSOSUFFIX)
PMDAINIT	= $(IAM)_init
PMDATMPDIR	= $(PCP_PMDAS_DIR)/$(IAM)
PMDAADMDIR	= $(PCP_PMDASADM_DIR)/$(IAM)
PMDACONFIG	= $(PCP_SYSCONF_DIR)/$(IAM)
PMIEDIR		= $(PCP_SYSCONF_DIR)/pmieconf/$(IAM)
PMIEVARDIR	= $(PCP_VAR_DIR)/config/pmieconf/$(IAM)

CFILES		= pmda.c  metrictab.c ss_refresh.c ss_parse.c ss_stream.c
HFILES		= indom.h cluster.h ss_stats.h
LLDLIBS		= $(PCP_PMDALIB)
LCFLAGS		= $(INVISIBILITY)

SCRIPTS		= Install Remove Upgrade
CONF		= filter.conf
VERSION_SCRIPT	= exports
LDIRT		= domain.h $(VERSION_SCRIPT) $(IAM).log

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
	$(INSTALL) -m 755 -d $(PMDACONFIG)
	$(INSTALL) -m 644 -t $(PMDATMPDIR)/$(CONF) $(CONF) $(PMDACONFIG)/$(CONF)
	$(INSTALL) -m 755 -t $(PMDATMPDIR) $(LIBTARGET) $(CMDTARGET) $(SCRIPTS) $(PMDAADMDIR)
	$(INSTALL) -m 644 -t $(PMDATMPDIR) domain.h help $(PMNSFILES) $(PMDAADMDIR)
	@$(INSTALL_MAN)
else
build-me:
install:
endif

default_pcp : default

install_pcp : install

domain.h: ../../pmns/stdpmid
	$(DOMAIN_MAKERULE)

$(VERSION_SCRIPT):
	$(VERSION_SCRIPT_MAKERULE)

$(OBJECTS): domain.h ss_stats.h

pmda.o: $(TOPDIR)/src/include/pcp/libpcp.h
pmda.o: $(VERSION_SCRIPT)

check:: $(MAN_PAGES)
	$(MANLINT) $^
