#
# Copyright (c) 2018 Red Hat.
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

IAM		= smart
DOMAIN		= SMART
CFILES		= pmda.c smart_stats.c
HFILES		= pmdasmart.h smart_stats.h
CMDTARGET	= pmda$(IAM)
LIBTARGET	= pmda_$(IAM).$(DSOSUFFIX)
PMDAINIT	= $(IAM)_init
PMDADIR		= $(PCP_PMDAS_DIR)/$(IAM)

LLDLIBS		= $(PCP_PMDALIB)
LCFLAGS		= $(INVISIBILITY)

LDIRT		= domain.h $(IAM).log $(VERSION_SCRIPT)

SCRIPTS		= Install Remove
VERSION_SCRIPT	= exports

MAN_SECTION	= 1
MAN_PAGES	= pmda$(IAM).$(MAN_SECTION)
MAN_DEST	= $(PCP_MAN_DIR)/man$(MAN_SECTION)

default:	build-me

include $(BUILDRULES)

ifeq "$(TARGET_OS)" "linux"
build-me: $(CMDTARGET) $(LIBTARGET)

install: default
	$(INSTALL) -m 755 -d $(PMDADIR)
	$(INSTALL) -m 644 root pmns domain.h help $(PMDADIR)
	$(INSTALL) -m 755 $(LIBTARGET) $(CMDTARGET) $(SCRIPTS) $(PMDADIR)
	@$(INSTALL_MAN)
else
build-me:
install:
endif

default_pcp : default

install_pcp : install

$(OBJECTS): domain.h

$(VERSION_SCRIPT):
	$(VERSION_SCRIPT_MAKERULE)

domain.h: ../../pmns/stdpmid
	$(DOMAIN_MAKERULE)

pmda.o:			pmdasmart.h
pmda.o smart_stats.o:	smart_stats.h
