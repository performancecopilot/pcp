#
# Copyright (c) 2026 Red Hat.
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

IAM		= pmimport
DOMAIN		= PMIMPORT
PMDAINIT	= pmimport_init
LIBTARGET	= pmda_$(IAM).$(DSOSUFFIX)
TARGETS		= $(LIBTARGET)

CFILES		= pmda_pmimport.c
VERSION_SCRIPT	= exports
LLDLIBS		= $(PCP_PMDALIB)
LCFLAGS		= $(INVISIBILITY)
HELPTARGETS	= help.dir
LDIRT		= domain.h exports $(HELPTARGETS) *.log

PMCDCONF_LINE	= "$(IAM)	43	dso	$(PMDAINIT)	$(PMDATMPDIR)/$(LIBTARGET)"
LOCALCONF_LINE	= $(PMCDCONF_LINE)

PMDATMPDIR	= $(PCP_PMDAS_DIR)/$(IAM)
PMDAADMDIR	= $(PCP_PMDASADM_DIR)/$(IAM)

default_pcp default:	$(TARGETS) $(HELPTARGETS)
	$(PMCDCONF_MAKERULE)
	$(LOCALCONF_MAKERULE)

include $(BUILDRULES)

install_pcp install:	default
	$(INSTALL) -m 755 -d $(PMDAADMDIR)
	$(INSTALL) -m 755 -d $(PMDATMPDIR)
	$(INSTALL) -m 644 -t $(PMDATMPDIR) help.dir help.pag $(PMDAADMDIR)
	$(INSTALL) -m 755 -t $(PMDATMPDIR)/$(LIBTARGET) $(LIBTARGET) $(PMDAADMDIR)/$(LIBTARGET)
	$(INSTALL) -m 644 -t $(PCP_PMNS_DIR)/root_pmimport root_pmimport $(PCP_PMNSADM_DIR)/root_pmimport

$(OBJECTS):	domain.h

domain.h:	$(TOPDIR)/src/pmns/stdpmid
	$(DOMAIN_MAKERULE)

help.dir: help root_pmimport
	$(NEWHELP) -n root_pmimport -v 2 -o help < help

$(VERSION_SCRIPT):
	$(VERSION_SCRIPT_MAKERULE)
