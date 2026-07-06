#
# Copyright (c) 2023,2026 Red Hat.
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

IAM	= resctrl
DOMAIN	= RESCTRL
CMDTARGET = pmda$(IAM)$(EXECSUFFIX)
LIBTARGET = pmda_$(IAM).$(DSOSUFFIX)
PMDAINIT = $(IAM)_init
LOCALCONF_LINE = "$(IAM)	159	dso	$(PMDAINIT)	$(PMDATMPDIR)/$(LIBTARGET)"

PMDAADMDIR = $(PCP_PMDASADM_DIR)/$(IAM)
PMDATMPDIR = $(PCP_PMDAS_DIR)/$(IAM)
PMDACONFIG = $(PCP_SYSCONF_DIR)/$(IAM)

LLDLIBS = $(PCP_PMDALIB)

CFILES	= resctrl.c
DFILES	= help

VERSION_SCRIPT = exports
HELPTARGETS = help.dir help.pag
LDIRT	= domain.h *.o $(IAM).log $(CMDTARGET) $(LIBTARGET) $(VERSION_SCRIPT) $(HELPTARGETS)

MAN_SECTION = 1
MAN_PAGES = pmda$(IAM).$(MAN_SECTION)
MAN_DEST = $(PCP_MAN_DIR)/man$(MAN_SECTION)

LCFLAGS = -I.

default_pcp default:	build-me

include $(BUILDRULES)

ifeq "$(TARGET_OS)" "linux"
build-me:	$(CMDTARGET) $(LIBTARGET) $(HELPTARGETS)
	$(LOCALCONF_MAKERULE)

install_pcp install:	default
	$(INSTALL) -m 755 -d $(PMDAADMDIR)
	$(INSTALL) -m 755 -d $(PMDATMPDIR)
	$(INSTALL) -m 755 -t $(PMDATMPDIR) Install Remove $(CMDTARGET) $(LIBTARGET) $(PMDAADMDIR)
	$(INSTALL) -m 644 -t $(PMDATMPDIR) root root_resctrl domain.h $(DFILES) help.dir help.pag $(PMDAADMDIR)
	$(INSTALL) -m 644 -t $(PCP_PMNS_DIR)/root_resctrl root_resctrl $(PMDAADMDIR)/root_resctrl
	$(INSTALL) -S $(PMDAADMDIR)/root_resctrl $(PCP_PMNSADM_DIR)/root_resctrl
	$(INSTALL) -m 644 sys-fs-resctrl.mount $(PCP_SYSTEMDUNIT_DIR)/sys-fs-resctrl.mount
	@$(INSTALL_MAN)
else
build-me:
install_pcp install:
	@$(INSTALL_MAN)
endif

$(HELPTARGETS) : help
	$(NEWHELP) -n root_resctrl -v 2 -o help < help

$(VERSION_SCRIPT):
	$(VERSION_SCRIPT_MAKERULE)

domain.h: ../../pmns/stdpmid
	$(DOMAIN_MAKERULE)

$(OBJECTS): domain.h

resctrl.o: $(VERSION_SCRIPT)

resctrl.o: $(TOPDIR)/src/include/pcp/libpcp.h

check:: $(MAN_PAGES)
	$(MANLINT) $^
