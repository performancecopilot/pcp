#
# Copyright (c) 2013-2018 Red Hat.
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

TOPDIR = ../../..
include $(TOPDIR)/src/include/builddefs
-include ./GNUlocaldefs

HFILES = pmapi.h impl.h pmda.h pmtime.h pmdaroot.h pmafm.h \
	 trace.h trace_dev.h mmv_stats.h mmv_dev.h import.h \
	 config32.h config64.h platform32.h platform64.h \
	 pmjson.h pmhttp.h pmdbg.h deprecated.h
INFILES = config.h.in configsz.h.in platform_defs.h.in platformsz.h.in
CONFFILES = config.h configsz.h platform_defs.h platformsz.h
EXTERNAL_HFILES = sds.h
GENERATED_HFILES = $(CONFFILES)
NOSHIP_HFILES = libpcp.h series.h sds.h

LDIRT = $(GENERATED_HFILES)

default :: default_pcp

default_pcp : $(HEADERS) $(GENERATED_HFILES) $(EXTERNAL_HFILES)
	@if grep HAVE_64BIT config.h.in >/dev/null; then \
	   echo "HAVE_64 in config.h.in; move to configsz.h.in for multilib." \
	   "This requires manual intervention after running an 'autoreconf'." \
	   "Nuke it with fire... or just discard the change to config.h.in in git."; \
	   exit 1; \
	fi
	@if grep PM_SIZEOF config.h.in >/dev/null; then \
	   echo "PM_SIZEOF macro in config.h.in; move to configsz.h.in for multilib." \
	   "This requires manual intervention after running an 'autoreconf'." \
	   "Nuke it with fire... or just discard the change to config.h.in in git."; \
	   exit 1; \
	fi

include $(BUILDRULES)

$(EXTERNAL_HFILES):
	$(LN_S) -f $(TOPDIR)/src/external/$@ .

install :: default_pcp install_pcp

install_pcp : default_pcp
	$(INSTALL) -m 644 $(HFILES) $(GENERATED_HFILES) $(PCP_INC_DIR)
