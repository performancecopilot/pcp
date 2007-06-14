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
# $Id: GNUmakefile,v 2.29 2004/11/26 04:24:50 kenmcd Exp $
#

TOPDIR = ../../..
include $(TOPDIR)/src/include/builddefs

TARGET	= pmkstat$(EXECSUFFIX)
CFILES	= pmkstat.c
LSRCFILES = pmnsmap.spec
LDIRT 	= pmnsmap.h runme.sh $(TARGET)

LLDLIBS	= -lpcp

default:	$(TARGET)

include $(BUILDRULES)

pmkstat:	$(OBJECTS)

$(TARGET):	pmkstat.o
pmkstat.o:	pmnsmap.h

install:	default
	$(INSTALL) -m 755 pmkstat $(PCP_BINADM_DIR)/pmkstat

pmnsmap.h:	pmnsmap.spec
	PCP_CONF=$(TOPDIR)/src/include/pcp.conf; export PCP_CONF; \
	sed -e "s;^\. /etc/pcp.env;. $(TOPDIR)/src/include/pcp.env;" \
	../../pmgenmap/pmgenmap.sh >runme.sh; \
	sh ./runme.sh pmnsmap.spec >pmnsmap.h

default_pcp:	default

install_pcp:	install
