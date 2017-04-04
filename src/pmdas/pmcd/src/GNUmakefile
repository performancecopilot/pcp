#
# Copyright (c) 2000,2003,2004 Silicon Graphics, Inc.  All Rights Reserved.
# Copyright (c) 2015 Red Hat Inc.
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

LIBTARGET = pmda_pmcd.$(DSOSUFFIX)
PMDAINIT = pmcd_init
DOMAIN = PMCD

CFILES = pmcd.c
VERSION_SCRIPT = exports
LDIRT = $(VERSION_SCRIPT) domain.h

# Add to CFLAGS to find files in pmcd/src...
LCFLAGS = -I$(TOPDIR)/src
LCFLAGS += -I$(TOPDIR)/src/pmie/src

ifneq (, $(filter linux kfreebsd gnu, $(TARGET_OS)))
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
LCFLAGS += $(INVISIBILITY)

LLDLIBS = $(PCP_PMDALIB)
ifeq "$(TARGET_OS)" "mingw"
LLDLIBS += -lpcp_pmcd
PCPLIB_LDFLAGS += -L$(TOPDIR)/src/libpcp_pmcd/src
endif

default: $(LIBTARGET)

install: default
	$(INSTALL) -m 755 -d $(PCP_PMDAS_DIR)/pmcd
	$(INSTALL) -m 755 $(LIBTARGET) $(PCP_PMDAS_DIR)/pmcd/$(LIBTARGET)

include $(BUILDRULES)

default_pcp : default

install_pcp : install

$(OBJECTS): domain.h

domain.h: $(TOPDIR)/src/pmns/stdpmid
	$(DOMAIN_MAKERULE)

$(VERSION_SCRIPT):
	$(VERSION_SCRIPT_MAKERULE)

$(LIBTARGET): $(VERSION_SCRIPT)
