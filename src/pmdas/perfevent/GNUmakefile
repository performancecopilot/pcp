#
# Copyright (c) 2000,2003,2004,2008 Silicon Graphics, Inc.  All Rights Reserved.
# Copyright (c) 2007-2010 Aconex.  All Rights Reserved.
# Copyright (c) 2013-2015 Red Hat.
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
include	$(TOPDIR)/src/include/builddefs

IAM		= perfevent
DOMAIN		= PERFEVENT
CMDTARGET	= pmda$(IAM)

PMDADIR		= $(PCP_PMDAS_DIR)/$(IAM)

CFILES	= pmda.c perfmanager.c perfinterface.c architecture.c \
		  rapl-interface.c configparser.yy.c perflock.c

HFILES  = architecture.h configparser.h perfinterface.h perfmanager.h \
		  rapl-interface.h perflock.h

SCRIPTS		= Install Remove

DFILES	= README help

VERSION := $(shell head -n 1 ChangeLog | cut -d ":" -f 1)

default:	build-me

include $(BUILDRULES)

ifeq "$(PMDA_PERFEVENT)" "true"
build-me: $(CMDTARGET) $(HELPTARGETS) perfalloc

install: default
	$(INSTALL) -m 755 -d $(PMDADIR)
	$(INSTALL) -m 644 domain.h help root pmns perfevent.conf $(PMDADIR)
	$(INSTALL) -m 755 $(CMDTARGET) $(SCRIPTS) perfalloc $(PMDADIR)
else
build-me:
install:
endif

default_pcp : default

install_pcp : install

TARGETS = $(CMDTARGET) perfalloc

LLDLIBS = $(PCP_PMDALIB) $(PFM_LIBS)
LLDLIBS += $(LIB_FOR_RT) $(LIB_FOR_MATH) $(LIB_FOR_PTHREADS)
LCFLAGS = -I.
CFLAGS += -DVERSION=\"$(VERSION)\"

$(OBJECTS): domain.h

domain.h: ../../pmns/stdpmid
	$(DOMAIN_MAKERULE)

configparser.yy.c: configparser.l configparser.h
	$(LEX) -o $@ $<

perfalloc: perfalloc.o perflock.o

clean clobber::
	$(RM) configparser.yy.c perfalloc perfalloc.o

