#!gmake
#
# Copyright (c) 2000,2003,2004 Silicon Graphics, Inc.  All Rights Reserved.
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
# 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
#

TOPDIR = ../../../..
include $(TOPDIR)/src/include/builddefs

LIBTARGET = pmda_pmcd.$(DSOSUFFIX)

CFILES	= pmcd.c
LSRCFILES = objstyle

# Add to CFLAGS to find files in pmcd/src...
LCFLAGS = -I$(TOPDIR)/src
ifneq (, $(filter linux kfreebsd, $(TARGET_OS)))
ABI = $(shell ./objstyle)
LCFLAGS += -DSIM_ABI=\""$(ABI)"\"
endif
ifeq "$(TARGET_OS)" "darwin"
ABI = $(shell ./objstyle)
LCFLAGS += -DSIM_ABI=\"$(ABI)\"
endif
ifdef PACKAGE_BUILD
BUILD = $(PACKAGE_BUILD)
else
BUILD = unknown
endif

LCFLAGS += -DBUILD=\"$(BUILD)\"
LLDLIBS = -lpcp_pmda -lpcp
ifeq "$(TARGET_OS)" "mingw"
LLDLIBS += -lpcp_pmcd
PCP_LIBS += -L$(TOPDIR)/src/libpcp_pmcd/src
endif

default: $(LIBTARGET)

install: default
	$(INSTALL) -m 755 -d $(PCP_PMDAS_DIR)/pmcd
	$(INSTALL) -m 755 $(LIBTARGET) $(PCP_PMDAS_DIR)/pmcd/$(LIBTARGET)

include $(BUILDRULES)

default_pcp : default

install_pcp : install
