#! gmake
#
# Copyright (c) 2000-2004 Silicon Graphics, Inc.  All Rights Reserved.
# 
# This library is free software; you can redistribute it and/or modify it
# under the terms of the GNU Lesser General Public License as published
# by the Free Software Foundation; either version 2.1 of the License, or
# (at your option) any later version.
# 
# This library is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
# or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public
# License for more details.
# 
# You should have received a copy of the GNU Lesser General Public License
# along with this library; if not, write to the Free Software Foundation,
# Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA.
# 
# Contact information: Silicon Graphics, Inc., 1500 Crittenden Lane,
# Mountain View, CA 94043, USA, or: http://www.sgi.com
#

TOPDIR = ../../..
include	$(TOPDIR)/src/include/builddefs

DSOVERSION = 1
LIBTARGET = libpcp_pmc.$(DSOSUFFIX).$(DSOVERSION)
SYMTARGET = libpcp_pmc.$(DSOSUFFIX)
ifeq "$(TARGET_OS)" "darwin"
LIBTARGET = libpcp_pmc.$(DSOVERSION).$(DSOSUFFIX)
endif
ifeq "$(TARGET_OS)" "cygwin"
LIBTARGET = libpcp_pmc.$(DSOSUFFIX)
SYMTARGET =
endif
STATICLIBTARGET = libpcp_pmc.a

CXXFILES = String.c++ Desc.c++ Indom.c++ Source.c++ Context.c++ \
	   Group.c++ Metric.c++ 

LCXXFLAGS += -I..
LOBJECTS=${CXXFILES:%.c++=%.o}

LDIRT	= $(SYMTARGET) pcp

default: $(LIBTARGET) $(STATICLIBTARGET) $(SYMTARGET)

ifneq "$(SYMTARGET)" ""
$(SYMTARGET):
	$(LN_S) -f $(LIBTARGET) $(SYMTARGET)
endif

include $(BUILDRULES)

$(LIBTARGET) : $(LOBJECTS)

# header dependencies have to be explicit ...
#
Context.o:	../pcp/pmc/Context.h ../pcp/pmc/Metric.h
Desc.o:		../pcp/pmc/Desc.h
Group.o:	../pcp/pmc/String.h ../pcp/pmc/Hash.h ../pcp/pmc/Group.h \
		../pcp/pmc/Source.h ../pcp/pmc/Context.h ../pcp/pmc/Metric.h
Indom.o:	../pcp/pmc/Indom.h ../pcp/pmc/Desc.h
Metric.o:	../pcp/pmc/Metric.h ../pcp/pmc/Group.h
Source.o:	../pcp/pmc/Source.h ../pcp/pmc/String.h ../pcp/pmc/Bool.h

install: default
	$(INSTALL) -m 755 $(LIBTARGET) $(PCP_LIB_DIR)/$(LIBTARGET)
	$(INSTALL) -m 755 $(STATICLIBTARGET) $(PCP_LIB_DIR)/$(STATICLIBTARGET)
ifneq "$(SYMTARGET)" ""
	$(INSTALL) -S $(LIBTARGET) $(PCP_LIB_DIR)/$(SYMTARGET)
endif

$(LOBJECTS): pcp

pcp:
	$(LN_S) $(TOPDIR)/src/include pcp

default_pcp:	default

install_pcp:	install
