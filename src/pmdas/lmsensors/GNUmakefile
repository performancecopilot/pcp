#
# Original implementation by Troy Dawson (dawson@fnal.gov)
# 
# Copyright (c) 2001,2004 Silicon Graphics, Inc.  All Rights Reserved.
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

IAM	= lmsensors
DOMAIN	= LMSENSORS
TARGETS	= $(IAM)
CFILES	= lmsensors.c
HFILES	= lmsensors.h
SCRIPTS	= Install Remove
DFILES	= README

PMDADIR	= $(PCP_PMDAS_DIR)/$(IAM)

LDIRT	= domain.h $(IAM).log pmda$(IAM) pmda_$(IAM).so $(TARGETS) \
	help.pag help.dir
LLDLIBS	= $(PCP_PMDALIB)

MAN_SECTION = 1
MAN_PAGES = pmda$(IAM).$(MAN_SECTION)
MAN_DEST = $(PCP_MAN_DIR)/man$(MAN_SECTION)

default: build-me

include $(BUILDRULES)

ifeq "$(TARGET_OS)" "linux"
build-me: $(TARGETS)

install : default
	$(INSTALL) -m 755 -d $(PMDADIR)
	$(INSTALL) -m 755 $(IAM) $(PMDADIR)/pmda$(IAM)
	$(INSTALL) -m 755 $(SCRIPTS) $(PMDADIR)
	$(INSTALL) -m 644 $(DFILES) pmns help root domain.h $(PMDADIR)
	@$(INSTALL_MAN)
else
build-me:
install:
endif

$(IAM):	$(OBJECTS)

$(OBJECTS): domain.h

default_pcp: default

install_pcp: install

domain.h: ../../pmns/stdpmid
	$(DOMAIN_MAKERULE)
