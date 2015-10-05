#
# Copyright (c) 2012-2015 Red Hat Inc.
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

CMDTARGET = pmdasystemd$(EXECSUFFIX)
DFILES 	= README
CFILES	= systemd.c
LCFLAGS = $(SYSTEMD_CFLAGS)
LLDLIBS = $(PCP_PMDALIB) $(SYSTEMD_LIBS)

IAM	= systemd
DOMAIN	= SYSTEMD
PMDADIR	= $(PCP_PMDAS_DIR)/$(IAM)

LDIRT	= domain.h *.o $(IAM).log $(CMDTARGET)

MAN_SECTION = 1
MAN_PAGES = pmda$(IAM).$(MAN_SECTION)
MAN_DEST = $(PCP_MAN_DIR)/man$(MAN_SECTION)

default:	build-me

include $(BUILDRULES)

ifeq "$(PMDA_SYSTEMD)" "true"
build-me: $(CMDTARGET)

install:	default
	$(INSTALL) -m 755 -d $(PMDADIR)
	$(INSTALL) -m 755 Install Remove $(PMDADIR)
	$(INSTALL) -m 644 $(DFILES) root help pmns $(PMDADIR)
	$(INSTALL) -m 644 domain.h $(PMDADIR)/domain.h
	$(INSTALL) -m 755 $(CMDTARGET) $(PMDADIR)/$(CMDTARGET)
	@$(INSTALL_MAN)
else
build-me:
install:
endif

$(OBJECTS): domain.h

default_pcp : default

install_pcp : install

domain.h: ../../pmns/stdpmid
	$(DOMAIN_MAKERULE)
