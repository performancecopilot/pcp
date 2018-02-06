#
# Copyright (c) 2015-2018 Red Hat.
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

IAM		= dm
DOMAIN		= DM
PMNSFILES	= pmns.dmcache pmns.dmthin pmns.dmstats pmns.vdo
CMDTARGET	= pmda$(IAM)
LIBTARGET	= pmda_$(IAM).$(DSOSUFFIX)
PMDAINIT	= $(IAM)_init
PMDADIR		= $(PCP_PMDAS_DIR)/$(IAM)
PMIEDIR		= $(PCP_VAR_DIR)/config/pmieconf/$(IAM)

CFILES		= pmda.c dmthin.c dmcache.c vdo.c
HFILES		= indom.h dmthin.h dmcache.h dmstats.h vdo.h
LLDLIBS		= $(PCP_PMDALIB)
LCFLAGS		= $(INVISIBILITY)

ifeq "$(HAVE_DEVMAPPER)" "true"
CFILES		+= dmstats.c
LLDLIBS		+= $(LIB_FOR_DEVMAPPER)
LCFLAGS		+= -DHAVE_DEVMAPPER=1
endif

SCRIPTS		= Install Remove
VERSION_SCRIPT	= exports
LDIRT		= domain.h $(IAM).log $(VERSION_SCRIPT)

MAN_SECTION	= 1
MAN_PAGES	= pmda$(IAM).$(MAN_SECTION)
MAN_DEST	= $(PCP_MAN_DIR)/man$(MAN_SECTION)

default:	build-me

include $(BUILDRULES)

ifeq "$(TARGET_OS)" "linux"
build-me: $(CMDTARGET) $(LIBTARGET)

install: default
	$(INSTALL) -m 755 -d $(PMDADIR)
	$(INSTALL) -m 644 domain.h help $(PMDADIR)
	$(INSTALL) -m 644 root $(PMNSFILES) $(PMDADIR)
	$(INSTALL) -m 755 $(LIBTARGET) $(CMDTARGET) $(SCRIPTS) $(PMDADIR)
	$(INSTALL) -m 644 dmthin.data_high_util.pmie $(PMIEDIR)/data_high_util
	$(INSTALL) -m 644 dmthin.metadata_high_util.pmie $(PMIEDIR)/metadata_high_util
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
pmda.o dmcache.o dmthin.o dm.o vdo.o: indom.h
pmda.o dmcache.o: dmcache.h
pmda.o dmthin.o: dmthin.h
ifeq "$(HAVE_DEVMAPPER)" "true"
pmda.o dmstats.o: dmstats.h
endif
pmda.o vdo.o: vdo.h

dmcache.o dmthin.o vdo.o:	$(TOPDIR)/src/include/pcp/libpcp.h
