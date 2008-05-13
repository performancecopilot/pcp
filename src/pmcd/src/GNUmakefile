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

TOPDIR = ../../..
include $(TOPDIR)/src/include/builddefs

TARGET = pmcd$(EXECSUFFIX)
HFILES = client.h pmcd.h
CFILES = pmcd.c config.c dofetch.c dopdus.c util.c dostore.c \
	  client.c agent.c trace.c

LLDLIBS	= -lpcp $(LIB_FOR_BASENAME) $(LIB_FOR_DLOPEN)
LDIRT = $(TARGET)

# Note: on platforms that need it (see configure.in) the "-rdynamic" flag
# allows the pmcd pmda (dynamicaly loaded shared library) to access the
# global symbols in pmcd.
#LCFLAGS = $(RDYNAMIC_FLAG)
LLDFLAGS = $(RDYNAMIC_FLAG)

default:	$(TARGET)

$(TARGET):	$(OBJECTS)

include $(BUILDRULES)

install:	default
	$(INSTALL) -m 755 $(TARGET) $(PCP_BINADM_DIR)/$(TARGET)

default_pcp:	default

install_pcp:	install
