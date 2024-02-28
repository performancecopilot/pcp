#
# Copyright (c) 2018,2020-2021 Red Hat.
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

IAM		= podman
DOMAIN		= PODMAN
CMDTARGET	= pmda$(IAM)
LIBTARGET	= pmda_$(IAM).$(DSOSUFFIX)
PMDAINIT	= $(IAM)_init

PMDAADMDIR	= $(PCP_PMDASADM_DIR)/$(IAM)
PMDATMPDIR	= $(PCP_PMDAS_DIR)/$(IAM)

LLDLIBS		= $(PCP_WEBLIB)

JSONSL_HFILES	= $(addprefix deps/jsonsl/, jsonsl.h)
JSONSL_CFILES	= $(addprefix deps/jsonsl/, jsonsl.c)
JSONSL_XFILES	= $(JSONSL_HFILES) $(JSONSL_CFILES)

CFILES		= $(JSONSL_CFILES) podman.c pmda.c
HFILES		= $(JSONSL_HFILES) podman.h

LCFLAGS		= -I$(TOPDIR)/src/libpcp_web/src -Ideps

SCRIPTS		= Install Remove
VERSION_SCRIPT	= exports
LDIRT		= domain.h $(VERSION_SCRIPT) $(JSONSL_XFILES)

MAN_SECTION	= 1
MAN_PAGES	= pmda$(IAM).$(MAN_SECTION)
MAN_DEST	= $(PCP_MAN_DIR)/man$(MAN_SECTION)

default:	build-me

include $(BUILDRULES)

ifeq "$(TARGET_OS)" "linux"
build-me: $(JSONSL_XFILES) $(LIBTARGET) $(CMDTARGET)

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

$(JSONSL_XFILES):
	mkdir -p deps/jsonsl
	$(LN_S) -f $(realpath $(TOPDIR))/vendor/github.com/mnunberg/$(@:deps/%=%) $@

default_pcp : default

install_pcp : install

$(VERSION_SCRIPT):
	$(VERSION_SCRIPT_MAKERULE)

domain.h: ../../pmns/stdpmid
	$(DOMAIN_MAKERULE)

$(OBJECTS): domain.h
pmda.o: $(VERSION_SCRIPT)
pmda.o:	$(TOPDIR)/src/include/pcp/libpcp.h
podman.o: $(JSONSL_HFILES)

check:: $(MAN_PAGES)
	$(MANLINT) $^
