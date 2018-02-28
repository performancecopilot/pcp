#
# Copyright (c) 2018 Red Hat.
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
-include ./GNUlocaldefs

CONFIGDIR = $(PCP_SYSCONF_DIR)/discover
CONFIGS = $(shell echo *.conf)

SCRIPTDIR = $(PCP_BINADM_DIR)/discover
SHSCRIPTS = $(shell echo *.sh | sed -e 's/\.sh$$//g')

default:	$(CONFIGFILES)

include $(BUILDRULES)

install:	default
	$(INSTALL) -m 755 -d $(CONFIGDIR)
	$(INSTALL) -m 644 $(CONFIGS) $(CONFIGDIR)/$(CONFIGS)
	$(INSTALL) -m 755 -d $(SCRIPTDIR)
	for script in $(SHSCRIPTS); \
	do \
	    $(INSTALL) -m 755 $${script}.sh $(SCRIPTDIR)/$${script}; \
	done

default_pcp:	default

install_pcp:	install
