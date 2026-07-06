#
# Copyright (c) 2014-2015,2020,2025,2026 Red Hat.
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

IAM	= nvidia
DOMAIN	= NVML

CMDTARGET = pmdanvidia$(EXECSUFFIX)
LIBTARGET = pmda_nvidia.$(DSOSUFFIX)
PMDAINIT = nvidia_init
CFILES	= localnvml.c nvidia.c
HFILES	= localnvml.h
DFILES	= README
LLDLIBS	= $(PCP_PMDALIB) $(LIB_FOR_DLOPEN)
LCFLAGS += -DDSOSUFFIX=\"$(DSOSUFFIX)\"

PMDAADMDIR = $(PCP_PMDASADM_DIR)/$(IAM)
PMDATMPDIR = $(PCP_PMDAS_DIR)/$(IAM)
LOGCONFDIR = $(PCP_SYSCONF_DIR)/pmlogconf/$(IAM)
LOGCONFVARDIR = $(PCP_VAR_DIR)/config/pmlogconf/$(IAM)
REWRITEDIR	= $(PCP_SYSCONF_DIR)/pmlogrewrite
REWRITEVARDIR	= $(PCP_VAR_DIR)/config/pmlogrewrite

LOCALCONF_LINE = "nvidia	120	dso	$(PMDAINIT)	$(PMDATMPDIR)/$(LIBTARGET)"
VERSION_SCRIPT	= exports
HELPTARGETS	= help.dir help.pag

LDIRT	= domain.h *.log $(HELPTARGETS) so_locations $(VERSION_SCRIPT)

default:	$(LIBTARGET) $(CMDTARGET) $(HELPTARGETS)

include $(BUILDRULES)

install:	default
	$(INSTALL) -m 755 -d $(PMDAADMDIR)
	$(INSTALL) -m 755 -d $(PMDATMPDIR)
	$(INSTALL) -m 755 -t $(PMDATMPDIR) Install Remove $(PMDAADMDIR)
	$(INSTALL) -m 755 -t $(PMDATMPDIR) $(LIBTARGET) $(CMDTARGET) $(PMDAADMDIR)
	$(INSTALL) -m 644 -t $(PMDATMPDIR) $(DFILES) root root_nvidia help help.dir help.pag pmns domain.h $(PMDAADMDIR)
	$(INSTALL) -m 644 -t $(PCP_PMNS_DIR)/root_nvidia root_nvidia $(PMDAADMDIR)/root_nvidia
	$(INSTALL) -S $(PMDAADMDIR)/root_nvidia $(PCP_PMNSADM_DIR)/root_nvidia
	$(INSTALL) -m 755 -d $(LOGCONFDIR)
	$(INSTALL) -m 755 -d $(LOGCONFVARDIR)
	$(INSTALL) -m 644 -t $(LOGCONFVARDIR)/config pmlogconf.config $(LOGCONFDIR)/config
	$(INSTALL) -m 644 -t $(LOGCONFVARDIR)/summary pmlogconf.summary $(LOGCONFDIR)/summary
	$(INSTALL) -m 644 -t $(REWRITEVARDIR)/$(IAM).conf rewrite.conf $(REWRITEDIR)/$(IAM).conf

$(OBJECTS): domain.h

domain.h: ../../pmns/stdpmid
	$(DOMAIN_MAKERULE)

$(HELPTARGETS) : help
	$(NEWHELP) -n root_nvidia -v 2 -o help < help

$(VERSION_SCRIPT):
	$(VERSION_SCRIPT_MAKERULE)

default_pcp:	default

install_pcp:	install
