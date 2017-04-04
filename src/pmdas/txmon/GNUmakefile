#
# Copyright (c) 2012-2015 Red Hat.
# Copyright (c) 2000,2003,2004 Silicon Graphics, Inc.  All Rights Reserved.
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

DFILES	= README
CFILES	= txmon.c txrecord.c
HFILES	= txmon.h
TARGETS	= txmon$(EXECSUFFIX) txrecord$(EXECSUFFIX)

LCFLAGS	= -I.
LLDLIBS	= $(PCP_PMDALIB)

SCRIPTS	= Install Remove genload
OTHERS	= pmns help root
LSRCFILES= $(SCRIPTS) $(OTHERS) GNUmakefile.install $(DFILES)

IAM	= txmon
DOMAIN	= TXMON
PMDADIR	= $(PCP_PMDAS_DIR)/$(IAM)

LDIRT	= *.o *.log *.dir *.pag domain.h $(TARGETS)

default:	build-me

include $(BUILDRULES)

ifneq "$(TARGET_OS)" "mingw"
build-me:	$(TARGETS)

install:	build-me
	$(INSTALL) -m 755 -d $(PMDADIR)
	$(INSTALL) -m 755 $(SCRIPTS) $(PMDADIR)
	$(INSTALL) -m 644 $(DFILES) root $(HFILES) $(PMDADIR)
	$(INSTALL) -m 644 help pmns domain.h $(CFILES) $(PMDADIR)
	$(INSTALL) -m 644 GNUmakefile.install $(PMDADIR)/Makefile
else
build-me:
install:
endif

domain.h:	../../pmns/stdpmid
	$(DOMAIN_MAKERULE)

$(OBJECTS):	domain.h

txmon$(EXECSUFFIX):	txmon.o
	$(CCF) -o $@ $(LDFLAGS) txmon.o $(LDLIBS)

txrecord$(EXECSUFFIX):	txrecord.o
	$(CCF) -o $@ $(LDFLAGS) txrecord.o $(LDLIBS)

default_pcp:	default

install_pcp:	install
