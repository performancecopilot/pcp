#
# Copyright (c) 2016-2017,2020,2026 Red Hat.
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

IAM	= docker
DOMAIN	= DOCKER
CMDTARGET = pmda$(IAM)$(EXECSUFFIX)
LIBTARGET = pmda_$(IAM).$(DSOSUFFIX)
PMDAINIT = $(IAM)_init
PMCDCONF_LINE  = "docker	141	pipe	binary		$(PMDATMPDIR)/$(CMDTARGET) -d 141"
LOCALCONF_LINE  = "docker	141	dso	$(PMDAINIT)	$(PMDATMPDIR)/$(LIBTARGET)"

PMDAADMDIR = $(PCP_PMDASADM_DIR)/$(IAM)
PMDATMPDIR = $(PCP_PMDAS_DIR)/$(IAM)

LLDLIBS = $(PCP_WEBLIB) $(LIB_FOR_PTHREADS)

CFILES	= docker.c
DFILES	= help

VERSION_SCRIPT	= exports
HELPTARGETS	= help.dir help.pag
LDIRT	= $(HELPTARGETS) domain.h *.o $(IAM).log $(CMDTARGET) $(LIBTARGET) $(VERSION_SCRIPT)

MAN_SECTION = 1
MAN_PAGES = pmda$(IAM).$(MAN_SECTION)
MAN_DEST = $(PCP_MAN_DIR)/man$(MAN_SECTION)

default_pcp default:	build-me

include $(BUILDRULES)

ifeq "$(TARGET_OS)" "linux"
build-me:	$(CMDTARGET) $(LIBTARGET) $(HELPTARGETS)

install_pcp install:	default
	$(INSTALL) -m 755 -d $(PMDAADMDIR)
	$(INSTALL) -m 755 -d $(PMDATMPDIR)
	$(INSTALL) -m 755 -t $(PMDATMPDIR) Install Remove $(CMDTARGET) $(LIBTARGET) $(PMDAADMDIR)
	$(INSTALL) -m 644 -t $(PMDATMPDIR) help.dir help.pag root root_docker pmns domain.h $(DFILES) $(PMDAADMDIR)
	$(INSTALL) -m 644 -t $(PCP_PMNS_DIR)/root_docker root_docker $(PMDAADMDIR)/root_docker
	$(INSTALL) -S $(PMDAADMDIR)/root_docker $(PCP_PMNSADM_DIR)/root_docker
	@$(INSTALL_MAN)
else
build-me:
install_pcp install:
	@$(INSTALL_MAN)
endif

$(HELPTARGETS) : help
	$(NEWHELP) -n root_docker -v 2 -o help < help

$(VERSION_SCRIPT):
	$(VERSION_SCRIPT_MAKERULE)

domain.h: ../../pmns/stdpmid
	$(DOMAIN_MAKERULE)

$(OBJECTS): domain.h

docker.o: $(VERSION_SCRIPT)

docker.o:	$(TOPDIR)/src/include/pcp/libpcp.h

check:: $(MAN_PAGES)
	$(MANLINT) $^
