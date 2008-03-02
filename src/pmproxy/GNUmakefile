#!gmake
#
# Copyright (c) 2000-2002 Silicon Graphics, Inc.  All Rights Reserved.
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
# $Id: GNUmakefile,v 1.6 2005/07/10 23:46:11 kenmcd Exp $
#

TOPDIR = ../..
include $(TOPDIR)/src/include/builddefs

TARGET = pmproxy
HFILES = pmproxy.h
CFILES = pmproxy.c client.c
LSRCFILES = CHECKOUT rc_pmproxy pmproxy.options

LLDLIBS	= -lpcp $(LIB_FOR_BASENAME)
LDIRT = $(TARGET) pmproxy.log

default: $(TARGET)

$(TARGET): $(OBJECTS)

include $(BUILDRULES)

install: default
	$(INSTALL) -m 755 -d `dirname $(PCP_PMPROXYOPTIONS_PATH)`
	$(INSTALL) -m 644 pmproxy.options $(PCP_PMPROXYOPTIONS_PATH)
	$(INSTALL) -m 755 rc_pmproxy $(PCP_RC_DIR)/pmproxy
	$(INSTALL) -m 755 $(TARGET) $(PCP_BINADM_DIR)/$(TARGET)
	$(INSTALL) -m 755 -d $(PCP_LOG_DIR)/pmproxy

default_pcp : default

install_pcp : install

pmproxy.o client.o:	pmproxy.h
