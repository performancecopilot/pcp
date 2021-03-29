#
# Copyright (c) 2000-2001,2004 Silicon Graphics, Inc.  All Rights Reserved.
# Copyright (c) 2015,2020 Red Hat.
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

IAM	= sendmail
DOMAIN	= SENDMAIL
CMDTARGET = $(IAM)$(EXECSUFFIX)
LIBTARGET = pmda_$(IAM).$(DSOSUFFIX)
CFILES	= sendmail.c
SCRIPTS	= Install Remove
DFILES	= README

VERSION_SCRIPT = exports
PMDAINIT = $(IAM)_init

PMDATMPDIR = $(PCP_PMDAS_DIR)/$(IAM)
PMDAADMDIR = $(PCP_PMDASADM_DIR)/$(IAM)
PMCHARTDIR = $(PCP_SYSCONF_DIR)/pmchart
PMCHARTVARDIR = $(PCP_VAR_DIR)/config/pmchart

LDIRT	= domain.h *.o $(IAM).log $(LIBTARGET) $(CMDTARGET) $(VERSION_SCRIPT)
LLDLIBS	= $(PCP_PMDALIB)
LCFLAGS = $(INVISIBILITY)

default_pcp default:	$(CMDTARGET) $(LIBTARGET)

include $(BUILDRULES)

install install_pcp:	default
	$(INSTALL) -m 755 -d $(PMDAADMDIR)
	$(INSTALL) -m 755 -d $(PMDATMPDIR)
	$(INSTALL) -m 755 -t $(PMDATMPDIR)/pmda$(IAM)$(EXECSUFFIX) $(CMDTARGET) $(PMDAADMDIR)/pmda$(IAM)$(EXECSUFFIX)
	$(INSTALL) -m 755 -t $(PMDATMPDIR) $(LIBTARGET) $(SCRIPTS) $(PMDAADMDIR)
	$(INSTALL) -m 644 -t $(PMDATMPDIR) $(DFILES) pmns help root domain.h $(PMDAADMDIR)
ifeq "$(ENABLE_QT)" "true"
	$(INSTALL) -m 644 -t $(PMCHARTVARDIR)/Sendmail Sendmail.pmchart $(PMCHARTDIR)/Sendmail
endif

$(CMDTARGET):	$(OBJECTS)

$(LIBTARGET):	$(OBJECTS) $(VERSION_SCRIPT)

$(OBJECTS): domain.h

domain.h: ../../pmns/stdpmid
	$(DOMAIN_MAKERULE)

$(VERSION_SCRIPT):
	$(VERSION_SCRIPT_MAKERULE)

sendmail.o:	$(TOPDIR)/src/include/pcp/libpcp.h
