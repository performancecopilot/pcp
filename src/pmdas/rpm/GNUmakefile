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

CFILES	= rpm.c
CMDTARGET = pmdarpm$(EXECSUFFIX)
LIBTARGET = pmda_rpm.so
LLDLIBS = $(PCP_PMDALIB) -lrpm -lgc -lpthread

LCFLAGS = -I.
LSRCFILES = Install Remove pmns root help

IAM	= rpm
DOMAIN	= RPM
PMDADIR	= $(PCP_PMDAS_DIR)/$(IAM)

LDIRT	= domain.h *.o $(IAM).log pmda$(IAM) pmda_$(IAM).so help.dir help.pag

default_pcp default: domain.h $(CMDTARGET) $(LIBTARGET) help.dir help.pag

include $(BUILDRULES)

help.dir help.pag : help
	$(RUN_IN_BUILD_ENV) $(TOPDIR)/src/newhelp/newhelp -n root -v 2 -o help < help

install_pcp install:	default 
	$(INSTALL) -m 755 -d $(PMDADIR)
	$(INSTALL) -m 644 root pmns domain.h help help.dir help.pag $(PMDADIR)
	$(INSTALL) -m 755 $(CMDTARGET) $(LIBTARGET) $(PMDADIR)

domain.h: ../../pmns/stdpmid
	$(DOMAIN_MAKERULE)
 
