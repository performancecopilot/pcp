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

CFILES	= pmclient.c
TARGET	= pmclient$(EXECSUFFIX)
LLDLIBS	= -lpcp
LDIRT	= pmnsmap.h mylog.* runme.sh $(TARGET)
LSRCFILES = README GNUmakefile.install pmlogger.config pmnsmap.spec

PMLOGGERCONFIG=$(PCP_VAR_DIR)/config/pmlogger
DEMODIR=$(PCP_DEMOS_DIR)/pmclient

default:	$(TARGET)

include $(BUILDRULES)

install:	$(TARGET)
	$(INSTALL) -m 755 pmclient $(PCP_BIN_DIR)/pmclient
	$(INSTALL) -m 644 pmlogger.config $(PMLOGGERCONFIG)/config.pmclient
	$(INSTALL) -m 755 -d $(DEMODIR)
	$(INSTALL) -m 644 GNUmakefile.install $(DEMODIR)/Makefile
	$(INSTALL) -m 644 pmclient.c README pmnsmap.spec $(DEMODIR)

$(TARGET):	pmclient.o
pmclient.o:	pmnsmap.h

pmnsmap.h:	pmnsmap.spec
	PCP_CONF=$(TOPDIR)/src/include/pcp.conf; export PCP_CONF; \
	sed -e "s;^\. /etc/pcp.env;. $(TOPDIR)/src/include/pcp.env;" \
	../pmgenmap/pmgenmap.sh >runme.sh; \
	sh ./runme.sh pmnsmap.spec >pmnsmap.h

default_pcp:	default

install_pcp:	install
