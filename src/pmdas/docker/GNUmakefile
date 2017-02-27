#
# Copyright (c) 2016 Red Hat.
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
PMDADIR	= $(PCP_PMDAS_DIR)/$(IAM)
PMDAINIT = $(IAM)_init

LLDLIBS = $(PCP_PMDALIB) $(PCP_WEBLIB) $(LIB_FOR_PTHREADS)
LLDFLAGS = -L$(TOPDIR)/src/libpcp_web/src
#LCFLAGS = -I.
LCFLAGS = -I$(TOPDIR)/src/libpcp_web/src
LCFLAGS += -g

CFILES	= docker.c
DFILES	= help

LDIRT	= domain.h *.o $(IAM).log $(CMDTARGET) $(LIBTARGET)

MAN_SECTION = 1
MAN_PAGES = pmda$(IAM).$(MAN_SECTION)
MAN_DEST = $(PCP_MAN_DIR)/man$(MAN_SECTION)

default_pcp default:	build-me

include $(BUILDRULES)

ifeq "$(TARGET_OS)" "linux"
build-me:	$(CMDTARGET) $(LIBTARGET)

install_pcp install:	default 
	$(INSTALL) -m 755 -d $(PMDADIR)
	$(INSTALL) -m 755 Install Remove $(CMDTARGET) $(LIBTARGET) $(PMDADIR)
	$(INSTALL) -m 644 root pmns domain.h $(DFILES) $(PMDADIR)
	@$(INSTALL_MAN)
else
build-me:
install_pcp install:
	@$(INSTALL_MAN)
endif

$(VERSION_SCRIPT):
	$(VERSION_SCRIPT_MAKERULE)

domain.h: ../../pmns/stdpmid
	$(DOMAIN_MAKERULE)

$(OBJECTS): domain.h

docker.o: $(VERSION_SCRIPT)
