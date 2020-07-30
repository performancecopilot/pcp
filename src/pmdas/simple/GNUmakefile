#
# Copyright (c) 2012-2015,2020 Red Hat.
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
LIBTARGET = pmda_simple.$(DSOSUFFIX)
LLDLIBS = $(PCP_PMDALIB)
LCFLAGS = -I.
CONFIG	= simple.conf
DFILES	= README help
SCRIPTS = pmdasimple.perl
ifeq "$(HAVE_PYTHON)" "true"
SCRIPTS += pmdasimple.python
endif

IAM	= simple
DOMAIN	= SIMPLE

PMDATMPDIR = $(PCP_PMDAS_DIR)/$(IAM)
PMDACONFIG = $(PCP_SYSCONF_DIR)/$(IAM)
PMDAADMDIR = $(PCP_PMDASADM_DIR)/$(IAM)

LDIRT	= domain.h *.o $(IAM).log pmda$(IAM) pmda_$(IAM).$(DSOSUFFIX)

default_pcp default:	$(CMDTARGET) $(LIBTARGET)

include $(BUILDRULES)

install_pcp install:	default 
	$(INSTALL) -m 755 -d $(PMDAADMDIR)
	$(INSTALL) -m 755 -d $(PMDATMPDIR)
	$(INSTALL) -m 755 -t $(PMDATMPDIR) Install Remove $(SCRIPTS) $(PMDAADMDIR)
	$(INSTALL) -m 644 -t $(PMDATMPDIR)/Makefile GNUmakefile.install $(PMDAADMDIR)/Makefile
	$(INSTALL) -m 644 -t $(PMDATMPDIR) root pmns domain.h $(PMDAADMDIR)
	$(INSTALL) -m 644 -t $(PMDATMPDIR) $(CFILES) $(DFILES) $(PMDAADMDIR)
	$(INSTALL) -m 755 -d $(PMDACONFIG)
	$(INSTALL) -m 644 -t $(PMDATMPDIR)/$(CONFIG) $(CONFIG) $(PMDACONFIG)/$(CONFIG)

$(OBJECTS): domain.h

domain.h: ../../pmns/stdpmid
	$(DOMAIN_MAKERULE)
