#!gmake
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
# You should have received a copy of the GNU General Public License along
# with this program; if not, write to the Free Software Foundation, Inc.,
# 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
# 
# Contact information: Silicon Graphics, Inc., 1500 Crittenden Lane,
# Mountain View, CA 94043, USA, or: http://www.sgi.com
#

TOPDIR = ../..
include $(TOPDIR)/src/include/builddefs

CFILES	= procmemstat.c
TARGETS	=
LLDLIBS	= -lpcp
LDIRT	= pmnsmap.h mylog.* runme.sh $(TARGETS) pmnsmap.i GNUmakefile.install.xxx
LSRCFILES = README GNUmakefile.install pmnsmap.spec

DEMODIR=$(PCP_DEMOS_DIR)/procmemstat

default: $(TARGETS) GNUmakefile.install.xxx

include $(BUILDRULES)

install: $(TARGETS)
	$(INSTALL) -m 755 -d $(DEMODIR)
	$(INSTALL) -m 644 GNUmakefile.install.xxx $(DEMODIR)/Makefile
	$(INSTALL) -m 644 procmemstat.c README pmnsmap.spec $(DEMODIR)

procmemstat.o : pmnsmap.h

pmnsmap.h:	pmnsmap.spec
	PCP_CONF=$(TOPDIR)/src/include/pcp.conf; export PCP_CONF; \
	sed -e "s;^\. \$PCP_DIR/etc/pcp.env;. $(TOPDIR)/src/include/pcp.env;" \
	../pmgenmap/pmgenmap.sh >runme.sh; \
	$(CPP) <pmnsmap.spec >pmnsmap.i
	sh ./runme.sh pmnsmap.i >pmnsmap.h

GNUmakefile.install.xxx:	GNUmakefile.install
	sed -e "s;<CPP>;$(CPP_SIMPLE);" <GNUmakefile.install >GNUmakefile.install.xxx

default_pcp : default

install_pcp : install
