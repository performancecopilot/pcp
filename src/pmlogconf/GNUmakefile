#
# Copyright (c) 2011-2018,2020,2023 Red Hat.
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

TOPDIR = ../..
include $(TOPDIR)/src/include/builddefs

CMDTARGET = pmlogconf$(EXECSUFFIX)
DISTARGET = $(PCP_BIN_DIR)/$(CMDTARGET)
CFILES = util.c pmlogconf.c pmrepconf.c
NFILES = util.h pmlogconf.h
LLDLIBS = $(PCPLIB) $(LIB_FOR_REGEX)
LDIRT = pmrepconf

SUBDIRS = \
	cpu \
	disk \
	filesystem \
        gfs2 \
	kernel \
	kvm \
	libvirt \
	memory \
	networking \
	openmetrics \
	platform \
	statsd \
	storage \
	tools \
	services

# Only evaluate zeroconf configurations if we package that.
# Otherwise zeroconf will be installed for all platforms as
# the default, which is not what we want (it's opt-in).
#
ifeq "$(TARGET_OS)" "linux"
SUBDIRS += zeroconf
endif

default: $(CMDTARGET) pmrepconf makefiles

include $(BUILDRULES)

install: default $(SUBDIRS)
	$(SUBDIRS_MAKERULE)
	$(INSTALL) -m 755 $(CMDTARGET) $(DISTARGET)
	$(INSTALL) -S $(DISTARGET) $(PCP_BIN_DIR)/pmrepconf
	$(INSTALL) -S $(DISTARGET) $(PCP_BINADM_DIR)/$(CMDTARGET)

default_pcp : default

install_pcp : install

pmrepconf:	$(CMDTARGET)
	@$(LN_S) -f $^ $@

# for src-link-pcp target from buildrules
$(SUBDIRS):	makefiles

makefiles:
	@for d in $(SUBDIRS); do \
	    rm -f $$d/GNUmakefile; \
	    cp GNUmakefile.groups $$d/GNUmakefile; \
	done

groups: $(SUBDIRS)
	$(SUBDIRS_MAKERULE)
