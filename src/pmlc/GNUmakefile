#
# Copyright (c) 2000,2004 Silicon Graphics, Inc.  All Rights Reserved.
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

CMDTARGET = pmlc$(EXECSUFFIX)
CFILES	= pmlc.c util.c actions.c
HFILES	= pmlc.h
LFILES  = lex.l
YFILES	= gram.y
LDIRT	= *.log foo.* $(YFILES:%.y=%.tab.?) $(CMDTARGET) $(LFILES:.l=.c) gram.h
LLDLIBS = $(PCPLIB)

default: $(CMDTARGET)

$(CMDTARGET):	$(OBJECTS)

install:	$(CMDTARGET) 
	$(INSTALL) -m 755 $(CMDTARGET) $(PCP_BIN_DIR)/$(CMDTARGET)

pmlc.o:	gram.h

.NOTPARALLEL:
gram.tab.h gram.tab.c:	gram.y
	$(YACC) -d -b `basename $< .y` $<

gram.h:	gram.tab.h
	rm -f $@ && $(LN_S) $< $@

include $(BUILDRULES)

default_pcp:	default

install_pcp:	install
