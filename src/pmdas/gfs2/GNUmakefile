#
# Copyright (c) 2013-2023 Red Hat.
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

IAM		= gfs2
DOMAIN		= GFS2
CFILES		= latency.c control.c glstats.c worst_glock.c glocks.c sbstats.c ftrace.c pmda.c
HFILES		= latency.c control.h glstats.h worst_glock.h glocks.h sbstats.h ftrace.h pmdagfs2.h
CMDTARGET	= pmda$(IAM)
LIBTARGET	= pmda_$(IAM).$(DSOSUFFIX)
PMDAINIT	= $(IAM)_init

LLDLIBS		= $(PCP_PMDALIB)
LCFLAGS		= $(INVISIBILITY)

LDIRT		= domain.h $(IAM).log $(VERSION_SCRIPT)

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
build-me: $(CMDTARGET) $(LIBTARGET)

install: default
	$(INSTALL) -m 755 -d $(PMDAADMDIR)
	$(INSTALL) -m 775 -d $(PMDATMPDIR)
	$(INSTALL) -m 644 -t $(PMDATMPDIR) root pmns domain.h help $(PMDAADMDIR)
	$(INSTALL) -m 775 -t $(PMDATMPDIR) $(LIBTARGET) $(CMDTARGET) $(SCRIPTS) $(PMDAADMDIR)
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

pmda.o glocks.o:	       glocks.h
pmda.o sbstats.o:	       sbstats.h
pmda.o:			       pmdagfs2.h
pmda.o glstats.o:	       glstats.h
pmda.o ftrace.o:	       ftrace.h
pmda.o ftrace.o worst_glock.o: worst_glock.h
pmda.o ftrace.o latency.o:     latency.h
pmda.o control.o:              control.h

pmda.o sbstats.o worst_glock.o:	$(TOPDIR)/src/include/pcp/libpcp.h

check:: $(MAN_PAGES)
	$(MANLINT) $^
