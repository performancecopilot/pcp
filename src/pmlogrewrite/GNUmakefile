#
# Copyright (c) 2000,2004 Silicon Graphics, Inc.  All Rights Reserved.
# Copyright (c) 2011 Ken McDonell.  All Rights Reserved.
# Copyright (c) 2012 Red Hat, Inc. All Rights Reserved.
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

TOPDIR = ../..
include $(TOPDIR)/src/include/builddefs
-include ./GNUlocaldefs

CFILES	= pmlogrewrite.c util.c logio.c metric.c indom.c result.c
HFILES	= logger.h
LFILES  = lex.l
YFILES	= gram.y

CMDTARGET = pmlogrewrite$(EXECSUFFIX)
LLDLIBS	= $(PCPLIB) $(LIB_FOR_MATH)
LDIRT	+= $(YFILES:%.y=%.tab.?)

default:	$(CMDTARGET)

include $(BUILDRULES)

pmlogextract:	$(OBJECTS)

install:	$(CMDTARGET)
	$(INSTALL) -m 755 -d $(PCP_VAR_DIR)/config/pmlogrewrite
	$(INSTALL) -m 755 $(CMDTARGET) $(PCP_BINADM_DIR)/$(CMDTARGET)

.NOTPARALLEL:
gram.tab.h gram.tab.c:	gram.y

lex.o gram.tab.o:	gram.tab.h logger.h
indom.o metric.o result.o util.o pmlogrewrite.o:	logger.h

default_pcp:	default

install_pcp:	install
