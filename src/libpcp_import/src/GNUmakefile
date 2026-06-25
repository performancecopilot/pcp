#
# Copyright (c) 2013-2015 Red Hat.
# Copyright (c) 2001,2009 Silicon Graphics, Inc.  All Rights Reserved.
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

TOPDIR = ../../..
include $(TOPDIR)/src/include/builddefs
-include ./GNUlocaldefs

CFILES	= import.c stuff.c archive.c
HFILES	= private.h

LIBCONFIG = libpcp_import.pc
TMPFILESCONF = libpcp_import.conf
VERSION_SCRIPT = exports

STATICLIBTARGET	= libpcp_import.a
DSOVERSION = 2
LIBTARGET = libpcp_import.$(DSOSUFFIX).$(DSOVERSION)
SYMTARGET = libpcp_import.$(DSOSUFFIX) 
ifeq "$(TARGET_OS)" "darwin"
LIBTARGET = libpcp_import.$(DSOVERSION).$(DSOSUFFIX)
endif
ifeq "$(TARGET_OS)" "mingw"
LIBTARGET = libpcp_import.$(DSOSUFFIX) 
SYMTARGET =
STATICLIBTARGET	=
endif
ifeq "$(ENABLE_SHARED)" "no"
LIBTARGET =
SYMTARGET =
endif

LCFLAGS = -DPMI_INTERNAL
LLDLIBS = -lpcp -lpcp_archive
LDIRT = $(SYMTARGET) domain.h $(LIBCONFIG) $(TMPFILESCONF)

DOMAIN = PMI_DOMAIN

default:  $(LIBTARGET) $(SYMTARGET) $(STATICLIBTARGET) $(LIBCONFIG) $(TMPFILESCONF)

$(OBJECTS): $(HFILES) domain.h

include $(BUILDRULES)

install: default
ifneq ($(PCP_SYSTEMDTMPFILES_DIR),)
	$(INSTALL) -m 644 $(TMPFILESCONF) $(PCP_SYSTEMDTMPFILES_DIR)/$(TMPFILESCONF)
endif
ifneq ($(PCP_IMPORT_DIR),)
	$(INSTALL) -m 755 -d $(PCP_IMPORT_DIR)
endif
ifneq ($(LIBTARGET),)
	$(INSTALL) -m 755 $(LIBTARGET) $(PCP_LIB_DIR)/$(LIBTARGET)
ifneq ($(DEB_HOST_MULTIARCH),)
	$(INSTALL) -m 644 $(LIBCONFIG) $(PCP_LIB_DIR)/$(DEB_HOST_MULTIARCH)/pkgconfig/$(LIBCONFIG)
else
	$(INSTALL) -m 644 $(LIBCONFIG) $(PCP_LIB_DIR)/pkgconfig/$(LIBCONFIG)
endif
endif
ifneq ($(SYMTARGET),)
	for tt in $(SYMTARGET); do \
		$(INSTALL) -S $(LIBTARGET) $(PCP_LIB_DIR)/$$tt || exit 1; \
	done
endif
ifneq ($(STATICLIBTARGET),)
	$(INSTALL) -m 755 $(STATICLIBTARGET) $(PCP_LIB_DIR)/$(STATICLIBTARGET)
endif

default_pcp: default

install_pcp: install

ifneq ($(SYMTARGET),)
$(SYMTARGET):
	$(LN_S) -f $(LIBTARGET) $@
endif

ifneq ($(LIBTARGET),)
$(LIBTARGET): $(VERSION_SCRIPT)
$(LIBCONFIG): $(LIBCONFIG).in
	@sed -e's|@PREFIX@|$(PCP_PREFIX)|g' \
	     -e's|@LIBDIR@|$(PCP_LIB_DIR)|g' \
	     -e's|@INCDIR@|$(PCP_INC_DIR)|g' \
	     -e's|@VERSION@|$(PCP_VERSION)|g' <$? >$@
endif

$(TMPFILESCONF): $(TMPFILESCONF).in
	@sed -e's|@PCP_IMPORT_DIR@|$(PCP_IMPORT_DIR)|g' <$< >$@

domain.h: $(TOPDIR)/src/pmns/stdpmid
	$(DOMAIN_MAKERULE)

$(OBJECTS):	$(TOPDIR)/src/include/pcp/libpcp.h
