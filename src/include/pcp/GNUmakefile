#
# Copyright (c) 2013 Red Hat.
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

HFILES = pmapi.h impl.h pmda.h pmtime.h pmafm.h pmiestats.h \
	 trace.h trace_dev.h mmv_stats.h mmv_dev.h import.h
GENERATED_HFILES = pmdbg.h platform_defs.h config.h
CONFFILES = config.h platform_defs.h

LSRCFILES = config.h.in platform_defs.h.in mk_pmdbg fault.h
LDIRT = $(GENERATED_HFILES)

default :: default_pcp

default_pcp : $(HEADERS) $(GENERATED_HFILES)

include $(BUILDRULES)

install :: default_pcp install_pcp

install_pcp : default_pcp
	$(INSTALL) -m 644 $(HFILES) $(GENERATED_HFILES) $(PCP_INC_DIR)

pmdbg.h : pmapi.h impl.h
	./mk_pmdbg
