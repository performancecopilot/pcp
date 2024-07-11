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

TOPDIR = ../..
include $(TOPDIR)/src/include/builddefs

CMDTARGET = pmstat$(EXECSUFFIX)
CFILES  = pmstat.c
LDIRT = $(CMDTARGET) pmstat.pmlogger.options

CONFIGDIR = $(PCP_SYSCONF_DIR)/pmlogger
CONFIGVARDIR = $(PCP_VAR_DIR)/config/pmlogger

LLDFLAGS = -L$(TOPDIR)/src/libpcp_gui/src
LLDLIBS = $(PCP_GUILIB)

default:	$(CMDTARGET) pmstat.pmlogger.options

include $(BUILDRULES)

install:	default
	$(INSTALL) -m 755 $(CMDTARGET) $(PCP_BIN_DIR)/$(CMDTARGET)
	$(INSTALL) -m 644 -t $(CONFIGVARDIR)/config.pmstat pmstat.pmlogger $(CONFIGDIR)/config.pmstat
	$(INSTALL) -m 644 -t $(CONFIGVARDIR)/options.pmstat pmstat.pmlogger.options $(CONFIGDIR)/options.pmstat

default_pcp:	default

install_pcp:	install

pmstat.o:	$(TOPDIR)/src/include/pcp/libpcp.h

pmstat.pmlogger.options:
	echo '# pmlogger config options for pmstat.config' >pmstat.pmlogger.options
	echo >>pmstat.pmlogger.options
ifeq ($(PCP_PLATFORM),openbsd)
	echo '# no disk block stats for OpenBSD' >>pmstat.pmlogger.options
	echo '%undef have_disk_block_stats' >>pmstat.pmlogger.options
else
	echo '%define have_disk_block_stats' >>pmstat.pmlogger.options
endif
ifeq ($(PCP_PLATFORM),linux)
	echo '%define have_cpu_virt_stats' >>pmstat.pmlogger.options
else
	echo '# no CPU virtualization metrics' >>pmstat.pmlogger.options
	echo '%undef have_cpu_virt_stats' >>pmstat.pmlogger.options
endif
