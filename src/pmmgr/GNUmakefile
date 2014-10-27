#
# Copyright (c) 2013-2014 Red Hat.
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

SUBDIRS = config
CXXMDTARGET = pmmgr$(EXECSUFFIX)
HFILES = pmmgr.h
CXXFILES = pmmgr.cxx
LLDLIBS	= $(PCPLIB) $(LIB_FOR_PTHREADS) $(LIB_FOR_ATOMIC)
LLDFLAGS += $(RDYNAMIC_FLAG) $(PIELDFLAGS)
LCFLAGS += $(PIECFLAGS)
LDIRT = *.log pmmgr.service

default: build-me

ifeq "$(ENABLE_MANAGER)" "true"
build-me: $(SUBDIRS) $(CXXMDTARGET) pmmgr.service
	$(SUBDIRS_MAKERULE)

pmmgr.service: pmmgr.service.in
	$(SED) -e 's;@path@;'$(PCP_RC_DIR)';' $< > $@

install: $(SUBDIRS) $(CXXMDTARGET)
	$(SUBDIRS_MAKERULE)
	$(INSTALL) -m 755 -d `dirname $(PCP_PMMGROPTIONS_PATH)`
	$(INSTALL) -m 644 pmmgr.options $(PCP_PMMGROPTIONS_PATH)
	$(INSTALL) -m 755 rc_pmmgr $(PCP_RC_DIR)/pmmgr
ifeq ($(ENABLE_SYSTEMD),true)
	$(INSTALL) -m 644 pmmgr.service $(PCP_SYSTEMDUNIT_DIR)/pmmgr.service
endif
	$(INSTALL) -m 755 $(CXXMDTARGET) $(PCP_BINADM_DIR)/$(CXXMDTARGET)
	$(INSTALL) -m 775 -o $(PCP_USER) -g $(PCP_GROUP) -d $(PCP_LOG_DIR)/pmmgr
else
build-me:
install:
endif

$(OBJECTS): $(HFILES)

include $(BUILDRULES)

default_pcp : default

install_pcp : install

.PHONY: build-me
