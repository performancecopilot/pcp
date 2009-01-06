# !gmake
#
# Copyright (c) 2007,2008 Silicon Graphics, Inc.  All Rights Reserved.
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
# You should have received a copy of the GNU General Public License along
# with this program; if not, write to the Free Software Foundation, Inc.,
# 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA

TOPDIR = ../../..
include $(TOPDIR)/src/include/builddefs

IAM	= ib
DOMAIN  = IB
CMDTARGET= pmda$(IAM)
LIBTARGET= pmda_$(IAM).so
TARGETS	= $(CMDTARGET) $(LIBTARGET)

CFILES	= ib.c pmda.c
HFILES  = ibpmda.h

OTHER = help root pmns Install Remove 
LSRCFILES = $(OTHER) daemon.c dso.c

$(CMDTARGET)_OBJS = $(CFILES:.c=.o) daemon.o
$(LIBTARGET)_OBJS = $(CFILES:.c=.o) dso.o

IBLIBS = $(PCP_LIBIBUMAD) $(PCP_LIBIBMAD) $(PCP_LIBIBCOMMON) 

LLDLIBS	= $(IBLIBS) $(PCP_LIBS) -lpcp_pmda -lpcp

PMDADIR = $(PCP_PMDAS_DIR)/$(IAM)
LDIRT	= *.o $(TARGETS) *.log *.dir *.pag pcp domain.h

default: build-me

include $(BUILDRULES)

ifeq ($(HAVE_IBDEV),1)
build-me: pcp help.pag help.dir $(TARGETS)
install: default
	$(INSTALL) -m 755 -d $(PMDADIR)
	$(INSTALL) -m 755 $(CMDTARGET) $(PMDADIR)/$(CMDTARGET)
	$(INSTALL) -m 755 $(LIBTARGET) $(PMDADIR)/$(LIBTARGET)
	$(INSTALL) -m 755 Install Remove $(PMDADIR)
	$(INSTALL) -m 644 pmns root domain.h $(PMDADIR)
	$(INSTALL) -m 644 help help.pag help.dir $(PMDADIR)
else
build-me:
install:
endif

domain.h:       ../../pmns/stdpmid
	rm -f $@
	egrep "#define +$(DOMAIN)" $? > $@

pcp:
	ln -s $(TOPDIR)/src/include pcp

help.dir help.pag : help domain.h
	$(RUN_IN_BUILD_ENV) $(TOPDIR)/src/newhelp/newhelp -n root -v 2 -o help < help

default_pcp: default

install_pcp: install

$(CMDTARGET) : OBJECTS=$($(CMDTARGET)_OBJS)
$(CMDTARGET) : $($(CMDTARGET)_OBJS)

$(LIBTARGET) : OBJECTS=$($(LIBTARGET)_OBJS)
$(LIBTARGET) : $($(LIBTARGET)_OBJS)
