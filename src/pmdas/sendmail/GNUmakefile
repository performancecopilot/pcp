#
# Copyright (c) 2000-2001,2004 Silicon Graphics, Inc.  All Rights Reserved.
# Copyright (c) 2015 Red Hat.
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
LSRCFILES= $(SCRIPTS) pmns help root Sendmail.pmchart $(DFILES)

VERSION_SCRIPT = exports
PMDAINIT = $(IAM)_init
PMDADIR	= $(PCP_PMDAS_DIR)/$(IAM)
PMCHART	= $(PCP_VAR_DIR)/config/pmchart

LDIRT	= domain.h *.o $(IAM).log $(LIBTARGET) $(CMDTARGET) $(VERSION_SCRIPT)
LLDLIBS	= $(PCP_PMDALIB)
LCFLAGS = $(INVISIBILITY)

default_pcp default:	$(CMDTARGET) $(LIBTARGET)

include $(BUILDRULES)

install install_pcp:	default
	$(INSTALL) -m 755 -d $(PMDADIR)
	$(INSTALL) -m 755 $(LIBTARGET) $(PMDADIR)/$(LIBTARGET)
	$(INSTALL) -m 755 $(CMDTARGET) $(PMDADIR)/pmda$(IAM)$(EXECSUFFIX)
	$(INSTALL) -m 755 $(SCRIPTS) $(PMDADIR)
	$(INSTALL) -m 644 $(DFILES) pmns help root domain.h $(PMDADIR)
	$(INSTALL) -m 644 Sendmail.pmchart $(PMCHART)/Sendmail

$(CMDTARGET):	$(OBJECTS)

$(LIBTARGET):	$(OBJECTS) $(VERSION_SCRIPT)

$(OBJECTS): domain.h

domain.h: ../../pmns/stdpmid
	$(DOMAIN_MAKERULE)

$(VERSION_SCRIPT):
	$(VERSION_SCRIPT_MAKERULE)
