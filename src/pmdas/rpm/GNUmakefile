#
# Copyright (c) 2013 Red Hat.
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


CMDTARGET = pmda$(IAM)$(EXECSUFFIX)
LIBTARGET = pmda_$(IAM).$(DSOSUFFIX)
TARGETS = $(CMDTARGET) $(LIBTARGET)

IAM	= rpm
DOMAIN	= RPM
PMDADIR	= $(PCP_PMDAS_DIR)/$(IAM)

HFILES	= timer.h
CFILES	= rpm.c timer.c
LSRCFILES = Install Remove pmns root help
LDIRT	= $(TARGETS) domain.h *.o $(IAM).log

# XXX: move these to configure/builddefs
LIB_FOR_RPM = -lrpm
HAVE_RPM_SUPPORT = 1

LCFLAGS = -I.
LLDLIBS = $(PCP_PMDALIB) $(LIB_FOR_RPM) $(LIB_FOR_PTHREADS)


default:	build-me

include $(BUILDRULES)

ifeq "$(HAVE_RPM_SUPPORT)" "1"
build-me: domain.h $(CMDTARGET) $(LIBTARGET)

install:	default 
	$(INSTALL) -m 755 -d $(PMDADIR)
	$(INSTALL) -m 644 root pmns domain.h help $(PMDADIR)
	$(INSTALL) -m 755 $(CMDTARGET) $(LIBTARGET) $(PMDADIR)
else
build-me:
install:
endif

default_pcp : default

install_pcp : install

domain.h: ../../pmns/stdpmid
	$(DOMAIN_MAKERULE)

rpm.o timer.o: timer.h
 
