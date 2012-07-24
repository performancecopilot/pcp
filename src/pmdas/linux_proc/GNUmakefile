#
# Copyright (c) 2000,2003,2004,2008 Silicon Graphics, Inc.  All Rights Reserved.
# Copyright (c) 2007-2010 Aconex.  All Rights Reserved.
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

IAM		= proc
DOMAIN		= PROC
CMDTARGET	= pmdaproc
LIBTARGET	= pmda_proc.so
PMDADIR		= $(PCP_PMDAS_DIR)/$(IAM)

CFILES		= pmda.c cgroups.c dynamic.c filesys.c proc_pid.c proc_runq.c \
		  ksym.c getinfo.c

HFILES		= cgroups.h clusters.h dynamic.h proc_pid.h indom.h getinfo.h \
		  proc_runq.h ksym.h filesys.h

LSRCFILES 	= Install Remove help pmns root
LDIRT		= help.dir help.pag domain.h

LLDLIBS		= -lpcp_pmda -lpcp

# Uncomment these flags for profiling
# LCFLAGS	= -pg
# LLDFLAGS	= -pg

default:	build-me

include $(BUILDRULES)

ifeq "$(TARGET_OS)" "linux"
build-me: domain.h $(LIBTARGET) $(CMDTARGET) help.dir help.pag

install: default
	$(INSTALL) -m 755 -d $(PMDADIR)
	$(INSTALL) -m 755 Install Remove $(PMDADIR)
	$(INSTALL) -m 644 domain.h pmns root help help.dir help.pag $(PMDADIR)
	$(INSTALL) -m 755 $(LIBTARGET) $(CMDTARGET) $(PMDADIR)
else
build-me:
install:
endif

help.dir help.pag : help
	$(RUN_IN_BUILD_ENV) $(TOPDIR)/src/newhelp/newhelp -n root -v 2 -o help < help

default_pcp : default

install_pcp : install

domain.h: ../../pmns/stdpmid
	$(DOMAIN_MAKERULE)

cgroups.o dynamic.o pmda.o: clusters.h
