#
# Copyright (c) 2000,2004 Silicon Graphics, Inc.  All Rights Reserved.
# Copyright (c) 2011 Ken McDonell.  All Rights Reserved.
# Copyright (c) 2012,2018,2020 Red Hat, Inc. All Rights Reserved.
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

CFILES	= pmlogrewrite.c util.c metric.c indom.c result.c label.c text.c \
	  replace.c
HFILES	= logger.h
LFILES  = lex.l
YFILES	= gram.y

CMDTARGET = pmlogrewrite$(EXECSUFFIX)
LLDLIBS	= $(PCP_ARCHIVELIB) $(PCPLIB) $(LIB_FOR_MATH)
LDIRT	+= $(YFILES:%.y=%.tab.?)

default:	$(CMDTARGET)

include $(BUILDRULES)

pmlogrewrite$(EXECSUFFIX):	$(OBJECTS)

install:	$(CMDTARGET)
	$(INSTALL) -m 755 -d $(PCP_SYSCONF_DIR)/pmlogrewrite
	$(INSTALL) -m 755 -d $(PCP_VAR_DIR)/config/pmlogrewrite
	$(INSTALL) -m 755 $(CMDTARGET) $(PCP_BIN_DIR)/$(CMDTARGET)
	$(INSTALL) -S $(PCP_BIN_DIR)/$(CMDTARGET) $(PCP_BINADM_DIR)/$(CMDTARGET)

.NOTPARALLEL:
gram.tab.h gram.tab.c:	gram.y

lex.o gram.tab.o:	gram.tab.h logger.h
indom.o label.o text.o metric.o result.o util.o pmlogrewrite.o:	logger.h

default_pcp:	default

install_pcp:	install

gram.tab.o indom.o label.o text.o lex.o metric.o pmlogrewrite.o result.o:	$(TOPDIR)/src/include/pcp/libpcp.h
