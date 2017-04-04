#
# Copyright (c) 2004 Silicon Graphics, Inc.  All Rights Reserved.
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

CFILES	= pmlogreduce.c logio.c dometric.c rewrite.c indom.c scan.c
HFILES	= pmlogreduce.h

CMDTARGET = pmlogreduce$(EXECSUFFIX)
LLDLIBS	= $(PCPLIB)

default: $(CMDTARGET)

include $(BUILDRULES)

pmlogreduce : $(OBJECTS)

install: $(CMDTARGET)
	$(INSTALL) -m 755 $(CMDTARGET) $(PCP_BINADM_DIR)/$(CMDTARGET)

pmlogreduce.o:	pmlogreduce.h
logio.o:	pmlogreduce.h
dometric.o:	pmlogreduce.h
rewrite.o:	pmlogreduce.h
indom.o:	pmlogreduce.h
wrap.o:		pmlogreduce.h
scan.o:		pmlogreduce.h

default_pcp : default

install_pcp : install
