#
# Copyright (c) 2013-2015 Red Hat.
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

PMDADIR		= $(PCP_PMDAS_DIR)/$(IAM)
PMDAINIT	 = $(IAM)_init

HFILES		= rpm.h timer.h
CFILES		= rpm.c timer.c

SCRIPTS		= Install Remove
VERSION_SCRIPT	= exports
LSRCFILES	= Install Remove pmns root help
LDIRT		= domain.h $(IAM).log $(VERSION_SCRIPT)

LIB_FOR_RPM	= -lrpm
LLDLIBS		= $(PCP_PMDALIB) $(LIB_FOR_RPM) $(LIB_FOR_PTHREADS)
LCFLAGS		= $(INVISIBILITY)

default:	build-me

include $(BUILDRULES)

ifeq "$(PMDA_RPM)" "true"
build-me: $(CMDTARGET) $(LIBTARGET)

install:	default 
	$(INSTALL) -m 755 -d $(PMDADIR)
	$(INSTALL) -m 644 root pmns domain.h help $(PMDADIR)
	$(INSTALL) -m 755 $(CMDTARGET) $(LIBTARGET) $(SCRIPTS) $(PMDADIR)
	$(INSTALL) -m 644 migrate.conf $(PCP_VAR_DIR)/config/pmlogrewrite/rpm_migrate.conf
else
build-me:
install:
endif

default_pcp : default

install_pcp : install

$(VERSION_SCRIPT):
	$(VERSION_SCRIPT_MAKERULE)

$(OBJECTS): domain.h

domain.h: ../../pmns/stdpmid
	$(DOMAIN_MAKERULE)

rpm.o: rpm.h
rpm.o timer.o: timer.h
rpm.o: $(VERSION_SCRIPT)
