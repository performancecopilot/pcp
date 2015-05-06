#
# Copyright (c) 2012-2015 Red Hat.
# Copyright (c) 2000,2003,2004 Silicon Graphics, Inc.  All Rights Reserved.
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

CFILES	= simple.c
CMDTARGET = pmdasimple$(EXECSUFFIX)
LLDLIBS = $(PCP_PMDALIB)
LCFLAGS = -I.
DFILES	= README help
SCRIPTS = pmdasimple.perl
ifeq "$(HAVE_PYTHON)" "true"
SCRIPTS += pmdasimple.python
endif
LSRCFILES = Install Remove pmns root $(DFILES) $(SCRIPTS) \
	simple.conf GNUmakefile.install

IAM	= simple
DOMAIN	= SIMPLE
PMDADIR	= $(PCP_PMDAS_DIR)/$(IAM)

LDIRT	= domain.h *.o $(IAM).log pmda$(IAM) pmda_$(IAM).so

default_pcp default:	domain.h $(CMDTARGET)

include $(BUILDRULES)

install_pcp install:	default 
	$(INSTALL) -m 755 -d $(PMDADIR)
	$(INSTALL) -m 755 Install Remove $(PMDADIR)
	$(INSTALL) -m 644 GNUmakefile.install $(PMDADIR)/Makefile
	$(INSTALL) -m 644 root pmns domain.h simple.conf $(PMDADIR)
	$(INSTALL) -m 644 $(CFILES) $(DFILES) $(SCRIPTS) $(PMDADIR)

domain.h: ../../pmns/stdpmid
	$(DOMAIN_MAKERULE)
