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

TOPDIR = ../../../..
include $(TOPDIR)/src/include/builddefs

#
# The TARGETS macro must be set to the name of the executable we
# are building.
#
CMDTARGET = pmdasample$(EXECSUFFIX)
LIBTARGET = pmda_sample.$(DSOSUFFIX)
TARGETS = $(CMDTARGET) $(LIBTARGET)

PMDADIR = $(PCP_PMDAS_DIR)/sample

LDIRT = $(TARGETS)

CFILES = pmda.c sample.c percontext.c events.c
HFILES = percontext.h events.h

LCFLAGS = -DDYNAMIC_SPEC=\"$(PCP_PMDAS_DIR)/sample/dynamic.indom\"
LLDLIBS = $(PCP_PMDALIB)

default:	../domain.h $(TARGETS)

pmda.o sample.o percontext.o: percontext.h
sample.o events.o: events.h

../domain.h:
	$(MAKE) -C .. domain.h

install:	default
	$(INSTALL) -m 755 -d $(PMDADIR)
	$(INSTALL) -m 755 $(CMDTARGET) $(LIBTARGET) $(PMDADIR)

include $(BUILDRULES)

default_pcp:	default

install_pcp:	install
