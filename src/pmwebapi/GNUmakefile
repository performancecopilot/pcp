#
# Copyright (c) 2011-2013 Red Hat Inc.
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

TOPDIR = ../..
include $(TOPDIR)/src/include/builddefs

CMDTARGET = pmwebd$(EXECSUFFIX)
HFILES = pmwebapi.h
CFILES	= main.c util.c pmwebapi.c pmresapi.c
LSRCFILES = rc_pmwebd pmwebd.options

LLDLIBS = $(PCPLIB) -lmicrohttpd
LDIRT = pmwebd.log

LCFLAGS += -Wextra
LCFLAGS += $(PIECFLAGS)
LLDFLAGS += $(PIELDFLAGS)

SUBDIRS = jsdemos

default:	build-me

include $(BUILDRULES)

ifeq "$(HAVE_LIBMICROHTTPD)" "1"
build-me:	$(CMDTARGET)

install:	default
	$(INSTALL) -m 755 -d `dirname $(PCP_PMWEBDOPTIONS_PATH)`
	$(INSTALL) -m 644 pmwebd.options $(PCP_PMWEBDOPTIONS_PATH)
	$(INSTALL) -m 755 rc_pmwebd $(PCP_RC_DIR)/pmwebd
	$(INSTALL) -m 755 $(CMDTARGET) $(PCP_BINADM_DIR)/$(CMDTARGET)
	$(INSTALL) -m 775 -o $(PCP_USER) -g $(PCP_GROUP) -d $(PCP_LOG_DIR)/pmwebd
else
build-me:
install:
endif

default_pcp ::	default

default_pcp :: $(SUBDIRS)
	$(SUBDIRS_MAKERULE)

install_pcp :: install

install_pcp :: $(SUBDIRS)
	$(SUBDIRS_MAKERULE)
