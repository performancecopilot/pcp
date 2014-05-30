#
# Copyright (c) 2011-2014 Red Hat Inc.
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

CXXMDTARGET = pmwebd$(EXECSUFFIX)
HFILES = pmwebapi.h
CXXFILES = main.cxx pmwebapi.cxx pmresapi.cxx pmgraphite.cxx util.cxx
LSRCFILES = rc_pmwebd pmwebd.options

LLDLIBS = $(PCPLIB) -lmicrohttpd
LDIRT = pmwebd.log pmwebd.service

LCFLAGS += -Wextra
LCFLAGS += $(PIECFLAGS)
LLDFLAGS += $(PIELDFLAGS)

SUBDIRS = jsdemos

default:	build-me

$(OBJECTS): $(HFILES)

include $(BUILDRULES)

ifeq "$(HAVE_CAIRO)" "1"
LLDLIBS += $(LIB_FOR_CAIRO)
LCFLAGS += -DHAVE_CAIRO
endif

ifeq "$(HAVE_LIBMICROHTTPD)" "1"
build-me:	$(CXXMDTARGET) pmwebd.service

pmwebd.service:	pmwebd.service.in
	$(SED) -e 's;@path@;'$(PCP_RC_DIR)';' $< > $@

install:	default
	$(INSTALL) -m 755 -d `dirname $(PCP_PMWEBDOPTIONS_PATH)`
	$(INSTALL) -m 644 pmwebd.options $(PCP_PMWEBDOPTIONS_PATH)
	$(INSTALL) -m 755 rc_pmwebd $(PCP_RC_DIR)/pmwebd
ifeq ($(ENABLE_SYSTEMD),true)
	$(INSTALL) -m 644 pmwebd.service $(PCP_SYSTEMDUNIT_DIR)/pmwebd.service
endif
	$(INSTALL) -m 755 $(CXXMDTARGET) $(PCP_BINADM_DIR)/$(CXXMDTARGET)
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

indent:
	indent -nhnl -l400 $(CXXFILES) $(HFILES)
	astyle  -A10 -n -xd -v -H -U -xy -xC90 $(CXXFILES) $(HFILES)
