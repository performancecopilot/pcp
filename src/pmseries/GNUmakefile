#
# Copyright (c) 2017-2019,2025 Red Hat.
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

CFILES = pmseries.c
CMDTARGET = pmseries$(EXECSUFFIX)
CMDIMPORT = pmseries_import
CONTAINER = build/containers/archive-analysis/root/usr/bin/archive-import.py
XFILES = $(CMDIMPORT).py
LLDLIBS	= $(PCP_WEBLIB)
LCFLAGS = $(LIBUVCFLAGS)
ifeq "$(TARGET_OS)" "mingw"
LLDLIBS += -lws2_32
endif
LDIRT += $(XFILES) $(CMDIMPORT).service
BASHDIR = $(PCP_BASHSHARE_DIR)/completions

default:	build-me

ifeq "$(HAVE_LIBUV)" "true"
build-me:	$(CMDTARGET) $(XFILES) $(CMDIMPORT).service

install:	default
	$(INSTALL) -m 755 $(CMDTARGET) $(PCP_BIN_DIR)/$(CMDTARGET)
	$(INSTALL) -m 755 -d $(PCP_SYSCONF_DIR)/pmseries
	# compatibility symlink (sharing pmproxy config key server list)
	$(INSTALL) -S $(PCP_SYSCONF_DIR)/pmproxy/pmproxy.conf $(PCP_SYSCONF_DIR)/pmseries/pmseries.conf
	$(INSTALL) -S $(BASHDIR)/pcp $(BASHDIR)/$(CMDTARGET)
ifeq ($(ENABLE_PYTHON3),true)
	$(INSTALL) -m 755 $(CMDIMPORT).py $(PCP_BINADM_DIR)/$(CMDIMPORT)
ifeq ($(ENABLE_SYSTEMD),true)
	$(INSTALL) -m 644 $(CMDIMPORT).service $(PCP_SYSTEMDUNIT_DIR)/$(CMDIMPORT).service
endif
endif
else
build-me:
install:
endif

include $(BUILDRULES)

# TODO: move this file here instead of symlinking, once transition completed
$(XFILES):
	$(LN_S) $(TOPDIR)/$(CONTAINER) $@

default_pcp : default

install_pcp : install

$(CMDIMPORT).service: $(CMDIMPORT).service.in
	$(SED) <$< >$@ \
	    -e 's;@PCP_BINADM_DIR@;'$(PCP_BINADM_DIR)';' \
	    -e 's;@PCP_RUN_DIR@;'$(PCP_RUN_DIR)';' \
	# END
