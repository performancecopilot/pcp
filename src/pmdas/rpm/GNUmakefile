#
# Copyright (c) 2013-2015,2020 Red Hat.
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

IAM		= rpm
DOMAIN		= RPM
CMDTARGET	= pmda$(IAM)$(EXECSUFFIX)
LIBTARGET	= pmda_$(IAM).$(DSOSUFFIX)
PMDAINIT	 = $(IAM)_init

HFILES		= rpm.h timer.h
CFILES		= rpm.c timer.c

SCRIPTS		= Install Remove
VERSION_SCRIPT	= exports
LDIRT		= domain.h $(IAM).log $(VERSION_SCRIPT)

PMDAADMDIR	= $(PCP_PMDASADM_DIR)/$(IAM)
PMDATMPDIR	= $(PCP_PMDAS_DIR)/$(IAM)
REWRITEDIR	= $(PCP_SYSCONF_DIR)/pmlogrewrite
REWRITEVARDIR	= $(PCP_VAR_DIR)/config/pmlogrewrite

MAN_SECTION	= 1
MAN_PAGES	= pmda$(IAM).$(MAN_SECTION)
MAN_DEST	= $(PCP_MAN_DIR)/man$(MAN_SECTION)

LIB_FOR_RPM	= -lrpm
LLDLIBS		= $(PCP_PMDALIB) $(LIB_FOR_RPM) $(LIB_FOR_PTHREADS)
LCFLAGS		= $(INVISIBILITY)

default_pcp default:	build-me

include $(BUILDRULES)

ifeq "$(PMDA_RPM)" "true"
build-me: $(CMDTARGET) $(LIBTARGET)

install_pcp install:	default 
	$(INSTALL) -m 755 -d $(PMDAADMDIR)
	$(INSTALL) -m 755 -d $(PMDATMPDIR)
	$(INSTALL) -m 644 -t $(PMDATMPDIR) root pmns domain.h help $(PMDAADMDIR)
	$(INSTALL) -m 755 -t $(PMDATMPDIR) $(CMDTARGET) $(LIBTARGET) $(SCRIPTS) $(PMDAADMDIR)
	$(INSTALL) -m 644 -t $(REWRITEVARDIR)/rpm_migrate.conf migrate.conf $(REWRITEDIR)/rpm_migrate.conf
	@$(INSTALL_MAN)
else
build-me:
install_pcp install:
endif

$(VERSION_SCRIPT):
	$(VERSION_SCRIPT_MAKERULE)

$(OBJECTS): domain.h

domain.h: ../../pmns/stdpmid
	$(DOMAIN_MAKERULE)

rpm.o: rpm.h
rpm.o timer.o: timer.h
rpm.o: $(VERSION_SCRIPT)

rpm.o:	$(TOPDIR)/src/include/pcp/libpcp.h

check:: $(MAN_PAGES)
	$(MANLINT) $^
