#
# Copyright (c) 2008 Silicon Graphics, Inc.  All Rights Reserved.
#
# Author: Scott Emery <emery@sgi.com> 
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

IAM	= lustrecomm
DOMAIN	= LUSTRECOMM
PMDADIR	= $(PCP_PMDAS_DIR)/$(IAM)

DFILES 	= README
HFILES	= libreadfiles.h
CFILES	= $(IAM).c file_indexed.c file_single.c refresh_file.c timespec_routines.c

LIBTARGET = pmda_$(IAM).so
CMDTARGET = pmda$(IAM)
TARGETS = $(CMDTARGET)
#TARGETS = $(LIBTARGET) 

LDOPTS	=
LSRCFILES = Install Remove README help pmns root
LLDLIBS	= $(PCP_PMDALIB) -lrt
LCFLAGS = -I.

LDIRT	= domain.h *.o $(IAM).log pmda$(IAM) pmda_$(IAM).so

default: build-me

include $(BUILDRULES)

ifeq "$(TARGET_OS)" "linux"
build-me:	domain.h $(TARGETS)

install:	default
	$(INSTALL) -m 755 -d $(PMDADIR)
	$(INSTALL) -m 755 $(CMDTARGET) $(PMDADIR)/$(CMDTARGET)
	#$(INSTALL) -m 755 $(LIBTARGET) $(PMDADIR)/$(LIBTARGET)
	$(INSTALL) -m 755 Install Remove $(PMDADIR)
	$(INSTALL) -m 644 $(DFILES) root pmns domain.h help $(PMDADIR)
else
build-me:
install:
endif

default_pcp: default

install_pcp: install

domain.h: ../../pmns/stdpmid
	$(DOMAIN_MAKERULE)
