#
# Copyright (c) 2000,2003,2004,2008 Silicon Graphics, Inc.  All Rights Reserved.
# Copyright (c) 2007-2010 Aconex.  All Rights Reserved.
# Copyright (c) 2013-2015,2020 Red Hat.
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
CMDTARGET	= pmdaperfevent
LIBTARGET	= pmda_perfevent.so
PMDAINIT	= perfevent_init
PMDAADMDIR	= $(PCP_PMDASADM_DIR)/$(IAM)
PMDACONFIG	= $(PCP_SYSCONF_DIR)/$(IAM)
PMDATMPDIR	= $(PCP_PMDAS_DIR)/$(IAM)

CFILES	= pmda.c perfmanager.c perfinterface.c architecture.c \
		  rapl-interface.c configparser.yy.c perflock.c \
		  parse_events.c

HFILES  = architecture.h configparser.h perfinterface.h perfmanager.h \
		  rapl-interface.h perflock.h parse_events.h

SCRIPTS	= Install Remove Upgrade perfevent-makerewrite.pl

DFILES	= README help

TARGETS = $(CMDTARGET) $(LIBTARGET) perfalloc

LLDLIBS = $(PCP_PMDALIB) $(PFM_LIBS)
LLDLIBS += $(LIB_FOR_RT) $(LIB_FOR_MATH) $(LIB_FOR_PTHREADS)
LCFLAGS = -I.
CFLAGS	+= -DVERSION=\"$(VERSION)\"

VERSION := $(shell head -n 1 ChangeLog | cut -d ":" -f 1)

default_pcp default:	build-me

include $(BUILDRULES)

ifeq "$(PMDA_PERFEVENT)" "true"
build-me: $(TARGETS)

install_pcp install: default
	$(INSTALL) -m 755 -d $(PMDAADMDIR)
	$(INSTALL) -m 755 -d $(PMDATMPDIR)
	$(INSTALL) -m 644 -t $(PMDATMPDIR) domain.h help root pmns $(PMDAADMDIR)
	$(INSTALL) -m 755 -t $(PMDATMPDIR) $(CMDTARGET) $(LIBTARGET) $(SCRIPTS) perfalloc $(PMDAADMDIR)
	$(INSTALL) -m 755 -d $(PMDACONFIG)
	$(INSTALL) -m 644 -t $(PMDATMPDIR)/$(IAM).conf $(IAM).conf $(PMDACONFIG)/$(IAM).conf
else
build-me:
install_pcp install:
endif

$(OBJECTS): domain.h

domain.h: ../../pmns/stdpmid
	$(DOMAIN_MAKERULE)

configparser.yy.c: configparser.l configparser.h
	$(LEX) -o $@ $<

perfalloc: perfalloc.o perflock.o

clean clobber::
	$(RM) configparser.yy.c perfalloc perfalloc.o
